// =============================================================================
//  Distributed tournament -- the wire
//
//  Three ways to share one tournament between machines, all resting on the
//  same fact: a game's seed and its combination are pure functions of a global
//  game index, so any machine handed the field and a phase of a stride plays
//  battles nobody else plays, and the standings are the plain sum.
//
//    offline  -- the host writes one shard file per machine (the field plus a
//                phase); each machine runs its shard and saves a result file;
//                the host adds the result files up.
//    server   -- the host listens; joiners get the field the moment Run is
//                pressed and stream their contribution back while they play.
//    join     -- connect to a host and play whatever it sends.
//
//  The protocol is blocks of text lines ending in a line that says `end`, so
//  the field and result packs cross the wire exactly as they sit in a file:
//
//    client -> server    hello 1 / name <n> / end
//                        <result pack>              every second while running
//                        done 1 / run <id> / end    lanes have wound down
//    server -> client    <field pack>               Run pressed (or a late join)
//                        stop 1 / end               target reached, or Stop
//                        <result pack from "total"> the merged board, for watching
//
//  Sockets are non-blocking and polled from the frame. Nothing here is fast
//  enough to need a thread: a field is ~100 KB once, a result pack a few KB a
//  second.
// =============================================================================

#pragma comment(lib, "ws2_32.lib")
#include <shellapi.h>   // ShellExecuteExW -- the firewall button

static const int kNetStride = 64;     // phases a hosted tournament is cut into

struct NetPeer
{
    SOCKET      s = INVALID_SOCKET;
    std::string name;                 // unique on this server
    std::string addr;
    std::string in, out;
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
    std::vector<NetPeer> peers;
    std::string note;
    double  lastBoard = 0.0;          // when the merged board was last sent
};

enum { NET_IDLE = 0, NET_CONNECTING, NET_WAITING, NET_RUNNING, NET_FAILED };

struct NetClient
{
    SOCKET      s = INVALID_SOCKET;
    int         state = NET_IDLE;
    std::string in, out;
    std::string note;
    double      started = 0.0;
    double      lastSend = 0.0;
    // A joiner switched on before its host keeps knocking: where to, and
    // until when.
    std::string host;
    int         port = 0;
    double      retryUntil = 0.0;
    double      retryAt = 0.0;
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
// Result packs from elsewhere -- files added in offline mode, peers in server
// mode -- keyed by sender, so a pack re-sent or a file added twice replaces
// rather than doubles.
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
    case WSAECONNREFUSED: return "refused: that machine is up, but nothing listens on the port (Listen not pressed? wrong port?)";
    case WSAETIMEDOUT:    return "timed out: a firewall on the host is dropping the port, or the address is wrong";
    case WSAEHOSTUNREACH: return "host unreachable: wrong address, or not on the same network";
    case WSAENETUNREACH:  return "network unreachable: this machine is not on that network";
    case WSAEACCES:       return "blocked on this machine: a firewall or antivirus here stops CWEST8086 connecting";
    case WSAEADDRNOTAVAIL:return "not a usable address";
    case WSAEADDRINUSE:   return "port already in use (another copy listening?)";
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

// The next complete block in `in`, if one has fully arrived. Blocks end at a
// line that is exactly `end`.
static bool NetTakeBlock(std::string& in, std::string& block)
{
    size_t pos = 0;
    for (;;)
    {
        const size_t nl = in.find('\n', pos);
        if (nl == std::string::npos) return false;
        std::string line = in.substr(pos, nl - pos);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line == "end")
        {
            block = in.substr(0, nl + 1);
            in.erase(0, nl + 1);
            return true;
        }
        pos = nl + 1;
    }
}

static std::string NetFirstLine(const std::string& block)
{
    const size_t nl = block.find('\n');
    return nl == std::string::npos ? block : block.substr(0, nl);
}

// `key value` lines of a small block (hello, done)
static std::string NetField(const std::string& block, const char* key)
{
    const std::string k = std::string(key) + " ";
    size_t pos = 0;
    while (pos < block.size())
    {
        size_t nl = block.find('\n', pos);
        if (nl == std::string::npos) nl = block.size();
        if (block.compare(pos, k.size(), k) == 0) return block.substr(pos + k.size(), nl - pos - k.size());
        pos = nl + 1;
    }
    return std::string();
}

static std::string NetComputerName()
{
    char buf[MAX_COMPUTERNAME_LENGTH + 1] = "";
    DWORD n = sizeof(buf);
    if (!GetComputerNameA(buf, &n) || !buf[0]) return "cwest";
    return buf;
}

// ---- server ---------------------------------------------------------------

static void NetServerStop()
{
    for (NetPeer& p : g_NetServer.peers)
        if (p.s != INVALID_SOCKET) closesocket(p.s);
    g_NetServer.peers.clear();
    if (g_NetServer.listener != INVALID_SOCKET) closesocket(g_NetServer.listener);
    g_NetServer.listener = INVALID_SOCKET;
    g_NetServer.port = 0;
}

static bool NetServerStart(int port, std::string& why)
{
    if (!NetInit()) { why = "winsock would not start"; return false; }
    NetServerStop();

    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) { why = NetErr(WSAGetLastError()); return false; }
    BOOL reuse = TRUE;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));

    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_ANY);
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
    return true;
}

static bool NetServerUp() { return g_NetServer.listener != INVALID_SOCKET; }

// The addresses this machine answers on, for telling joiners what to type --
// best guess first. A typical PC also carries Hyper-V / WSL / VirtualBox
// adapters whose addresses nobody else can reach, and listing those first is
// how a joiner ends up knocking on the wrong door.
struct NetAddr
{
    std::string ip;
    std::string adapter;
    int         rank = 0;      // 0 real adapter with a gateway, 1 real, 2 virtual
};

static std::vector<NetAddr> NetLocalAddresses()
{
    std::vector<NetAddr> out;
    if (!NetInit()) return out;

    // Loaded at run time so the link line needs no iphlpapi.
    typedef ULONG (WINAPI *GaaFn)(ULONG, ULONG, PVOID, PIP_ADAPTER_ADDRESSES, PULONG);
    static GaaFn gaa = []() -> GaaFn {
        HMODULE m = LoadLibraryA("iphlpapi.dll");
        return m ? (GaaFn)(void*)GetProcAddress(m, "GetAdaptersAddresses") : nullptr;
    }();

    if (gaa)
    {
        std::vector<unsigned char> buf;
        ULONG size = 32 * 1024, rc = ERROR_BUFFER_OVERFLOW;
        for (int tries = 0; tries < 3 && rc == ERROR_BUFFER_OVERFLOW; tries++)
        {
            buf.resize(size);
            rc = gaa(AF_INET, GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_SKIP_ANYCAST |
                              GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
                     nullptr, (PIP_ADAPTER_ADDRESSES)buf.data(), &size);
        }
        if (rc == NO_ERROR)
            for (PIP_ADAPTER_ADDRESSES a = (PIP_ADAPTER_ADDRESSES)buf.data(); a; a = a->Next)
            {
                if (a->OperStatus != IfOperStatusUp) continue;
                if (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK || a->IfType == IF_TYPE_TUNNEL) continue;

                char name[256] = "", desc[256] = "";
                WideCharToMultiByte(CP_UTF8, 0, a->FriendlyName, -1, name, sizeof(name), NULL, NULL);
                WideCharToMultiByte(CP_UTF8, 0, a->Description,  -1, desc, sizeof(desc), NULL, NULL);
                const std::string both = ToLower(std::string(name) + " " + desc);
                bool virt = false;
                for (const char* v : { "vethernet", "hyper-v", "virtualbox", "vmware", "wsl", "docker", "loopback" })
                    if (both.find(v) != std::string::npos) virt = true;
                const int rank = virt ? 2 : (a->FirstGatewayAddress ? 0 : 1);

                for (PIP_ADAPTER_UNICAST_ADDRESS u = a->FirstUnicastAddress; u; u = u->Next)
                {
                    if (!u->Address.lpSockaddr || u->Address.lpSockaddr->sa_family != AF_INET) continue;
                    char ip[64] = "";
                    inet_ntop(AF_INET, &((sockaddr_in*)u->Address.lpSockaddr)->sin_addr, ip, sizeof(ip));
                    if (strncmp(ip, "169.254.", 8) == 0) continue;   // self-assigned: no network behind it
                    NetAddr n;
                    // The description ("Intel Wi-Fi 6 AX201") rather than the
                    // friendly name: on a localized Windows the name is in the
                    // UI language ("Wi-Fi" in Hebrew, with bidi marks), which
                    // the app's fonts cannot draw.
                    n.ip = ip; n.adapter = desc[0] ? desc : name; n.rank = rank;
                    out.push_back(n);
                }
            }
    }

    if (out.empty())
    {
        // the old way: every address the host name resolves to, unranked
        char host[256] = "";
        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        addrinfo* res = nullptr;
        if (gethostname(host, sizeof(host)) == 0 && getaddrinfo(host, nullptr, &hints, &res) == 0)
        {
            for (addrinfo* p = res; p; p = p->ai_next)
            {
                char ip[64];
                inet_ntop(AF_INET, &((sockaddr_in*)p->ai_addr)->sin_addr, ip, sizeof(ip));
                NetAddr n; n.ip = ip; n.rank = 1;
                out.push_back(n);
            }
            freeaddrinfo(res);
        }
    }

    std::stable_sort(out.begin(), out.end(), [](const NetAddr& x, const NetAddr& y) { return x.rank < y.rank; });
    return out;
}

// Lets joiners in through Windows Defender Firewall. Runs netsh elevated, so
// Windows asks the user for admin approval first; nothing happens without it.
//   1. removes the inbound rules Windows made for THIS exe -- pressing Cancel
//      on the first-run "allow access" prompt leaves a Block rule behind, and
//      a block rule beats any allow rule;
//   2. replaces our own named rule with one for the port, TCP, open to the
//      local subnet only (no password on the wire), on every network profile.
// Returns a one-line outcome for the panel.
static std::string NetAllowThroughFirewall(int port)
{
    wchar_t exe[MAX_PATH] = L"";
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    const std::wstring p = std::to_wstring(port);
    const std::wstring args =
        L"/c netsh advfirewall firewall delete rule name=all dir=in program=\"" + std::wstring(exe) + L"\" >nul 2>&1"
        L" & netsh advfirewall firewall delete rule name=\"CWEST8086 server\" >nul 2>&1"
        L" & netsh advfirewall firewall add rule name=\"CWEST8086 server\" dir=in action=allow protocol=TCP"
        L" localport=" + p + L" remoteip=localsubnet profile=any";

    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    sei.hwnd = g_Hwnd;
    sei.lpVerb = L"runas";
    sei.lpFile = L"cmd.exe";
    sei.lpParameters = args.c_str();
    sei.nShow = SW_HIDE;
    if (!ShellExecuteExW(&sei))
        return GetLastError() == ERROR_CANCELLED ? "firewall unchanged: the admin prompt was declined"
                                                 : "firewall unchanged: could not run netsh";
    DWORD code = 1;
    if (sei.hProcess)
    {
        if (WaitForSingleObject(sei.hProcess, 20000) != WAIT_OBJECT_0)
        { CloseHandle(sei.hProcess); return "netsh is taking long -- check the firewall by hand"; }
        GetExitCodeProcess(sei.hProcess, &code);
        CloseHandle(sei.hProcess);
    }
    return code == 0 ? "firewall: TCP port " + std::to_string(port) + " is open to this network"
                     : "netsh failed (code " + std::to_string(code) + ") -- add the rule by hand";
}

static void NetServerSend(NetPeer& p, const std::string& block) { p.out += block; }

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
        p.lastSeen = AppNow();
        g_NetServer.peers.push_back(p);
    }
}

// Hands the peer a phase, and the field if a run is going. Called on hello.
static void NetServerSeat(NetPeer& p);

static void NetServerPoll()
{
    if (!NetServerUp()) return;
    NetServerAccept();

    const double now = AppNow();
    for (size_t i = 0; i < g_NetServer.peers.size(); )
    {
        NetPeer& p = g_NetServer.peers[i];
        bool alive = NetRead(p.s, p.in) && NetFlush(p.s, p.out);
        std::string block;
        while (alive && NetTakeBlock(p.in, block))
        {
            p.lastSeen = now;
            const std::string head = NetFirstLine(block);
            if (head.rfind("hello", 0) == 0)
            {
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
                NetServerSeat(p);
            }
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

static void NetClientDisconnect()
{
    if (g_NetClient.s != INVALID_SOCKET) closesocket(g_NetClient.s);
    g_NetClient = NetClient();
}

// One connection attempt; the retry loop in NetClientPoll calls this again.
static bool NetClientDial(std::string& why)
{
    NetClient& c = g_NetClient;
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    const std::string portStr = std::to_string(c.port);
    if (getaddrinfo(c.host.c_str(), portStr.c_str(), &hints, &res) != 0 || !res)
    {
        why = "cannot resolve " + c.host;
        return false;
    }
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) { freeaddrinfo(res); why = NetErr(WSAGetLastError()); return false; }
    NetNonBlocking(s);
    connect(s, res->ai_addr, (int)res->ai_addrlen);     // completes in the background
    freeaddrinfo(res);

    c.s = s;
    c.state = NET_CONNECTING;
    c.started = AppNow();
    c.attempts++;
    // Why the last try failed stays in c.lastWhy, which the panel shows on a
    // line of its own -- it used to flash for the two seconds between tries
    // and then give way to "knocking again".
    const std::string where = c.host + ":" + std::to_string(c.port);
    c.note = c.attempts == 1 ? "connecting to " + where + "..."
                             : "no host at " + where + " yet (try " + std::to_string(c.attempts) + ")";
    return true;
}

static bool NetClientConnect(const char* host, int port, std::string& why)
{
    if (!NetInit()) { why = "winsock would not start"; return false; }
    NetClientDisconnect();
    g_NetClient.host = host;
    g_NetClient.port = port;
    g_NetClient.retryUntil = AppNow() + 600.0;   // ten minutes of knocking
    return NetClientDial(why);
}

static void NetClientSend(const std::string& block) { g_NetClient.out += block; }

static void NetClientFail(const std::string& why)
{
    if (g_NetClient.s != INVALID_SOCKET) closesocket(g_NetClient.s);
    g_NetClient.s = INVALID_SOCKET;
    g_NetClient.state = NET_FAILED;
    g_NetClient.note = why;
    g_NetClient.haveField = false;
    g_NetClient.stopAsked = true;       // whatever is running should wind down
}

// A failed dial: give up, or -- while the host may simply not be up yet --
// close this attempt and book another.
static void NetClientDialFailed(const std::string& why)
{
    NetClient& c = g_NetClient;
    const double now = AppNow();
    if (now >= c.retryUntil) { NetClientFail(why); return; }
    closesocket(c.s);
    c.s = INVALID_SOCKET;
    c.state = NET_CONNECTING;
    c.retryAt = now + 2.0;
    c.lastWhy = why;
    c.note = "no host at " + c.host + ":" + std::to_string(c.port) + " yet (try " + std::to_string(c.attempts) + ")";
}

static void NetClientPoll()
{
    NetClient& c = g_NetClient;
    if (c.s == INVALID_SOCKET)
    {
        // between attempts
        if (c.state == NET_CONNECTING && c.retryAt > 0.0 && AppNow() >= c.retryAt)
        {
            c.retryAt = 0.0;
            std::string why;
            if (!NetClientDial(why)) NetClientFail(why);
        }
        return;
    }

    if (c.state == NET_CONNECTING)
    {
        fd_set w, e;
        FD_ZERO(&w); FD_ZERO(&e);
        FD_SET(c.s, &w); FD_SET(c.s, &e);
        timeval tv{0, 0};
        select(0, nullptr, &w, &e, &tv);
        if (FD_ISSET(c.s, &e))
        {
            int err = 0, len = sizeof(err);
            getsockopt(c.s, SOL_SOCKET, SO_ERROR, (char*)&err, &len);
            NetClientDialFailed(NetErr(err));
            return;
        }
        if (!FD_ISSET(c.s, &w))
        {
            if (AppNow() - c.started > 15.0) NetClientDialFailed("no answer in 15 s: a firewall on the host is dropping the port, or the address is wrong");
            return;
        }
        c.attempts = 0;
        c.state = NET_WAITING;
        c.lastWhy.clear();
        c.note = "connected -- waiting for the host to press Run";
        NetClientSend("hello 1\nname " + std::string(g_App.compNetName) + "\nend\n");
    }

    bool alive = NetRead(c.s, c.in) && NetFlush(c.s, c.out);
    std::string block;
    while (alive && NetTakeBlock(c.in, block))
    {
        const std::string head = NetFirstLine(block);
        if (head.rfind("cwest-field", 0) == 0)
        {
            cw::FieldPack f;
            std::string why;
            if (cw::parseFieldPack(block, f, why))
            {
                c.field = f;
                c.haveField = true;
                c.stopAsked = false;
            }
            else c.note = "bad field from the host: " + why;
        }
        else if (head.rfind("stop", 0) == 0)
        {
            c.stopAsked = true;
        }
        else if (head.rfind("cwest-result", 0) == 0)
        {
            cw::ResultPack rp;
            std::string why;
            if (cw::parseResultPack(block, rp, why) && !c.board.empty())
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
    if (!alive) NetClientFail("the host went away");
}
