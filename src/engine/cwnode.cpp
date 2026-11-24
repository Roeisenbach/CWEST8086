// =============================================================================
//  cwnode -- a headless joiner for a hosted tournament.
//
//  Speaks the same wire as the app's Join mode (see gui_net.inl and
//  netlink.h): connects to a host, says hello, plays whatever field it is
//  handed on the phase it is given, and streams its standings back once a
//  second until told to stop. For a machine with no display, or a rack of them.
//
//    cwnode <address> [--password <p>] [--name <n>] [--threads <t>] [--once] [--quiet]
//
//  <address> is the host's https://....trycloudflare.com, or host:port for a
//  host on this machine. --password is for a host that set one. --once leaves
//  after the first run instead of waiting for the next one.
// =============================================================================
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")

#include "compete.h"
#include "netlink.h"
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

static bool takeBlock(std::string& in, std::string& block) { return netTakeBlock(in, block); }
static std::string firstLine(const std::string& b) { return netFirstLine(b); }

static std::string g_Name = "cwnode";
static int  g_Threads = 0;
static bool g_Once = false, g_Quiet = false;

static void say(const char* fmt, ...) {
    if (g_Quiet) return;
    va_list ap; va_start(ap, fmt); std::vfprintf(stdout, fmt, ap); va_end(ap);
    std::fputc('\n', stdout); std::fflush(stdout);
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
    if (argc < 2) { std::printf("cwnode <address> [--password p] [--name n] [--threads t] [--once] [--quiet]\n"); return 2; }
    const std::string address = argv[1];
    std::string password;
    {
        char buf[MAX_COMPUTERNAME_LENGTH + 1] = ""; DWORD n = sizeof(buf);
        if (GetComputerNameA(buf, &n) && buf[0]) g_Name = buf;
    }
    for (int i = 2; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--name") && i + 1 < argc)    g_Name = argv[++i];
        else if (!std::strcmp(argv[i], "--password") && i + 1 < argc) password = argv[++i];
        else if (!std::strcmp(argv[i], "--threads") && i + 1 < argc) g_Threads = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--once"))  g_Once = true;
        else if (!std::strcmp(argv[i], "--quiet")) g_Quiet = true;
    }

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { std::printf("winsock would not start\n"); return 1; }

    // The host may not be up yet -- a lab's machines get switched on in any
    // order -- so the link keeps knocking for up to ten minutes.
    NetLink link;
    link.open(address, 8086, 600.0);
    int saidTry = 0;
    while (link.state() == NetLink::Connecting) {
        if (link.attempts() >= 2 && saidTry == 0) {
            say("%s is not answering (%s) -- retrying for up to 10 minutes", link.where().c_str(), link.why().c_str());
            saidTry = 1;
        }
        Sleep(100);
    }
    if (link.state() != NetLink::Open) { std::printf("cannot connect: %s\n", link.why().c_str()); return 1; }

    NetJoinAuth auth;
    auth.name = g_Name;
    auth.password = password;
    auto sendBlock = [&](const std::string& b) { link.send(auth.wrap(b)); };
    say("%s: connected to %s as \"%s\"", argv[0], link.where().c_str(), g_Name.c_str());
    const auto openedAt = std::chrono::steady_clock::now();

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
        sendBlock(writeResultPack(snap, g_Name, run->field.runId));
        if (final) sendBlock("done 1\nrun " + std::to_string(run->field.runId) + "\nend\n");
    };

    for (;;) {
        if (std::chrono::steady_clock::now() >= leaveAt) break;
        // the state first, then what arrived: a host that turns us away says
        // why and closes at once, and the why must still be read
        const bool open = link.state() == NetLink::Open;
        if (!auth.greeted && std::chrono::steady_clock::now() - openedAt > std::chrono::seconds(20)) {
            say("connected, but the other end never said hello -- is that a CWEST8086 host?");
            break;
        }
        link.take(in);
        bool refused = false;
        while (takeBlock(in, block)) {
            std::string reply, why;
            const int r = auth.onBlock(block, reply, why);
            if (!reply.empty()) link.send(reply);
            if (r == NetJoinAuth::Refused) { say("the host would not let us in: %s", why.c_str()); refused = true; break; }
            if (r != NetJoinAuth::Pass) continue;
            const std::string head = firstLine(block);
            if (head.rfind("welcome", 0) == 0) {
                say("in -- waiting for the host to press Run");
            } else if (head.rfind("waiting", 0) == 0) {
                say("knocked -- waiting for the host to let us in");
            } else if (head.rfind("cwest-field", 0) == 0) {
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
        if (refused) break;
        if (!open) {
            const std::string w = link.why();
            say("connection lost: %s", w.empty() ? "the host went away" : w.c_str());
            break;
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
        Sleep(50);
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
    link.close();
    WSACleanup();
    return 0;
}
