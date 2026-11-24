// =============================================================================
//  Competition runner -- many wars, scored, spread over the machine's cores.
//
//  A war is played out for each of a run of seeds and the scores are summed.
//  Nothing is recorded: the debugger's replay machinery is for watching one
//  fight, and keeping 70 MB per war would put a ceiling on the run length.
//  Only the final standings of each war are kept.
//
//  Scoring is the reference's: every survivor still standing when a war ends
//  splits one point between them, zombies excluded.
//
//  A real field is far bigger than one war can hold -- twenty warriors is the
//  engine's ceiling and a tournament is fifty-odd groups -- so each battle is
//  fought by a combination of `comboSize` groups drawn from the field. The draw
//  is a pure function of (pairingSeed, game index), which is what lets any
//  worker, or any machine in a split run, play game k without talking to
//  anyone about which groups it should use.
// =============================================================================
#pragma once

#include "engine.h"
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

namespace cw {

struct CompeteOptions {
    s64  firstSeed   = 0;
    // 0 means open-ended: play until CompeteProgress::cancel is raised. That is
    // how one lane of a split runs -- it does not know how many games the other
    // lane will manage, so the caller stops both when the total is reached.
    int  games       = 100;
    int  zombieSpeed = 2;
    int  maxRounds   = War::MAX_ROUND;
    int  threads     = 0;      // 0 = as many as the machine has

    // Groups drawn into each battle. 0 (or a number at or past the size of the
    // field) puts the whole field in every war, which is what a small field
    // wants; the reference tournament uses 4.
    int  comboSize   = 0;
    // The draw is deterministic from this, and independent of the battle seed
    // so the same pairings can be replayed against a different fight seed.
    s64  pairingSeed = 0;

    // Seeds are handed out as firstSeed + (seedPhase + k * seedStride). One
    // runner uses stride 1 phase 0; two lanes sharing a run take stride 2 with
    // phases 0 and 1, which is how a second (GPU) backend would interleave with
    // this one without either having to know about the other.
    int  seedStride  = 1;
    int  seedPhase   = 0;

    // Separate fields in one run. When fieldCount > 1, groupField[i] says which
    // field the i-th group handed in belongs to (zombie groups are ignored and
    // stay in every battle), and battle g is drawn only from field g % fieldCount
    // -- so groups of two different fields never meet. A group that should play
    // in several fields is simply handed in once per field.
    int              fieldCount = 1;
    std::vector<int> groupField;
};

// Whether the engine's own translation units were compiled with optimisation.
// An unoptimised interpreter is about four times slower and looks otherwise
// identical, so a front end can warn instead of leaving the user to wonder.
bool engineOptimized();

struct CompeteResult {
    // identity, in the order the groups were handed in
    std::vector<std::string> groupName;
    std::vector<std::string> warriorName;
    std::vector<int>         warriorGroup;   // index into groupName

    std::vector<double>      warriorScore;   // summed share of a point
    std::vector<int>         warriorSurvived;// wars it was still standing in
    std::vector<double>      groupScore;
    std::vector<int>         groupSurvived;

    int    gamesRun    = 0;
    int    gamesFailed = 0;                  // warriors could not be placed
    double seconds     = 0.0;                // wall time of the run
    long long rounds   = 0;                  // rounds fought, for a rate
    std::string note;

    bool empty() const { return warriorName.empty(); }
    double gamesPerSecond() const { return seconds > 0.0 ? gamesRun / seconds : 0.0; }
};

struct CompeteProgress {
    std::atomic<int>  done{0};
    std::atomic<bool> cancel{false};

    // Workers fold their partial standings into the caller's CompeteResult every
    // so often, not just at the end, so a board can be drawn while the run is
    // still going. Anyone reading that result mid-run must hold this.
    std::mutex        lock;
    // Bumped on every fold, so a reader can tell whether anything moved without
    // taking the lock to find out.
    std::atomic<int>  revision{0};
};

// Builds the identity tables from `groups` without running anything, so a
// result is meaningful even for a run that is cancelled immediately.
void prepareResult(const std::vector<WarriorGroup>& groups, CompeteResult& out);

// Which groups are in every battle rather than drawn: a group whose warriors
// are all zombies is scenery, not a contestant.
bool isZombieGroup(const WarriorGroup& g);

// The combination of field entries game `k` is fought by. `field` counts only
// the drawn (non-zombie) groups. Exposed so a split run can be checked, and so
// a second backend can reproduce the same pairings.
void drawCombo(s64 pairingSeed, int k, int fieldSize, int comboSize, int* out);

// Blocking. Spreads the games over `opt.threads` workers.
void runCompetition(const std::vector<WarriorGroup>& groups, const CompeteOptions& opt,
                    CompeteResult& out, CompeteProgress* prog);

} // namespace cw
