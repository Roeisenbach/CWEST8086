// Realistic tournament benchmark: a field of groups, a random combo of N of
// them per battle, wars run to the real MAX_ROUND cap. This is the shape the
// competition tab actually runs, and the only honest measure of games/s.
//
//   tourbench <survivorDir> <games> <combo> <threads> [zombieDir]
#include "compete.h"
#include "engine.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
namespace fs = std::filesystem;
using namespace cw;

static std::vector<u8> readFile(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::vector<u8> v((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (v.size() > 512) v.resize(512);
    return v;
}

// The reference's grouping rule: files sorted case-insensitively, "<x>1"+"<x>2"
// form group "<x>", anything else is a lone group.
static std::vector<WarriorGroup> loadField(const std::string& dir) {
    std::vector<fs::path> files;
    if (fs::exists(dir))
        for (const auto& e : fs::directory_iterator(dir))
            if (e.is_regular_file() && !e.path().has_extension()) files.push_back(e.path());
    std::sort(files.begin(), files.end(), [](const fs::path& a, const fs::path& b) {
        std::string x = a.filename().string(), y = b.filename().string();
        for (char& c : x) c = (char)tolower(c);
        for (char& c : y) c = (char)tolower(c);
        return x < y;
    });
    std::vector<WarriorGroup> out;
    WarriorGroup cur;
    bool open = false;
    for (const fs::path& p : files) {
        std::string n = p.filename().string();
        std::vector<u8> code = readFile(p);
        if (code.empty()) continue;
        if (n.size() && n.back() == '1') {
            cur = WarriorGroup();
            cur.name = n.substr(0, n.size() - 1);
            cur.warriors.push_back({n, code, WarriorType::SURVIVOR_1, 0});
            open = true;
        } else if (n.size() && n.back() == '2' && open) {
            cur.warriors.push_back({n, code, WarriorType::SURVIVOR_2, 0});
            out.push_back(cur);
            open = false;
        } else {
            WarriorGroup g;
            g.name = n;
            g.warriors.push_back({n, code, WarriorType::SURVIVOR, 0});
            out.push_back(g);
            open = false;
        }
    }
    return out;
}

static std::vector<WarriorGroup> loadZombies(const std::string& dir) {
    std::vector<WarriorGroup> out;
    if (dir.empty() || !fs::exists(dir)) return out;
    WarriorGroup g;
    g.name = "Zombies";
    for (const auto& e : fs::directory_iterator(dir)) {
        if (!e.is_regular_file() || e.path().has_extension()) continue;
        std::vector<u8> code = readFile(e.path());
        if (code.empty()) continue;
        g.warriors.push_back({e.path().filename().string(), code, WarriorType::ZOMBIE, 0});
    }
    if (!g.warriors.empty()) out.push_back(g);
    return out;
}

int main(int argc, char** argv) {
    const std::string dir  = (argc > 1) ? argv[1] : "CGX-engine/survivors";
    const int games        = (argc > 2) ? std::atoi(argv[2]) : 2000;
    const int combo        = (argc > 3) ? std::atoi(argv[3]) : 4;
    const int threads      = (argc > 4) ? std::atoi(argv[4]) : (int)std::thread::hardware_concurrency();
    const std::string zdir = (argc > 5) ? argv[5] : "";

    std::vector<WarriorGroup> field = loadField(dir);
    std::vector<WarriorGroup> zomb  = loadZombies(zdir);
    if ((int)field.size() < combo) { std::printf("need >= %d groups in %s\n", combo, dir.c_str()); return 2; }
    std::printf("%d groups, combo %d, %d games, %d threads%s\n",
                (int)field.size(), combo, games, threads, zomb.empty() ? "" : ", zombies");

    std::atomic<int> next{0};
    std::atomic<long long> totRounds{0}, totWars{0}, totFailed{0}, totOps{0}, totLong{0};

    struct Counter : CycleObserver {
        long long ops = 0;
        void onCycleBegin(int) override { ++ops; }
        void onCycleEnd(int) override {}
        void onTurnEnd(int) override {}
        void onDeath(int, Fault) override {}
    };
    const bool count = getenv("TB_COUNT") != nullptr;

    auto worker = [&]() {
        War war(nullptr, nullptr, 2);
        std::vector<const WarriorGroup*> ptrs;
        std::vector<int> pick;
        std::vector<std::pair<std::string,float>> scores;
        long long rounds = 0, wars = 0, failed = 0, lng = 0;
        Counter counter;
        if (count) war.setObserver(&counter);
        for (;;) {
            const int k = next.fetch_add(1);
            if (k >= games) break;
            // the tournament runner's own draw, so the benchmark measures the
            // path the competition tab actually takes
            pick.resize(field.size());
            drawCombo(0, k, (int)field.size(), combo, pick.data());
            ptrs.clear();
            for (int i = 0; i < combo; ++i) ptrs.push_back(&field[(size_t)pick[i]]);
            for (const WarriorGroup& g : zomb) ptrs.push_back(&g);

            war.reset();
            war.setSeed(k);
            try { war.loadWarriorGroups(ptrs); }
            catch (const CpuFault&) { ++failed; continue; }
            int r = 0;
            for (; r < War::MAX_ROUND; ++r) {
                if (war.isOver()) break;
                war.nextRound(r);
            }
            scores.clear();
            war.collectScores(scores);
            rounds += r; ++wars; if (r >= War::MAX_ROUND) ++lng;
        }
        totRounds += rounds; totWars += wars; totFailed += failed;
        totOps += counter.ops; totLong += lng;
    };

    const auto t0 = std::chrono::steady_clock::now();
    std::vector<std::thread> pool;
    for (int i = 1; i < threads; ++i) pool.emplace_back(worker);
    worker();
    for (std::thread& t : pool) t.join();
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

    std::printf("  %.3f s   %.0f games/s   %lld wars (%lld void)   avg %.0f rounds/war\n",
                secs, totWars.load() / secs, totWars.load(), totFailed.load(),
                totWars.load() ? (double)totRounds.load() / (double)totWars.load() : 0.0);
    if (totWars.load())
        std::printf("  %lld hit MAX_ROUND (%.1f%%)   %.0f opcodes/war   %.1f ns/opcode\n",
                    totLong.load(), 100.0 * (double)totLong.load() / (double)totWars.load(),
                    (double)totOps.load() / (double)totWars.load(),
                    totOps.load() ? secs * threads / (double)totOps.load() * 1e9 : 0.0);
    return 0;
}
