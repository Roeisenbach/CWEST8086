// A tournament must score the same however many cores it is spread over: the
// seeds are fixed, so thread count is only a scheduling detail.
#include "compete.h"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>
namespace fs = std::filesystem;
using namespace cw;

static void load(const std::string& dir, std::vector<WarriorGroup>& out, int want) {
    if (!fs::exists(dir)) return;
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
        wd.name = g.name + "1";
        wd.code = code;
        wd.type = WarriorType::SURVIVOR_1;
        g.warriors.push_back(wd);
        out.push_back(g);
    }
}

int main(int argc, char** argv) {
    const std::string dir = (argc > 1) ? argv[1] : "tests/data/nightly-2014/survivors";
    const int games = (argc > 2) ? std::atoi(argv[2]) : 60;

    std::vector<WarriorGroup> groups;
    if (dir == "SPIN") {
        // Two warriors that only ever "jmp $": neither can ever win, so every
        // game runs the full round count. Real warriors at a low round cap
        // finish almost at once, which measures war setup rather than the
        // engine -- a games/s taken that way says nothing about opcode speed.
        for (int i = 0; i < 2; ++i) {
            WarriorGroup g;
            g.name = "spin" + std::to_string(i);
            WarriorData w;
            w.name = g.name + "1";
            w.code = { 0xEB, 0xFE };
            w.type = WarriorType::SURVIVOR_1;
            g.warriors.push_back(w);
            groups.push_back(g);
        }
    } else {
        load(dir, groups, 24);
    }
    if (groups.size() < 2) { std::printf("need warriors in %s\n", dir.c_str()); return 2; }
    const int combo = (argc > 4) ? std::atoi(argv[4]) : 4;
    std::printf("%d groups, %d games, %d per battle\n", (int)groups.size(), games, combo);

    CompeteOptions opt;
    opt.comboSize   = (dir == "SPIN") ? 0 : combo;
    opt.pairingSeed = 20260731;
    opt.firstSeed = 4242;
    opt.games = games;
    opt.maxRounds = (argc > 3) ? std::atoi(argv[3]) : 40000;

    CompeteResult one, many;
    CompeteProgress p1, p2;

    auto clk = [] { return std::chrono::steady_clock::now(); };

    opt.threads = 1;
    auto t0 = clk();
    runCompetition(groups, opt, one, &p1);
    const double secs1 = std::chrono::duration<double>(clk() - t0).count();

    opt.threads = (int)std::thread::hardware_concurrency();
    t0 = clk();
    runCompetition(groups, opt, many, &p2);
    const double secsN = std::chrono::duration<double>(clk() - t0).count();

    std::printf("single thread : %d run, %d failed, %.3f s -> %.0f games/s\n",
                one.gamesRun, one.gamesFailed, secs1, one.gamesRun / secs1);
    std::printf("%d threads    : %d run, %d failed, %.3f s -> %.0f games/s\n",
                opt.threads, many.gamesRun, many.gamesFailed, secsN, many.gamesRun / secsN);

    int bad = 0;
    if (one.gamesRun != many.gamesRun || one.gamesFailed != many.gamesFailed) bad++;
    for (size_t i = 0; i < one.warriorName.size(); i++) {
        const double a = one.warriorScore[i], b = many.warriorScore[i];
        const bool same = (a - b < 1e-9) && (b - a < 1e-9) &&
                          one.warriorSurvived[i] == many.warriorSurvived[i];
        std::printf("  %-28s %8.3f  %8.3f  %s\n", one.warriorName[i].c_str(), a, b,
                    same ? "" : "  <-- DIFFERS");
        if (!same) bad++;
    }

    // and the group totals have to be the sum of their survivors
    for (size_t g = 0; g < one.groupName.size(); g++) {
        double sum = 0.0;
        for (size_t i = 0; i < one.warriorName.size(); i++)
            if (one.warriorGroup[i] == (int)g) sum += one.warriorScore[i];
        if (sum - one.groupScore[g] > 1e-9 || one.groupScore[g] - sum > 1e-9) {
            std::printf("  group %s total %.3f != %.3f\n", one.groupName[g].c_str(),
                        one.groupScore[g], sum);
            bad++;
        }
    }

    // ---- and splitting a run between two lanes must lose nothing ---------
    // This is what the GPU lane relies on: one lane plays the even global
    // indices and the other the odd ones, and the two results add up to the
    // single-lane run. Both the seed and the combination are drawn from the
    // global index, so the two cover the tournament exactly once between them.
    if (games % 2 == 0) {
        CompeteResult a, b;
        CompeteProgress pa, pb;
        CompeteOptions sp = opt;
        sp.games = games / 2;
        sp.seedStride = 2;
        sp.threads = 0;

        sp.seedPhase = 0; runCompetition(groups, sp, a, &pa);
        sp.seedPhase = 1; runCompetition(groups, sp, b, &pb);

        int splitBad = 0;
        if (a.gamesRun + b.gamesRun != one.gamesRun) splitBad++;
        for (size_t i = 0; i < one.warriorName.size(); i++) {
            const double got  = a.warriorScore[i] + b.warriorScore[i];
            const double want = one.warriorScore[i];
            if (got - want > 1e-9 || want - got > 1e-9) {
                std::printf("  split %-24s %8.3f + %8.3f = %8.3f  want %8.3f\n",
                            one.warriorName[i].c_str(), a.warriorScore[i], b.warriorScore[i],
                            got, want);
                splitBad++;
            }
        }
        std::printf("split into two lanes (%d + %d games): %s\n",
                    a.gamesRun, b.gamesRun,
                    splitBad ? "DIFFERS" : "sums to the single-lane run");
        bad += splitBad;
    }

    std::printf("\n%s\n", bad ? "FAILED"
                              : "OK -- identical across thread counts, and across a split");
    return bad ? 1 : 0;
}
