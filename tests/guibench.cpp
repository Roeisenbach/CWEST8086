// The competition exactly as the GUI drives it: cw::runCompetition with a
// progress block (so workers fold every 150 ms) and a "frame" thread that
// copies the standings out under the lock sixty times a second. Compare against tourbench on the same field to see whether
// the runner and its live folding cost anything over a bare loop.
//
//   guibench <survivorDir> <games> <combo> <threads> [--no-reader]
#include "compete.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
namespace fs = std::filesystem;
using namespace cw;

static void loadField(const std::string& dir, std::vector<WarriorGroup>& out) {
    std::vector<fs::path> files;
    for (const auto& e : fs::directory_iterator(dir))
        if (e.is_regular_file() && !e.path().has_extension()) files.push_back(e.path());
    std::sort(files.begin(), files.end());
    WarriorGroup* open = nullptr;
    for (const fs::path& p : files) {
        std::ifstream f(p, std::ios::binary);
        std::vector<u8> code((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        if (code.empty()) continue;
        if (code.size() > 512) code.resize(512);
        const std::string name = p.filename().string();
        const char last = name.back();
        WarriorData w; w.name = name; w.code = code;
        if (last == '1') {
            out.push_back(WarriorGroup());
            open = &out.back();
            open->name = name.substr(0, name.size() - 1);
            w.type = WarriorType::SURVIVOR_1;
            open->warriors.push_back(w);
        } else if (last == '2' && open) {
            w.type = WarriorType::SURVIVOR_2;
            open->warriors.push_back(w);
            open = nullptr;
        } else {
            WarriorGroup g; g.name = name;
            w.type = WarriorType::SURVIVOR;
            g.warriors.push_back(w);
            out.push_back(g);
            open = nullptr;
        }
    }
    // drop a dangling "...1"
    if (open && open->warriors.size() == 1) out.pop_back();
}

int main(int argc, char** argv) {
    const std::string dir = (argc > 1) ? argv[1] : "CGX-engine/survivors";
    const int games   = (argc > 2) ? std::atoi(argv[2]) : 600;
    const int combo   = (argc > 3) ? std::atoi(argv[3]) : 4;
    const int threads = (argc > 4) ? std::atoi(argv[4]) : 0;
    bool reader = true;
    for (int i = 5; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--no-reader"))   reader = false;
    }

    std::vector<WarriorGroup> groups;
    loadField(dir, groups);
    if (groups.size() < 2) { std::printf("no field in %s\n", dir.c_str()); return 2; }

    CompeteOptions opt;
    opt.firstSeed = 0;
    opt.games = games;
    opt.comboSize = combo;
    opt.pairingSeed = 20260731;
    opt.threads = threads;

    CompeteResult out;
    CompeteProgress prog;
    std::atomic<bool> stop{false};
    std::atomic<long> copies{0};

    // the GUI's frame: copy the standings out whenever the revision moved
    std::thread frame;
    if (reader) frame = std::thread([&]() {
        int seen = -1;
        while (!stop.load()) {
            const int rev = prog.revision.load(std::memory_order_acquire);
            if (rev != seen) {
                std::lock_guard<std::mutex> lk(prog.lock);
                CompeteResult copy = out;
                (void)copy;
                seen = rev;
                copies.fetch_add(1);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    });

    const auto t0 = std::chrono::steady_clock::now();
    runCompetition(groups, opt, out, &prog);
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    stop.store(true);
    if (frame.joinable()) frame.join();

    std::printf("%d groups, combo %d, %d games, %s threads, reader %s\n",
                (int)groups.size(), combo, games,
                threads ? std::to_string(threads).c_str() : "auto",
                reader ? "on" : "off");
    std::printf("  %.3f s   %.0f games/s   (%ld board copies)\n",
                secs, out.gamesRun / secs, copies.load());
    return 0;
}
