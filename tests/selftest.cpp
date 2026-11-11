// =============================================================================
//  Headless engine self-test / regression harness.
//
//  Runs one war (a fixed set of groups + zombies at a given seed) with the
//  C++ engine and prints a deterministic summary: round count, end reason,
//  survivors, scores, and a checksum over the whole 1 MiB core plus the
//  ordered memory-write journal length. Two runs with the same inputs must
//  print byte-for-byte the same thing; when a JDK is available the same war
//  can be run through tests/ref/java and diffed.
//
//  build (from repo root, MSYS2 UCRT64):
//    g++ -O2 -std=c++17 tests/selftest.cpp src/engine/cpu.cpp src/engine/war.cpp -o build/tests/selftest.exe
//
//  usage:
//    build/tests/selftest.exe [survivorsDir] [zombiesDir] [seed] [comboSize] [zombieSpeed]
//    (survivorsDir default "tests/data/nightly-2014/survivors", zombiesDir "" = none,
//     seed 0, comboSize = every group, zombieSpeed 2)
// =============================================================================
#include "engine.h"
#include "replay.h"
#include "disasm.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace cw;

static constexpr int MAX_WARRIOR_SIZE = 512;   // WarriorRepository.MAX_WARRIOR_SIZE

static std::vector<u8> readAll(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::vector<u8> v((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if ((int)v.size() > MAX_WARRIOR_SIZE) v.resize(MAX_WARRIOR_SIZE);
    return v;
}

// Mirrors WarriorRepository.readWarriorsFileFromPath: files sorted
// case-insensitively; a name ending in '1' opens a group, '2' closes it,
// anything else is a one-warrior group. Files containing '.' are ignored
// (the Java loader deletes them; we just skip).
static std::vector<WarriorGroup> loadSurvivors(const fs::path& dir) {
    std::vector<fs::path> files;
    if (fs::is_directory(dir))
        for (auto& e : fs::directory_iterator(dir))
            if (e.is_regular_file() && e.path().filename().string().find('.') == std::string::npos)
                files.push_back(e.path());
    std::sort(files.begin(), files.end(), [](const fs::path& a, const fs::path& b) {
        std::string x = a.filename().string(), y = b.filename().string();
        std::transform(x.begin(), x.end(), x.begin(), ::tolower);
        std::transform(y.begin(), y.end(), y.begin(), ::tolower);
        return x < y;
    });

    std::vector<WarriorGroup> groups;
    WarriorGroup* cur = nullptr;
    for (auto& p : files) {
        std::string name = p.filename().string();
        std::vector<u8> code = readAll(p);
        char last = name.empty() ? 0 : name.back();
        if (last == '1') {
            groups.push_back(WarriorGroup{ name.substr(0, name.size() - 1), {} });
            cur = &groups.back();
            cur->warriors.push_back({ name, code, WarriorType::SURVIVOR_1 });
        } else if (last == '2' && cur) {
            cur->warriors.push_back({ name, code, WarriorType::SURVIVOR_2 });
            cur = nullptr;
        } else {
            groups.push_back(WarriorGroup{ name, { { name, code, WarriorType::SURVIVOR } } });
            cur = nullptr;
        }
    }
    return groups;
}

static WarriorGroup loadZombies(const fs::path& dir) {
    WarriorGroup z{ "ZoMbIeS", {} };
    if (!fs::is_directory(dir)) return z;
    std::vector<fs::path> files;
    for (auto& e : fs::directory_iterator(dir))
        if (e.is_regular_file()) files.push_back(e.path());
    std::sort(files.begin(), files.end());
    for (auto& p : files) {
        std::string name = p.filename().string();
        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        WarriorType t = (!lower.empty() && lower.back() == 'h') ? WarriorType::ZOMBIE_H
                                                               : WarriorType::ZOMBIE;
        z.warriors.push_back({ name, readAll(p), t });
    }
    return z;
}

struct CountingMem : MemoryListener {
    u64 writes = 0;
    u64 rolling = 14695981039346656037ULL;   // FNV-1a over (linear, seq)
    void onWrite(s32 linear, u8 /*oldVal*/, u8 /*newVal*/) override {
        rolling ^= (u64)(u32)linear;
        rolling *= 1099511628211ULL;
        ++writes;
    }
};

struct PrintListener : WarListener {
    int births = 0, deaths = 0;
    void onWarriorBirth(const std::string&) override { ++births; }
    void onWarriorDeath(const std::string& n, const char* r) override {
        ++deaths;
        std::printf("  death: %-28s (%s)\n", n.c_str(), r);
    }
};

static u64 coreChecksum(const Core& c) {
    const u8* p = c.data();
    u64 h = 14695981039346656037ULL;
    for (s32 i = 0; i < RMAddr::MEMORY_SIZE; ++i) { h ^= p[i]; h *= 1099511628211ULL; }
    return h;
}

int main(int argc, char** argv) {
    std::string survDir = (argc > 1) ? argv[1] : "tests/data/nightly-2014/survivors";
    std::string zombDir = (argc > 2) ? argv[2] : "";
    s64 seed            = (argc > 3) ? std::stoll(argv[3]) : 0;
    int comboSize       = (argc > 4) ? std::atoi(argv[4]) : -1;
    int zombieSpeed     = (argc > 5) ? std::atoi(argv[5]) : 2;

    std::vector<WarriorGroup> groups = loadSurvivors(survDir);
    WarriorGroup zombies = loadZombies(zombDir);

    if (groups.empty()) { std::fprintf(stderr, "no survivors in %s\n", survDir.c_str()); return 2; }
    if (comboSize < 0 || comboSize > (int)groups.size()) comboSize = (int)groups.size();

    std::vector<const WarriorGroup*> chosen;
    for (int i = 0; i < comboSize; ++i) chosen.push_back(&groups[(size_t)i]);
    if (!zombies.warriors.empty()) chosen.push_back(&zombies);

    std::printf("survivors dir : %s  (%d groups, using %d)\n", survDir.c_str(),
                (int)groups.size(), comboSize);
    std::printf("zombies       : %d\n", (int)zombies.warriors.size());
    std::printf("seed          : %lld   zombieSpeed %d\n", (long long)seed, zombieSpeed);

    CountingMem  mem;
    PrintListener wl;
    War war(&mem, &wl, zombieSpeed);
    war.setSeed(seed);

    try {
        war.loadWarriorGroups(chosen);
    } catch (const CpuFault&) {
        std::fprintf(stderr, "load failed (no room for a warrior)\n");
        return 3;
    }

    std::printf("warriors      : %d\n", war.numWarriors());

    int round = 0;
    for (; round < War::MAX_ROUND; ++round) {
        if (war.isOver()) break;
        war.nextRound(round);
    }

    std::vector<std::pair<std::string,float>> scores;
    war.collectScores(scores);

    std::printf("\n--- result ---\n");
    std::printf("rounds run    : %d\n", round);
    std::printf("survivors     : %d alive\n", war.numRemainingSurvivors());
    std::printf("mem writes     : %llu\n", (unsigned long long)mem.writes);
    std::printf("write journal  : %016llx\n", (unsigned long long)mem.rolling);
    std::printf("core checksum  : %016llx\n", (unsigned long long)coreChecksum(war.core()));
    std::printf("scoring share  : %s\n", scores.empty() ? "(none)" : "");
    for (auto& s : scores) std::printf("  %-28s %.6f\n", s.first.c_str(), s.second);

    // ---------------------------------------------------------------------
    //  Replay verification: record the same war, then check that replaying it
    //  reproduces the live run exactly, and that seeking is path-independent.
    // ---------------------------------------------------------------------
    if (getenv("CW_SKIP_REPLAY")) return 0;

    std::printf("\n--- replay ---\n");
    RecordOptions ropt;
    ropt.seed = seed;
    ropt.zombieSpeed = zombieSpeed;
    ropt.maxRounds = War::MAX_ROUND;
    ropt.breakPatterns.push_back({ 0xEB, 0x00 });

    std::vector<WarriorGroup> chosenCopy;
    for (const WarriorGroup* g : chosen) chosenCopy.push_back(*g);

    Replay rep;
    if (!recordWar(chosenCopy, ropt, rep, nullptr)) {
        std::printf("record failed: %s\n", rep.note.c_str());
        return 4;
    }
    std::printf("cycles         : %llu\n", (unsigned long long)rep.cycleCount());
    std::printf("rounds         : %d\n", rep.totalRounds);
    std::printf("snapshots      : %d (every %d cycles)\n", (int)rep.snaps.size(), rep.snapInterval);
    std::printf("deltas         : %llu\n", (unsigned long long)rep.deltas.size());
    std::printf("recording size : %.2f MB\n", (double)rep.bytes() / (1024.0 * 1024.0));

    Player pl;
    pl.bind(&rep);
    pl.seek(rep.cycleCount());

    // 1) the replayed end state must equal the live core, byte for byte
    const u8* live = war.core().data();
    size_t bad = 0, firstBad = 0;
    for (s32 a = SNAP_LO; a < rep.memHi; ++a)
        if (pl.readByte(a) != live[a]) { if (!bad) firstBad = (size_t)a; ++bad; }
    std::printf("end-state match: %s", bad ? "FAIL" : "OK");
    if (bad) std::printf("  (%zu bytes differ, first at %05zX)", bad, firstBad);
    std::printf("\n");

    // 2) registers and liveness must match too
    size_t badReg = 0;
    for (int i = 0; i < rep.numWarriors; ++i) {
        const CpuState& a = pl.cpu(i);
        const CpuState& b = war.warrior(i).cpu();
        u16 sa[RS_COUNT], sb[RS_COUNT];
        cpuToSlots(a, sa); cpuToSlots(b, sb);
        for (int s = 0; s < RS_COUNT; ++s) if (sa[s] != sb[s]) ++badReg;
        if (pl.alive(i) != war.warrior(i).alive()) ++badReg;
    }
    std::printf("cpu/liveness   : %s%s\n", badReg ? "FAIL " : "OK", badReg ? "" : "");

    // 3) seeking must be path independent: wander, then land on the same cycle
    //    as a clean seek and compare the whole reconstructed state
    const u64 n = rep.cycleCount();
    size_t badSeek = 0;
    if (n > 0) {
        const u64 probes[] = { 0, n / 7, n / 3, n / 2, (n * 5) / 6, n - 1, n };
        for (u64 t : probes) {
            Player a; a.bind(&rep); a.seek(t);
            Player b; b.bind(&rep);
            b.seek(n); b.seek(0); b.seek(n / 2); b.seek(t);     // wander first
            for (s32 x = SNAP_LO; x < rep.memHi; ++x)
                if (a.readByte(x) != b.readByte(x)) { ++badSeek; break; }
            for (int i = 0; i < rep.numWarriors; ++i) {
                u16 sa[RS_COUNT], sb[RS_COUNT];
                cpuToSlots(a.cpu(i), sa); cpuToSlots(b.cpu(i), sb);
                for (int s = 0; s < RS_COUNT; ++s) if (sa[s] != sb[s]) ++badSeek;
                if (a.ownerAt(0x1000) != b.ownerAt(0x1000)) ++badSeek;
            }
        }
    }
    std::printf("seek stability : %s\n", badSeek ? "FAIL" : "OK");

    // 4) stress the player the way the timeline does: random seeks, single
    //    steps in both directions, round steps and event jumps. Every so often
    //    the wandered state is compared against a clean seek to the same cycle.
    size_t badStress = 0;
    {
        u64 rng = 0x9E3779B97F4A7C15ull ^ (u64)seed;
        auto next = [&rng]() { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return rng; };
        Player w; w.bind(&rep);
        Player fresh;
        const int kOps = 60000;
        for (int op = 0; op < kOps && !badStress; ++op) {
            switch (next() % 6) {
                case 0: w.seek(n ? next() % (n + 1) : 0); break;
                case 1: w.stepCycle(+1); break;
                case 2: w.stepCycle(-1); break;
                case 3: w.stepRound(+1); break;
                case 4: w.stepRound(-1); break;
                default: w.seekEvent((next() & 1) ? +1 : -1); break;
            }
            if (w.cycle() > n) { badStress = 1; break; }
            if (w.round() < 0 || w.round() > rep.totalRounds) { badStress = 2; break; }
            if (op % 997 == 0) {
                fresh.bind(&rep);
                fresh.seek(w.cycle());
                for (s32 x = SNAP_LO; x < rep.memHi; ++x)
                    if (w.readByte(x) != fresh.readByte(x)) { badStress = 3; break; }
                for (int i = 0; i < rep.numWarriors && !badStress; ++i) {
                    u16 sa[RS_COUNT], sb[RS_COUNT];
                    cpuToSlots(w.cpu(i), sa); cpuToSlots(fresh.cpu(i), sb);
                    for (int q = 0; q < RS_COUNT; ++q) if (sa[q] != sb[q]) { badStress = 4; break; }
                    if (w.alive(i) != fresh.alive(i)) badStress = 5;
                    if (w.ipOffset(i) != fresh.ipOffset(i)) badStress = 6;
                }
                for (int off = 0; off < War::ARENA_SIZE && !badStress; off += 61)
                    if (w.ownerAt(off) != fresh.ownerAt(off)) badStress = 7;
            }
        }
    }
    std::printf("seek stress    : %s", badStress ? "FAIL" : "OK");
    if (badStress) std::printf("  (code %zu)", badStress);
    std::printf("\n");

    // 5) disassembler agreement: for every non-branching instruction the engine
    //    actually executed, the decoded length must equal how far IP moved.
    //    This pins the disassembler to the CPU's own byte consumption.
    size_t badLen = 0, checked = 0, firstBadCycle = 0;
    char firstBadText[64] = {0};
    {
        struct Tally { char m[10]; u64 n; };
        std::vector<Tally> hist;
        Player d; d.bind(&rep);
        for (u64 c = 0; c < n; ++c) {
            const CycleRec& rc = rep.cycles[(size_t)c];
            const int wi = rc.warrior;
            const u16 ipBefore = (u16)d.cpu(wi).ip;
            const u8* ops = rep.opsOf(c);
            Insn in = disasm(ops, kOpBytes);

            d.stepCycle(+1);
            const u16 ipAfter = (u16)d.cpu(wi).ip;

            // count what ran, for a sanity histogram
            bool seen = false;
            for (Tally& t : hist) if (std::strcmp(t.m, in.mnem) == 0) { ++t.n; seen = true; break; }
            if (!seen && hist.size() < 128) { Tally t{}; snprintf(t.m, sizeof(t.m), "%s", in.mnem); t.n = 1; hist.push_back(t); }

            if (in.control || in.invalid || in.unsupported) continue;
            if (rc.flags & CF_DEATH) continue;          // faulted part-way through
            ++checked;
            if ((u16)(ipBefore + in.len) != ipAfter) {
                if (!badLen) { firstBadCycle = (size_t)c; snprintf(firstBadText, sizeof(firstBadText), "%s", in.text); }
                ++badLen;
            }
        }
        std::printf("disasm lengths : %s  (%llu checked)", badLen ? "FAIL" : "OK",
                    (unsigned long long)checked);
        if (badLen) std::printf("  %zu bad, first at cycle %zu: %s", badLen, firstBadCycle, firstBadText);
        std::printf("\n");

        std::sort(hist.begin(), hist.end(), [](const Tally& a, const Tally& b){ return a.n > b.n; });
        std::printf("top opcodes    : ");
        for (size_t k = 0; k < hist.size() && k < 8; ++k)
            std::printf("%s x%llu  ", hist[k].m, (unsigned long long)hist[k].n);
        std::printf("\n");

        // raw first-byte histogram, to catch a mis-decode wearing a plausible
        // mnemonic
        std::vector<u64> byteHist(256, 0);
        for (u64 c2 = 0; c2 < n; ++c2) byteHist[rep.opsOf(c2)[0]]++;
        std::vector<int> order(256);
        for (int k = 0; k < 256; ++k) order[k] = k;
        std::sort(order.begin(), order.end(),
                  [&](int a2, int b2){ return byteHist[(size_t)a2] > byteHist[(size_t)b2]; });
        std::printf("top opbytes    : ");
        for (int k = 0; k < 8; ++k)
            if (byteHist[(size_t)order[k]])
                std::printf("%02X x%llu  ", order[k], (unsigned long long)byteHist[(size_t)order[k]]);
        std::printf("\n");
    }

    return (bad || badReg || badSeek || badStress || badLen) ? 5 : 0;
}
