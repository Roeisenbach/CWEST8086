// =============================================================================
//  Packs -- the two things a distributed tournament has to move around.
//
//  A *field pack* is everything another machine needs to fight the very same
//  battles: the warriors (name, type, bytes), the options that shape a war, and
//  which lane of the tournament that machine is -- its phase of the stride. Two
//  machines given the same pack and different phases play disjoint games of the
//  same tournament, and a pure sum of what they scored is the standings.
//
//  A *result pack* is one participant's contribution so far, keyed by warrior
//  name. It is cumulative, not a delta, so re-sending it is harmless and a lost
//  message costs nothing: the receiver keeps the latest pack per sender and
//  sums those.
//
//  Both are line-oriented text. That is deliberate: they travel as files
//  between offline machines and as a socket stream between online ones, and a
//  format you can read in a text editor is the one you can debug.
// =============================================================================
#pragma once

#include "compete.h"
#include <string>
#include <vector>

namespace cw {

// Everything in one pack, so a shard file is self-contained.
struct FieldPack {
    std::vector<WarriorGroup> groups;
    s64 seed        = 0;
    s64 pairingSeed = 0;
    int comboSize   = 4;
    int maxRounds   = War::MAX_ROUND;
    int zombieSpeed = 2;
    int stride      = 1;       // this machine plays phase, phase+stride, ...
    int phase       = 0;
    int games       = 0;       // for this lane; 0 = until told to stop
    // A host that starts a second tournament while a slow participant is still
    // reporting on the first needs to tell their packs apart.
    int runId       = 0;
    std::string title;         // free text: who made it, when
};

std::string writeFieldPack(const FieldPack& f);
bool        parseFieldPack(const std::string& text, FieldPack& out, std::string& why);

struct ResultPack {
    std::string sender;        // machine / lane name, for the panel
    int    runId = 0;          // the field pack this answers
    int    gamesRun = 0;
    int    gamesFailed = 0;
    double seconds = 0.0;
    std::vector<std::string> name;
    std::vector<double>      score;
    std::vector<int>         survived;
};

std::string writeResultPack(const CompeteResult& r, const std::string& sender, int runId);
bool        parseResultPack(const std::string& text, ResultPack& out, std::string& why);

// Add a pack's scores into `into`, matching rows by warrior name. Names the
// receiver does not know are dropped and counted, so a pack from a different
// field is noticed rather than silently half-applied.
int addResultPack(CompeteResult& into, const ResultPack& p);

} // namespace cw
