// Throughput benchmark for the CPU engine.
//
// Two warriors that do nothing but "jmp $" (EB FE) keep the war alive for the
// full round count without ever ending it, so the number that comes out is the
// engine's raw opcode rate rather than a measure of how quickly a fight
// resolves. Counting is done with a CycleObserver so the observer path is
// measured too -- that is what the debugger uses.
//
//   benchwar [rounds] [warriors]
#include "engine.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace cw;

struct Counter : CycleObserver {
    unsigned long long ops = 0;
    void onCycleBegin(int) override { ++ops; }
    void onCycleEnd(int) override {}
    void onTurnEnd(int) override {}
    void onDeath(int, Fault) override {}
};

int main(int argc, char** argv) {
    const int rounds  = (argc > 1) ? std::atoi(argv[1]) : 200000;
    const int nwar    = (argc > 2) ? std::atoi(argv[2]) : 2;
    const bool observe = (argc > 3) ? (std::atoi(argv[3]) != 0) : false;

    std::vector<WarriorGroup> groups;
    for (int i = 0; i < nwar; ++i) {
        WarriorGroup g;
        g.name = "spin" + std::to_string(i);
        WarriorData w;
        w.name = g.name + "1";
        w.code = { 0xEB, 0xFE };            // jmp $
        w.type = WarriorType::SURVIVOR_1;
        g.warriors.push_back(w);
        groups.push_back(g);
    }
    std::vector<const WarriorGroup*> ptrs;
    for (const WarriorGroup& g : groups) ptrs.push_back(&g);

    Counter counter;
    War war(nullptr, nullptr, 2);
    war.setSeed(12345);
    war.loadWarriorGroups(ptrs);
    if (observe) war.setObserver(&counter);

    const auto t0 = std::chrono::steady_clock::now();
    int r = 0;
    for (; r < rounds; ++r) {
        if (war.isOver()) break;
        war.nextRound(r);
    }
    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

    std::printf("%d warriors, %d rounds%s\n", nwar, r, observe ? "  (observer attached)" : "");
    std::printf("  %.4f s   %.0f rounds/s\n", secs, r / secs);
    if (observe)
        std::printf("  %llu opcodes   %.2f M opcodes/s   %.1f ns/opcode\n",
                    counter.ops, counter.ops / secs / 1e6, secs / (double)counter.ops * 1e9);
    return 0;
}
