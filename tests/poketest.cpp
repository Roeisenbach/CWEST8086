// A debugger edit has to behave like a change to the war itself: identical up
// to the cycle it was made at, different from there on, and the poked byte has
// to actually be the value asked for at that moment.
#include "replay.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>
namespace fs = std::filesystem;
using namespace cw;

static bool load(const std::string& dir, std::vector<WarriorGroup>& out, int want) {
    if (!fs::exists(dir)) return false;
    for (const auto& e : fs::directory_iterator(dir)) {
        if ((int)out.size() >= want) break;
        if (!e.is_regular_file() || e.path().has_extension()) continue;
        std::ifstream f(e.path(), std::ios::binary);
        std::vector<u8> code((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        if (code.empty()) continue;
        if (code.size() > 512) code.resize(512);
        WarriorGroup g;
        g.name = e.path().filename().string();
        WarriorData wd;
        wd.name = g.name;
        wd.code = code;
        wd.type = WarriorType::SURVIVOR_1;
        g.warriors.push_back(wd);
        out.push_back(g);
    }
    return out.size() >= 2;
}

int main(int argc, char** argv) {
    const std::string dir = (argc > 1) ? argv[1] : "tests/data/nightly-2014/survivors";
    std::vector<WarriorGroup> groups;
    if (!load(dir, groups, 3)) { std::printf("need at least 2 warriors in %s\n", dir.c_str()); return 2; }

    RecordOptions opt;
    opt.seed = 12345;
    opt.maxRounds = 3000;
    opt.runUntilAllSurvivorsDead = true;
    opt.breakPatterns.push_back({ 0xEB, 0x00 });

    Replay base;
    if (!recordWar(groups, opt, base, nullptr)) { std::printf("record failed\n"); return 2; }
    std::printf("baseline: %llu cycles, %d rounds\n",
                (unsigned long long)base.cycleCount(), base.totalRounds);
    if (base.cycleCount() < 400) { std::printf("war too short to test\n"); return 2; }

    // poke a byte the warriors are actually using: pick the arena address the
    // 300th cycle wrote to, and force it to a value it did not have
    const u64 at = 300;
    Player pb; pb.bind(&base); pb.seek(at);
    const s32 target = SNAP_LO + 0x40;
    const u8 was = pb.readByte(target);
    const u8 now = (u8)(was ^ 0xFF);

    Poke p;
    p.cycle = at;
    p.linear = target;
    p.value = now;
    RecordOptions opt2 = opt;
    opt2.pokes.push_back(p);

    Replay edited;
    if (!recordWar(groups, opt2, edited, nullptr)) { std::printf("edited record failed\n"); return 2; }
    std::printf("edited  : %llu cycles, %d rounds\n",
                (unsigned long long)edited.cycleCount(), edited.totalRounds);

    int bad = 0;

    // 1. identical up to the poke
    Player pe; pe.bind(&edited);
    for (u64 c = 0; c <= at; c += 37) {
        pb.seek(c); pe.seek(c);
        for (s32 a = SNAP_LO; a < SNAP_LO + 0x800; a += 7)
            if (pb.readByte(a) != pe.readByte(a)) { bad++; break; }
    }
    std::printf("before the edit  : %s\n", bad ? "DIFFERS (wrong)" : "identical");

    // 2. the poked byte really took the new value
    pe.seek(at + 1);
    const u8 got = pe.readByte(target);
    const bool tookIt = (got == now);
    std::printf("poked byte       : was %02X, asked %02X, got %02X  -> %s\n",
                was, now, got, tookIt ? "applied" : "NOT APPLIED");
    if (!tookIt) bad++;

    // 3. and the baseline still has the old value there
    pb.seek(at + 1);
    std::printf("baseline byte    : %02X (unchanged: %s)\n",
                pb.readByte(target), pb.readByte(target) == was ? "yes" : "no");

    // 4. the war afterwards is a different war
    bool diverged = base.cycleCount() != edited.cycleCount();
    if (!diverged) {
        const u64 late = base.cycleCount() - 1;
        pb.seek(late); pe.seek(late);
        for (s32 a = SNAP_LO; a < SNAP_LO + War::ARENA_SIZE; a += 13)
            if (pb.readByte(a) != pe.readByte(a)) { diverged = true; break; }
    }
    std::printf("after the edit   : %s\n", diverged ? "diverged" : "identical (edit had no effect)");

    // ---- and the register path, which always changes the outcome ----------
    pb.seek(at);
    const int victim = 0;
    const u16 oldIp = (u16)pb.cpu(victim).ip;
    Poke rp;
    rp.cycle = at;
    rp.isReg = true;
    rp.warrior = victim;
    rp.reg = PR_IP;
    rp.regValue = (u16)(oldIp + 1);        // shove it off its instruction boundary

    RecordOptions opt3 = opt;
    opt3.pokes.push_back(rp);
    Replay reg;
    if (!recordWar(groups, opt3, reg, nullptr)) { std::printf("register record failed\n"); return 2; }

    // A register edit is held until its own warrior's next cycle -- that is the
    // only cycle whose record carries that warrior's registers -- so the check
    // has to look at the state just after that cycle, not at at+1.
    u64 acts = 0;
    for (u64 c = at; c < reg.cycleCount(); ++c)
        if (reg.cycles[(size_t)c].warrior == victim) { acts = c; break; }

    Player pr; pr.bind(&reg);
    pr.seek(acts + 1);
    const u16 gotIp = (u16)pr.cpu(victim).ip;
    pb.seek(acts + 1);
    const u16 baseIp = (u16)pb.cpu(victim).ip;
    std::printf("\npoked IP         : was %04X, asked %04X\n", oldIp, (unsigned)(u16)(oldIp + 1));
    std::printf("warrior %d next ran at cycle %llu; after it IP = %04X (baseline %04X)\n",
                victim, (unsigned long long)acts, gotIp, baseIp);
    const bool regTook = (gotIp != baseIp);
    std::printf("register path    : %s\n", regTook ? "applied" : "NOT APPLIED");
    if (!regTook) bad++;

    const bool regDiverged = (reg.cycleCount() != base.cycleCount()) ||
                             (reg.totalRounds != base.totalRounds);
    std::printf("war after it     : %s (%llu cycles vs %llu)\n",
                regDiverged ? "diverged" : "same length",
                (unsigned long long)reg.cycleCount(), (unsigned long long)base.cycleCount());

    std::printf("\n%s\n", bad ? "FAILED" : "OK");
    return bad ? 1 : 0;
}
