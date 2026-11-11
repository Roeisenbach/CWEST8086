// The Java lane (CGX-engine/lane/JavaLane.java) must score a tournament exactly
// as the C++ runner does: same draw, same seeds, same engine semantics.
//
// Reads a Java-lane field file (the one the app writes to %TEMP%\cwest_java),
// plays it through cw::runCompetition, and compares the result with the scores
// file JavaLane wrote for the same options.
//
//   javalanetest <field.txt> <scores.txt> <seed> <pairSeed> <combo> <stride>
//                <phase> <games> <rounds> <zspeed>
#include "compete.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
using namespace cw;

static std::vector<u8> unhex(const std::string& s) {
    std::vector<u8> b(s.size() / 2);
    for (size_t i = 0; i < b.size(); ++i) b[i] = (u8)std::stoi(s.substr(2 * i, 2), nullptr, 16);
    return b;
}

int main(int argc, char** argv) {
    if (argc < 11) { std::printf("usage: see the top of javalanetest.cpp\n"); return 2; }
    std::ifstream ff(argv[1]);
    std::string line;
    std::getline(ff, line);
    std::vector<WarriorGroup> groups;
    while (std::getline(ff, line)) {
        std::istringstream in(line);
        std::string tag;
        in >> tag;
        if (tag == "g") {
            WarriorGroup g;
            int n = 0;
            in >> g.name >> n;
            for (int i = 0; i < n && std::getline(ff, line); ++i) {
                std::istringstream w(line);
                std::string t2, hex;
                int type = 0;
                WarriorData wd;
                w >> t2 >> wd.name >> type >> hex;
                wd.code = unhex(hex);
                wd.type = type == 2 ? WarriorType::ZOMBIE_H : type == 1 ? WarriorType::ZOMBIE
                                                                        : WarriorType::SURVIVOR;
                g.warriors.push_back(wd);
            }
            groups.push_back(g);
        }
    }

    CompeteOptions opt;
    opt.firstSeed   = std::atoll(argv[3]);
    opt.pairingSeed = std::atoll(argv[4]);
    opt.comboSize   = std::atoi(argv[5]);
    opt.seedStride  = std::atoi(argv[6]);
    opt.seedPhase   = std::atoi(argv[7]);
    opt.games       = std::atoi(argv[8]);
    opt.maxRounds   = std::atoi(argv[9]);
    opt.zombieSpeed = std::atoi(argv[10]);
    CompeteResult r;
    runCompetition(groups, opt, r, nullptr);

    std::ifstream sf(argv[2]);
    std::map<std::string, std::pair<double, int>> java;
    int jRun = -1, jFailed = -1;
    while (std::getline(sf, line)) {
        std::istringstream in(line);
        std::string tag;
        in >> tag;
        if (tag == "games") { double s; in >> jRun >> jFailed >> s; }
        else if (tag == "w") { std::string n; double sc; int sv; in >> n >> sc >> sv; java[n] = { sc, sv }; }
    }

    int bad = 0;
    std::printf("C++  games %d failed %d\nJava games %d failed %d\n", r.gamesRun, r.gamesFailed, jRun, jFailed);
    if (r.gamesRun != jRun || r.gamesFailed != jFailed) ++bad;
    double worst = 0.0;
    for (size_t i = 0; i < r.warriorName.size(); ++i) {
        auto it = java.find(r.warriorName[i]);
        if (it == java.end()) { std::printf("missing %s\n", r.warriorName[i].c_str()); ++bad; continue; }
        const double d = std::fabs(it->second.first - r.warriorScore[i]);
        if (d > worst) worst = d;
        if (d > 1e-6 || it->second.second != r.warriorSurvived[i]) {
            std::printf("DIFF %-6s C++ %.6f/%d  Java %.6f/%d\n", r.warriorName[i].c_str(),
                        r.warriorScore[i], r.warriorSurvived[i], it->second.first, it->second.second);
            ++bad;
        }
    }
    std::printf("%zu warriors, worst |score diff| %.3g (summation order only)\n%s\n",
                r.warriorName.size(), worst, bad ? "MISMATCH" : "IDENTICAL");
    return bad ? 1 : 0;
}
