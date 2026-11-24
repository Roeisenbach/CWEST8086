// =============================================================================
//  Distributed tournament -- the wire
//
//  Two sides of sharing one tournament between machines, resting on one
//  fact: a game's seed and its combination are pure functions of a global
//  game index, so any machine handed the field and a phase of a stride plays
//  battles nobody else plays, and the standings are the plain sum.
//
//    server   -- the host listens; joiners get the field the moment Run is
//                pressed and stream their contribution back while they play.
//    join     -- connect to a host and play whatever it sends.
//
//  Over the internet, not just a LAN: the host listens on 127.0.0.1 only and
//  runs `cloudflared` (a Cloudflare quick tunnel -- no account, no router
//  setup), which gives it a public https://....trycloudflare.com address and
//  brings every joiner in to that port as a WebSocket. Server mode needs
//  cloudflared.exe next to app.exe. The transport, the WebSocket and the
//  password sealing live in engine/netlink.{h,cpp}, shared with cwnode.
//
//  The protocol is blocks of text lines ending in a line that says `end`, so
//  the field and result packs cross the wire exactly as they sit in a file:
//
//    server -> client    cwest-host 1 / auth / salt / nonce / end   on connect
//    client -> server    hello 1 / name <n> [/ proof] / end
//    server -> client    welcome | waiting | denied                  the door
//    client -> server    <result pack>              every second while running
//                        done 1 / run <id> / end    lanes have wound down
//    server -> client    <field pack>               Run pressed (or a late join)
//                        stop 1 / end               target reached, or Stop
//                        <result pack from "total"> the merged board, for watching
//                        ping 1 / end               every 20 s, so the tunnel
//                                                   never sees an idle line
//
//  The host's sockets are non-blocking and polled from the frame; the
//  joiner's link reads on threads of its own (NetLink) and is polled too.
// =============================================================================

#pragma comment(lib, "ws2_32.lib")
#include <shellapi.h>   // ShellExecuteA -- the download page
#include "engine/netlink.h"

static const int kNetStride = 64;     // phases a hosted tournament is cut into

// Who may come in: anyone, whoever knows the password (and then everything is
// sealed), or whoever the host accepts by hand.
enum { NET_OPEN = 0, NET_PASSWORD = 1, NET_ASK = 2 };

struct NetPeer
{
    SOCKET      s = INVALID_SOCKET;
    std::string name;                 // unique on this server
    std::string addr;
    int         proto = 0;            // 0 not known yet, 1 line protocol, 2 WebSocket, 3 reading an HTTP request
    std::string raw;                  // bytes off the socket, before the WebSocket is unwrapped
    std::string in, out;
    std::string nonce;                // for the password proof
    bool        sealed = false;       // proved the password: everything both ways is sealed
    bool        pending = false;      // "ask": waiting for the host to accept
    bool        admitted = false;     // in: seated, or waiting for a seat
    bool        closeAfterFlush = false;
    int         phase = -1;           // -1 until the host hands one out
    bool        running = false;      // sent a field, not yet heard `done`
    int         runId = 0;
    double      lastSeen = 0.0;
    int         gamesRun = 0;         // from the latest pack, for the panel
    double      rate = 0.0;
};

struct NetServer
{
    SOCKET  listener = INVALID_SOCKET;
    int     port = 0;
    int     protect = NET_OPEN;       // fixed while listening
    std::string salt;                 // for the password's key, new each Listen
    cw::NetKey  key;
    std::vector<NetPeer> peers;
    std::string note;
    double  lastBoard = 0.0;          // when the merged board was last sent
    double  lastPing = 0.0;
};

enum { NET_IDLE = 0, NET_CONNECTING, NET_WAITING, NET_RUNNING, NET_FAILED };

struct NetClient
{
    int         state = NET_IDLE;
    std::string in;
    std::string note;
    double      started = 0.0;        // when the line opened (for the greeting's timeout)
    std::string host;                 // the address as typed
    int         attempts = 0;
    std::string lastWhy;              // why the previous attempt failed, kept on show
    bool        haveField = false;    // a field arrived and is waiting to start
    cw::FieldPack field;
    bool        stopAsked = false;    // the host said stop; wind down and report
    bool        doneOwed = false;     // lanes were cancelled; send the final pack
    int         runId = 0;
    cw::CompeteResult board;          // the host's merged standings, for the panel
    double      boardAt = 0.0;
};

static NetServer g_NetServer;
static NetClient g_NetClient;
static cw::NetLink     g_NetLink;     // the joiner's line to its host
static cw::NetJoinAuth g_NetAuth;     // and its half of the first exchange
// Result packs from the joined machines, keyed by sender, so a pack re-sent
// replaces rather than doubles.
static std::map<std::string, cw::ResultPack> g_NetPacks;
static int  g_NetPacksRev = 0;        // bumped when a pack arrives, so the board recombines
static bool g_NetReady = false;

static bool NetInit()
{
    if (g_NetReady) return true;
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
    g_NetReady = true;
    return true;
}

static void NetNonBlocking(SOCKET s)
{
    u_long on = 1;
    ioctlsocket(s, FIONBIO, &on);
    BOOL nd = TRUE;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char*)&nd, sizeof(nd));
}

static std::string NetErr(int code)
{
    switch (code)
    {
    case WSAEACCES:       return "blocked on this machine: a firewall or antivirus here stops CWEST8086 listening";
    case WSAEADDRINUSE:   return "port already in use (another copy listening?) -- pick another port";
    case WSAECONNRESET:   return "connection reset";
    default: break;
    }
    return "socket error " + std::to_string(code);
}

// Push what is queued; false when the connection is gone.
static bool NetFlush(SOCKET s, std::string& out)
{
    while (!out.empty())
    {
        const int n = send(s, out.data(), (int)ImMin<size_t>(out.size(), 1 << 16), 0);
        if (n > 0) { out.erase(0, (size_t)n); continue; }
        if (n == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK) return true;
        return false;
    }
    return true;
}

// Pull what has arrived; false when the connection is gone.
static bool NetRead(SOCKET s, std::string& in)
{
    char buf[1 << 15];
    for (;;)
    {
        const int n = recv(s, buf, sizeof(buf), 0);
        if (n > 0) { in.append(buf, (size_t)n); continue; }
        if (n == 0) return false;
        return WSAGetLastError() == WSAEWOULDBLOCK;
    }
}

static bool        NetTakeBlock(std::string& in, std::string& block) { return cw::netTakeBlock(in, block); }
static std::string NetFirstLine(const std::string& block)            { return cw::netFirstLine(block); }
static std::string NetField(const std::string& block, const char* key) { return cw::netField(block, key); }

static std::string NetComputerName()
{
    char buf[MAX_COMPUTERNAME_LENGTH + 1] = "";
    DWORD n = sizeof(buf);
    if (!GetComputerNameA(buf, &n) || !buf[0]) return "cwest";
    return buf;
}

// ---- the tunnel -----------------------------------------------------------
//
// cloudflared.exe, run next to app.exe: `tunnel --url http://127.0.0.1:<port>`
// asks Cloudflare for a quick tunnel and prints its public address. It runs
// in a job object, so it goes when the app goes, however the app ends.

static const char* kCloudflaredPage = "https://github.com/cloudflare/cloudflared/releases/latest";

struct NetTunnel
{
    HANDLE      proc = NULL, job = NULL, pipe = NULL;
    std::thread reader;
    std::mutex  mu;
    std::string url;                  // https://....trycloudflare.com once it is up
    std::string lastLine;             // cloudflared's latest complaint, for the panel
    std::atomic<bool> running{false};
    double      startedAt = 0.0;
};
static NetTunnel g_Tunnel;

static std::string CloudflaredPath() { return ExeDir() + "cloudflared.exe"; }
static bool CloudflaredPresent()
{
    const DWORD a = GetFileAttributesA(CloudflaredPath().c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static std::string NetTunnelUrl()
{
    std::lock_guard<std::mutex> lk(g_Tunnel.mu);
    return g_Tunnel.url;
}

static void NetTunnelStop()
{
    NetTunnel& t = g_Tunnel;
    if (t.job) { TerminateJobObject(t.job, 0); CloseHandle(t.job); t.job = NULL; }
    if (t.proc) { CloseHandle(t.proc); t.proc = NULL; }
    if (t.reader.joinable()) t.reader.join();        // the pipe broke when cloudflared went
    if (t.pipe) { CloseHandle(t.pipe); t.pipe = NULL; }
    std::lock_guard<std::mutex> lk(t.mu);
    t.url.clear();
    t.lastLine.clear();
    t.running.store(false);
}

static bool NetTunnelStart(int port, std::string& why)
{
    NetTunnelStop();
    NetTunnel& t = g_Tunnel;
    if (!CloudflaredPresent()) { why = "cloudflared.exe is not next to app.exe"; return false; }

    SECURITY_ATTRIBUTES sa{ sizeof(sa), NULL, TRUE };
    HANDLE rd = NULL, wr = NULL;
    if (!CreatePipe(&rd, &wr, &sa, 0)) { why = "could not make a pipe for cloudflared"; return false; }
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = wr;
    si.hStdError = wr;
    si.hStdInput = NULL;
    PROCESS_INFORMATION pi{};
    std::string cmd = "\"" + CloudflaredPath() + "\" tunnel --no-autoupdate --url http://127.0.0.1:" + std::to_string(port);
    if (!CreateProcessA(NULL, &cmd[0], NULL, NULL, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED, NULL,
                        ExeDir().c_str(), &si, &pi))
    {
        why = "could not start cloudflared.exe (error " + std::to_string(GetLastError()) + ")";
        CloseHandle(rd); CloseHandle(wr);
        return false;
    }
    CloseHandle(wr);
    t.job = CreateJobObjectA(NULL, NULL);
    if (t.job)
    {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION li{};
        li.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(t.job, JobObjectExtendedLimitInformation, &li, sizeof(li));
        AssignProcessToJobObject(t.job, pi.hProcess);
    }
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);
    t.proc = pi.hProcess;
    t.pipe = rd;
    t.startedAt = AppNow();
    t.running.store(true);

    t.reader = std::thread([rd]() {
        NetTunnel& tt = g_Tunnel;
        std::string acc;
        char buf[4096];
        DWORD got = 0;
        while (ReadFile(rd, buf, sizeof(buf), &got, NULL) && got > 0)
        {
            acc.append(buf, got);
            size_t nl;
            while ((nl = acc.find('\n')) != std::string::npos)
            {
                std::string line = acc.substr(0, nl);
                acc.erase(0, nl + 1);
                const size_t at = line.find("https://");
                if (at != std::string::npos)
                {
                    size_t e = at + 8;
                    while (e < line.size() && (isalnum((unsigned char)line[e]) || line[e] == '-' || line[e] == '.')) e++;
                    const std::string u = line.substr(at, e - at);
                    if (u.size() > 26 && u.compare(u.size() - 18, 18, ".trycloudflare.com") == 0)
                    {
                        std::lock_guard<std::mutex> lk(tt.mu);
                        tt.url = u;
                    }
                }
                if (line.find(" ERR ") != std::string::npos || line.find("failed") != std::string::npos)
                {
                    // the part after the timestamp and level
                    const size_t lv = line.find(" ERR ");
                    std::lock_guard<std::mutex> lk(tt.mu);
                    tt.lastLine = lv != std::string::npos ? line.substr(lv + 5) : line;
                }
            }
        }
        tt.running.store(false);
    });
    return true;
}

// ---- server ---------------------------------------------------------------

static void NetServerStop()
{
    NetTunnelStop();
    for (NetPeer& p : g_NetServer.peers)
        if (p.s != INVALID_SOCKET) closesocket(p.s);
    g_NetServer.peers.clear();
    if (g_NetServer.listener != INVALID_SOCKET) closesocket(g_NetServer.listener);
    g_NetServer.listener = INVALID_SOCKET;
    g_NetServer.port = 0;
}

// Listens on 127.0.0.1 only -- everyone comes in through the tunnel -- and
// starts the tunnel. Needs cloudflared.exe next to app.exe.
static bool NetServerStart(int port, std::string& why)
{
    if (!CloudflaredPresent()) { why = "server mode needs cloudflared.exe next to app.exe"; return false; }
    if (!NetInit()) { why = "winsock would not start"; return false; }
    NetServerStop();

    g_NetServer.protect = ImClamp(g_App.compNetProtect, 0, 2);
    g_NetServer.salt = cw::netRandomHex(16);
    g_NetServer.key = cw::NetKey();
    if (g_NetServer.protect == NET_PASSWORD)
    {
        if (!g_App.compNetPassword[0]) { why = "type a password first (or choose Open)"; return false; }
        if (!cw::netDeriveKey(g_App.compNetPassword, g_NetServer.salt, g_NetServer.key))
        { why = "could not work out the password's key"; return false; }
    }

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) { why = NetErr(WSAGetLastError()); return false; }
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = htons((u_short)port);
    if (bind(s, (sockaddr*)&a, sizeof(a)) == SOCKET_ERROR || listen(s, 16) == SOCKET_ERROR)
    {
        why = NetErr(WSAGetLastError());
        closesocket(s);
        return false;
    }
    NetNonBlocking(s);
    g_NetServer.listener = s;
    g_NetServer.port = port;
    g_NetServer.note.clear();
    if (!NetTunnelStart(port, why))
    {
        NetServerStop();
        return false;
    }
    return true;
}

static bool NetServerUp() { return g_NetServer.listener != INVALID_SOCKET; }

// Bytes to a peer, in a WebSocket's frames when it came that way.
static void NetPeerWrite(NetPeer& p, const std::string& bytes)
{
    p.out += (p.proto == 2) ? cw::wsServerFrames(bytes) : bytes;
}

// A block to a peer -- sealed once it has proved the password.
static void NetPeerSay(NetPeer& p, const std::string& block)
{
    NetPeerWrite(p, p.sealed ? cw::netSeal(g_NetServer.key, block) : block);
}

// Tournament traffic: only to peers that are in.
static void NetServerSend(NetPeer& p, const std::string& block)
{
    if (p.admitted) NetPeerSay(p, block);
}

static void NetServerBroadcast(const std::string& block)
{
    for (NetPeer& p : g_NetServer.peers) NetServerSend(p, block);
}

static int NetServerFreePhase()
{
    for (int ph = 1; ph < kNetStride; ph++)
    {
        bool used = false;
        for (const NetPeer& p : g_NetServer.peers) if (p.phase == ph) used = true;
        if (!used) return ph;
    }
    return -1;
}

static void NetServerAccept()
{
    for (;;)
    {
        sockaddr_in a{};
        int alen = sizeof(a);
        SOCKET c = accept(g_NetServer.listener, (sockaddr*)&a, &alen);
        if (c == INVALID_SOCKET)
        {
            const int err = WSAGetLastError();
            if (err != WSAEWOULDBLOCK)
            {
                // the listener itself is gone; say so rather than sit on a dead port
                g_NetServer.note = "stopped listening: " + NetErr(err);
                closesocket(g_NetServer.listener);
                g_NetServer.listener = INVALID_SOCKET;
            }
            return;
        }
        NetNonBlocking(c);
        NetPeer p;
        p.s = c;
        char ip[64];
        inet_ntop(AF_INET, &a.sin_addr, ip, sizeof(ip));
        p.addr = ip;
        p.name = ip;                     // until it says hello
        p.nonce = cw::netRandomHex(16);
        p.lastSeen = AppNow();
        g_NetServer.peers.push_back(p);
    }
}

// The first thing every joiner hears: how the door works here.
static void NetServerGreet(NetPeer& p)
{
    static const char* kAuth[] = { "open", "password", "ask" };
    NetPeerWrite(p, std::string("cwest-host 1\nauth ") + kAuth[g_NetServer.protect] +
                    "\nsalt " + g_NetServer.salt + "\nnonce " + p.nonce + "\nend\n");
}

// Hands the peer a phase, and the field if a run is going.
static void NetServerSeat(NetPeer& p);

static void NetServerAdmit(NetPeer& p)
{
    p.pending = false;
    p.admitted = true;
    NetPeerSay(p, "welcome 1\nend\n");
    NetServerSeat(p);
}

static void NetServerTurnAway(NetPeer& p, const char* why)
{
    p.pending = false;
    NetPeerSay(p, std::string("denied 1\nwhy ") + why + "\nend\n");
    p.closeAfterFlush = true;
}

static void NetServerPoll()
{
    if (!NetServerUp()) return;
    NetServerAccept();

    const double now = AppNow();
    const bool ping = now - g_NetServer.lastPing > 20.0;
    if (ping) g_NetServer.lastPing = now;
    for (size_t i = 0; i < g_NetServer.peers.size(); )
    {
        NetPeer& p = g_NetServer.peers[i];
        bool alive = NetRead(p.s, p.raw);

        // what came in: a WebSocket through the tunnel (cloudflared sends its
        // request at once), or plain lines from this machine -- a joiner that
        // dials host:port says nothing until it is greeted, so a short silence
        // means the line protocol
        if (alive && p.proto == 0)
        {
            const size_t n = ImMin<size_t>(4, p.raw.size());
            if (n > 0 && p.raw.compare(0, n, std::string("GET ").substr(0, n)) != 0) { p.proto = 1; NetServerGreet(p); }
            else if (n == 4) p.proto = 3;
            else if (n == 0 && now - p.lastSeen > 0.3) { p.proto = 1; NetServerGreet(p); }
        }
        // a connection that never says hello does not keep a socket forever
        if (alive && !p.admitted && !p.pending && now - p.lastSeen > 60.0) alive = false;
        if (alive && p.proto == 3)
        {
            std::string resp, ip;
            bool upgrade = false;
            if (cw::wsServerHandshake(p.raw, resp, upgrade, ip))
            {
                p.out += resp;
                if (upgrade)
                {
                    p.proto = 2;
                    if (!ip.empty()) { p.addr = ip; p.name = ip; }
                    NetServerGreet(p);
                }
                else p.closeAfterFlush = true;          // a browser: it got the page
            }
        }
        if (alive && p.proto == 1) { p.in += p.raw; p.raw.clear(); }
        if (alive && p.proto == 2)
        {
            std::string reply;
            if (!cw::wsServerDecode(p.raw, p.in, reply)) alive = false;
            p.out += reply;
        }
        if (alive && ping && (p.admitted || p.pending)) NetPeerSay(p, "ping 1\nend\n");

        std::string block;
        while (alive && NetTakeBlock(p.in, block))
        {
            p.lastSeen = now;
            std::string head = NetFirstLine(block);
            if (head.rfind("sealed", 0) == 0)
            {
                if (!p.sealed) continue;
                std::string plain;
                if (!cw::netUnseal(g_NetServer.key, block, plain)) { alive = false; break; }   // tampered with
                block.swap(plain);
                head = NetFirstLine(block);
            }
            else if (p.sealed) continue;                 // plain text after the seal: not from that joiner

            if (head.rfind("hello", 0) == 0)
            {
                if (p.admitted || p.pending) continue;
                std::string want = NetField(block, "name");
                if (want.empty()) want = p.addr;
                // names key the packs, so two laptops both called LAPTOP get
                // numbered -- and none may borrow the host's own name
                std::string name = want;
                for (int n = 2; ; n++)
                {
                    bool taken = (name == NetComputerName());
                    for (const NetPeer& q : g_NetServer.peers)
                        if (&q != &p && q.name == name) taken = true;
                    if (!taken) break;
                    name = want + " #" + std::to_string(n);
                }
                p.name = name;
                if (g_NetServer.protect == NET_PASSWORD)
                {
                    if (NetField(block, "proof") != cw::netProof(g_NetServer.key, p.nonce))
                    {
                        NetServerTurnAway(p, "wrong password");
                        continue;
                    }
                    p.sealed = true;
                }
                if (g_NetServer.protect == NET_ASK)
                {
                    p.pending = true;
                    NetPeerSay(p, "waiting 1\nend\n");
                    g_App.status = p.name + " asks to join -- Accept or Deny on the Competition panel";
                    continue;
                }
                NetServerAdmit(p);
            }
            else if (!p.admitted) continue;              // nothing counts before the door
            else if (head.rfind("cwest-result", 0) == 0)
            {
                cw::ResultPack rp;
                std::string why;
                if (cw::parseResultPack(block, rp, why) && rp.runId == p.runId && p.running)
                {
                    rp.sender = p.name;
                    p.gamesRun = rp.gamesRun;
                    p.rate = rp.seconds > 0.0 ? rp.gamesRun / rp.seconds : 0.0;
                    g_NetPacks[p.name] = std::move(rp);
                    g_NetPacksRev++;
                }
            }
            else if (head.rfind("done", 0) == 0)
            {
                if (atoi(NetField(block, "run").c_str()) == p.runId) p.running = false;
            }
        }
        if (alive) alive = NetFlush(p.s, p.out);
        if (alive && p.closeAfterFlush && p.out.empty()) alive = false;
        if (!alive)
        {
            closesocket(p.s);
            // its last pack stays on the board: the games it played were played
            g_NetServer.peers.erase(g_NetServer.peers.begin() + (ptrdiff_t)i);
            continue;
        }
        i++;
    }
}

// ---- client -----------------------------------------------------------------

static bool NetClientLinked() { return g_NetLink.state() == cw::NetLink::Open; }

static void NetClientDisconnect()
{
    g_NetLink.close();
    g_NetAuth = cw::NetJoinAuth();
    g_NetClient = NetClient();
}

// Dials `address` -- the host's https://....trycloudflare.com, or host:port
// on this machine -- and keeps knocking for ten minutes if nobody answers yet.
static bool NetClientConnect(const char* address, std::string& why)
{
    if (!NetInit()) { why = "winsock would not start"; return false; }
    NetClientDisconnect();
    std::string a = address;
    while (!a.empty() && (a.back() == ' ' || a.back() == '/')) a.pop_back();
    while (!a.empty() && a.front() == ' ') a.erase(0, 1);
    if (a.empty()) { why = "type the host's address first"; return false; }
    g_NetClient.host = a;
    g_NetAuth.name = g_App.compNetName;
    g_NetAuth.password = g_App.compNetPassword;
    g_NetLink.open(a, g_App.compNetPort, 600.0);
    g_NetClient.state = NET_CONNECTING;
    g_NetClient.note = "connecting to " + a + "...";
    return true;
}

static void NetClientSend(const std::string& block)
{
    if (NetClientLinked()) g_NetLink.send(g_NetAuth.wrap(block));
}

static void NetClientFail(const std::string& why)
{
    g_NetLink.close();
    g_NetClient.state = NET_FAILED;
    g_NetClient.note = why;
    g_NetClient.haveField = false;
    g_NetClient.stopAsked = true;       // whatever is running should wind down
}

static void NetClientPoll()
{
    NetClient& c = g_NetClient;
    if (c.state == NET_IDLE || c.state == NET_FAILED) return;
    const cw::NetLink::State st = g_NetLink.state();

    if (c.state == NET_CONNECTING)
    {
        if (st == cw::NetLink::Connecting)
        {
            c.attempts = g_NetLink.attempts();
            c.lastWhy = g_NetLink.why();
            if (c.attempts > 1) c.note = "no host at " + c.host + " yet (try " + std::to_string(c.attempts) + ")";
            return;
        }
        if (st != cw::NetLink::Open) { NetClientFail(g_NetLink.why().empty() ? "could not connect" : g_NetLink.why()); return; }
        c.state = NET_WAITING;
        c.started = AppNow();
        c.lastWhy.clear();
        c.note = "connected -- saying hello";
    }

    if (!g_NetAuth.greeted && AppNow() - c.started > 20.0)
    {
        NetClientFail("connected, but the other end never said hello -- is that a CWEST8086 host?");
        return;
    }

    // what arrived before the line closed is read first: a host that turns us
    // away says why, then hangs up
    g_NetLink.take(c.in);
    std::string block;
    while (NetTakeBlock(c.in, block))
    {
        std::string reply, why;
        const int r = g_NetAuth.onBlock(block, reply, why);
        if (!reply.empty()) g_NetLink.send(reply);
        if (r == cw::NetJoinAuth::Refused) { NetClientFail(why); return; }
        if (r != cw::NetJoinAuth::Pass) continue;

        const std::string head = NetFirstLine(block);
        if (head.rfind("welcome", 0) == 0)
        {
            if (c.state == NET_WAITING) c.note = "in -- waiting for the host to press Run";
        }
        else if (head.rfind("waiting", 0) == 0)
        {
            c.note = "knocked -- waiting for the host to let you in";
        }
        else if (head.rfind("cwest-field", 0) == 0)
        {
            cw::FieldPack f;
            std::string fwhy;
            if (cw::parseFieldPack(block, f, fwhy))
            {
                c.field = f;
                c.haveField = true;
                c.stopAsked = false;
            }
            else c.note = "bad field from the host: " + fwhy;
        }
        else if (head.rfind("stop", 0) == 0)
        {
            c.stopAsked = true;
        }
        else if (head.rfind("cwest-result", 0) == 0)
        {
            cw::ResultPack rp;
            std::string pwhy;
            if (cw::parseResultPack(block, rp, pwhy) && !c.board.empty())
            {
                cw::CompeteResult b = c.board;
                for (size_t i = 0; i < b.warriorScore.size(); i++)
                {
                    b.warriorScore[i] = 0.0; b.warriorSurvived[i] = 0;
                }
                for (size_t g = 0; g < b.groupScore.size(); g++)
                {
                    b.groupScore[g] = 0.0; b.groupSurvived[g] = 0;
                }
                b.gamesRun = 0; b.gamesFailed = 0;
                cw::addResultPack(b, rp);
                b.seconds = rp.seconds;
                c.board = std::move(b);
                c.boardAt = AppNow();
            }
        }
    }
    if (st != cw::NetLink::Open)
    {
        const std::string w = g_NetLink.why();
        NetClientFail(w.empty() ? "the host went away" : w);
    }
}
