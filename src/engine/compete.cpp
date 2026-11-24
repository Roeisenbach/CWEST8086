#include "compete.h"

#include <chrono>
#include <climits>
#include <mutex>
#include <thread>
#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

namespace cw {
namespace {

// splitmix64: turns (pairingSeed, k) into a well-spread 64-bit value, so the
// draws for consecutive games share nothing. Unsigned throughout -- the mixing
// relies on wrap-around, which is only defined for unsigned types.
inline u64 mix64(u64 z) {
    z += 0x9E3779B97F4A7C15ULL;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

// Per-worker scratch, so a war costs no allocation at all once the first one
// has run.
struct Scratch {
    std::vector<int>                  order;   // field indices, shuffled in place
    std::vector<const WarriorGroup*>  ptrs;    // the groups of one battle
    std::vector<int>                  slots;   // scoring slots of the survivors
};

} // namespace

bool engineOptimized() {
#ifdef __OPTIMIZE__
    return true;
#else
    return false;
#endif
}

bool isZombieGroup(const WarriorGroup& g) {
    if (g.warriors.empty()) return false;
    for (const WarriorData& w : g.warriors)
        if (!isZombie(w.type)) return false;
    return true;
}

void drawCombo(s64 pairingSeed, int k, int fieldSize, int comboSize, int* out) {
    if (comboSize > fieldSize) comboSize = fieldSize;
    // A partial Fisher-Yates over a fresh 0..fieldSize-1: the first comboSize
    // entries are a uniform combination, and only that many swaps are needed.
    // java.util.Random, so the draw is reproducible outside C++ too.
    JavaRandom rnd((s64)mix64((u64)pairingSeed + (u64)(s64)k * 0x9E3779B97F4A7C15ULL));
    for (int i = 0; i < fieldSize; ++i) out[i] = i;
    for (int i = 0; i < comboSize; ++i) {
        const int j = i + rnd.nextInt(fieldSize - i);
        const int t = out[i]; out[i] = out[j]; out[j] = t;
    }
}

void prepareResult(const std::vector<WarriorGroup>& groups, CompeteResult& out) {
    out = CompeteResult();
    for (const WarriorGroup& g : groups) {
        const int gi = (int)out.groupName.size();
        out.groupName.push_back(g.name);
        for (const WarriorData& w : g.warriors) {
            if (isZombie(w.type)) continue;          // zombies are never scored
            out.warriorName.push_back(w.name);
            out.warriorGroup.push_back(gi);
        }
    }
    out.warriorScore.assign(out.warriorName.size(), 0.0);
    out.warriorSurvived.assign(out.warriorName.size(), 0);
    out.groupScore.assign(out.groupName.size(), 0.0);
    out.groupSurvived.assign(out.groupName.size(), 0);
}

void runCompetition(const std::vector<WarriorGroup>& groups, const CompeteOptions& opt,
                    CompeteResult& out, CompeteProgress* prog) {
    prepareResult(groups, out);
    if (out.warriorName.empty() || opt.games < 0) {
        out.note = "nothing to run";
        return;
    }
    // Open-ended: nothing bounds the loop but the caller's cancel flag. The
    // count is kept far from INT_MAX so a chunk claimed past it cannot wrap.
    const bool unlimited = (opt.games == 0);
    const int  gamesCap  = unlimited ? (INT_MAX / 2) : opt.games;

    // A local copy of the field, with each warrior tagged with the row it
    // scores into. That tag is what makes folding a finished war into the
    // standings a couple of array increments rather than a name lookup.
    std::vector<WarriorGroup> field;      // drawn into each battle
    std::vector<WarriorGroup> always;     // zombies: present in every battle
    std::vector<int>          fieldOf;    // per entry of `field`: which separate field it is in
    {
        size_t slot = 0;
        for (size_t gi = 0; gi < groups.size(); ++gi) {
            const WarriorGroup& g = groups[gi];
            WarriorGroup copy = g;
            for (WarriorData& w : copy.warriors)
                w.slot = isZombie(w.type) ? -1 : (int)slot++;
            if (isZombieGroup(g)) always.push_back(std::move(copy));
            else {
                field.push_back(std::move(copy));
                const int f = (opt.fieldCount > 1 && gi < opt.groupField.size()) ? opt.groupField[gi] : 0;
                fieldOf.push_back(f);
            }
        }
    }
    if (field.empty()) {
        out.note = "no scoring groups -- only zombies";
        return;
    }

    // The separate fields, as lists of indices into `field`. One field (the
    // usual case) is simply all of them.
    const int nFields = opt.fieldCount > 1 ? opt.fieldCount : 1;
    std::vector<std::vector<int>> members((size_t)nFields);
    for (int i = 0; i < (int)field.size(); ++i) {
        const int f = fieldOf[(size_t)i];
        if (f >= 0 && f < nFields) members[(size_t)f].push_back(i);
    }
    for (const std::vector<int>& m : members)
        if (m.empty()) { out.note = "a field has no groups in it"; return; }

    const int fieldSize = (int)field.size();
    int maxMembers = 0;
    for (const std::vector<int>& m : members) if ((int)m.size() > maxMembers) maxMembers = (int)m.size();
    int comboSize = (opt.comboSize > 0 && opt.comboSize < maxMembers) ? opt.comboSize : maxMembers;
    (void)fieldSize;

    std::vector<const WarriorGroup*> alwaysPtrs;
    for (const WarriorGroup& g : always) alwaysPtrs.push_back(&g);

    int nThreads = opt.threads > 0 ? opt.threads : (int)std::thread::hardware_concurrency();
    if (nThreads < 1) nThreads = 1;
    if (nThreads > gamesCap) nThreads = gamesCap;

    // Games are claimed a chunk at a time: one atomic per chunk instead of one
    // per war, and a chunk is still small enough that a slow war cannot leave a
    // core idle at the end of the run.
    static constexpr int CHUNK = 4;
    std::atomic<int> nextGame{0};

    // The standings live in `out` from the first fold onwards rather than being
    // assembled at the end, so a caller can draw a board that is still moving.
    // `prog` owns the lock when there is one; without a caller watching, a local
    // one keeps the same code path.
    std::mutex localLock;
    std::mutex& resultLock = prog ? prog->lock : localLock;

    const auto t0 = std::chrono::steady_clock::now();

    auto fold = [&](std::vector<double>& ds, std::vector<int>& dn,
                    int& run, int& failed, long long& rounds) {
        std::lock_guard<std::mutex> lk(resultLock);
        for (size_t i = 0; i < ds.size(); ++i) {
            out.warriorScore[i]    += ds[i];
            out.warriorSurvived[i] += dn[i];
            ds[i] = 0.0;
            dn[i] = 0;
        }
        out.gamesRun    += run;    run    = 0;
        out.gamesFailed += failed; failed = 0;
        out.rounds      += rounds; rounds = 0;
        out.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

        // A group's score is the sum of its survivors', so it is recomputed from
        // them rather than accumulated -- folding twice must not count twice.
        for (size_t g = 0; g < out.groupName.size(); ++g) {
            out.groupScore[g] = 0.0;
            out.groupSurvived[g] = 0;
        }
        for (size_t i = 0; i < out.warriorName.size(); ++i) {
            const int g = out.warriorGroup[i];
            if (g < 0 || g >= (int)out.groupName.size()) continue;
            out.groupScore[(size_t)g]    += out.warriorScore[i];
            out.groupSurvived[(size_t)g] += out.warriorSurvived[i];
        }
        if (prog) prog->revision.fetch_add(1, std::memory_order_release);
    };

    auto worker = [&]() {
#ifdef _WIN32
        // Never run a worker below normal priority. Windows 11 files a
        // below-normal thread as low-QoS and, on a hybrid CPU, parks it on the
        // efficiency cores: a 14-core machine ran three times slower than the
        // reference engine's plain threads. Opting out of power throttling is
        // the documented way to say "performance-critical", which keeps the
        // scheduler's P-core preference and turbo behaviour for these threads.
        THREAD_POWER_THROTTLING_STATE pt;
        pt.Version = THREAD_POWER_THROTTLING_CURRENT_VERSION;
        pt.ControlMask = THREAD_POWER_THROTTLING_EXECUTION_SPEED;
        pt.StateMask = 0;
        SetThreadInformation(GetCurrentThread(), ThreadPowerThrottling, &pt, sizeof(pt));
#endif
        // One war per worker, reused for every game it runs: building one costs
        // a megabyte of core and the page faults that come with it, which
        // dwarfed the fight itself in a short game.
        War war(nullptr, nullptr, opt.zombieSpeed);

        Scratch sc;
        sc.order.resize((size_t)maxMembers);
        sc.ptrs.reserve((size_t)maxMembers + alwaysPtrs.size());
        sc.slots.resize((size_t)War::MAX_WARRIORS);

        // These hold what this worker has scored *since its last fold*, not its
        // whole run -- fold() drains them.
        std::vector<double> myScore(out.warriorName.size(), 0.0);
        std::vector<int>    mySurv(out.warriorName.size(), 0);
        int myRun = 0, myFailed = 0;
        long long myRounds = 0;
        auto lastFold = std::chrono::steady_clock::now();

        for (;;) {
            const int base = nextGame.fetch_add(CHUNK);
            if (base >= gamesCap) break;
            if (prog && prog->cancel.load(std::memory_order_relaxed)) break;

            const int end = (base + CHUNK < gamesCap) ? base + CHUNK : gamesCap;
            for (int k = base; k < end; ++k) {
                // Which battle of the whole tournament this is. A lane running
                // one phase of a split plays a subset of these, and both the
                // combination and the seed come from the global index -- so two
                // lanes cover the tournament exactly once between them without
                // having to agree on anything but the stride.
                const s64 g = (s64)opt.seedPhase + (s64)k * (s64)opt.seedStride;
                // With separate fields, battle g belongs to field g % n alone.
                const std::vector<int>& pool = members[(size_t)(nFields > 1 ? (int)(g % nFields) : 0)];
                const int poolSize = (int)pool.size();
                const int take = comboSize < poolSize ? comboSize : poolSize;
                drawCombo(opt.pairingSeed, (int)g, poolSize, take, sc.order.data());
                sc.ptrs.clear();
                for (int i = 0; i < take; ++i)
                    sc.ptrs.push_back(&field[(size_t)pool[(size_t)sc.order[(size_t)i]]]);
                for (const WarriorGroup* z : alwaysPtrs) sc.ptrs.push_back(z);

                const s64 seed = opt.firstSeed + g;
                war.reset();
                war.setSeed(seed);
                try {
                    war.loadWarriorGroups(sc.ptrs);
                } catch (const CpuFault&) {
                    ++myFailed;                     // a void game, not a loss
                    continue;
                }

                myRounds += war.runToEnd(opt.maxRounds);
                ++myRun;

                float share = 0.0f;
                const int n = war.collectScoreSlots(sc.slots.data(), share);
                for (int i = 0; i < n; ++i) {
                    const int s = sc.slots[(size_t)i];
                    myScore[(size_t)s] += (double)share;
                    mySurv[(size_t)s]  += 1;
                }
            }
            if (prog) prog->done.fetch_add(end - base, std::memory_order_relaxed);

            // Often enough to look live, rarely enough that the lock is never
            // contended: a fold is a few hundred adds.
            const auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration<double>(now - lastFold).count() > 0.15) {
                fold(myScore, mySurv, myRun, myFailed, myRounds);
                lastFold = now;
            }
        }

        fold(myScore, mySurv, myRun, myFailed, myRounds);
    };

    std::vector<std::thread> pool;
    pool.reserve((size_t)nThreads);
    for (int i = 1; i < nThreads; ++i) pool.emplace_back(worker);
    worker();                                   // this thread pulls its share too
    for (std::thread& t : pool) t.join();

    // Every worker folded before it returned, so the standings are already
    // complete; only the wall time needs the final word.
    {
        std::lock_guard<std::mutex> lk(resultLock);
        out.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        if (prog) prog->revision.fetch_add(1, std::memory_order_release);
    }

    if (out.gamesRun == 0 && out.gamesFailed > 0)
        out.note = "every war failed to place its warriors";
}

} // namespace cw
