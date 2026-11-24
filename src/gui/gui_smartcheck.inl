// =============================================================================
//  gui_smartcheck.inl -- "Smart check": what should a survivor's int 0x89 look for?
//
//  `int 0x89` in source assembles to INT 87h (see asm.cpp), so in every real
//  war a survivor that uses it is exactly one that wrote INT 87h: it bombs
//  the first AX:DX it finds. The smart check plays that group against the
//  active field once for every candidate AX:DX and ranks them by the share of
//  the points the group takes. The candidates are every 4 bytes of every
//  other warrior's code, zombies included, and -- found by playing the first
//  games through -- every 4 bytes that memory actually holds at the moment an
//  int 0x89 runs: code as the war has changed it, bombs, and the windows a
//  change only reaches in part ("CC CC CC 5A"). The group plays a copy of its code
//  with CD 89 at the int 0x89 places, and the engine (cw::SmartCheckOverride,
//  one per worker thread) makes that INT 87h search for the candidate.
//
//  Quick, because:
//    * every candidate plays the same battles -- game k has the same seed and
//      the same opponents for all of them -- so the comparison is paired and a
//      handful of games already tells a good pattern from a useless one;
//    * the candidates race in stages: 10 games each, then the best quarter go
//      on to 40, the best quarter of those to 160 ... and only the finalists
//      play the full count. Most of them stop after 10 games;
//    * all cores, a War per worker, nothing recorded.
//
//  Included by main.cpp after gui_compete.inl.
// =============================================================================

struct SmartCand
{
    cw::s16 ax = 0, dx = 0;
    std::string from;                       // where the 4 bytes were first found
    bool baseline = false;                  // the code as written: real binary, no override
    int  assigned = 0;                      // games handed out (coordinator only)
    std::atomic<int>       upTo{0};         // the same, for the window to read
    std::atomic<int>       played{0};       // games played to the end
    std::atomic<long long> sumMicro{0};     // points, x 1e6
    std::atomic<int>       out{-1};         // the stage it was dropped after; -1 racing
    double Mean() const
    {
        const int n = played.load();
        return n ? (double)sumMicro.load() / 1e6 / (double)n : 0.0;
    }
};

struct SmartState
{
    bool        open = false;
    std::string subjectKey;                 // lower "folder|name" of the group checked
    int         games = 200;                // for the finalists

    // a run
    std::thread              coord;
    std::atomic<bool>        stop{false};
    std::atomic<bool>        running{false};
    std::vector<std::unique_ptr<SmartCand>> cands;   // [0] is the baseline; fixed during a run
    std::atomic<long long>   gamesPlayed{0};   // wars actually fought (probes included)
    std::atomic<long long>   gamesShared{0};   // results a candidate took from its game's probe
    std::atomic<long long>   gamesJoined{0};   // wars stopped early: they had become the probe's war
    std::unordered_map<unsigned, int> candOf;  // the 4 bytes -> candidate
    std::mutex               candsMu;          // cands grows once mid-run (from memory); the window locks to read
    std::atomic<bool>        gathering{false}; // playing the first games to see what memory holds
    int                      fromCode = 0, fromMemory = 0;
    std::atomic<int>         stage{0};
    // the code as written, game by game (points x 1e6; -1 not played, -2 no
    // room to place everyone), so each candidate is set against it over the
    // very games it played
    std::unique_ptr<std::atomic<int>[]> baseGame;
    int                      baseGames = 0;
    int                      stages = 0;
    double                   startedAt = 0.0, endedAt = 0.0;
    std::string              note;
    bool                     finished = false;

    // the field, fixed for the run
    std::string                    subjectName;
    cw::WarriorGroup               subjReal, subjCheck, zombies;
    std::vector<cw::WarriorGroup>  others;
    int                            othersPerBattle = 0;
    cw::s64                        firstSeed = 0, pairSeed = 0;
    int                            zombieSpeed = 2, maxRounds = cw::War::MAX_ROUND, threads = 1;
    int                            sitesCount = 0;
};
static SmartState g_Smart;

static std::string SmartGroupKey(const Player& pl) { return ToLower(pl.folder) + "|" + ToLower(pl.name); }

static bool PlayerUsesInt89(const Player& pl)
{
    for (const Survivor& sv : pl.slots)
        if (sv.present && !sv.smartSites.empty()) return true;
    return false;
}

static bool AnyGroupUsesInt89()
{
    for (const Player& pl : g_App.players) if (PlayerUsesInt89(pl)) return true;
    return false;
}

static void SmartCheckToggle() { g_Smart.open = !g_Smart.open; }
static bool SmartCheckIsOpen() { return g_Smart.open; }
static bool SmartCheckIsRunning() { return g_Smart.running.load(); }

static void SmartCheckStop()
{
    g_Smart.stop.store(true);
    if (g_Smart.coord.joinable()) g_Smart.coord.join();
    g_Smart.running.store(false);
}

// The battle of game k: the group (as written, or with its CD 89s), k's
// opponents, the zombies. Its seed and opponents depend on k alone.
static void SmartBattle(int k, bool real, std::vector<const cw::WarriorGroup*>& ptrs, std::vector<int>& pick)
{
    SmartState& M = g_Smart;
    ptrs.clear();
    ptrs.push_back(real ? &M.subjReal : &M.subjCheck);
    const int n = (int)M.others.size();
    if (M.othersPerBattle >= n)
        for (int i = 0; i < n; i++) ptrs.push_back(&M.others[(size_t)i]);
    else
    {
        cw::drawCombo(M.pairSeed, k, n, M.othersPerBattle, pick.data());
        for (int j = 0; j < M.othersPerBattle; j++) ptrs.push_back(&M.others[(size_t)pick[(size_t)j]]);
    }
    if (!M.zombies.warriors.empty()) ptrs.push_back(&M.zombies);
}

// The rounds at which a probe's war is fingerprinted (War::stateDigest), so a
// candidate's war that has become the probe's war again can stop there.
static const int kSmartCheckpoints[] = { 128, 512, 2048, 8192, 32768, 131072 };
static const int kSmartNumCp = (int)(sizeof(kSmartCheckpoints) / sizeof(kSmartCheckpoints[0]));

// What one probe found out about game k.
struct SmartProbe
{
    long long micro = -1;                   // the war a candidate that finds nothing plays; -1 not placed
    bool      clean = true;                 // every search could be read whole
    std::vector<int> hits;                  // sorted: found somewhere, at some call
    std::vector<std::vector<int>> callHits; // the same, call by call
    std::vector<char> callClean;
    cw::u64 digest[kSmartNumCp] = {};
    bool    hasDigest[kSmartNumCp] = {};    // the war was still on at that round
    int     callsAt[kSmartNumCp] = {};      // INT 89h calls made by then
};

// Whether candidate `c` could still play differently from the probe after
// checkpoint `i`: a later call would find it, or could not be searched whole.
static bool SmartDiffersAfter(const SmartProbe& pr, int c, int i)
{
    for (size_t j = (size_t)pr.callsAt[i]; j < pr.callHits.size(); j++)
    {
        if (!pr.callClean[j]) return true;
        if (std::binary_search(pr.callHits[j].begin(), pr.callHits[j].end(), c)) return true;
    }
    return false;
}

// Plays one war; the group's points x 1e6, or -1 when the warriors could not
// all be placed (which happens for every candidate of that game alike).
// `record` (the probe) keeps fingerprints along the way; `against` (a
// candidate's run) stops as soon as the war is the probe's war again and
// takes its result -- from the same state the same war follows.
static long long SmartPlayWar(cw::War& war, int k, const std::vector<const cw::WarriorGroup*>& ptrs,
                              const cw::SmartCheckOverride* o, std::vector<int>& slots,
                              SmartProbe* record, const std::vector<std::vector<cw::u8>>* seen,
                              const SmartProbe* against, int cand)
{
    SmartState& M = g_Smart;
    war.reset();
    war.setSeed(M.firstSeed + k);
    cw::setSmartCheckOverride(o);
    long long micro = -1;
    bool placed = true;
    try { war.loadWarriorGroups(ptrs); }
    catch (const cw::CpuFault&) { placed = false; }
    if (placed)
    {
        int round = 0;
        bool joined = false;
        for (int i = 0; i < kSmartNumCp && !joined; i++)
        {
            const int cp = kSmartCheckpoints[i];
            if (cp >= M.maxRounds) break;
            round = war.runToEnd(cp, round);
            if (war.isOver()) break;
            if (record)
            {
                record->digest[i] = war.stateDigest();
                record->hasDigest[i] = true;
                record->callsAt[i] = seen ? (int)seen->size() : 0;
            }
            if (against && against->hasDigest[i] && !SmartDiffersAfter(*against, cand, i) &&
                war.stateDigest() == against->digest[i])
                joined = true;
        }
        if (joined)
        {
            micro = against->micro;
            M.gamesJoined.fetch_add(1);
        }
        else
        {
            war.runToEnd(M.maxRounds, round);
            float share = 0.0f;
            const int scorers = war.collectScoreSlots(slots.data(), share);   // only the group's survivors carry a slot
            micro = llround((double)scorers * share * 1e6);
        }
    }
    cw::setSmartCheckOverride(nullptr);
    return micro;
}

static void SmartCredit(SmartCand& c, int k, long long micro)
{
    SmartState& M = g_Smart;
    if (micro >= 0)
    {
        c.sumMicro.fetch_add(micro);
        c.played.fetch_add(1);
    }
    if (c.baseline && k < M.baseGames) M.baseGame[(size_t)k].store(micro >= 0 ? (int)micro : -2);
}

// Runs `jobs` jobs on every worker; each worker has a War of its own.
template <class F>
static void SmartParallel(size_t jobs, F fn)
{
    SmartState& M = g_Smart;
    std::atomic<size_t> next(0);
    auto work = [&]() {
        cw::War war(nullptr, nullptr, M.zombieSpeed);
        std::vector<const cw::WarriorGroup*> ptrs;
        std::vector<int> pick((size_t)M.others.size() + 1);
        std::vector<int> slots((size_t)cw::War::MAX_WARRIORS + 4);
        for (;;)
        {
            if (M.stop.load(std::memory_order_relaxed)) break;
            const size_t j = next.fetch_add(1);
            if (j >= jobs) break;
            fn(j, war, ptrs, pick, slots);
        }
    };
    std::vector<std::thread> pool;
    for (int t = 1; t < M.threads && (size_t)t < jobs; t++) pool.emplace_back(work);
    work();
    for (std::thread& t : pool) t.join();
}

// Game k's probe: the war a candidate that finds nothing plays, its
// fingerprints, and a copy of the memory every int 0x89 call could search.
static void SmartRunProbe(int k, cw::War& war, std::vector<const cw::WarriorGroup*>& ptrs, std::vector<int>& pick,
                          std::vector<int>& slots, SmartProbe& pr, std::vector<std::vector<cw::u8>>& seen)
{
    cw::SmartCheckOverride o;
    o.probe = true;
    o.seen = &seen;
    SmartBattle(k, false, ptrs, pick);
    pr.micro = SmartPlayWar(war, k, ptrs, &o, slots, &pr, &seen, nullptr, -1);
    g_Smart.gamesPlayed.fetch_add(1);
}

// The 4 bytes at `off` of a searched segment, wrapping the way the search's
// words do.
static inline unsigned SmartWindow(const std::vector<cw::u8>& seg, unsigned off)
{
    return (unsigned)seg[off] | ((unsigned)seg[(off + 1) & 0xFFFF] << 8) |
           ((unsigned)seg[(off + 2) & 0xFFFF] << 16) | ((unsigned)seg[(off + 3) & 0xFFFF] << 24);
}

// Which candidates each of the probe's searches would have found.
static void SmartProbeHits(SmartProbe& pr, const std::vector<std::vector<cw::u8>>& seen)
{
    SmartState& M = g_Smart;
    for (const std::vector<cw::u8>& seg : seen)
    {
        pr.callHits.emplace_back();
        pr.callClean.push_back(seg.size() == 0x10000 ? 1 : 0);
        if (seg.size() != 0x10000) { pr.clean = false; continue; }
        std::vector<int>& ch = pr.callHits.back();
        for (unsigned off = 0; off <= 0xFFFF; off++)
        {
            auto it = M.candOf.find(SmartWindow(seg, off));
            if (it != M.candOf.end()) ch.push_back(it->second);
        }
        std::sort(ch.begin(), ch.end());
        ch.erase(std::unique(ch.begin(), ch.end()), ch.end());
        pr.hits.insert(pr.hits.end(), ch.begin(), ch.end());
    }
    std::sort(pr.hits.begin(), pr.hits.end());
    pr.hits.erase(std::unique(pr.hits.begin(), pr.hits.end()), pr.hits.end());
}

static void SmartCoordinator()
{
    SmartState& M = g_Smart;
    const int T = ImMax(1, M.games);
    std::vector<int> marks;                         // games each racer has played after each stage
    for (int g = ImMin(T, 10);; g = ImMin(T, g * 4)) { marks.push_back(g); if (g >= T) break; }
    M.stages = (int)marks.size();

    // 0. Play the first stage's games through once, with every int 0x89
    //    finding nothing, and keep what memory held at each call -- the moment
    //    the group's search runs, after everyone's moves up to then. Whatever
    //    4 bytes are there is something the search can find, so each of them
    //    joins the candidates: code as the war has changed it, bombs, and the
    //    windows a change only reaches in part ("CC CC CC 5A"). Anything that
    //    is not there is found nowhere and plays the probe's war anyway. These
    //    plays are also the first stage's probes.
    std::vector<SmartProbe> firstProbes((size_t)marks[0]);
    std::vector<std::vector<std::vector<cw::u8>>> firstSeen((size_t)marks[0]);
    M.gathering.store(true);
    SmartParallel(firstProbes.size(), [&](size_t j, cw::War& war, std::vector<const cw::WarriorGroup*>& ptrs,
                                          std::vector<int>& pick, std::vector<int>& slots) {
        SmartRunProbe((int)j, war, ptrs, pick, slots, firstProbes[j], firstSeen[j]);
    });
    if (!M.stop.load())
    {
        std::vector<std::unique_ptr<SmartCand>> add;
        for (size_t k = 0; k < firstSeen.size(); k++)
            for (size_t call = 0; call < firstSeen[k].size(); call++)
            {
                const std::vector<cw::u8>& seg = firstSeen[k][call];
                if (seg.size() != 0x10000) continue;
                for (unsigned off = 0; off <= 0xFFFF; off++)
                {
                    const unsigned v = SmartWindow(seg, off);
                    if (!M.candOf.emplace(v, (int)(M.cands.size() + add.size())).second) continue;
                    auto c = std::make_unique<SmartCand>();
                    c->ax = (cw::s16)(v & 0xFFFF);
                    c->dx = (cw::s16)(v >> 16);
                    char at[64];
                    snprintf(at, sizeof(at), "memory at the call, +%04X (game %d)", off, (int)k + 1);
                    c->from = at;
                    add.push_back(std::move(c));
                }
            }
        std::lock_guard<std::mutex> lk(M.candsMu);
        M.fromMemory = (int)add.size();
        for (auto& c : add) M.cands.push_back(std::move(c));
    }
    M.gathering.store(false);

    int lo = 0;
    for (size_t st = 0; st < marks.size() && !M.stop.load(); st++)
    {
        M.stage.store((int)st + 1);
        const int hi = marks[st];

        // 1. one probe per game: the war a candidate that finds nothing plays,
        //    which candidates each search would have found, and fingerprints
        //    (the first stage's were played above)
        std::vector<SmartProbe> probes;
        if (st == 0)
        {
            probes.swap(firstProbes);
            for (size_t j = 0; j < probes.size(); j++) SmartProbeHits(probes[j], firstSeen[j]);
            firstSeen.clear();
        }
        else
        {
            probes.resize((size_t)(hi - lo));
            SmartParallel(probes.size(), [&](size_t j, cw::War& war, std::vector<const cw::WarriorGroup*>& ptrs,
                                             std::vector<int>& pick, std::vector<int>& slots) {
                std::vector<std::vector<cw::u8>> seen;
                SmartRunProbe(lo + (int)j, war, ptrs, pick, slots, probes[j], seen);
                SmartProbeHits(probes[j], seen);
            });
        }
        if (M.stop.load()) break;

        // 2. a candidate found nowhere plays the probe's war move for move:
        //    it takes the probe's result without the war being fought again
        std::vector<std::pair<int, int>> units;     // (candidate, game) that must really be played
        for (int k = lo; k < hi; k++)
        {
            const SmartProbe& pr = probes[(size_t)(k - lo)];
            for (size_t i = 0; i < M.cands.size(); i++)
            {
                SmartCand& c = *M.cands[i];
                if (c.out.load() >= 0) continue;
                if (pr.micro < 0) { SmartCredit(c, k, -1); continue; }      // nobody could be placed
                const bool hit = std::binary_search(pr.hits.begin(), pr.hits.end(), (int)i);
                if (!c.baseline && pr.clean && !hit) { SmartCredit(c, k, pr.micro); M.gamesShared.fetch_add(1); }
                else units.push_back({ (int)i, k });
            }
        }
        for (auto& c : M.cands)
            if (c->out.load() < 0) { c->assigned = hi; c->upTo.store(hi); }

        // 3. the rest are fought for real -- until their war is the probe's again
        SmartParallel(units.size(), [&](size_t j, cw::War& war, std::vector<const cw::WarriorGroup*>& ptrs,
                                        std::vector<int>& pick, std::vector<int>& slots) {
            SmartCand& c = *M.cands[(size_t)units[j].first];
            const int k = units[j].second;
            const SmartProbe& pr = probes[(size_t)(k - lo)];
            cw::SmartCheckOverride o;
            o.ax = c.ax;
            o.dx = c.dx;
            SmartBattle(k, c.baseline, ptrs, pick);
            SmartCredit(c, k, SmartPlayWar(war, k, ptrs, c.baseline ? nullptr : &o, slots, nullptr, nullptr,
                                           c.baseline ? nullptr : &pr, units[j].first));
            M.gamesPlayed.fetch_add(1);
        });
        lo = hi;
        if (M.stop.load() || st + 1 == marks.size()) break;

        // the best quarter race on (and never fewer than a dozen)
        std::vector<SmartCand*> racing;
        for (size_t i = 1; i < M.cands.size(); i++) if (M.cands[i]->out.load() < 0) racing.push_back(M.cands[i].get());
        std::stable_sort(racing.begin(), racing.end(), [](const SmartCand* a, const SmartCand* b) { return a->Mean() > b->Mean(); });
        const size_t keep = ImMax(ImMin((size_t)12, racing.size()), (racing.size() + 3) / 4);
        for (size_t i = keep; i < racing.size(); i++) racing[i]->out.store((int)st + 1);
    }
    M.endedAt = AppNow();
    M.finished = !M.stop.load();
    M.running.store(false);
}

static void SmartCheckStart(const Player& subject)
{
    SmartState& M = g_Smart;
    SmartCheckStop();
    M.cands.clear();
    M.others.clear();
    M.zombies = cw::WarriorGroup();
    M.note.clear();
    M.finished = false;
    M.gamesPlayed.store(0);
    M.gamesShared.store(0);
    M.gamesJoined.store(0);
    M.candOf.clear();
    M.fromCode = M.fromMemory = 0;
    M.gathering.store(false);
    M.stage.store(0);
    M.stop.store(false);
    AppState& S = g_App;

    // the group, as written and with CD 89 where it said int 0x89
    M.subjectName = subject.name;
    M.subjReal = cw::WarriorGroup();
    M.subjReal.name = subject.name;
    M.sitesCount = 0;
    for (int s = 0; s < kSlotCount; s++)
    {
        const Survivor& sv = subject.slots[s];
        if (!sv.present || sv.bin.empty()) continue;
        cw::WarriorData wd;
        wd.name = subject.name + std::to_string(s + 1);
        wd.code.assign(sv.bin.begin(), sv.bin.end());
        wd.type = (s == 0) ? cw::WarriorType::SURVIVOR_1 : cw::WarriorType::SURVIVOR_2;
        wd.slot = 0;                        // the only scores counted
        M.subjReal.warriors.push_back(wd);
        M.sitesCount += (int)sv.smartSites.size();
    }
    M.subjCheck = M.subjReal;
    {
        size_t w = 0;
        for (int s = 0; s < kSlotCount; s++)
        {
            const Survivor& sv = subject.slots[s];
            if (!sv.present || sv.bin.empty()) continue;
            for (int at : sv.smartSites)
                if (at >= 0 && at + 1 < (int)M.subjCheck.warriors[w].code.size() && M.subjCheck.warriors[w].code[(size_t)at] == 0xCD)
                    M.subjCheck.warriors[w].code[(size_t)at + 1] = 0x89;
            w++;
        }
    }
    if (M.subjReal.warriors.empty()) { M.note = subject.name + " has nothing to run"; return; }
    if (M.sitesCount == 0) { M.note = subject.name + " has no int 0x89 in its code"; return; }

    // everyone else who is active, and the zombies
    const std::string subjKey = SmartGroupKey(subject);
    std::vector<const Player*> othersFrom;
    for (const Player& pl : S.players)
    {
        if (!pl.enabled || SmartGroupKey(pl) == subjKey) continue;
        cw::WarriorGroup g;
        g.name = pl.name;
        for (int s = 0; s < kSlotCount; s++)
        {
            const Survivor& sv = pl.slots[s];
            if (!sv.runnable()) continue;
            cw::WarriorData wd;
            wd.name = pl.name + std::to_string(s + 1);
            wd.code.assign(sv.bin.begin(), sv.bin.end());
            wd.type = (s == 0) ? cw::WarriorType::SURVIVOR_1 : cw::WarriorType::SURVIVOR_2;
            g.warriors.push_back(std::move(wd));
        }
        if (!g.warriors.empty()) { M.others.push_back(std::move(g)); othersFrom.push_back(&pl); }
    }
    // same names would make one team of two groups, as in Debug: the subject
    // keeps its own, the others with a clash take their folder
    {
        std::vector<cw::WarriorGroup> all;
        std::vector<const Player*> from;
        all.push_back(M.subjReal); from.push_back(nullptr);
        for (size_t i = 0; i < M.others.size(); i++) { all.push_back(M.others[i]); from.push_back(othersFrom[i]); }
        SeparateSameNames(all, from);
        for (size_t i = 0; i < M.others.size(); i++) M.others[i] = std::move(all[i + 1]);
    }
    M.zombies.name = "ZoMbIeS";
    for (const Zombie& zb : S.zombies)
    {
        if (!zb.runnable()) continue;
        cw::WarriorData wd;
        wd.name = zb.name.empty() ? zb.sv.name : zb.name;
        wd.code.assign(zb.sv.bin.begin(), zb.sv.bin.end());
        wd.type  = zb.heavy ? cw::WarriorType::ZOMBIE_H : cw::WarriorType::ZOMBIE;
        wd.speed = zb.speed;
        M.zombies.warriors.push_back(std::move(wd));
    }
    if (M.others.empty() && M.zombies.warriors.empty()) { M.note = "Make some other group active: there is nobody to bomb"; return; }

    // groups per battle, as the competition draws them
    const int field = (int)M.others.size() + 1;
    M.othersPerBattle = (S.compCombo <= 0 || S.compCombo >= field) ? (int)M.others.size() : S.compCombo - 1;
    {
        int biggest = (int)M.subjReal.warriors.size() + (int)M.zombies.warriors.size();
        std::vector<int> sizes;
        for (const cw::WarriorGroup& g : M.others) sizes.push_back((int)g.warriors.size());
        std::sort(sizes.rbegin(), sizes.rend());
        for (int i = 0; i < M.othersPerBattle && i < (int)sizes.size(); i++) biggest += sizes[(size_t)i];
        if (biggest > cw::War::MAX_WARRIORS)
        {
            M.note = "A battle would hold " + std::to_string(biggest) + " warriors -- a war holds " +
                     std::to_string(cw::War::MAX_WARRIORS) + ". Lower the groups per battle.";
            return;
        }
    }

    // the candidates: every 4 bytes of every warrior it can meet, once each
    {
        auto base = std::make_unique<SmartCand>();
        base->baseline = true;
        base->from = "as written";
        M.cands.push_back(std::move(base));
        std::unordered_map<unsigned, int> seen;
        auto take = [&](const cw::WarriorData& w) {
            const std::vector<cw::u8>& b = w.code;
            for (size_t i = 0; i + 3 < b.size(); i++)
            {
                const unsigned v = (unsigned)b[i] | ((unsigned)b[i + 1] << 8) | ((unsigned)b[i + 2] << 16) | ((unsigned)b[i + 3] << 24);
                if (!seen.emplace(v, (int)M.cands.size()).second) continue;
                M.candOf[v] = (int)M.cands.size();
                auto c = std::make_unique<SmartCand>();
                c->ax = (cw::s16)(v & 0xFFFF);
                c->dx = (cw::s16)(v >> 16);
                char at[16];
                snprintf(at, sizeof(at), " +%03X", (unsigned)i);
                c->from = w.name + at;
                M.cands.push_back(std::move(c));
            }
        };
        for (const cw::WarriorGroup& g : M.others) for (const cw::WarriorData& w : g.warriors) take(w);
        for (const cw::WarriorData& w : M.zombies.warriors) take(w);
    }
    M.fromCode = (int)M.cands.size() - 1;

    cw::CompeteOptions opt;
    CompetitionOptionsFromApp(opt);
    M.firstSeed   = opt.firstSeed;
    M.pairSeed    = opt.pairingSeed;
    M.zombieSpeed = opt.zombieSpeed;
    M.maxRounds   = opt.maxRounds;
    const int hw  = (int)std::thread::hardware_concurrency();
    M.threads     = S.compThreads > 0 ? S.compThreads : ImMax(1, hw);
    M.games       = ImClamp(M.games, 10, 100000);
    M.baseGames   = M.games;
    M.baseGame.reset(new std::atomic<int>[(size_t)M.baseGames]);
    for (int k = 0; k < M.baseGames; k++) M.baseGame[(size_t)k].store(-1);
    M.startedAt   = AppNow();
    M.endedAt     = 0.0;
    M.running.store(true);
    M.coord = std::thread(SmartCoordinator);
}

static void SmartCopyPattern(const SmartCand& c)
{
    char t[96];
    snprintf(t, sizeof(t), "mov ax, 0x%04X\nmov dx, 0x%04X\n", (unsigned)(cw::u16)c.ax, (unsigned)(cw::u16)c.dx);
    ImGui::SetClipboardText(t);
    g_App.status = "Copied: mov ax / mov dx for " + c.from;
}

static void DrawSmartCheckWindow()
{
    SmartState& M = g_Smart;
    if (!M.running.load() && M.coord.joinable()) M.coord.join();
    if (!M.open) return;
    AppState& S = g_App;

    static bool gamesFromCompetition = false;
    if (!gamesFromCompetition) { gamesFromCompetition = true; if (S.compGames > 0) M.games = ImClamp(S.compGames, 10, 100000); }
    ImGui::SetNextWindowSize(ImVec2(640 * g_Scale, 520 * g_Scale), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Smart check###smartcheck", &M.open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking))
    {
        ImGui::End();
        return;
    }
    if (!HasWorkspace()) { ImGui::TextDisabled("Open a workspace first."); ImGui::End(); return; }
    const bool running = M.running.load();

    // which group: the ones whose code has int 0x89
    std::vector<int> users;
    for (int p = 0; p < (int)S.players.size(); p++) if (PlayerUsesInt89(S.players[(size_t)p])) users.push_back(p);
    int subject = -1;
    for (int p : users) if (SmartGroupKey(S.players[(size_t)p]) == M.subjectKey) subject = p;
    if (subject < 0 && !users.empty())
    {
        // the open survivor's group if it is one of them
        subject = users[0];
        if (g_App.open.Valid() && g_App.open.player >= 0)
            for (int p : users) if (p == g_App.open.player) subject = p;
        M.subjectKey = SmartGroupKey(S.players[(size_t)subject]);
    }

    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextColored(Hex(Col::TextDim),
                       "Plays the group against the active groups once for every AX:DX its int 0x89 could look for "
                       "-- every 4 bytes of every other warrior, and every 4 bytes memory holds when its int 0x89 runs "
                       "in the first games -- and ranks them by the share of the points it takes. "
                       "In a real war int 0x89 is INT 87h with the AX:DX the code sets.");
    ImGui::PopTextWrapPos();
    ImGui::Dummy(ImVec2(0, 4 * g_Scale));

    if (users.empty())
    {
        ImGui::TextColored(Hex(Col::Warn), "No group has int 0x89 in its code yet.");
        ImGui::End();
        return;
    }

    ImGui::BeginDisabled(running);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Group");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200 * g_Scale);
    if (ImGui::BeginCombo("##smartsubject", S.players[(size_t)subject].name.c_str()))
    {
        for (int p : users)
        {
            const Player& pl = S.players[(size_t)p];
            const std::string label = (pl.folder.empty() ? pl.name : pl.folder + "/" + pl.name) + "##" + std::to_string(p);
            if (ImGui::Selectable(label.c_str(), p == subject)) M.subjectKey = SmartGroupKey(pl);
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine(0, 14 * g_Scale);
    ImGui::TextUnformatted("Games for the finalists");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90 * g_Scale);
    ImGui::InputInt("##smartgames", &M.games, 0, 0);
    M.games = ImClamp(M.games, 10, 100000);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Every candidate plays 10 games; the best quarter play on (x4 each stage)\n"
                          "until the finalists have played this many.");
    ImGui::EndDisabled();
    ImGui::SameLine(0, 14 * g_Scale);
    {
        const unsigned c = running ? Col::Danger : Col::Accent;
        ImGui::PushStyleColor(ImGuiCol_Button, Hex(c, 0.88f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Hex(c));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, Hex(c, 0.75f));
        ImGui::PushStyleColor(ImGuiCol_Text, Hex(0xFFFFFF));
        if (running) { if (ImGui::Button("Stop", ImVec2(80 * g_Scale, 0))) SmartCheckStop(); }
        else if (ImGui::Button("Check", ImVec2(80 * g_Scale, 0))) SmartCheckStart(S.players[(size_t)subject]);
        ImGui::PopStyleColor(4);
    }
    ImGui::TextColored(Hex(Col::TextDim), "On the Competition panel's settings: %s per battle, its seed, %d thread(s)%s.",
                       S.compCombo <= 0 ? "every group" : (std::to_string(S.compCombo) + " groups").c_str(),
                       S.compThreads > 0 ? S.compThreads : (int)std::thread::hardware_concurrency(),
                       S.compRunning ? " -- a tournament is running too, so both go slower" : "");

    if (!M.note.empty()) ImGui::TextColored(Hex(Col::Warn), "%s", M.note.c_str());

    // the coordinator adds the candidates found in memory once, mid-run
    std::lock_guard<std::mutex> candsLock(M.candsMu);

    if (running && M.gathering.load())
        ImGui::TextColored(Hex(Col::Accent), "Playing the first %d games to see what memory holds at each int 0x89...",
                           ImMin(M.games, 10));

    // progress
    if (!M.cands.empty() && !M.gathering.load())
    {
        int racing = 0;
        for (size_t i = 1; i < M.cands.size(); i++) if (M.cands[i]->out.load() < 0) racing++;
        const double secs = (running ? AppNow() : M.endedAt) - M.startedAt;
        const long long played = M.gamesPlayed.load(), shared = M.gamesShared.load(), joined = M.gamesJoined.load();
        char line[520];
        snprintf(line, sizeof(line), "%s %s -- %d candidates (%d from code, %d more from memory at the calls), "
                                     "%d still racing -- stage %d of %d -- %.1fs\n"
                                     "%lld wars fought (%.0f/s), %lld of them cut short when they became their probe's war again; "
                                     "%lld results taken straight from a probe (the bomb would have found nothing)",
                 running ? "Checking" : (M.finished ? "Checked" : "Stopped"), M.subjectName.c_str(),
                 (int)M.cands.size() - 1, M.fromCode, M.fromMemory, racing, M.stage.load(), M.stages, secs,
                 played, secs > 0 ? played / secs : 0.0, joined, shared);
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextColored(Hex(running ? Col::Accent : Col::Text), "%s", line);
        ImGui::PopTextWrapPos();
    }

    // the table: racing ones by their average, then the ones dropped later first
    if (!M.cands.empty())
    {
        // sorted four times a second, not every frame: a big field has tens
        // of thousands of candidates
        static std::vector<const SmartCand*> rows;
        static double sortedAt = -1.0;
        static size_t sortedFor = 0;
        if (rows.size() != M.cands.size() || sortedFor != (size_t)M.cands[0].get() || AppNow() - sortedAt > 0.25)
        {
            rows.clear();
            for (const auto& c : M.cands) rows.push_back(c.get());
            std::vector<std::pair<double, int>> key(rows.size());
            for (size_t i = 0; i < rows.size(); i++)
            {
                const int o = rows[i]->out.load();
                key[i] = { rows[i]->Mean(), o < 0 ? 1 << 20 : o };
            }
            std::vector<size_t> order(rows.size());
            for (size_t i = 0; i < order.size(); i++) order[i] = i;
            std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
                if (key[a].second != key[b].second) return key[a].second > key[b].second;
                return key[a].first > key[b].first;
            });
            std::vector<const SmartCand*> sorted;
            for (size_t i : order) sorted.push_back(rows[i]);
            rows.swap(sorted);
            sortedAt = AppNow();
            sortedFor = (size_t)M.cands[0].get();
        }
        // the code as written over its first k games, for every k
        std::vector<double> baseSum((size_t)M.baseGames + 1, 0.0);
        std::vector<int>    baseN((size_t)M.baseGames + 1, 0);
        for (int k = 0; k < M.baseGames; k++)
        {
            const int v = M.baseGame ? M.baseGame[(size_t)k].load() : -1;
            baseSum[(size_t)k + 1] = baseSum[(size_t)k] + (v >= 0 ? v / 1e6 : 0.0);
            baseN[(size_t)k + 1]   = baseN[(size_t)k] + (v >= 0 ? 1 : 0);
        }
        auto baseOver = [&](int upTo) {
            upTo = ImClamp(upTo, 0, M.baseGames);
            return baseN[(size_t)upTo] ? baseSum[(size_t)upTo] / baseN[(size_t)upTo] : 0.0;
        };
        const ImGuiTableFlags tf = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                                   ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit;
        if (ImGui::BeginTable("##smarttable", 7, tf, ImVec2(0, ImGui::GetContentRegionAvail().y)))
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("#");
            ImGui::TableSetupColumn("AX");
            ImGui::TableSetupColumn("DX");
            ImGui::TableSetupColumn("Bytes");
            ImGui::TableSetupColumn("Found in", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Games");
            ImGui::TableSetupColumn("Points  (vs as written)");
            ImGui::TableHeadersRow();
            ImGuiListClipper clip;
            clip.Begin((int)rows.size());
            while (clip.Step())
                for (int r = clip.DisplayStart; r < clip.DisplayEnd; r++)
                {
                    const SmartCand& c = *rows[(size_t)r];
                    const bool dropped = c.out.load() >= 0;
                    ImGui::TableNextRow();
                    ImGui::PushID(r);
                    if (c.baseline) ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, HexU(Col::Accent, 0.16f));
                    ImGui::PushStyleColor(ImGuiCol_Text, Hex(dropped ? Col::TextDim : Col::Text));
                    ImGui::TableSetColumnIndex(0);
                    char rank[16];
                    snprintf(rank, sizeof(rank), "%d", r + 1);
                    ImGui::Selectable(rank, false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap);
                    if (!c.baseline && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        SmartCopyPattern(c);
                    if (ImGui::BeginPopupContextItem("##smartrow"))
                    {
                        if (c.baseline) ImGui::TextDisabled("The code as written");
                        else if (ImGui::MenuItem("Copy  mov ax / mov dx")) SmartCopyPattern(c);
                        ImGui::EndPopup();
                    }
                    if (ImGui::IsItemHovered() && !c.baseline)
                        ImGui::SetTooltip("Double-click (or right-click) copies:\n  mov ax, 0x%04X\n  mov dx, 0x%04X%s",
                                          (unsigned)(cw::u16)c.ax, (unsigned)(cw::u16)c.dx,
                                          dropped ? "\n\nDropped from the race early -- fewer games behind its number" : "");
                    ImGui::TableSetColumnIndex(1);
                    if (c.baseline) ImGui::TextUnformatted("--"); else ImGui::Text("%04X", (unsigned)(cw::u16)c.ax);
                    ImGui::TableSetColumnIndex(2);
                    if (c.baseline) ImGui::TextUnformatted("--"); else ImGui::Text("%04X", (unsigned)(cw::u16)c.dx);
                    ImGui::TableSetColumnIndex(3);
                    if (c.baseline) ImGui::TextUnformatted("");
                    else
                        ImGui::Text("%02X %02X %02X %02X", (unsigned)((cw::u16)c.ax & 0xFF), (unsigned)((cw::u16)c.ax >> 8),
                                    (unsigned)((cw::u16)c.dx & 0xFF), (unsigned)((cw::u16)c.dx >> 8));
                    ImGui::TableSetColumnIndex(4);
                    ImGui::TextUnformatted(c.from.c_str());
                    ImGui::TableSetColumnIndex(5);
                    ImGui::Text("%d", c.played.load());
                    ImGui::TableSetColumnIndex(6);
                    const double m = c.Mean() * 100.0;
                    if (c.baseline || c.played.load() == 0) ImGui::Text("%5.1f%%", m);
                    else
                    {
                        // against the code as written, over the same games
                        const double d = (c.Mean() - baseOver(c.upTo.load())) * 100.0;
                        ImGui::Text("%5.1f%%", m);
                        ImGui::SameLine();
                        ImGui::TextColored(Hex(d > 0.05 ? Col::Ok : d < -0.05 ? Col::Danger : Col::TextDim), "%+.1f", d);
                    }
                    ImGui::PopStyleColor();
                    ImGui::PopID();
                }
            ImGui::EndTable();
        }
    }
    ImGui::End();
}
