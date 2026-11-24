// netlink -- see netlink.h.
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>

#include "netlink.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <memory>
#include <thread>

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#endif
#ifndef WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY
#define WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY 4
#endif

namespace cw {

// ---- text helpers -------------------------------------------------------------

std::string netHex(const void* p, size_t n)
{
    static const char* d = "0123456789abcdef";
    const uint8_t* b = (const uint8_t*)p;
    std::string s;
    s.reserve(n * 2);
    for (size_t i = 0; i < n; i++) { s += d[b[i] >> 4]; s += d[b[i] & 15]; }
    return s;
}

bool netUnhex(const std::string& s, std::vector<uint8_t>& out)
{
    out.clear();
    if (s.size() % 2) return false;
    auto v = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    for (size_t i = 0; i < s.size(); i += 2)
    {
        const int a = v(s[i]), b = v(s[i + 1]);
        if (a < 0 || b < 0) return false;
        out.push_back((uint8_t)(a * 16 + b));
    }
    return true;
}

static const char* kB64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string netBase64(const void* p, size_t n)
{
    const uint8_t* b = (const uint8_t*)p;
    std::string s;
    s.reserve((n + 2) / 3 * 4);
    for (size_t i = 0; i < n; i += 3)
    {
        const uint32_t v = (uint32_t)b[i] << 16 | (i + 1 < n ? (uint32_t)b[i + 1] << 8 : 0) | (i + 2 < n ? b[i + 2] : 0);
        s += kB64[(v >> 18) & 63];
        s += kB64[(v >> 12) & 63];
        s += i + 1 < n ? kB64[(v >> 6) & 63] : '=';
        s += i + 2 < n ? kB64[v & 63] : '=';
    }
    return s;
}

bool netUnbase64(const std::string& s, std::vector<uint8_t>& out)
{
    out.clear();
    int map[256];
    for (int& m : map) m = -1;
    for (int i = 0; i < 64; i++) map[(uint8_t)kB64[i]] = i;
    uint32_t acc = 0;
    int bits = 0;
    for (char ch : s)
    {
        if (ch == '=' || ch == '\r' || ch == '\n') continue;
        const int v = map[(uint8_t)ch];
        if (v < 0) return false;
        acc = (acc << 6) | (uint32_t)v;
        bits += 6;
        if (bits >= 8) { bits -= 8; out.push_back((uint8_t)((acc >> bits) & 0xFF)); }
    }
    return true;
}

bool netRandom(void* p, size_t n)
{
    return BCryptGenRandom(NULL, (PUCHAR)p, (ULONG)n, BCRYPT_USE_SYSTEM_PREFERRED_RNG) == STATUS_SUCCESS;
}

std::string netRandomHex(size_t bytes)
{
    std::vector<uint8_t> b(bytes);
    netRandom(b.data(), b.size());
    return netHex(b.data(), b.size());
}

bool netTakeBlock(std::string& in, std::string& block)
{
    size_t pos = 0;
    for (;;)
    {
        const size_t nl = in.find('\n', pos);
        if (nl == std::string::npos) return false;
        size_t len = nl - pos;
        if (len && in[pos + len - 1] == '\r') len--;
        if (len == 3 && in.compare(pos, 3, "end") == 0)
        {
            block = in.substr(0, nl + 1);
            in.erase(0, nl + 1);
            return true;
        }
        pos = nl + 1;
    }
}

std::string netFirstLine(const std::string& block)
{
    const size_t nl = block.find('\n');
    std::string s = nl == std::string::npos ? block : block.substr(0, nl);
    if (!s.empty() && s.back() == '\r') s.pop_back();
    return s;
}

std::string netField(const std::string& block, const char* key)
{
    const std::string k = std::string(key) + " ";
    size_t pos = 0;
    while (pos < block.size())
    {
        size_t nl = block.find('\n', pos);
        if (nl == std::string::npos) nl = block.size();
        if (block.compare(pos, k.size(), k) == 0)
        {
            std::string v = block.substr(pos + k.size(), nl - pos - k.size());
            if (!v.empty() && v.back() == '\r') v.pop_back();
            return v;
        }
        pos = nl + 1;
    }
    return std::string();
}

// ---- hashing and sealing (Windows CNG) ------------------------------------------

static bool hashOnce(LPCWSTR alg, const void* key, size_t keyLen, const void* data, size_t len, uint8_t* out, size_t outLen)
{
    BCRYPT_ALG_HANDLE a = NULL;
    BCRYPT_HASH_HANDLE h = NULL;
    bool ok = BCryptOpenAlgorithmProvider(&a, alg, NULL, key ? BCRYPT_ALG_HANDLE_HMAC_FLAG : 0) == STATUS_SUCCESS &&
              BCryptCreateHash(a, &h, NULL, 0, (PUCHAR)key, (ULONG)keyLen, 0) == STATUS_SUCCESS &&
              BCryptHashData(h, (PUCHAR)data, (ULONG)len, 0) == STATUS_SUCCESS &&
              BCryptFinishHash(h, out, (ULONG)outLen, 0) == STATUS_SUCCESS;
    if (h) BCryptDestroyHash(h);
    if (a) BCryptCloseAlgorithmProvider(a, 0);
    return ok;
}

bool netDeriveKey(const std::string& password, const std::string& saltHex, NetKey& key)
{
    key = NetKey();
    std::vector<uint8_t> salt;
    if (!netUnhex(saltHex, salt) || salt.empty()) return false;
    BCRYPT_ALG_HANDLE a = NULL;
    bool ok = BCryptOpenAlgorithmProvider(&a, BCRYPT_SHA256_ALGORITHM, NULL, BCRYPT_ALG_HANDLE_HMAC_FLAG) == STATUS_SUCCESS &&
              BCryptDeriveKeyPBKDF2(a, (PUCHAR)password.data(), (ULONG)password.size(), salt.data(), (ULONG)salt.size(),
                                    100000, key.k, sizeof(key.k), 0) == STATUS_SUCCESS;
    if (a) BCryptCloseAlgorithmProvider(a, 0);
    key.set = ok;
    return ok;
}

std::string netProof(const NetKey& key, const std::string& nonceHex)
{
    const std::string msg = "cwest-proof|" + nonceHex;
    uint8_t mac[32];
    if (!hashOnce(BCRYPT_SHA256_ALGORITHM, key.k, sizeof(key.k), msg.data(), msg.size(), mac, sizeof(mac))) return std::string();
    return netHex(mac, sizeof(mac));
}

// AES-256-GCM with a fresh random 12-byte nonce per message:
// data = base64(nonce | tag | ciphertext)
static bool aesGcm(const NetKey& key, bool encrypt, const uint8_t* iv, uint8_t* tag,
                   const uint8_t* in, size_t len, uint8_t* out)
{
    BCRYPT_ALG_HANDLE a = NULL;
    BCRYPT_KEY_HANDLE k = NULL;
    bool ok = BCryptOpenAlgorithmProvider(&a, BCRYPT_AES_ALGORITHM, NULL, 0) == STATUS_SUCCESS &&
              BCryptSetProperty(a, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM,
                                sizeof(BCRYPT_CHAIN_MODE_GCM), 0) == STATUS_SUCCESS &&
              BCryptGenerateSymmetricKey(a, &k, NULL, 0, (PUCHAR)key.k, sizeof(key.k), 0) == STATUS_SUCCESS;
    if (ok)
    {
        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
        memset(&info, 0, sizeof(info));
        info.cbSize = sizeof(info);
        info.dwInfoVersion = BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO_VERSION;
        info.pbNonce = (PUCHAR)iv;
        info.cbNonce = 12;
        info.pbTag = tag;
        info.cbTag = 16;
        ULONG done = 0;
        const NTSTATUS st = encrypt
            ? BCryptEncrypt(k, (PUCHAR)in, (ULONG)len, &info, NULL, 0, out, (ULONG)len, &done, 0)
            : BCryptDecrypt(k, (PUCHAR)in, (ULONG)len, &info, NULL, 0, out, (ULONG)len, &done, 0);
        ok = st == STATUS_SUCCESS && done == (ULONG)len;
    }
    if (k) BCryptDestroyKey(k);
    if (a) BCryptCloseAlgorithmProvider(a, 0);
    return ok;
}

std::string netSeal(const NetKey& key, const std::string& block)
{
    std::vector<uint8_t> buf(12 + 16 + block.size());
    netRandom(buf.data(), 12);
    uint8_t* ct = buf.data() + 28;
    if (!block.empty() && !aesGcm(key, true, buf.data(), buf.data() + 12, (const uint8_t*)block.data(), block.size(), ct))
        return std::string();
    if (block.empty()) return std::string();
    return "sealed 1\ndata " + netBase64(buf.data(), buf.size()) + "\nend\n";
}

bool netUnseal(const NetKey& key, const std::string& sealed, std::string& block)
{
    std::vector<uint8_t> buf;
    if (!netUnbase64(netField(sealed, "data"), buf) || buf.size() <= 28) return false;
    std::string out(buf.size() - 28, '\0');
    if (!aesGcm(key, false, buf.data(), buf.data() + 12, buf.data() + 28, out.size(), (uint8_t*)&out[0])) return false;
    block.swap(out);
    return true;
}

// ---- WebSocket, the host's side ---------------------------------------------------

static std::string lower(std::string s)
{
    for (char& c : s) c = (char)tolower((unsigned char)c);
    return s;
}

static std::string trim(const std::string& s)
{
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t')) a++;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) b--;
    return s.substr(a, b - a);
}

bool wsServerHandshake(std::string& in, std::string& response, bool& upgrade, std::string& clientIp)
{
    upgrade = false;
    const size_t end = in.find("\r\n\r\n");
    if (end == std::string::npos)
    {
        if (in.size() < 16384) return false;          // not all here yet
        response = "HTTP/1.1 431 Request Header Fields Too Large\r\nConnection: close\r\nContent-Length: 0\r\n\r\n";
        in.clear();
        return true;
    }
    const std::string head = in.substr(0, end);
    in.erase(0, end + 4);
    std::string key, up;
    size_t pos = head.find("\r\n");
    while (pos != std::string::npos && pos < head.size())
    {
        pos += 2;
        size_t nl = head.find("\r\n", pos);
        if (nl == std::string::npos) nl = head.size();
        const std::string line = head.substr(pos, nl - pos);
        const size_t colon = line.find(':');
        if (colon != std::string::npos)
        {
            const std::string name = lower(trim(line.substr(0, colon))), value = trim(line.substr(colon + 1));
            if (name == "sec-websocket-key") key = value;
            else if (name == "upgrade") up = lower(value);
            else if (name == "cf-connecting-ip") clientIp = value;
        }
        pos = nl;
    }
    if (up.find("websocket") != std::string::npos && !key.empty())
    {
        const std::string src = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        uint8_t sha[20];
        hashOnce(BCRYPT_SHA1_ALGORITHM, nullptr, 0, src.data(), src.size(), sha, sizeof(sha));
        response = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                   "Sec-WebSocket-Accept: " + netBase64(sha, sizeof(sha)) + "\r\n\r\n";
        upgrade = true;
        return true;
    }
    const std::string body =
        "This is a CWEST8086 tournament host.\r\n\r\n"
        "To join: open CWEST8086, choose Join on the Competition panel, and paste this page's address.\r\n";
    response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\nConnection: close\r\n"
               "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
    return true;
}

static std::string wsFrame(int opcode, const char* p, size_t n)
{
    std::string f;
    f += (char)(0x80 | opcode);
    if (n < 126) f += (char)n;
    else if (n <= 0xFFFF) { f += (char)126; f += (char)(n >> 8); f += (char)(n & 0xFF); }
    else { f += (char)127; for (int i = 7; i >= 0; i--) f += (char)((uint64_t)n >> (8 * i)); }
    f.append(p, n);
    return f;
}

bool wsServerDecode(std::string& in, std::string& out, std::string& reply)
{
    for (;;)
    {
        if (in.size() < 2) return true;
        const uint8_t b0 = (uint8_t)in[0], b1 = (uint8_t)in[1];
        const int op = b0 & 0x0F;
        const bool masked = (b1 & 0x80) != 0;
        uint64_t len = b1 & 0x7F;
        size_t at = 2;
        if (len == 126)
        {
            if (in.size() < 4) return true;
            len = ((uint64_t)(uint8_t)in[2] << 8) | (uint8_t)in[3];
            at = 4;
        }
        else if (len == 127)
        {
            if (in.size() < 10) return true;
            len = 0;
            for (int i = 0; i < 8; i++) len = (len << 8) | (uint8_t)in[2 + i];
            at = 10;
        }
        if (len > (64ull << 20)) return false;            // no message is anywhere near this big
        const size_t maskAt = at;
        if (masked) at += 4;
        if (in.size() < at + len) return true;
        std::string payload = in.substr(at, (size_t)len);
        if (masked)
            for (size_t i = 0; i < payload.size(); i++) payload[i] ^= in[maskAt + (i & 3)];
        in.erase(0, at + (size_t)len);
        switch (op)
        {
        case 0: case 1: case 2: out += payload; break;   // data (continuations simply follow on)
        case 8: return false;                             // the joiner closed
        case 9: reply += wsFrame(10, payload.data(), payload.size()); break;
        default: break;                                   // pongs and the rest
        }
    }
}

std::string wsServerFrames(const std::string& data)
{
    std::string f;
    for (size_t at = 0; at < data.size(); at += 65536)
        f += wsFrame(2, data.data() + at, std::min<size_t>(65536, data.size() - at));
    return f;
}

std::string wsServerPing() { return wsFrame(9, "", 0); }

// ---- the joiner's link ------------------------------------------------------------

bool netIsWebAddress(const std::string& address)
{
    const std::string a = lower(trim(address));
    return a.rfind("https://", 0) == 0 || a.rfind("http://", 0) == 0 || a.rfind("wss://", 0) == 0 ||
           a.rfind("ws://", 0) == 0 || a.find(".trycloudflare.com") != std::string::npos;
}

static std::wstring widen(const std::string& s)
{
    std::wstring w(s.size(), L'\0');
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], (int)w.size());
    w.resize(n > 0 ? (size_t)n : 0);
    return w;
}

static std::string winhttpWhy(DWORD e, const std::string& host)
{
    switch (e)
    {
    case 12007: return "cannot find " + host + " -- check the address (a tunnel address changes every time the host starts listening)";
    case 12029: return "cannot reach " + host + " -- no internet here, or something on this network blocks it";
    case 12002: return "timed out reaching " + host;
    case 12175: case 12044: case 12045: case 12169: return "the secure connection to " + host + " failed (" + std::to_string(e) + ")";
    case 12030: return "the connection to " + host + " dropped";
    default: break;
    }
    return "could not connect to " + host + " (error " + std::to_string(e) + ")";
}

// Everything the link's threads share. It outlives NetLink::close(): a dial
// stuck in a slow DNS lookup finishes on its own and then lets go.
struct NetLinkCore
{
    std::string address, host, path;
    int    port = 0;
    bool   web = false, secure = false;
    double retryFor = 0.0;

    std::atomic<int>  state{NetLink::Connecting};
    std::atomic<bool> stop{false};
    std::atomic<int>  attempts{0};
    std::mutex mu;                       // why, in, out
    std::condition_variable cv;
    std::string why, in, out;

    std::atomic<uintptr_t> sock{(uintptr_t)INVALID_SOCKET};
    std::mutex hmu;                      // the WinHTTP handles
    HINTERNET session = NULL, connect = NULL, ws = NULL;
    std::atomic<bool> sending{false};

    void setWhy(const std::string& w) { std::lock_guard<std::mutex> lk(mu); why = w; }

    // Closes the WebSocket once, whoever gets there first; it wakes a
    // receive that is waiting on it. `tryOnly` (the caller's close) gives up
    // rather than wait behind a send in progress -- the sender then sees
    // `stop` and the link's own thread closes it.
    void dropWs(bool tryOnly = false)
    {
        std::unique_lock<std::mutex> lk(hmu, std::defer_lock);
        if (tryOnly) { if (!lk.try_lock()) return; }
        else lk.lock();
        if (ws) { WinHttpCloseHandle(ws); ws = NULL; }
    }
    void dropAll()
    {
        std::lock_guard<std::mutex> lk(hmu);
        if (ws) { WinHttpCloseHandle(ws); ws = NULL; }
        if (connect) { WinHttpCloseHandle(connect); connect = NULL; }
        if (session) { WinHttpCloseHandle(session); session = NULL; }
    }

    bool dialTcp(std::string& w);
    bool dialWeb(std::string& w);
    void pumpTcp();
    void pumpWeb();
    void sender();
    void run();
};

bool NetLinkCore::dialTcp(std::string& w)
{
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) != 0 || !res)
    {
        w = "cannot resolve " + host;
        return false;
    }
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) { freeaddrinfo(res); w = "no socket"; return false; }
    u_long nb = 1;
    ioctlsocket(s, FIONBIO, &nb);
    ::connect(s, res->ai_addr, (int)res->ai_addrlen);
    freeaddrinfo(res);
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    for (;;)
    {
        fd_set wr, ex;
        FD_ZERO(&wr); FD_ZERO(&ex);
        FD_SET(s, &wr); FD_SET(s, &ex);
        timeval tv{0, 100000};
        select(0, nullptr, &wr, &ex, &tv);
        if (FD_ISSET(s, &ex))
        {
            int err = 0, len = sizeof(err);
            getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&err, &len);
            closesocket(s);
            w = err == WSAECONNREFUSED ? "refused: nothing listens there (is the host listening?)"
                                       : "could not connect (error " + std::to_string(err) + ")";
            return false;
        }
        if (FD_ISSET(s, &wr)) break;
        if (stop.load() || std::chrono::steady_clock::now() > until)
        {
            closesocket(s);
            w = "no answer from " + host + ":" + std::to_string(port);
            return false;
        }
    }
    nb = 0;
    ioctlsocket(s, FIONBIO, &nb);
    BOOL nd = TRUE;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char*)&nd, sizeof(nd));
    sock.store((uintptr_t)s);
    return true;
}

bool NetLinkCore::dialWeb(std::string& w)
{
    HINTERNET ses = WinHttpOpen(L"CWEST8086", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!ses) ses = WinHttpOpen(L"CWEST8086", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!ses) { w = "WinHTTP would not start"; return false; }
    // The host pings every 20 s, so a minute of silence means the line is dead.
    WinHttpSetTimeouts(ses, 15000, 15000, 30000, 60000);
    HINTERNET con = WinHttpConnect(ses, widen(host).c_str(), (INTERNET_PORT)port, 0);
    HINTERNET req = con ? WinHttpOpenRequest(con, L"GET", widen(path).c_str(), NULL, WINHTTP_NO_REFERER,
                                             WINHTTP_DEFAULT_ACCEPT_TYPES, secure ? WINHTTP_FLAG_SECURE : 0) : NULL;
    bool ok = req && WinHttpSetOption(req, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, NULL, 0) &&
              WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
              WinHttpReceiveResponse(req, NULL);
    if (!ok)
    {
        w = winhttpWhy(GetLastError(), host);
        if (req) WinHttpCloseHandle(req);
        if (con) WinHttpCloseHandle(con);
        WinHttpCloseHandle(ses);
        return false;
    }
    DWORD status = 0, sz = sizeof(status);
    WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                        &status, &sz, WINHTTP_NO_HEADER_INDEX);
    HINTERNET sock2 = status == 101 ? WinHttpWebSocketCompleteUpgrade(req, 0) : NULL;
    WinHttpCloseHandle(req);
    if (!sock2)
    {
        if (status == 530 || status == 1033) w = "the host is not listening any more (its tunnel is down) -- ask for the new address";
        else if (status == 502 || status == 504) w = "the host's tunnel is up but CWEST8086 is not answering behind it";
        else if (status == 101) w = "the WebSocket would not open";
        else w = host + " answered HTTP " + std::to_string(status) + " -- not a CWEST8086 host?";
        WinHttpCloseHandle(con);
        WinHttpCloseHandle(ses);
        return false;
    }
    std::lock_guard<std::mutex> lk(hmu);
    session = ses;
    connect = con;
    ws = sock2;
    if (stop.load()) { WinHttpCloseHandle(ws); ws = NULL; }
    return ws != NULL;
}

void NetLinkCore::pumpTcp()
{
    const SOCKET s = (SOCKET)sock.load();
    char buf[1 << 15];
    while (!stop.load())
    {
        fd_set rd;
        FD_ZERO(&rd);
        FD_SET(s, &rd);
        timeval tv{0, 200000};
        const int r = select(0, &rd, nullptr, nullptr, &tv);
        if (r == SOCKET_ERROR) break;
        if (r == 0) continue;
        const int n = recv(s, buf, sizeof(buf), 0);
        if (n <= 0) break;
        std::lock_guard<std::mutex> lk(mu);
        in.append(buf, (size_t)n);
    }
}

void NetLinkCore::pumpWeb()
{
    std::vector<char> buf(1 << 16);
    for (;;)
    {
        HINTERNET h;
        { std::lock_guard<std::mutex> lk(hmu); h = ws; }
        if (!h || stop.load()) break;
        DWORD got = 0;
        WINHTTP_WEB_SOCKET_BUFFER_TYPE type;
        const DWORD err = WinHttpWebSocketReceive(h, buf.data(), (DWORD)buf.size(), &got, &type);
        if (err != NO_ERROR || type == WINHTTP_WEB_SOCKET_CLOSE_BUFFER_TYPE) break;
        std::lock_guard<std::mutex> lk(mu);
        in.append(buf.data(), got);
    }
}

void NetLinkCore::sender()
{
    for (;;)
    {
        std::string chunk;
        {
            std::unique_lock<std::mutex> lk(mu);
            cv.wait(lk, [&] { return !out.empty() || stop.load() || state.load() != NetLink::Open; });
            if (stop.load() || state.load() != NetLink::Open) return;
            chunk.swap(out);
        }
        bool ok = true;
        for (size_t at = 0; ok && at < chunk.size(); )
        {
            const size_t n = std::min<size_t>(65536, chunk.size() - at);
            if (web)
            {
                std::lock_guard<std::mutex> lk(hmu);
                ok = ws && WinHttpWebSocketSend(ws, WINHTTP_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE,
                                                (PVOID)(chunk.data() + at), (DWORD)n) == NO_ERROR;
            }
            else
            {
                const int k = send((SOCKET)sock.load(), chunk.data() + at, (int)n, 0);
                ok = k > 0;
                if (ok) { at += (size_t)k; continue; }
            }
            at += n;
        }
        if (!ok) return;                // the reader sees the line drop
    }
}

void NetLinkCore::run()
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds((long long)(retryFor * 1000.0));
    for (;;)
    {
        if (stop.load()) return;
        attempts.fetch_add(1);
        std::string w;
        if (web ? dialWeb(w) : dialTcp(w)) break;
        setWhy(w);
        if (stop.load()) return;
        if (std::chrono::steady_clock::now() >= deadline) { state.store(NetLink::Failed); return; }
        for (int i = 0; i < 20 && !stop.load(); i++) Sleep(100);
    }
    setWhy("");
    state.store(NetLink::Open);
    std::thread snd([this] { sender(); });
    if (web) pumpWeb(); else pumpTcp();
    if (!stop.load())
    {
        std::lock_guard<std::mutex> lk(mu);
        if (why.empty()) why = "the host went away";
    }
    state.store(stop.load() ? NetLink::Idle : NetLink::Failed);
    cv.notify_all();
    // wake a send stuck on a dead line, then let go of everything
    if (!web) shutdown((SOCKET)sock.load(), SD_BOTH);
    dropWs();
    snd.join();
    if (!web) { closesocket((SOCKET)sock.load()); sock.store((uintptr_t)INVALID_SOCKET); }
    dropAll();
}

std::shared_ptr<NetLinkCore> NetLink::core() const
{
    std::lock_guard<std::mutex> lk(m_mu);
    return m_core;
}

void NetLink::open(const std::string& address, int defaultPort, double retryFor)
{
    close();
    auto c = std::make_shared<NetLinkCore>();
    c->address = trim(address);
    c->retryFor = retryFor;
    c->web = netIsWebAddress(c->address);
    std::string a = c->address;
    if (c->web)
    {
        const std::string la = lower(a);
        c->secure = !(la.rfind("http://", 0) == 0 || la.rfind("ws://", 0) == 0);
        const size_t sch = a.find("://");
        if (sch != std::string::npos) a = a.substr(sch + 3);
        const size_t slash = a.find('/');
        c->path = slash == std::string::npos ? "/" : a.substr(slash);
        if (slash != std::string::npos) a = a.substr(0, slash);
        c->port = c->secure ? 443 : 80;
    }
    else c->port = defaultPort;
    const size_t colon = a.rfind(':');
    if (colon != std::string::npos) { c->port = atoi(a.c_str() + colon + 1); a = a.substr(0, colon); }
    c->host = a;
    {
        std::lock_guard<std::mutex> lk(m_mu);
        m_core = c;
    }
    std::thread([c] { c->run(); }).detach();
}

void NetLink::close()
{
    std::shared_ptr<NetLinkCore> c;
    {
        std::lock_guard<std::mutex> lk(m_mu);
        c.swap(m_core);
    }
    if (!c) return;
    c->stop.store(true);
    c->cv.notify_all();
    if (!c->web)
    {
        const uintptr_t s = c->sock.load();
        if (s != (uintptr_t)INVALID_SOCKET) shutdown((SOCKET)s, SD_BOTH);
    }
    else c->dropWs(true);
    // the threads finish on their own and drop their share of the core
}

NetLink::State NetLink::state() const { auto c = core(); return c ? (State)c->state.load() : Idle; }
int NetLink::attempts() const { auto c = core(); return c ? c->attempts.load() : 0; }

std::string NetLink::where() const
{
    auto c = core();
    if (!c) return std::string();
    return c->web ? c->host : c->host + ":" + std::to_string(c->port);
}

std::string NetLink::why() const
{
    auto c = core();
    if (!c) return std::string();
    std::lock_guard<std::mutex> lk(c->mu);
    return c->why;
}

void NetLink::send(const std::string& data)
{
    auto c = core();
    if (!c || data.empty()) return;
    {
        std::lock_guard<std::mutex> lk(c->mu);
        c->out += data;
    }
    c->cv.notify_all();
}

void NetLink::take(std::string& appendTo)
{
    auto c = core();
    if (!c) return;
    std::lock_guard<std::mutex> lk(c->mu);
    appendTo += c->in;
    c->in.clear();
}

// ---- the joiner's half of the first exchange ----------------------------------------

int NetJoinAuth::onBlock(std::string& block, std::string& reply, std::string& why)
{
    reply.clear();
    std::string head = netFirstLine(block);
    if (head.rfind("cwest-host", 0) == 0)
    {
        greeted = true;
        const std::string auth = netField(block, "auth");
        std::string hello = "hello 1\nname " + name + "\n";
        if (auth == "password")
        {
            if (password.empty()) { why = "the host wants a password -- type it next to the address"; return Refused; }
            if (!netDeriveKey(password, netField(block, "salt"), key)) { why = "could not work out the key"; return Refused; }
            reply = hello + "proof " + netProof(key, netField(block, "nonce")) + "\nend\n";
            sealed = true;                  // everything after the proof, both ways
            return Consumed;
        }
        reply = hello + "end\n";
        return Consumed;
    }
    if (head.rfind("sealed", 0) == 0)
    {
        if (!sealed) return Consumed;
        std::string plain;
        if (!netUnseal(key, block, plain)) { why = "a message failed its check -- not from the host, or the wrong password"; return Refused; }
        block.swap(plain);
        head = netFirstLine(block);
        if (head.rfind("denied", 0) == 0)
        {
            why = netField(block, "why");
            if (why.empty()) why = "the host turned you away";
            return Refused;
        }
        return Pass;
    }
    if (head.rfind("denied", 0) == 0)
    {
        why = netField(block, "why");
        if (why.empty()) why = "the host turned you away";
        return Refused;
    }
    if (sealed) return Consumed;            // plain text once sealed is not from the host: dropped
    return Pass;
}

} // namespace cw
