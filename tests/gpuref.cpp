// CPU-side mirror of the Vulkan engine's --digest output.
//
// `gpu-engine/engine.exe --digest` prints one line per war describing what
// the shader decided: how many rounds it ran, who was left, where each warrior
// was loaded and how each of the dead died. This runs the very same wars
// through cw::War -- the engine that is checked against the Java reference --
// and prints the same lines, so `diff` is a complete equivalence test for the
// shader, the loader and the scheduler at once.
//
// The load offsets are the sharpest part of it: they come straight out of the
// war's RNG stream, so a single extra or missing draw anywhere in loading
// shows up as a different line.
//
//   gpuref <survivorDir> <zombieDir|""> <games> <combo> <zspeed> <firstSeed> <pairSeed>
#include "compete.h"
#include "engine.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
namespace fs = std::filesystem;
using namespace cw;

// The Vulkan host's loader, matched exactly: files with a dot are skipped, the
// sort is case-insensitive, "<x>1"+"<x>2" pair up into group "<x>".
static std::vector<u8> readAll(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::vector<u8> v((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (v.size() > 512) v.resize(512);
    return v;
}
static bool ciLess(const std::string& a, const std::string& b) {
    return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(),
        [](char x, char y) { return std::tolower((unsigned char)x) < std::tolower((unsigned char)y); });
}
static std::vector<fs::path> listSorted(const std::string& dir) {
    std::vector<fs::path> out;
    if (!fs::is_directory(dir)) return out;
    for (const auto& e : fs::directory_iterator(dir)) {
        if (!e.is_regular_file()) continue;
        if (e.path().filename().string().find('.') != std::string::npos) continue;
        out.push_back(e.path());
    }
    std::sort(out.begin(), out.end(), [](const fs::path& a, const fs::path& b) {
        return ciLess(a.filename().string(), b.filename().string()); });
    return out;
}

static std::vector<WarriorGroup> loadField(const std::string& dir) {
    std::vector<WarriorGroup> out;
    bool open = false;
    for (const fs::path& p : listSorted(dir)) {
        const std::string n = p.filename().string();
        std::vector<u8> code = readAll(p);
        const char last = n.empty() ? 0 : n.back();
        if (last == '1') {
            WarriorGroup g;
            g.name = n.substr(0, n.size() - 1);
            g.warriors.push_back({n, code, WarriorType::SURVIVOR_1, 0, -1});
            out.push_back(g);
            open = true;
        } else if (last == '2' && open) {
            out.back().warriors.push_back({n, code, WarriorType::SURVIVOR_2, 0, -1});
            open = false;
        } else {
            WarriorGroup g;
            g.name = n;
            g.warriors.push_back({n, code, WarriorType::SURVIVOR, 0, -1});
            out.push_back(g);
            open = false;
        }
    }
    // the Vulkan host drops a dangling "...1", as the reference repository does
    if (open) out.pop_back();
    return out;
}

static WarriorGroup loadZombies(const std::string& dir) {
    WarriorGroup z;
    z.name = "ZoMbIeS";
    for (const fs::path& p : listSorted(dir)) {
        std::string n = p.filename().string(), low = n;
        for (char& c : low) c = (char)tolower((unsigned char)c);
        z.warriors.push_back({n, readAll(p),
                              (!low.empty() && low.back() == 'h') ? WarriorType::ZOMBIE_H
                                                                  : WarriorType::ZOMBIE, 0, -1});
    }
    return z;
}

// Death reasons in the shader's encoding: 0 none, 1 CPU, 2 memory.
struct Deaths : WarListener {
    std::vector<std::string> order;         // birth order == slot order
    std::vector<u32>         reason;
    void onWarriorBirth(const std::string& n) override {
        order.push_back(n);
        reason.push_back(0);
    }
    void onWarriorDeath(const std::string& n, const char* why) override {
        for (size_t i = 0; i < order.size(); ++i)
            if (order[i] == n) { reason[i] = (why[0] == 'm') ? 2u : 1u; return; }
    }
    void reset() { order.clear(); reason.clear(); }
};

int main(int argc, char** argv) {
    const std::string dir  = (argc > 1) ? argv[1] : "tests/data/nightly-2014/survivors";
    const std::string zdir = (argc > 2) ? argv[2] : "";
    const int games        = (argc > 3) ? std::atoi(argv[3]) : 64;
    int combo              = (argc > 4) ? std::atoi(argv[4]) : 4;
    const int zspeed       = (argc > 5) ? std::atoi(argv[5]) : 2;
    const s64 firstSeed    = (argc > 6) ? std::atoll(argv[6]) : 0;
    const s64 pairSeed     = (argc > 7) ? std::atoll(argv[7]) : 20260731;
    const int maxRounds    = (argc > 8) ? std::atoi(argv[8]) : War::MAX_ROUND;

    std::vector<WarriorGroup> field = loadField(dir);
    WarriorGroup zombies = loadZombies(zdir);
    if (field.empty()) { std::fprintf(stderr, "no groups in %s\n", dir.c_str()); return 2; }
    const int fieldSize = (int)field.size();
    if (combo > fieldSize) combo = fieldSize;

    Deaths deaths;
    War war(nullptr, &deaths, zspeed);
    std::vector<int> order((size_t)fieldSize);

    for (int k = 0; k < games; ++k) {
        drawCombo(pairSeed, k, fieldSize, combo, order.data());
        std::vector<const WarriorGroup*> chosen;
        for (int i = 0; i < combo; ++i) chosen.push_back(&field[(size_t)order[(size_t)i]]);
        if (!zombies.warriors.empty()) chosen.push_back(&zombies);

        deaths.reset();
        war.reset();
        const s64 seed = firstSeed + k;
        war.setSeed(seed);
        bool ok = true;
        try { war.loadWarriorGroups(chosen); } catch (const CpuFault&) { ok = false; }

        u32 endReason = 3u;                  // END_ABORTED
        int rounds = 0;
        u32 aliveMask = 0u, nSurv = 0u;
        if (ok) {
            rounds = war.runToEnd(maxRounds);
            for (int i = 0; i < war.numWarriors(); ++i)
                if (war.warrior(i).alive()) aliveMask |= 1u << i;
            nSurv = (u32)war.numRemainingSurvivors();
            // the shader's endReason: a single surviving group wins, otherwise
            // it is the round cap that stopped it
            if (rounds >= maxRounds) endReason = (nSurv == 1u) ? 1u : 2u;
            else                          endReason = (nSurv == 1u) ? 1u : 3u;
        }

        std::printf("war %d seed %lld end %u rounds %d alive %u mask %05x",
                    k, (long long)seed, endReason, rounds, nSurv, aliveMask);
        std::printf(" load");
        for (int i = 0; i < war.numWarriors(); ++i)
            std::printf(" %04x", (u32)(u16)war.warrior(i).loadOffset());
        std::printf(" death");
        for (int i = 0; i < war.numWarriors(); ++i)
            std::printf(" %u", i < (int)deaths.reason.size() ? deaths.reason[(size_t)i] : 0u);
        std::printf("\n");
    }
    return 0;
}
