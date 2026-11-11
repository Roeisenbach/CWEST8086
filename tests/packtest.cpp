// Pack round trip, and the property the distributed mode rests on: N shards
// of stride N, each run on its own and merged through result packs, equal one
// straight run of the same tournament.
//
//   packtest <survivorDir> [games] [shards]
#include "compete.h"
#include "netpack.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
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
        WarriorData w; w.name = name; w.code = code;
        const char last = name.back();
        if (last == '1') {
            out.push_back(WarriorGroup()); open = &out.back();
            open->name = name.substr(0, name.size() - 1);
            w.type = WarriorType::SURVIVOR_1; open->warriors.push_back(w);
        } else if (last == '2' && open) {
            w.type = WarriorType::SURVIVOR_2; open->warriors.push_back(w); open = nullptr;
        } else {
            WarriorGroup g; g.name = name; w.type = WarriorType::SURVIVOR;
            g.warriors.push_back(w); out.push_back(g); open = nullptr;
        }
    }
    if (open && open->warriors.size() == 1) out.pop_back();
}

int main(int argc, char** argv) {
    const std::string dir = (argc > 1) ? argv[1] : "CGX-engine/survivors";
    const int games  = (argc > 2) ? std::atoi(argv[2]) : 240;
    const int shards = (argc > 3) ? std::atoi(argv[3]) : 3;

    FieldPack f;
    loadField(dir, f.groups);
    if (f.groups.size() < 2) { std::printf("no field in %s\n", dir.c_str()); return 2; }
    // a zombie with its own speed, and a name with spaces, to exercise the format
    {
        WarriorGroup z; z.name = "the zombies";
        WarriorData a; a.name = "slow one"; a.type = WarriorType::ZOMBIE; a.speed = 3;
        a.code = { 0xEB, 0xFE };
        WarriorData b; b.name = "heavy"; b.type = WarriorType::ZOMBIE_H; b.speed = 0;
        b.code = { 0x90, 0xEB, 0xFD };
        z.warriors = { a, b };
        f.groups.push_back(z);
    }
    f.seed = 12345; f.pairingSeed = 20260731; f.comboSize = 4; f.games = games;
    f.title = "packtest run";

    // ---- 1. field pack round trip ----------------------------------------
    int fails = 0;
    {
        const std::string text = writeFieldPack(f);
        FieldPack g; std::string why;
        if (!parseFieldPack(text, g, why)) { std::printf("FAIL field parse: %s\n", why.c_str()); return 1; }
        const std::string again = writeFieldPack(g);
        if (again != text) { std::printf("FAIL field pack not stable\n"); fails++; }
        bool same = g.groups.size() == f.groups.size();
        for (size_t i = 0; same && i < f.groups.size(); ++i) {
            same = g.groups[i].name == f.groups[i].name && g.groups[i].warriors.size() == f.groups[i].warriors.size();
            for (size_t j = 0; same && j < f.groups[i].warriors.size(); ++j) {
                const WarriorData& a = f.groups[i].warriors[j]; const WarriorData& b = g.groups[i].warriors[j];
                same = a.name == b.name && a.code == b.code && a.type == b.type && a.speed == b.speed;
            }
        }
        if (!same) { std::printf("FAIL field pack lost something\n"); fails++; }
        std::printf("field pack: %zu bytes, %zu groups, round trip %s\n", text.size(), g.groups.size(), same ? "ok" : "BAD");
    }

    // ---- 2. one straight run -----------------------------------------------
    CompeteOptions opt;
    opt.firstSeed = f.seed; opt.games = games; opt.comboSize = f.comboSize;
    opt.pairingSeed = f.pairingSeed; opt.threads = 0;
    CompeteResult whole;
    runCompetition(f.groups, opt, whole, nullptr);

    // ---- 3. shards, each through a pack, merged -------------------------------
    CompeteResult merged;
    prepareResult(f.groups, merged);
    for (int p = 0; p < shards; ++p) {
        // the shard file, as the host would write it
        FieldPack sf = f; sf.stride = shards; sf.phase = p;
        sf.games = (games - p + shards - 1) / shards;     // games p, p+S, ... below `games`
        FieldPack got; std::string why;
        if (!parseFieldPack(writeFieldPack(sf), got, why)) { std::printf("FAIL shard %d: %s\n", p, why.c_str()); return 1; }

        CompeteOptions so = opt;
        so.games = got.games; so.seedStride = got.stride; so.seedPhase = got.phase;
        so.firstSeed = got.seed; so.pairingSeed = got.pairingSeed; so.comboSize = got.comboSize;
        CompeteResult part;
        runCompetition(got.groups, so, part, nullptr);

        // the result file, as the participant would save it
        ResultPack rp;
        if (!parseResultPack(writeResultPack(part, "shard " + std::to_string(p), 7), rp, why)) {
            std::printf("FAIL result parse: %s\n", why.c_str()); return 1;
        }
        const int unknown = addResultPack(merged, rp);
        if (unknown) { std::printf("FAIL %d unknown names in shard %d\n", unknown, p); fails++; }
        std::printf("shard %d/%d: %d games\n", p, shards, part.gamesRun);
    }

    bool same = merged.gamesRun == whole.gamesRun;
    for (size_t i = 0; i < whole.warriorScore.size(); ++i) {
        if (std::fabs(merged.warriorScore[i] - whole.warriorScore[i]) > 1e-9 ||
            merged.warriorSurvived[i] != whole.warriorSurvived[i]) {
            std::printf("  differs: %s  %.6f vs %.6f  (%d vs %d)\n", whole.warriorName[i].c_str(),
                        merged.warriorScore[i], whole.warriorScore[i],
                        merged.warriorSurvived[i], whole.warriorSurvived[i]);
            same = false;
        }
    }
    for (size_t g = 0; g < whole.groupScore.size(); ++g)
        if (std::fabs(merged.groupScore[g] - whole.groupScore[g]) > 1e-9) same = false;
    std::printf("%d shards merged: %d games vs %d straight -- %s\n",
                shards, merged.gamesRun, whole.gamesRun, same ? "IDENTICAL" : "DIFFERENT");
    if (!same) fails++;

    // ---- 4. a pack from another field is noticed --------------------------------
    {
        CompeteResult other = whole;
        other.warriorName[0] = "somebody else";
        ResultPack rp; std::string why;
        parseResultPack(writeResultPack(other, "stranger", 7), rp, why);
        CompeteResult sink; prepareResult(f.groups, sink);
        const int unknown = addResultPack(sink, rp);
        std::printf("foreign name flagged: %s\n", unknown == 1 ? "yes" : "NO");
        if (unknown != 1) fails++;
    }
    return fails ? 1 : 0;
}
