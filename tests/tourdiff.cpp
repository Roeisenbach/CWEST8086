// Bulk equivalence harness: runs a whole tournament and prints one digest line
// per war, in exactly the format tests/ref/RefTour.java prints for the Java
// engine. Diff the two and any divergence in the draw, the loading, the fight,
// the deaths or the scoring shows up as a changed line.
//
// tests/selftest.cpp proves one war in detail; this proves hundreds at once,
// which is what a tournament actually exercises.
//
//   tourdiff <survDir> <zombDir|""> <games> <combo> <zspeed> <firstSeed>
#include "compete.h"
#include "engine.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
namespace fs = std::filesystem;
using namespace cw;

static const u64 FNV_OFF = 14695981039346656037ULL, FNV_PRIME = 1099511628211ULL;

struct Journal : MemoryListener {
    u64 hash = FNV_OFF;
    u64 writes = 0;
    void onWrite(s32 linear, u8, u8) override {
        hash ^= (u64)(u32)linear;
        hash *= FNV_PRIME;
        ++writes;
    }
};

struct Deaths : WarListener {
    std::string s;
    void onWarriorDeath(const std::string& n, const char* reason) override {
        s += n;
        s += '/';
        s += (reason[0] == 'm') ? 'M' : 'C';
        s += ';';
    }
};

static std::vector<u8> readAll(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::vector<u8> v((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (v.size() > 512) v.resize(512);
    return v;
}

static std::vector<WarriorGroup> loadField(const std::string& dir) {
    std::vector<fs::path> files;
    if (fs::exists(dir))
        for (const auto& e : fs::directory_iterator(dir))
            if (e.is_regular_file() && e.path().filename().string().find('.') == std::string::npos)
                files.push_back(e.path());
    std::sort(files.begin(), files.end(), [](const fs::path& a, const fs::path& b) {
        std::string x = a.filename().string(), y = b.filename().string();
        for (char& c : x) c = (char)tolower((unsigned char)c);
        for (char& c : y) c = (char)tolower((unsigned char)c);
        return x < y;
    });
    std::vector<WarriorGroup> out;
    WarriorGroup cur;
    bool open = false;
    for (const fs::path& p : files) {
        const std::string n = p.filename().string();
        std::vector<u8> code = readAll(p);
        const char last = n.empty() ? 0 : n.back();
        if (last == '1') {
            cur = WarriorGroup();
            cur.name = n.substr(0, n.size() - 1);
            cur.warriors.push_back({n, code, WarriorType::SURVIVOR_1, 0, -1});
            out.push_back(cur);
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
    return out;
}

static WarriorGroup loadZombies(const std::string& dir) {
    WarriorGroup z;
    z.name = "ZoMbIeS";
    if (dir.empty() || !fs::exists(dir)) return z;
    std::vector<fs::path> files;
    for (const auto& e : fs::directory_iterator(dir))
        if (e.is_regular_file()) files.push_back(e.path());
    std::sort(files.begin(), files.end());
    for (const fs::path& p : files) {
        std::string n = p.filename().string();
        std::string low = n;
        for (char& c : low) c = (char)tolower((unsigned char)c);
        const WarriorType t = (!low.empty() && low.back() == 'h') ? WarriorType::ZOMBIE_H
                                                                 : WarriorType::ZOMBIE;
        z.warriors.push_back({n, readAll(p), t, 0, -1});
    }
    return z;
}

int main(int argc, char** argv) {
    const std::string dir  = (argc > 1) ? argv[1] : "CGX-engine/survivors";
    const std::string zdir = (argc > 2) ? argv[2] : "";
    const int games        = (argc > 3) ? std::atoi(argv[3]) : 100;
    int combo              = (argc > 4) ? std::atoi(argv[4]) : 4;
    const int zspeed       = (argc > 5) ? std::atoi(argv[5]) : 2;
    const s64 first        = (argc > 6) ? std::atoll(argv[6]) : 0;

    std::vector<WarriorGroup> field = loadField(dir);
    WarriorGroup zombies = loadZombies(zdir);
    if (combo > (int)field.size()) combo = (int)field.size();

    std::printf("field %d combo %d games %d zspeed %d zombies %d\n",
                (int)field.size(), combo, games, zspeed, (int)zombies.warriors.size());

    Journal journal;
    Deaths  deaths;
    War war(&journal, &deaths, zspeed);

    for (int k = 0; k < games; ++k) {
        // the tournament runner's own draw, so this checks the shipping
        // pairing code and not a copy of it
        std::vector<int> idx(field.size());
        drawCombo(0, k, (int)field.size(), combo, idx.data());
        std::vector<const WarriorGroup*> chosen;
        for (int i = 0; i < combo; ++i) chosen.push_back(&field[(size_t)idx[(size_t)i]]);
        if (!zombies.warriors.empty()) chosen.push_back(&zombies);

        journal.hash = FNV_OFF;
        journal.writes = 0;
        deaths.s.clear();

        const s64 seed = first + k;
        war.reset();
        war.setSeed(seed);
        try {
            war.loadWarriorGroups(chosen);
        } catch (const CpuFault&) {
            std::printf("war %d seed %lld VOID\n", k, (long long)seed);
            continue;
        }
        const int round = war.runToEnd(War::MAX_ROUND);

        std::string alive;
        for (int i = 0; i < war.numWarriors(); ++i) {
            const Warrior& w = war.warrior(i);
            if (w.alive() && !w.zombie()) {
                if (!alive.empty()) alive += ',';
                alive += w.name();
            }
        }
        std::printf("war %d seed %lld rounds %d alive %d [%s] deaths %s writes %llu wj %016llx\n",
                    k, (long long)seed, round, war.numRemainingSurvivors(), alive.c_str(),
                    deaths.s.c_str(), (unsigned long long)journal.writes,
                    (unsigned long long)journal.hash);
        std::fflush(stdout);
    }
    return 0;
}
