// =============================================================================
//  cwnode -- a headless joiner for a hosted tournament.
//
//  Speaks the same wire as the app's Join mode (see gui_net.inl): connects to
//  a host, says hello, plays whatever field it is handed on the phase it is
//  given, and streams its standings back once a second until told to stop.
//  For a machine with no display, or a rack of them.
//
//    cwnode <host[:port]> [--name <n>] [--threads <t>] [--once] [--quiet]
//
//  --once leaves after the first run instead of waiting for the next one.
// =============================================================================
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")

#include "compete.h"
#include "netpack.h"
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
using namespace cw;

static std::string g_Name = "cwnode";
static int  g_Threads = 0;
static bool g_Once = false, g_Quiet = false;

static void say(const char* fmt, ...) {
    if (g_Quiet) return;
    va_list ap; va_start(ap, fmt); std::vfprintf(stdout, fmt, ap); va_end(ap);
    std::fputc('\n', stdout); std::fflush(stdout);
}

static std::string firstLine(const std::string& b) {
    const size_t nl = b.find('\n');
    return nl == std::string::npos ? b : b.substr(0, nl);
}

static bool takeBlock(std::string& in, std::string& block) {
    size_t pos = 0;
    for (;;) {
        const size_t nl = in.find('\n', pos);
        if (nl == std::string::npos) return false;
        std::string line = in.substr(pos, nl - pos);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line == "end") { block = in.substr(0, nl + 1); in.erase(0, nl + 1); return true; }
        pos = nl + 1;
    }
}

static bool sendAll(SOCKET s, const std::string& data) {
    size_t off = 0;
    while (off < data.size()) {
        const int n = send(s, data.data() + off, (int)(data.size() - off), 0);
        if (n <= 0) return false;
        off += (size_t)n;
    }
    return true;
}

// One run: the field we were handed, played until the host says stop.
struct Run {
    FieldPack field;
    CompeteResult result;
    CompeteProgress prog;
    std::thread worker;
    std::atomic<bool> finished{false};
    bool active = false;
};

int main(int argc, char** argv) {
    if (argc < 2) { std::printf("cwnode <host[:port]> [--name n] [--threads t] [--once] [--quiet]\n"); return 2; }
    std::string host = argv[1];
    int port = 8086;
    const size_t colon = host.find(':');
    if (colon != std::string::npos) { port = std::atoi(host.c_str() + colon + 1); host = host.substr(0, colon); }
    {
        char buf[MAX_COMPUTERNAME_LENGTH + 1] = ""; DWORD n = sizeof(buf);
        if (GetComputerNameA(buf, &n) && buf[0]) g_Name = buf;
    }
    for (int i = 2; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--name") && i + 1 < argc)    g_Name = argv[++i];
        else if (!std::strcmp(argv[i], "--threads") && i + 1 < argc) g_Threads = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--once"))  g_Once = true;
        else if (!std::strcmp(argv[i], "--quiet")) g_Quiet = true;
    }

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { std::printf("winsock would not start\n"); return 1; }

    addrinfo hints{}; hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    const std::string portStr = std::to_string(port);
    if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0 || !res) {
        std::printf("cannot resolve %s\n", host.c_str()); return 1;
    }
    // The host may not be up yet -- a lab's machines get switched on in any
    // order -- so keep knocking for a while before giving up.
    SOCKET s = INVALID_SOCKET;
    for (int attempt = 0; ; ++attempt) {
        s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (connect(s, res->ai_addr, (int)res->ai_addrlen) != SOCKET_ERROR) break;
        const int err = WSAGetLastError();
        closesocket(s); s = INVALID_SOCKET;
        if (attempt == 0) say("%s:%d is not answering (error %d) -- retrying for up to 10 minutes", host.c_str(), port, err);
        if (attempt >= 300) { std::printf("cannot connect to %s:%d (error %d)\n", host.c_str(), port, err); return 1; }
        Sleep(2000);
    }
    freeaddrinfo(res);
    BOOL nd = TRUE; setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char*)&nd, sizeof(nd));
    DWORD tmo = 200; setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tmo, sizeof(tmo));

    say("%s: joined %s:%d as \"%s\", waiting for the host to press Run", argv[0], host.c_str(), port, g_Name.c_str());
    sendAll(s, "hello 1\nname " + g_Name + "\nend\n");

    std::string in, block;
    Run* run = nullptr;
    int runsPlayed = 0;
    auto lastSend = std::chrono::steady_clock::now();
    CompeteResult total; bool haveTotal = false;
    auto leaveAt = std::chrono::steady_clock::time_point::max();

    auto report = [&](bool final) {
        if (!run) return;
        CompeteResult snap;
        { std::lock_guard<std::mutex> lk(run->prog.lock); snap = run->result; }
        sendAll(s, writeResultPack(snap, g_Name, run->field.runId));
        if (final) sendAll(s, "done 1\nrun " + std::to_string(run->field.runId) + "\nend\n");
    };

    for (;;) {
        if (std::chrono::steady_clock::now() >= leaveAt) break;
        char buf[1 << 15];
        const int n = recv(s, buf, sizeof(buf), 0);
        if (n > 0) in.append(buf, (size_t)n);
        else if (n == 0) { say("host closed the connection"); break; }
        else if (WSAGetLastError() != WSAETIMEDOUT) { say("connection lost (error %d)", WSAGetLastError()); break; }

        while (takeBlock(in, block)) {
            const std::string head = firstLine(block);
            if (head.rfind("cwest-field", 0) == 0) {
                FieldPack f; std::string why;
                if (!parseFieldPack(block, f, why)) { say("bad field: %s", why.c_str()); continue; }
                if (run) {                       // the host moved on; drop the old run
                    run->prog.cancel.store(true);
                    if (run->worker.joinable()) run->worker.join();
                    delete run;
                }
                run = new Run();
                run->field = f;
                run->active = true;
                CompeteOptions opt;
                opt.firstSeed = f.seed; opt.pairingSeed = f.pairingSeed; opt.comboSize = f.comboSize;
                opt.maxRounds = f.maxRounds; opt.zombieSpeed = f.zombieSpeed; opt.threads = g_Threads;
                opt.seedStride = f.stride; opt.seedPhase = f.phase; opt.games = 0;   // until stop
                int warriors = 0; for (const WarriorGroup& g : f.groups) warriors += (int)g.warriors.size();
                say("run %d: %d groups, %d warriors, phase %d of %d, seed %lld -- playing",
                    f.runId, (int)f.groups.size(), warriors, f.phase, f.stride, (long long)f.seed);
                Run* r = run;
                r->worker = std::thread([r, opt]() {
                    runCompetition(r->field.groups, opt, r->result, &r->prog);
                    r->finished.store(true);
                });
                lastSend = std::chrono::steady_clock::now();
            } else if (head.rfind("stop", 0) == 0) {
                if (run && run->active) { say("host says stop"); run->prog.cancel.store(true); }
            } else if (head.rfind("cwest-result", 0) == 0) {
                ResultPack rp; std::string why;
                if (parseResultPack(block, rp, why) && run) {
                    CompeteResult t; prepareResult(run->field.groups, t);
                    addResultPack(t, rp);
                    t.seconds = rp.seconds;
                    total = t; haveTotal = true;
                }
            }
        }

        if (run && run->active) {
            const auto now = std::chrono::steady_clock::now();
            if (run->finished.load()) {
                if (run->worker.joinable()) run->worker.join();
                report(true);
                run->active = false;
                runsPlayed++;
                say("run %d: played %d games in %.1fs (%.0f games/s) -- reported, waiting",
                    run->field.runId, run->result.gamesRun, run->result.seconds, run->result.gamesPerSecond());
                if (g_Once) leaveAt = now + std::chrono::seconds(4);   // let the final board come in
            } else if (std::chrono::duration<double>(now - lastSend).count() >= 1.0) {
                report(false);
                lastSend = now;
            }
        }
    }

    if (haveTotal && !g_Quiet) {
        std::printf("host's board: %d games\n", total.gamesRun);
        for (size_t g = 0; g < total.groupName.size(); ++g)
            std::printf("  %-24s %10.4f  %d\n", total.groupName[g].c_str(), total.groupScore[g], total.groupSurvived[g]);
    }
    if (run) {
        run->prog.cancel.store(true);
        if (run->worker.joinable()) run->worker.join();
        delete run;
    }
    closesocket(s);
    WSACleanup();
    return 0;
}
