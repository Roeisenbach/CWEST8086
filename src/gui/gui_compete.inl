// =============================================================================
//  Competition panel
//
//  Runs a batch of wars across the machine's cores and shows the standings as a
//  sorted bar graph with a table beside it. Nothing is recorded -- a run of a
//  few thousand wars only keeps the final scores.
//
//  The runner always plays one *phase of a stride* of the global game index:
//  game g = phase + k * stride, for k = 0, 1, ... Alone that is stride 1, phase
//  0. Two lanes on one machine take stride 2. A tournament shared between
//  machines hands each one a phase of kNetStride, and a machine can cut its
//  phase in two again for its own GPU. Every level of that is disjoint, so the
//  standings are always a plain sum (gui_net.inl carries the sums around).
//
//  Included by main.cpp, after BuildGroups / SeedFromText / the palette helpers
//  and gui_net.inl.
// =============================================================================

static std::vector<cw::WarriorGroup>  g_CompGroups;    // owned while a run is live
static cw::CompeteResult              g_CompPending;   // the CPU worker writes here
static cw::CompeteResult              g_CompIdentity;  // names and zeroed rows
static cw::CompeteResult              g_CompLocal;     // this machine's lanes added up
static std::unique_ptr<std::thread>   g_CompThread;
static std::atomic<bool>              g_CompDone{false};
static int                            g_CompSeenRev = -1;   // last CPU revision folded in
static int                            g_CompPacksSeen = -1; // last g_NetPacksRev folded in
static int                            g_CompLimit = 0;      // stop once every lane adds up to this; 0 = never
static bool                           g_CompStopping = false;
static double                         g_CompStopAt = 0.0;
static double                         g_CompStartAt = 0.0;
static double                         g_CompLastSend = 0.0;
static int                            g_CompRunId = 0;
static int                            g_CompStride = 1, g_CompPhase = 0;   // what the local lanes play
static int                            g_CompComboUsed = 0;  // groups per battle in the run on the board
static std::string                    g_CompSender;         // this machine's name on the board

static std::string                    g_CompNetNote;        // last listen / join outcome
static bool                           g_CompRaise = false;  // bring the panel's tab to the front once

// COMP_GPU (the graphics card alone) is gone; COMP_BOTH is "Busy (C++)".
enum { COMP_CPU = 0, COMP_GPU = 1, COMP_BOTH = 2, COMP_JAVA = 3 };
// 1 was the offline mode (shard and result files), since removed; the numbers
// stay as they were so a saved setting still means the same thing.
enum { MODE_LOCAL = 0, MODE_SERVER = 2, MODE_JOIN = 3 };

// A run against folders: the picked groups play each folder as a field of its
// own, and the board keeps a page per folder. Empty names = an ordinary run.
static std::vector<std::string> g_CompFieldNames;
static std::vector<int>         g_CompFieldOf;       // per group of g_CompGroups; -1 zombies
static std::vector<std::string> g_CompFieldSubjects; // the groups that played every folder (their names)
static std::vector<std::string> g_CompFieldSubjectKeys; // and who they are (PlayerKey): a clash in a folder
                                                        // renames them for the war, so never match by name
static int                      g_CompFieldView = 0;
static bool                     g_CompForceCpu = false;
struct FolderRunSpec { bool on = false; std::vector<RefKey> groups; std::vector<std::string> folders; };
static FolderRunSpec            g_FolderRun;

// ---- options ------------------------------------------------------------------

// The tournament's own first seed, or the Debug panel's when none is set.
static const char* CompSeedText()
{
    return g_App.compSeedText[0] ? g_App.compSeedText : g_App.seedText;
}

static void CompetitionOptionsFromApp(cw::CompeteOptions& opt)
{
    opt.firstSeed   = SeedFromText(CompSeedText());
    opt.zombieSpeed = g_App.zombieSpeed;
    opt.maxRounds   = g_App.maxRounds;
    opt.threads     = g_App.compThreads;
    opt.comboSize   = ImMax(0, g_App.compCombo);
    opt.pairingSeed = g_App.compPairSeed;
}

// A field pack decides everything about the battles; only the thread count is
// this machine's business.
static void CompetitionOptionsFromField(const cw::FieldPack& f, cw::CompeteOptions& opt)
{
    opt.firstSeed   = f.seed;
    opt.zombieSpeed = f.zombieSpeed;
    opt.maxRounds   = f.maxRounds;
    opt.threads     = g_App.compThreads;
    opt.comboSize   = ImMax(0, f.comboSize);
    opt.pairingSeed = f.pairingSeed;
}

static cw::FieldPack CompetitionFieldFromApp(const std::vector<cw::WarriorGroup>& groups)
{
    cw::FieldPack f;
    f.groups      = groups;
    f.seed        = SeedFromText(CompSeedText());
    f.pairingSeed = g_App.compPairSeed;
    f.comboSize   = ImMax(0, g_App.compCombo);
    f.maxRounds   = g_App.maxRounds;
    f.zombieSpeed = g_App.zombieSpeed;
    char when[64];
    time_t now = time(NULL);
    strftime(when, sizeof(when), "%Y-%m-%d %H:%M", localtime(&now));
    f.title = NetComputerName() + " " + when;
    return f;
}

// ---- the run ------------------------------------------------------------------

static void CompetitionStop()
{
    if (!g_App.compRunning) return;
    g_App.compProg.cancel.store(true);
    GpuLaneStop();
    JavaLaneStop();
    if (g_App.compMode == MODE_SERVER && !g_CompStopping)
    {
        NetServerBroadcast("stop 1\nend\n");
        g_CompStopping = true;
        g_CompStopAt = AppNow();
    }
}

// Games finished so far, every lane and every other machine counted.
static int CompetitionGamesDone()
{
    int done = g_App.compProg.done.load() + g_Gpu.gamesRun + g_Java.gamesRun;
    for (const auto& kv : g_NetPacks) done += kv.second.gamesRun;
    return done;
}

// The board is every lane added together. None can score a war another
// scored -- they play disjoint phases of the global game index -- so a plain
// element-wise sum is the whole of the merge.
static void CompetitionCombine()
{
    cw::CompeteResult local = g_CompIdentity;
    if (local.empty()) return;

    auto add = [&local](const cw::CompeteResult& src) {
        if (src.warriorScore.size() != local.warriorScore.size()) return;
        for (size_t i = 0; i < local.warriorScore.size(); i++)
        {
            local.warriorScore[i]    += src.warriorScore[i];
            local.warriorSurvived[i] += src.warriorSurvived[i];
        }
        local.gamesRun    += src.gamesRun;
        local.gamesFailed += src.gamesFailed;
        local.rounds      += src.rounds;
        // wall time is how long the run has been going, not the sum of the lanes
        if (src.seconds > local.seconds) local.seconds = src.seconds;
    };

    {
        std::lock_guard<std::mutex> lk(g_App.compProg.lock);
        add(g_CompPending);
    }
    add(g_Gpu.result);
    add(g_Java.result);
    for (size_t i = 0; i < local.warriorName.size(); i++)
    {
        const int g = local.warriorGroup[i];
        if (g < 0 || g >= (int)local.groupName.size()) continue;
        local.groupScore[(size_t)g]    += local.warriorScore[i];
        local.groupSurvived[(size_t)g] += local.warriorSurvived[i];
    }
    g_CompLocal = local;

    // what the joined machines sent, on top of the lanes
    cw::CompeteResult r = local;
    for (const auto& kv : g_NetPacks)
    {
        cw::addResultPack(r, kv.second);
        if (kv.second.seconds > r.seconds) r.seconds = kv.second.seconds;
    }
    if (g_App.compRunning && g_CompStartAt > 0.0)
        r.seconds = ImMax(r.seconds, AppNow() - g_CompStartAt);
    g_App.compResult = std::move(r);
}

// Starts this machine's lanes on one phase of a stride. `target` is the games
// the whole run should reach (0 = until Stop); `openEnded` makes every lane
// play until told to stop, which is what a shared run needs -- no lane can
// know how many games the others will manage.
static bool CompetitionLaunch(const std::vector<cw::WarriorGroup>& groups, const cw::CompeteOptions& opt,
                              int stride, int phase, int target, bool openEnded)
{
    if (g_App.compRunning) return false;
    if (groups.empty())
    {
        g_App.status = "Need at least one group with an assembled survivor";
        return false;
    }

    g_CompGroups = groups;
    cw::prepareResult(g_CompGroups, g_CompIdentity);
    g_App.compResult = g_CompIdentity;          // names show up immediately
    g_CompPending = g_CompIdentity;
    g_CompLocal = g_CompIdentity;
    g_Gpu = GpuLane();
    g_Java = GpuLane();

    g_CompLimit  = ImMax(0, target);
    g_CompStride = stride;
    g_CompPhase  = phase;
    g_CompComboUsed = opt.comboSize;

    // Two lanes on this machine each take half of its phase: the CPU keeps the
    // phase at twice the stride and the GPU takes the one `stride` further on.
    // Either lane alone plays the phase as handed. A split run is open-ended
    // on both sides and the panel stops them once their finished games add up
    // -- neither is pre-assigned a share, so the CPU is never left idle waiting
    // on a slower GPU (or the other way round). The price is that exactly
    // which games were played depends on timing, so a split run is not
    // repeatable game-for-game the way a single-lane one is.
    // The Java engine and the graphics card hold 20 warriors a battle, as the
    // reference does; the C++ engine holds more. A field whose battles can be
    // bigger than that is played by C++ alone rather than half-failing.
    int biggest = 0;
    {
        std::vector<int> sizes;
        int zomb = 0;
        for (const cw::WarriorGroup& g : groups)
        {
            if (cw::isZombieGroup(g)) zomb += (int)g.warriors.size();
            else                      sizes.push_back((int)g.warriors.size());
        }
        std::sort(sizes.begin(), sizes.end(), std::greater<int>());
        const int take = opt.comboSize > 0 ? ImMin(opt.comboSize, (int)sizes.size()) : (int)sizes.size();
        for (int i = 0; i < take; i++) biggest += sizes[(size_t)i];
        biggest += zomb;
    }
    const bool tooBigForOthers = biggest > cw::War::REFERENCE_MAX_WARRIORS;
    const int backend = (g_CompForceCpu || tooBigForOthers) ? COMP_CPU : g_App.compBackend;
    const bool split = (backend == COMP_BOTH);
    int cpuGames = openEnded ? 0 : target, gpuGames = openEnded ? 0 : target;
    int cpuStride = stride, cpuPhase = phase, gpuStride = stride, gpuPhase = phase;
    if (split)
    {
        cpuStride = gpuStride = stride * 2;
        gpuPhase  = phase + stride;
        cpuGames = gpuGames = 0;
    }

    g_App.compGpuNote.clear();
    if (tooBigForOthers && !g_CompForceCpu && g_App.compBackend != COMP_CPU)
        g_App.compGpuNote = "Battles of up to " + std::to_string(biggest) + " warriors: only the C++ engine holds more "
                            "than " + std::to_string(cw::War::REFERENCE_MAX_WARRIORS) + ", so it plays them all";
    bool gpuUp = false;
    if (backend == COMP_BOTH)
    {
        cw::CompeteOptions g = opt;
        g.seedStride = gpuStride;
        g.seedPhase  = gpuPhase;
        std::string why;
        gpuUp = GpuLaneStart(g_CompGroups, g, gpuGames, why);
        if (!gpuUp)
        {
            g_App.compGpuNote = "GPU: " + why;
            // Without the GPU the CPU takes the whole phase rather than
            // silently running half of it.
            cpuGames  = openEnded ? 0 : target;
            cpuStride = stride;
            cpuPhase  = phase;
        }
    }

    // The Java lane plays the phase exactly as handed -- the same battles the
    // CPU lane would have -- and if it cannot start the CPU plays them instead.
    bool javaUp = false;
    if (backend == COMP_JAVA)
    {
        cw::CompeteOptions j = opt;
        j.seedStride = stride;
        j.seedPhase  = phase;
        std::string why;
        javaUp = JavaLaneStart(g_CompGroups, j, openEnded ? 0 : target, why);
        if (!javaUp) g_App.compGpuNote = "Java: " + why;
    }

    const bool cpuLane = (backend == COMP_CPU || backend == COMP_BOTH || backend == COMP_GPU)
                      || (backend == COMP_JAVA && !javaUp);

    g_App.compProg.done.store(0);
    g_App.compProg.cancel.store(false);
    g_CompDone.store(true);                    // no CPU lane unless one starts
    g_CompSeenRev = -1;
    g_CompPacksSeen = -1;
    g_CompStopping = false;
    g_CompStartAt = AppNow();
    g_CompLastSend = 0.0;
    g_App.compRunning = gpuUp || javaUp;

    if (cpuLane)
    {
        cw::CompeteOptions c = opt;
        c.games       = cpuGames;              // 0 = open-ended
        c.seedStride  = cpuStride;
        c.seedPhase   = cpuPhase;
        // The workers take every core at normal priority; the frame stays
        // smooth because this thread outranks them while the run is going.
        // Lowering the workers instead sent them all to the efficiency cores
        // on a hybrid CPU, at a third of the speed.
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
        g_CompDone.store(false);
        g_App.compRunning = true;
        g_CompThread.reset(new std::thread([c]() {
            // The worker folds into its own result; the panel adds the lanes
            // together under the progress lock, so it never reads a half-written
            // table.
            cw::runCompetition(g_CompGroups, c, g_CompPending, &g_App.compProg);
            g_CompDone.store(true);
        }));
    }

    if (!g_App.compRunning)
        g_App.status = g_App.compGpuNote.empty() ? "Nothing to run" : g_App.compGpuNote;
    return g_App.compRunning;
}

// The field the host hands a joiner: the same pack for everyone but the phase.
static std::string CompetitionFieldFor(const NetPeer& p)
{
    cw::FieldPack f = CompetitionFieldFromApp(g_CompGroups);
    f.stride = kNetStride;
    f.phase  = p.phase;
    f.games  = 0;
    f.runId  = g_CompRunId;
    return cw::writeFieldPack(f);
}

static void NetServerSeat(NetPeer& p)
{
    if (p.phase < 0) p.phase = NetServerFreePhase();
    if (p.phase < 0) return;                   // the tournament is full
    if (g_App.compRunning && g_App.compMode == MODE_SERVER && !g_CompStopping)
    {
        p.runId = g_CompRunId;
        p.running = true;
        NetServerSend(p, CompetitionFieldFor(p));
    }
}

// A group as it fights in a run against folders: every survivor it has, active
// or not -- picking the folders is the choice of who plays.
static bool FieldGroupOf(const Player& pl, cw::WarriorGroup& g)
{
    g = cw::WarriorGroup();
    g.name = pl.name;
    g.key  = PlayerKey(pl);
    for (int s = 0; s < kSlotCount; s++)
    {
        const Survivor& sv = pl.slots[s];
        if (!sv.present || sv.bin.empty()) continue;
        cw::WarriorData wd;
        wd.name = pl.name + std::to_string(s + 1);
        wd.code.assign(sv.bin.begin(), sv.bin.end());
        wd.type = (s == 0) ? cw::WarriorType::SURVIVOR_1 : cw::WarriorType::SURVIVOR_2;
        g.warriors.push_back(std::move(wd));
    }
    return !g.warriors.empty();
}

// Plays the remembered run against folders again.
static void CompetitionRunFolderSpec()
{
    AppState& S = g_App;
    std::vector<int> subjects;
    for (const RefKey& k : g_FolderRun.groups)
    {
        const Ref r = RefOfKey(k);
        if (r.player >= 0 && std::find(subjects.begin(), subjects.end(), r.player) == subjects.end())
            subjects.push_back(r.player);
    }
    std::sort(subjects.begin(), subjects.end());
    if (subjects.empty()) { S.status = "The groups of the run against folders are gone"; g_FolderRun.on = false; return; }

    std::vector<cw::WarriorGroup> groups;
    std::vector<int> fieldOf;
    std::vector<std::string> names;
    std::string skipped;
    for (const std::string& folder : g_FolderRun.folders)
    {
        if (FolderIndex(folder) < 0) { skipped += (skipped.empty() ? "" : ", ") + folder + " (gone)"; continue; }
        // the picked groups first, then the folder's own, in the order they load
        std::vector<int> order = subjects;
        for (int p : GroupsUnder(folder))
            if (std::find(order.begin(), order.end(), p) == order.end()) order.push_back(p);
        const int fieldIndex = (int)names.size();
        int added = 0, opponents = 0;
        std::vector<cw::WarriorGroup> fg;
        std::vector<const Player*> fgFrom;
        for (int p : order)
        {
            cw::WarriorGroup g;
            if (!FieldGroupOf(S.players[p], g)) continue;
            fg.push_back(std::move(g));
            fgFrom.push_back(&S.players[p]);
            added++;
            if (std::find(subjects.begin(), subjects.end(), p) == subjects.end()) opponents++;
        }
        if (opponents == 0) { skipped += (skipped.empty() ? "" : ", ") + FolderLeaf(folder) + " (nobody to fight)"; continue; }
        SeparateSameNames(fg, fgFrom);
        for (cw::WarriorGroup& g : fg) { groups.push_back(std::move(g)); fieldOf.push_back(fieldIndex); }
        names.push_back(folder);
        (void)added;
    }
    if (names.empty()) { S.status = "Nothing to run" + (skipped.empty() ? std::string() : ": " + skipped); return; }

    // the zombies, in every battle of every field, as always
    {
        cw::WarriorGroup z;
        z.name = "ZoMbIeS";
        for (const Zombie& zb : S.zombies)
        {
            if (!zb.runnable()) continue;
            cw::WarriorData wd;
            wd.name = zb.name.empty() ? zb.sv.name : zb.name;
            wd.code.assign(zb.sv.bin.begin(), zb.sv.bin.end());
            wd.type  = zb.heavy ? cw::WarriorType::ZOMBIE_H : cw::WarriorType::ZOMBIE;
            wd.speed = zb.speed;
            z.warriors.push_back(std::move(wd));
        }
        if (!z.warriors.empty()) { groups.push_back(std::move(z)); fieldOf.push_back(-1); }
    }

    cw::CompeteOptions opt;
    CompetitionOptionsFromApp(opt);
    opt.fieldCount = (int)names.size();
    opt.groupField = fieldOf;
    g_CompFieldNames = names;
    g_CompFieldOf = fieldOf;
    g_CompFieldSubjects.clear();
    g_CompFieldSubjectKeys.clear();
    for (int p : subjects)
    {
        g_CompFieldSubjects.push_back(S.players[p].name);
        g_CompFieldSubjectKeys.push_back(PlayerKey(S.players[p]));
    }
    if (g_CompFieldView >= (int)names.size()) g_CompFieldView = 0;
    g_NetPacks.clear();
    g_NetPacksRev++;
    g_CompSender = NetComputerName();
    g_CompForceCpu = true;            // the other lanes do not know about separate fields
    CompetitionLaunch(groups, opt, 1, 0, S.compGames, false);
    g_CompForceCpu = false;
    if (S.compRunning)
    {
        S.status = "Running " + std::to_string((int)subjects.size()) + " group(s) against " +
                   std::to_string((int)names.size()) + " folder(s), each on its own" +
                   (skipped.empty() ? std::string() : "  -- skipped " + skipped);
        if (S.compBackend != COMP_CPU) S.compGpuNote = "Runs against folders play on the C++ engine";
    }
}

// From a group's menu: these groups against each of these folders.
static void CompetitionStartFolders(const std::vector<int>& groupsIn, const std::vector<std::string>& folders)
{
    AppState& S = g_App;
    if (S.compRunning) { S.status = "Stop the running tournament first"; return; }
    if (S.compMode != MODE_LOCAL)
    {
        S.status = "Runs against folders are played on this machine -- set the Competition panel to Local";
        return;
    }
    g_FolderRun = FolderRunSpec();
    for (int p : groupsIn) g_FolderRun.groups.push_back(GroupKey(p));
    g_FolderRun.folders = folders;
    g_FolderRun.on = true;
    g_CompFieldView = 0;
    S.showBoard = true;
    g_CompRaise = true;
    CompetitionRunFolderSpec();
}

// The Run button.
static void CompetitionStart()
{
    if (g_App.compRunning) return;
    AppState& S = g_App;
    if (g_FolderRun.on && S.compMode == MODE_LOCAL) { CompetitionRunFolderSpec(); return; }
    g_CompFieldNames.clear();
    g_CompFieldOf.clear();
    g_CompFieldSubjects.clear();
    g_CompFieldSubjectKeys.clear();
    g_CompForceCpu = false;
    const bool needsWorkspace = (S.compMode != MODE_JOIN);
    if (needsWorkspace && !HasWorkspace()) { S.status = "Open a workspace first (Ctrl+O)"; return; }

    cw::CompeteOptions opt;
    switch (S.compMode)
    {
    case MODE_SERVER:
    {
        if (!NetServerUp())
        {
            S.status = "Press Listen first so other machines can join";
            return;
        }
        CompetitionOptionsFromApp(opt);
        g_NetPacks.clear();
        g_NetPacksRev++;
        g_CompRunId++;
        g_CompSender = NetComputerName();
        if (!CompetitionLaunch(BuildGroups(), opt, kNetStride, 0, S.compGames, true)) return;
        for (NetPeer& p : g_NetServer.peers)
            if (p.phase >= 0) NetServerSeat(p);
        return;
    }

    case MODE_JOIN:
        // nothing to press: the host starts us
        return;

    default:
        CompetitionOptionsFromApp(opt);
        g_NetPacks.clear();
        g_NetPacksRev++;
        g_CompSender = NetComputerName();
        CompetitionLaunch(BuildGroups(), opt, 1, 0, S.compGames, false);
        return;
    }
}

// A joiner starting the field its host sent.
static void CompetitionStartFromHost()
{
    NetClient& c = g_NetClient;
    cw::CompeteOptions opt;
    CompetitionOptionsFromField(c.field, opt);
    g_CompRunId = c.field.runId;
    g_CompSender = g_App.compNetName;
    g_NetPacks.clear();
    c.haveField = false;
    c.doneOwed = true;
    if (CompetitionLaunch(c.field.groups, opt, c.field.stride, c.field.phase, 0, true))
    {
        c.state = NET_RUNNING;
        c.note = "playing";
        c.board = g_CompIdentity;
    }
    else
    {
        // nothing could start (no GPU and GPU-only, say): tell the host we are done
        NetClientSend("done 1\nrun " + std::to_string(g_CompRunId) + "\nend\n");
        c.doneOwed = false;
        c.state = NET_WAITING;
        c.note = "could not start: " + g_App.status;
    }
}

static void CompetitionSendContribution()
{
    NetClientSend(cw::writeResultPack(g_CompLocal, g_App.compNetName, g_CompRunId));
}

// Call once a frame: picks up the standings so far, and adopts a finished run.
static void CompetitionPoll()
{
    AppState& S = g_App;
    NetServerPoll();
    NetClientPoll();
    const double now = AppNow();

    if (S.compMode == MODE_JOIN)
    {
        NetClient& c = g_NetClient;
        // the host said stop, or moved on to a new run while we were still on this one
        if (S.compRunning && (c.stopAsked || (c.haveField && c.field.runId != g_CompRunId)))
        {
            c.stopAsked = false;
            S.compProg.cancel.store(true);
            GpuLaneStop();
            JavaLaneStop();
        }
        if (!S.compRunning && c.haveField && (c.state == NET_WAITING || c.state == NET_RUNNING))
            CompetitionStartFromHost();
    }

    if (!S.compRunning)
    {
        // a joiner's last report can land after the run has ended
        if (g_NetPacksRev != g_CompPacksSeen)
        {
            g_CompPacksSeen = g_NetPacksRev;
            if (!g_CompIdentity.empty()) CompetitionCombine();
        }
        return;
    }

    bool gpuAlive = GpuLanePoll();
    bool javaAlive = JavaLanePoll();

    // The stop condition for a run with open-ended lanes in it: the games
    // every lane and every machine has finished add up to the target. Each
    // lane may overshoot by the chunk it had in hand, which is a handful of
    // games.
    if (g_CompLimit > 0 && !g_CompStopping)
    {
        const int done = CompetitionGamesDone();
        const bool cpuOpen = g_CompThread && !g_CompDone.load();
        if (done >= g_CompLimit && (gpuAlive || javaAlive || cpuOpen || S.compMode == MODE_SERVER))
        {
            g_CompStopping = true;
            g_CompStopAt = now;
            S.compProg.cancel.store(true);
            GpuLaneStop();
            JavaLaneStop();
            gpuAlive = javaAlive = false;
            if (S.compMode == MODE_SERVER) NetServerBroadcast("stop 1\nend\n");
        }
    }

    // Mid-run the CPU workers keep folding into g_CompPending; the revision
    // counter means a still board costs an atomic load a frame rather than a
    // lock and a copy.
    const int rev = S.compProg.revision.load(std::memory_order_acquire);
    const bool cpuMoved = (rev != g_CompSeenRev);
    if (cpuMoved) g_CompSeenRev = rev;
    const bool packsMoved = (g_NetPacksRev != g_CompPacksSeen);
    if (packsMoved) g_CompPacksSeen = g_NetPacksRev;
    if (cpuMoved || gpuAlive || javaAlive || packsMoved) CompetitionCombine();

    // once a second: a joiner reports in, a host shares the merged board
    if (now - g_CompLastSend >= 1.0)
    {
        g_CompLastSend = now;
        if (S.compMode == MODE_JOIN && g_NetClient.state == NET_RUNNING) CompetitionSendContribution();
        if (S.compMode == MODE_SERVER && !g_NetServer.peers.empty())
            NetServerBroadcast(cw::writeResultPack(S.compResult, "total", g_CompRunId));
    }

    const bool cpuAlive = g_CompThread && !g_CompDone.load();
    if (cpuAlive || gpuAlive || javaAlive) return;

    if (S.compMode == MODE_SERVER)
    {
        // Local lanes are done. Joiners are still winding down their last
        // chunk and reporting it; give them a moment before the board is
        // called final.
        if (!g_CompStopping)
        {
            g_CompStopping = true;
            g_CompStopAt = now;
            NetServerBroadcast("stop 1\nend\n");
        }
        bool waiting = false;
        for (const NetPeer& p : g_NetServer.peers) if (p.running) waiting = true;
        if (waiting && now - g_CompStopAt < 10.0) return;
        for (NetPeer& p : g_NetServer.peers) p.running = false;
    }

    if (g_CompThread)
    {
        if (g_CompThread->joinable()) g_CompThread->join();
        g_CompThread.reset();
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
    }
    S.compRunning = false;
    g_CompStopping = false;
    CompetitionCombine();

    if (S.compMode == MODE_JOIN && g_NetClient.doneOwed)
    {
        // the last of what we played, then the host knows this lane is closed
        if (NetClientLinked())
        {
            CompetitionSendContribution();
            NetClientSend("done 1\nrun " + std::to_string(g_CompRunId) + "\nend\n");
        }
        g_NetClient.doneOwed = false;
        if (g_NetClient.state == NET_RUNNING)
        {
            g_NetClient.state = NET_WAITING;
            g_NetClient.note = "done -- waiting for the host's next run";
        }
    }
    if (S.compMode == MODE_SERVER && !g_NetServer.peers.empty())
        NetServerBroadcast(cw::writeResultPack(S.compResult, "total", g_CompRunId));

    const std::string& laneNote = !g_Gpu.note.empty() ? g_Gpu.note : g_Java.note;
    char buf[400];
    snprintf(buf, sizeof(buf), "Competition: %d war(s) in %.2fs  --  %.0f games/s%s%s",
             S.compResult.gamesRun, S.compResult.seconds,
             S.compResult.gamesPerSecond(),
             S.compResult.gamesFailed
                 ? ("  -- " + std::to_string(S.compResult.gamesFailed) + " could not place").c_str()
                 : "",
             laneNote.empty() ? "" : ("  -- " + laneNote).c_str());
    S.status = buf;
}

// ---- drawing ----------------------------------------------------------------------

// The colour a name wears in the arena, matched back to the workspace so the
// board and the fight read as the same thing. A field that came over the wire
// has no workspace behind it, so its groups take the
// player palette in field order -- the colours a host with the same folder
// loaded would have given them.
static unsigned CompColorOf(const cw::CompeteResult& R, size_t row, bool isGroup)
{
    // A field played here knows each group's workspace identity, so two groups
    // of one name keep their own colours; only results from elsewhere match by
    // name.
    {
        const int g = isGroup ? (int)row : (row < R.warriorGroup.size() ? R.warriorGroup[row] : -1);
        if (g >= 0 && g_CompGroups.size() == R.groupName.size() && (size_t)g < g_CompGroups.size())
        {
            const int p = PlayerOfKey(g_CompGroups[(size_t)g].key);
            if (p >= 0)
            {
                const Player& pl = g_App.players[(size_t)p];
                if (isGroup) return pl.tint;
                int sl = 0;
                for (size_t i = 0; i < row; i++) if (R.warriorGroup[i] == g) sl++;
                sl = ImMin(sl, kSlotCount - 1);
                return pl.twoTone ? (sl == 1 ? pl.tint2 : pl.tint) : SlotTint(pl.tint, sl);
            }
        }
    }
    const std::string& name = isGroup ? R.groupName[row] : R.warriorName[row];
    for (size_t p = 0; p < g_App.players.size(); p++)
    {
        const Player& pl = g_App.players[p];
        if (isGroup)
        {
            if (pl.name == name) return pl.tint;
            continue;
        }
        for (int sl = 0; sl < kSlotCount; sl++)
            if (name == pl.name + std::to_string(sl + 1))
                return pl.twoTone ? (sl == 1 ? pl.tint2 : pl.tint) : SlotTint(pl.tint, sl);
    }
    const int g = isGroup ? (int)row : (row < R.warriorGroup.size() ? R.warriorGroup[row] : 0);
    const unsigned base = kPlayerTints[(size_t)ImMax(0, g) % (sizeof(kPlayerTints) / sizeof(kPlayerTints[0]))];
    if (isGroup) return base;
    // which of the group's warriors this is decides the shade, as in the arena
    int slot = 0;
    for (size_t i = 0; i < row; i++) if (R.warriorGroup[i] == g) slot++;
    return SlotTint(base, ImMin(slot, kSlotCount - 1));
}

struct CompRow { std::string name; double score; int survived; unsigned tint; };

// Text on a bar: the bar can be any colour, bright or dark, so the text gets
// a drop shadow one pixel down and to the right, in the opposite tone -- dark
// under light text, light under dark text -- and reads on all of them.
static void AddBarText(ImDrawList* dl, ImVec2 pos, ImU32 col, const char* text, bool up = false)
{
    const ImVec4 c = ImGui::ColorConvertU32ToFloat4(col);
    const float lum = 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z;
    const ImU32 shadow = (lum > 0.5f) ? IM_COL32(0, 0, 0, 170) : IM_COL32(255, 255, 255, 170);
    pos = ImVec2(ImTrunc(pos.x), ImTrunc(pos.y));
    if (!g_App.compBarShadow)                       // Options > Competition can turn it off
    {
        if (up) AddTextUp(dl, pos, col, text); else dl->AddText(pos, col, text);
        return;
    }
    const float o = ImMax(1.0f, ImFloor(g_Scale + 0.5f));
    const ImVec2 sp(pos.x + o, pos.y + o);
    if (up) { AddTextUp(dl, sp, shadow, text); AddTextUp(dl, pos, col, text); }
    else    { dl->AddText(sp, shadow, text);    dl->AddText(pos, col, text); }
}

static void DrawCompBars(const std::vector<CompRow>& rows, double best, int gamesRun, bool vertical, bool fit,
                         bool showLabels)
{
    const int n = (int)rows.size();
    if (n == 0) return;

    ImGuiWindowFlags flags = vertical ? ImGuiWindowFlags_HorizontalScrollbar : 0;
    ImGui::BeginChild("##compgraph", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding, flags);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float lineH = ImGui::GetTextLineHeight();

    // thickness of one bar, and the span they fill
    float thick;
    if (fit)  thick = ImMax(3.0f * g_Scale, (vertical ? avail.x : avail.y) / (float)n);
    else      thick = vertical ? 22.0f * g_Scale : lineH + 6.0f * g_Scale;
    thick = ImMax(thick, 1.0f);
    // Text rides on a bar whenever the bar is thick enough to carry a line of
    // it: along the bar when they lie down, turned on its side when they stand
    // up as columns. Too thin for that, a column still gets its rank.
    const bool labels = showLabels && thick >= lineH + 2.0f * g_Scale;
    const bool ranks  = vertical && !labels && thick >= 12.0f * g_Scale;
    float valueW = 0.0f;                                     // room kept for the value column
    if (labels)
    {
        char widest[48];
        snprintf(widest, sizeof(widest), "%.2f", best);
        valueW = ImGui::CalcTextSize(widest).x + 16 * g_Scale;
    }
    const float span  = thick * (float)n;                    // along the stacking axis
    const float reach = (vertical ? avail.y : avail.x) - valueW;   // along the score axis
    const ImVec2 total = vertical ? ImVec2(ImMax(span, avail.x), avail.y)
                                  : ImVec2(avail.x, ImMax(span, avail.y));

    // one item for the whole graph, so scrolling and hovering both work
    ImGui::InvisibleButton("##bars", ImVec2(ImMax(total.x, 1.0f), ImMax(total.y, 1.0f)));
    // climbing columns that overflow open at the winners' end, the way a
    // stacked list opens at the top
    static int lastShape = -1;
    const int shape = vertical ? (fit ? 1 : 2) * 100000 + n : 0;
    if (shape != lastShape)
    {
        lastShape = shape;
        if (vertical && span > avail.x) ImGui::SetScrollX(span - avail.x);
    }
    const bool hovered = ImGui::IsItemHovered();
    const ImVec2 mouse = ImGui::GetMousePos();
    int hot = -1;
    if (hovered)
    {
        const float along = vertical ? (mouse.x - origin.x) : (mouse.y - origin.y);
        hot = (int)(along / thick);
        if (hot < 0 || hot >= n) hot = -1;
    }

    // ascending to the right when climbing, best first when stacked down
    auto rowAt = [&](int i) -> const CompRow& { return vertical ? rows[(size_t)(n - 1 - i)] : rows[(size_t)i]; };

    const ImVec2 clipMin = ImGui::GetWindowPos();
    const ImVec2 clipMax = ImVec2(clipMin.x + ImGui::GetWindowSize().x, clipMin.y + ImGui::GetWindowSize().y);
    const float gap = (thick >= 6.0f * g_Scale) ? 1.0f : 0.0f;   // a hairline between thick bars

    // track behind the bars
    dl->AddRectFilled(origin, ImVec2(origin.x + (vertical ? span : avail.x),
                                     origin.y + (vertical ? avail.y : span)), HexU(Col::Bg2));

    for (int i = 0; i < n; i++)
    {
        const CompRow& r = rowAt(i);
        const float len = ImMax(fit ? 1.0f : 2.0f, (float)(r.score / best) * reach);
        ImVec2 a, b;
        if (vertical)
        {
            a = ImVec2(origin.x + thick * (float)i, origin.y + avail.y - len);
            b = ImVec2(a.x + thick - gap, origin.y + avail.y);
        }
        else
        {
            a = ImVec2(origin.x, origin.y + thick * (float)i);
            b = ImVec2(origin.x + len, a.y + thick - gap);
        }
        if (b.y < clipMin.y || a.y > clipMax.y || b.x < clipMin.x || a.x > clipMax.x) continue;

        const bool hi = (i == hot);
        dl->AddRectFilled(a, b, HexU(r.tint, hi ? 1.0f : 0.9f));
        if (hi) dl->AddRect(a, b, HexU(ThemeIsDark() ? 0xFFFFFF : 0x000000, 0.9f));

        if (labels && vertical)
        {
            // The same two pieces of text as a bar lying down, turned a quarter
            // turn: the name climbs the column from its foot -- or starts above
            // it when the column is too short to hold it -- and the score sits
            // at the head of the track, where the value column would be.
            const float tx = a.x + (thick - gap - lineH) * 0.5f;
            const ImVec2 ts = ImGui::CalcTextSize(r.name.c_str());
            char val[48];
            snprintf(val, sizeof(val), "%.2f", r.score);
            const ImVec2 vs = ImGui::CalcTextSize(val);
            const float valFoot = origin.y + 8 * g_Scale + vs.x;      // where the score's run starts
            const bool inside = len > ts.x + 16 * g_Scale;
            // outside only while there is track left above the column for it
            const bool above = !inside && (a.y - 8 * g_Scale - ts.x) > valFoot + 4 * g_Scale;
            if (inside) AddBarText(dl, ImVec2(tx, b.y - 8 * g_Scale), HexU(Col::Bg0), r.name.c_str(), true);
            else        AddTextUp(dl, ImVec2(tx, above ? a.y - 8 * g_Scale : b.y - 8 * g_Scale),
                                  HexU(Col::Text), r.name.c_str());
            AddTextUp(dl, ImVec2(tx, valFoot), HexU(Col::TextDim), val);
        }
        else if (labels)
        {
            // the name rides on the bar, and steps outside it when it would not fit
            const ImVec2 ts = ImGui::CalcTextSize(r.name.c_str());
            const bool inside = len > ts.x + 16 * g_Scale;
            const ImVec2 at(a.x + (inside ? 8 * g_Scale : len + 8 * g_Scale), a.y + (thick - gap - ts.y) * 0.5f);
            if (inside) AddBarText(dl, at, HexU(Col::Bg0), r.name.c_str());
            else        dl->AddText(at, HexU(Col::Text), r.name.c_str());
            char val[48];
            snprintf(val, sizeof(val), "%.2f", r.score);
            const ImVec2 vs = ImGui::CalcTextSize(val);
            dl->AddText(ImVec2(origin.x + avail.x - vs.x - 8 * g_Scale, a.y + (thick - gap - vs.y) * 0.5f),
                        HexU(Col::TextDim), val);
        }
        else if (ranks)
        {
            // rank at the foot of each column; the name is a hover away
            char rk[16];
            snprintf(rk, sizeof(rk), "%d", n - i);
            const ImVec2 rs = ImGui::CalcTextSize(rk);
            if (rs.x < thick)
            {
                const ImVec2 at(a.x + (thick - gap - rs.x) * 0.5f, b.y - rs.y - 2 * g_Scale);
                if (len > rs.y + 4 * g_Scale) AddBarText(dl, at, HexU(Col::Bg0), rk);
                else                          dl->AddText(at, HexU(Col::TextDim), rk);
            }
        }
    }

    if (hot >= 0)
    {
        const CompRow& r = rowAt(hot);
        const int rank = vertical ? n - hot : hot + 1;
        ImGui::BeginTooltip();
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(HexU(r.tint)), "#%d  %s", rank, r.name.c_str());
        ImGui::Text("%.3f points over %d war(s)", r.score, gamesRun);
        ImGui::TextColored(Hex(Col::TextDim), "still standing in %d of them", r.survived);
        ImGui::EndTooltip();
    }
    ImGui::EndChild();
}

// ---- seed search ------------------------------------------------------------------
//
// Random wars between the groups you pick, as Debug would fight them, until
// the survivors you pick are the ones standing at the end. Each seed that did
// it is kept; one click opens that war in Debug.

struct SeekState
{
    bool open = false;
    std::set<std::string>    play;            // "folder|name" of the groups that fight
    std::set<std::string>    win;             // "folder|name|slot" of the survivors that must stand
    bool                     onlyThem = true; // and nobody else
    // the run
    std::vector<std::thread> threads;
    std::atomic<bool>        stop{false};
    std::atomic<long long>   tried{0};
    std::atomic<int>         running{0};
    std::mutex               lock;
    std::vector<long long>   found;
    double                   startedAt = 0.0;
    std::vector<cw::WarriorGroup> field;      // what the workers fight, frozen at Search
    std::vector<int>         want;            // scoring slots that must be alive
    int                      scoring = 0;     // scoring warriors in the field
    int                      zombieSpeed = 2, maxRounds = 0;
    std::vector<RefKey>      players;         // the groups, to hand to Debug
    std::string              note;
};
static SeekState g_Seek;

static std::string SeekGroupKey(const Player& pl) { return ToLower(pl.folder) + "|" + ToLower(pl.name); }

static void SeekSetPlay(const Player& pl, bool on)
{
    const std::string key = SeekGroupKey(pl);
    if (on) { g_Seek.play.insert(key); return; }
    g_Seek.play.erase(key);
    for (int s = 0; s < kSlotCount; s++) g_Seek.win.erase(key + "|" + std::to_string(s));
}

// "Who plays" as the workspace's own tree. It keeps its own folds -- the
// groups bar's are not this list's business -- and every folder has a box that
// takes in (or out) everything inside it.
static void SeekPlayTree(const std::string& parent)
{
    AppState& S = g_App;
    for (int fi : FolderChildren(parent))
    {
        if (fi >= (int)S.folders.size()) continue;
        const std::string path = S.folders[fi].path;
        const std::vector<int> under = GroupsUnder(path);
        int on = 0;
        for (int p : under) if (g_Seek.play.count(SeekGroupKey(S.players[p]))) on++;
        ImGui::PushID(path.c_str());
        bool all = !under.empty() && on == (int)under.size();
        const bool mixed = on > 0 && !all;
        ImGui::BeginDisabled(under.empty());
        if (mixed) ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, true);
        if (ImGui::Checkbox("##all", &all))
            for (int p : under) SeekSetPlay(S.players[p], all);
        if (mixed) ImGui::PopItemFlag();
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip(under.empty() ? "No groups in here" : "Every group in %s (folders inside it too)",
                              path.c_str());
        ImGui::SameLine(0, 4 * g_Scale);
        const bool empty = FolderChildren(path).empty() && GroupsIn(path).empty();
        const bool open = ImGui::TreeNodeEx("##node", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth |
                                                      (empty ? ImGuiTreeNodeFlags_Leaf : 0),
                                            "%s  (%d)", FolderLeaf(path).c_str(), (int)under.size());
        if (open)
        {
            SeekPlayTree(path);
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    for (int p : GroupsIn(parent))
    {
        const Player& pl = S.players[p];
        bool on = g_Seek.play.count(SeekGroupKey(pl)) > 0;
        ImGui::PushID(p);
        if (ImGui::Checkbox(pl.name.c_str(), &on)) SeekSetPlay(pl, on);
        ImGui::PopID();
    }
}

static void SeedSearchStop()
{
    g_Seek.stop.store(true);
    for (std::thread& t : g_Seek.threads) if (t.joinable()) t.join();
    g_Seek.threads.clear();
}

static void SeekWorker(unsigned long long rngSeed)
{
    cw::War war(nullptr, nullptr, g_Seek.zombieSpeed);
    std::vector<const cw::WarriorGroup*> ptrs;
    for (const cw::WarriorGroup& g : g_Seek.field) ptrs.push_back(&g);
    std::vector<int> slots((size_t)cw::War::MAX_WARRIORS + 4);
    std::vector<char> alive((size_t)ImMax(1, g_Seek.scoring));
    std::mt19937_64 rng(rngSeed);
    while (!g_Seek.stop.load(std::memory_order_relaxed))
    {
        const long long seed = (long long)(rng() & 0x7FFFFFFFULL);
        war.reset();
        war.setSeed(seed);
        try { war.loadWarriorGroups(ptrs); }
        catch (const cw::CpuFault&) { g_Seek.tried.fetch_add(1); continue; }
        war.runToEnd(g_Seek.maxRounds);
        float share = 0.0f;
        const int n = war.collectScoreSlots(slots.data(), share);
        std::fill(alive.begin(), alive.end(), 0);
        for (int i = 0; i < n; i++)
            if (slots[(size_t)i] >= 0 && slots[(size_t)i] < (int)alive.size()) alive[(size_t)slots[(size_t)i]] = 1;
        bool ok = true;
        for (int s : g_Seek.want) if (!alive[(size_t)s]) ok = false;
        if (ok && g_Seek.onlyThem && n != (int)g_Seek.want.size()) ok = false;
        g_Seek.tried.fetch_add(1);
        if (ok)
        {
            std::lock_guard<std::mutex> lk(g_Seek.lock);
            if (g_Seek.found.size() < 2000) g_Seek.found.push_back(seed);
        }
    }
    g_Seek.running.fetch_sub(1);
}

static void SeedSearchStart()
{
    AppState& S = g_App;
    SeedSearchStop();
    g_Seek.field.clear();
    g_Seek.want.clear();
    g_Seek.players.clear();
    g_Seek.scoring = 0;
    g_Seek.note.clear();
    // the war exactly as Debug would build it with only these groups active:
    // the groups in workspace order, every survivor they have, then the zombies
    std::vector<const Player*> seekFrom;
    for (const Player& pl : S.players)
    {
        if (!g_Seek.play.count(SeekGroupKey(pl))) continue;
        cw::WarriorGroup g;
        g.name = pl.name;
        g.key  = PlayerKey(pl);
        for (int s = 0; s < kSlotCount; s++)
        {
            const Survivor& sv = pl.slots[s];
            if (!sv.present || sv.bin.empty()) continue;
            cw::WarriorData wd;
            wd.name = pl.name + std::to_string(s + 1);
            wd.code.assign(sv.bin.begin(), sv.bin.end());
            wd.type = (s == 0) ? cw::WarriorType::SURVIVOR_1 : cw::WarriorType::SURVIVOR_2;
            wd.slot = g_Seek.scoring++;
            if (g_Seek.win.count(SeekGroupKey(pl) + "|" + std::to_string(s))) g_Seek.want.push_back(wd.slot);
            g.warriors.push_back(std::move(wd));
        }
        if (g.warriors.empty()) continue;
        g_Seek.field.push_back(std::move(g));
        seekFrom.push_back(&pl);
        RefKey k; k.kind = 0; k.folder = pl.folder; k.name = pl.name; k.slot = -1;
        g_Seek.players.push_back(k);
    }
    SeparateSameNames(g_Seek.field, seekFrom);     // as Debug names them
    int warriors = g_Seek.scoring;
    {
        cw::WarriorGroup z;
        z.name = "ZoMbIeS";
        for (const Zombie& zb : S.zombies)
        {
            if (!zb.runnable()) continue;
            cw::WarriorData wd;
            wd.name = zb.name.empty() ? zb.sv.name : zb.name;
            wd.code.assign(zb.sv.bin.begin(), zb.sv.bin.end());
            wd.type  = zb.heavy ? cw::WarriorType::ZOMBIE_H : cw::WarriorType::ZOMBIE;
            wd.speed = zb.speed;
            wd.slot  = -1;
            z.warriors.push_back(std::move(wd));
            warriors++;
        }
        if (!z.warriors.empty()) g_Seek.field.push_back(std::move(z));
    }
    if (g_Seek.players.empty())   { g_Seek.note = "Pick the groups that fight"; return; }
    if (g_Seek.want.empty())      { g_Seek.note = "Pick the survivors that must win"; return; }
    if (warriors > cw::War::MAX_WARRIORS)
    { g_Seek.note = "That is " + std::to_string(warriors) + " warriors -- a war holds " + std::to_string(cw::War::MAX_WARRIORS); return; }

    g_Seek.zombieSpeed = S.zombieSpeed;
    g_Seek.maxRounds = S.maxRounds;
    {
        std::lock_guard<std::mutex> lk(g_Seek.lock);
        g_Seek.found.clear();
    }
    g_Seek.tried.store(0);
    g_Seek.stop.store(false);
    g_Seek.startedAt = AppNow();
    const int hw = (int)std::thread::hardware_concurrency();
    const int n = S.compThreads > 0 ? S.compThreads : ImMax(1, hw - 1);
    const unsigned long long base = (unsigned long long)time(NULL) * 0x9E3779B97F4A7C15ULL ^ (unsigned long long)(AppNow() * 1e6);
    g_Seek.running.store(n);
    for (int i = 0; i < n; i++) g_Seek.threads.emplace_back(SeekWorker, base + (unsigned long long)i * 0xBF58476D1CE4E5B9ULL);
}

// The found war in Debug: exactly the searched groups active, and the seed.
static void SeekOpenInDebug(long long seed)
{
    AppState& S = g_App;
    std::set<std::string> keys;
    for (const RefKey& k : g_Seek.players) keys.insert(ToLower(k.folder) + "|" + ToLower(k.name));
    for (Player& pl : S.players)
    {
        const bool in = keys.count(SeekGroupKey(pl)) > 0;
        pl.enabled = in;
        for (Survivor& sv : pl.slots) if (sv.present) sv.enabled = in;
    }
    snprintf(S.seedText, sizeof(S.seedText), "SEED!@#=%lld", seed);
    S.showArena = true;
    GameArm(/*keepSeed=*/true);
    S.status = "Opened seed " + std::to_string(seed) + " in Debug -- only the searched groups are active now";
}

static void DrawSeedSearchWindow()
{
    AppState& S = g_App;
    // workers that ran out of reasons to run (all stopped) are joined here
    if (g_Seek.running.load() == 0 && !g_Seek.threads.empty()) SeedSearchStop();
    if (!g_Seek.open) return;
    if (!S.compSeekTool) { g_Seek.open = false; return; }

    ImGui::SetNextWindowSize(ImVec2(560 * g_Scale, 460 * g_Scale), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Seed search", &g_Seek.open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking))
    {
        ImGui::End();
        return;
    }
    const bool running = g_Seek.running.load() > 0;
    if (!HasWorkspace()) { ImGui::TextDisabled("Open a workspace first."); ImGui::End(); return; }

    ImGui::BeginDisabled(running);
    const float colW = (ImGui::GetContentRegionAvail().x - 8 * g_Scale) * 0.5f;
    const float listH = ImMax(120.0f * g_Scale, ImGui::GetContentRegionAvail().y - 150 * g_Scale);

    // who plays
    ImGui::BeginGroup();
    ImGui::PushFont(NULL, ImGui::GetFontSize() * 0.82f);
    ImGui::TextColored(Hex(Col::TextDim), "WHO PLAYS");
    ImGui::PopFont();
    ImGui::SameLine();
    if (ImGui::SmallButton("the active ones"))
    {
        g_Seek.play.clear();
        for (const Player& pl : S.players) if (pl.enabled) g_Seek.play.insert(SeekGroupKey(pl));
    }
    ImGui::BeginChild("##seekplay", ImVec2(colW, listH), ImGuiChildFlags_Borders);
    SeekPlayTree(std::string());
    ImGui::EndChild();
    ImGui::EndGroup();

    ImGui::SameLine(0, 8 * g_Scale);

    // who wins
    ImGui::BeginGroup();
    ImGui::PushFont(NULL, ImGui::GetFontSize() * 0.82f);
    ImGui::TextColored(Hex(Col::TextDim), "WHO MUST WIN");
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::Checkbox("and nobody else", &g_Seek.onlyThem);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("On: these survivors are the only ones standing at the end.\n"
                          "Off: these are standing, whoever else is too.");
    ImGui::BeginChild("##seekwin", ImVec2(colW, listH), ImGuiChildFlags_Borders);
    int warriors = 0;
    for (const Player& pl : S.players)
    {
        const std::string key = SeekGroupKey(pl);
        if (!g_Seek.play.count(key)) continue;
        for (int s = 0; s < kSlotCount; s++)
        {
            const Survivor& sv = pl.slots[s];
            if (!sv.present || sv.bin.empty()) continue;
            warriors++;
            const std::string wk = key + "|" + std::to_string(s);
            bool on = g_Seek.win.count(wk) > 0;
            ImGui::PushID(wk.c_str());
            const unsigned tint = pl.twoTone ? (s == 1 ? pl.tint2 : pl.tint) : SlotTint(pl.tint, s);
            ImGui::PushStyleColor(ImGuiCol_CheckMark, Hex(tint));
            if (ImGui::Checkbox((pl.name + std::to_string(s + 1)).c_str(), &on))
            {
                if (on) g_Seek.win.insert(wk); else g_Seek.win.erase(wk);
            }
            ImGui::PopStyleColor();
            ImGui::PopID();
        }
    }
    if (g_Seek.play.empty()) ImGui::TextDisabled("Pick groups on the left");
    ImGui::EndChild();
    ImGui::EndGroup();
    ImGui::EndDisabled();

    int zombies = 0;
    for (const Zombie& z : S.zombies) if (z.runnable()) zombies++;
    ImGui::TextColored(Hex(warriors + zombies > cw::War::MAX_WARRIORS ? Col::Warn : Col::TextDim),
                       "%d warriors + %d active zombie(s) per war, seed text and rounds as in Debug",
                       warriors, zombies);

    // run
    if (running)
    {
        if (ImGui::Button("Stop", ImVec2(110 * g_Scale, 0))) { g_Seek.stop.store(true); }
    }
    else if (ImGui::Button("Search", ImVec2(110 * g_Scale, 0))) SeedSearchStart();
    ImGui::SameLine();
    {
        const long long tried = g_Seek.tried.load();
        size_t nFound;
        { std::lock_guard<std::mutex> lk(g_Seek.lock); nFound = g_Seek.found.size(); }
        const double secs = running ? AppNow() - g_Seek.startedAt : 0.0;
        if (!g_Seek.note.empty()) ImGui::TextColored(Hex(Col::Warn), "%s", g_Seek.note.c_str());
        else if (tried > 0)
            ImGui::TextColored(Hex(running ? Col::Accent2 : Col::TextDim), "%lld wars, %d found%s",
                               tried, (int)nFound,
                               secs > 0.5 ? ("  --  " + std::to_string((int)(tried / secs)) + " wars/s").c_str() : "");
        else ImGui::TextDisabled("Random seeds, every core but one");
    }

    // what it found
    std::vector<long long> found;
    { std::lock_guard<std::mutex> lk(g_Seek.lock); found = g_Seek.found; }
    ImGui::BeginChild("##seekfound", ImVec2(0, 0), ImGuiChildFlags_Borders);
    if (found.empty()) ImGui::TextDisabled("Seeds of the wars that matched land here. Click one to open it in Debug.");
    for (size_t i = 0; i < found.size(); i++)
    {
        char lbl[48];
        snprintf(lbl, sizeof(lbl), "%lld##f%d", found[i], (int)i);
        if (ImGui::Selectable(lbl)) SeekOpenInDebug(found[i]);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click: open this war in Debug\nRight-click: copy the seed");
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
        {
            char t[48];
            snprintf(t, sizeof(t), "SEED!@#=%lld", found[i]);
            ImGui::SetClipboardText(t);
            S.status = std::string("Copied ") + t;
        }
    }
    ImGui::EndChild();
    ImGui::End();
}

static void DrawLeaderboardPanel()
{
    AppState& S = g_App;
    if (g_CompRaise) { ImGui::SetNextWindowFocus(); g_CompRaise = false; }
    if (!ImGui::Begin("Competition", NULL, ImGuiWindowFlags_NoCollapse)) { ImGui::End(); return; }

    // ---- controls ---------------------------------------------------------
    const bool joined   = (S.compMode == MODE_JOIN);
    const bool limitless = (S.compGames <= 0);
    const int total = ImMax(1, S.compGames);
    const int done  = CompetitionGamesDone();

    // The controls sit on a strip of their own, like the Debug panel's
    // transport bar. No captions: each field says what it holds inside its own
    // value ("1000 games", "auto threads"), and the engine / mode pickers name
    // themselves. The related fields are joined into trains.
    const ImVec2 barO = ImGui::GetCursorScreenPos();
    const float  barPadY = ImGui::GetStyle().WindowPadding.y;
    const float  bandBottom = barO.y + kBarH * g_Scale + barPadY;
    {
        const ImVec2 wp = ImGui::GetWindowPos();
        ImDrawList* bdl = ImGui::GetWindowDrawList();
        bdl->AddRectFilled(ImVec2(wp.x, barO.y - barPadY),
                           ImVec2(wp.x + ImGui::GetWindowWidth(), bandBottom), HexU(Col::Bg1));
        bdl->AddLine(ImVec2(wp.x, bandBottom), ImVec2(wp.x + ImGui::GetWindowWidth(), bandBottom),
                     HexU(Col::Line));
    }
    const float group  = 10.0f * g_Scale;
    const float runW   = 76.0f * g_Scale;
    const float gamesW = 112.0f * g_Scale, thrW = 104.0f * g_Scale, perW = 100.0f * g_Scale;
    const float seedW  = 150.0f * g_Scale;
    const float engW   = 104.0f * g_Scale, modeW = 88.0f * g_Scale;
    // The seed is the first thing to go when the panel is narrow; Options has
    // it too, and it can be switched off for good there.
    // the smart check's button is there only while some group uses int 0x89
    const bool  smartBtn = HasWorkspace() && AnyGroupUsesInt89();
    const float smartW   = ImGui::CalcTextSize("Smart check").x + 22.0f * g_Scale;
    const float without = runW + group + gamesW + thrW + perW + group + engW + modeW +
                          (S.compSeekTool ? group + kBarH * g_Scale : 0.0f) +
                          (smartBtn ? group + smartW : 0.0f);
    const bool  showSeed = S.compSeedInBar && ImGui::GetContentRegionAvail().x >= without + group + seedW;

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        ImVec2(10 * g_Scale, (kBarH * g_Scale - ImGui::GetFontSize()) * 0.5f));
    // the one button that matters here wears the accent; Stop wears danger
    {
        const unsigned c = S.compRunning ? Col::Danger : Col::Accent;
        const bool needsWs = !joined;
        ImGui::BeginDisabled(!S.compRunning && (joined || (needsWs && !HasWorkspace())));
        ImGui::PushStyleColor(ImGuiCol_Button, Hex(c, 0.88f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Hex(c));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, Hex(c, 0.75f));
        ImGui::PushStyleColor(ImGuiCol_Text, Hex(0xFFFFFF));
        if (S.compRunning) { if (ImGui::Button("Stop", ImVec2(runW, 0))) CompetitionStop(); }
        else               { if (ImGui::Button("Run",  ImVec2(runW, 0))) CompetitionStart(); }
        ImGui::PopStyleColor(4);
        ImGui::EndDisabled();
        if (joined && !S.compRunning && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("Joined machines start when the host presses Run");
        else if (needsWs && !HasWorkspace() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("Open a workspace first (Ctrl+O)");
        else if (g_FolderRun.on && !S.compRunning && S.compMode == MODE_LOCAL && ImGui::IsItemHovered())
            ImGui::SetTooltip("Runs the groups against the folders again (the board above the graph\n"
                              "has the switch back to an ordinary run)");
    }

    ImGui::BeginDisabled(S.compRunning);
    ImGui::SameLine(0, group);
    ImGui::BeginDisabled(joined);
    {
        TrainField tf = TrainFieldBegin(gamesW, 0, 3);
        ImGui::DragInt("##games", &S.compGames, 5.0f, 0, 2000000, S.compGames ? "%d games" : "limitless");
        TrainFieldEnd(tf);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Games - how many battles to play. 0 keeps going until you press Stop.");
    ImGui::EndDisabled();
    ImGui::SameLine(0, 0);
    {
        TrainField tf = TrainFieldBegin(thrW, 1, 3);
        ImGui::DragInt("##threads", &S.compThreads, 0.2f, 0, 128, S.compThreads ? "%d threads" : "auto threads");
        TrainFieldEnd(tf);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Threads - 0 spreads the run over every core the machine has (%d here)",
                          (int)std::thread::hardware_concurrency());
    ImGui::SameLine(0, 0);
    ImGui::BeginDisabled(joined);
    {
        TrainField tf = TrainFieldBegin(perW, 2, 3);
        ImGui::DragInt("##perbattle", &S.compCombo, 0.1f, 0, 20, S.compCombo ? "%d per war" : "all per war");
        TrainFieldEnd(tf);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Groups per battle - how many are drawn into each war.\n"
                          "The C++ engine fits about 15 groups of two in one war (the\n"
                          "arena runs out of room after that); Java and Busy hold 20\n"
                          "warriors, about nine groups, and leave bigger battles to C++.\n"
                          "Draw a few per battle and let the seeds cover the\n"
                          "combinations. 0 puts everyone in.");
    if (showSeed)
    {
        // a seed is just a number or a word, so this one keeps a small tag
        ImGui::SameLine(0, group);
        const ImVec2 sp = ImGui::GetCursorScreenPos();
        ImGui::PushFont(NULL, ImGui::GetFontSize() * 0.82f);
        const ImVec2 tagSz = ImGui::CalcTextSize("SEED");
        ImGui::PopFont();
        const float tagW = tagSz.x + 18 * g_Scale;
        {
            // the tag is the train's first carriage, the field its second
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(sp, ImVec2(sp.x + tagW, sp.y + kBarH * g_Scale), HexU(Col::Bg3, 0.7f),
                              7.0f * g_Scale, ImDrawFlags_RoundCornersLeft);
            ImGui::PushFont(NULL, ImGui::GetFontSize() * 0.82f);
            dl->AddText(ImVec2(sp.x + 9 * g_Scale, sp.y + (kBarH * g_Scale - tagSz.y) * 0.5f),
                        HexU(Col::TextDim), "SEED");
            ImGui::PopFont();
            ImGui::Dummy(ImVec2(tagW, kBarH * g_Scale));
        }
        ImGui::SameLine(0, 0);
        TrainField tf = TrainFieldBegin(seedW - tagW, 1, 2);
        if (joined)
        {
            // the host decides the seed; show it rather than a field that does nothing
            char sv[32];
            snprintf(sv, sizeof(sv), "%lld", (long long)g_NetClient.field.seed);
            ImGui::InputText("##compseedro", sv, sizeof(sv), ImGuiInputTextFlags_ReadOnly);
        }
        else
            ImGui::InputTextWithHint("##compseed", S.seedText[0] ? S.seedText : "Debug's",
                                     S.compSeedText, sizeof(S.compSeedText));
        TrainFieldEnd(tf);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("The seed the first war is fought from; war k uses it + k.\n"
                              "Text or a number, resolved the way the reference does.\n"
                              "Empty: the Debug panel's seed.  -> %lld\n\n"
                              "Hidden when the panel is narrow; always in Options > Competition.",
                              (long long)SeedFromText(CompSeedText()));
    }
    ImGui::EndDisabled();
    ImGui::SameLine(0, group);
    {
        // C++ and Java always; Busy only once Options has allowed it
        const char* names[3] = { "C++", "Java", "Busy (C++)" };
        const int   ids[3]   = { COMP_CPU, COMP_JAVA, COMP_BOTH };
        const int   n = S.compAllowBusy ? 3 : 2;
        int cur = 0;
        for (int i = 0; i < n; i++) if (ids[i] == S.compBackend) cur = i;
        TrainField tf = TrainFieldBegin(engW, 0, 2);
        if (ImGui::Combo("##engine", &cur, names, n)) S.compBackend = ids[cur];
        TrainFieldEnd(tf);
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("C++   this app's own engine, on every core.\n"
                          "Java  the same battles on the CGX Java engine\n"
                          "      (CGX-engine\\engine). Needs Java 17 or newer.%s",
                          S.compAllowBusy ? "\nBusy  C++ and the graphics card together, half the battles\n"
                                            "      each -- runs the machine hot (see Options)."
                                          : "\n\nOptions > Competition can add Busy (C++): the C++ engine\n"
                                            "and the graphics card together.");
    ImGui::SameLine(0, 0);
    static const char* kModes[] = { "Local", "Server", "Join" };
    static const int   kModeIds[] = { MODE_LOCAL, MODE_SERVER, MODE_JOIN };
    bool modeChanged = false;
    {
        int cur = 0;
        for (int i = 0; i < 3; i++) if (kModeIds[i] == S.compMode) cur = i;
        TrainField tf = TrainFieldBegin(modeW, 1, 2);
        modeChanged = ImGui::Combo("##mode", &cur, kModes, 3);
        TrainFieldEnd(tf);
        S.compMode = kModeIds[cur];
    }
    if (modeChanged)
    {
        // leaving a mode drops its wire; the board it built stays
        if (S.compMode != MODE_SERVER) NetServerStop();
        if (S.compMode != MODE_JOIN)   NetClientDisconnect();
        g_CompNetNote.clear();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Local    this machine only.\n"
                          "Server   host: other machines join over the network and play\n"
                          "         their share the moment you press Run.\n"
                          "Join     play a share of a tournament somebody else is hosting.\n"
                          "\n"
                          "Every machine plays its own phase of the game index, so the\n"
                          "battles never overlap and the standings are a plain sum.");
    ImGui::EndDisabled();

    // the seed search, when Options puts it on the toolbar
    if (S.compSeekTool)
    {
        ImGui::SameLine(0, group);
        const float bs = kBarH * g_Scale;
        BtnState b = BareButton("##seek", ImVec2(bs, bs));
        ImDrawList* bdl = ImGui::GetWindowDrawList();
        bdl->AddRectFilled(b.mn, b.mx, HexU(g_Seek.open ? Col::Bg4 : b.hovered ? Col::Bg3 : Col::Bg2), 7 * g_Scale);
        const ImVec2 c((b.mn.x + b.mx.x) * 0.5f - 1.5f * g_Scale, (b.mn.y + b.mx.y) * 0.5f - 1.5f * g_Scale);
        const ImU32 ink = HexU(g_Seek.running.load() > 0 ? Col::Accent : (b.hovered ? Col::Text : Col::TextDim));
        bdl->AddCircle(c, 5.0f * g_Scale, ink, 0, 1.6f * g_Scale);
        bdl->AddLine(ImVec2(c.x + 3.6f * g_Scale, c.y + 3.6f * g_Scale), ImVec2(c.x + 7.5f * g_Scale, c.y + 7.5f * g_Scale),
                     ink, 2.0f * g_Scale);
        if (b.hovered) ImGui::SetTooltip("Seed search\nPlays random wars between the groups you pick and keeps\n"
                                         "the seeds of the ones the survivors you pick won");
        if (b.clicked) g_Seek.open = !g_Seek.open;
    }
    // the smart check: which AX:DX an int 0x89 should look for
    if (smartBtn)
    {
        ImGui::SameLine(0, group);
        ImGui::PushStyleColor(ImGuiCol_Button, Hex(SmartCheckIsOpen() ? Col::Bg4 : Col::Bg2));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Hex(Col::Bg3));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, Hex(Col::Bg4));
        ImGui::PushStyleColor(ImGuiCol_Text, Hex(SmartCheckIsRunning() ? Col::Accent : Col::Text));
        if (ImGui::Button("Smart check", ImVec2(smartW, kBarH * g_Scale))) SmartCheckToggle();
        ImGui::PopStyleColor(4);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Smart check\nA group's int 0x89 is INT 87h in a real war. This plays it against the\n"
                              "active groups once for every AX:DX it could search for -- every 4 bytes\n"
                              "of the other warriors -- and ranks them by the points it takes.");
    }
    ImGui::PopStyleVar();   // the toolbar's frame padding

    // out of the band, onto the panel proper
    ImGui::SetCursorScreenPos(ImVec2(barO.x, bandBottom + barPadY));

    if (!S.compGpuNote.empty())
    {
        ImGui::TextColored(Hex(Col::Warn), "%s", S.compGpuNote.c_str());
    }
    else if (g_Gpu.result.gamesRun > 0 || g_Gpu.running)
    {
        const double gr = g_Gpu.seconds > 0.0 ? g_Gpu.gamesRun / g_Gpu.seconds : 0.0;
        if (g_Gpu.gamesRun > 0)
            ImGui::TextColored(Hex(Col::Accent), "GPU %d war(s), %.0f games/s", g_Gpu.gamesRun, gr);
        else if (g_Gpu.passes > 0)
            // nothing finished yet, but the card is working: a war has to run to
            // its end before the lane has a score to report
            ImGui::TextColored(Hex(Col::Accent, 0.8f),
                               "GPU: pass %u, %u wars in flight, %u rounds/dispatch, %.0fs",
                               g_Gpu.passes, g_Gpu.inFlight, g_Gpu.budget, g_Gpu.seconds);
        else
            ImGui::TextColored(Hex(Col::Accent, 0.8f), "GPU: %s", g_Gpu.lastLine.c_str());
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s\n\nlog: %s", g_Gpu.lastLine.c_str(), g_Gpu.logPath.c_str());
    }

    if (S.compRunning)
    {
        // its own line: the controls row is full
        ImGui::SetNextItemWidth(-FLT_MIN);
        char ov[96];
        const double rate = S.compResult.gamesPerSecond();
        if (limitless || joined)
        {
            if (rate > 0.0) snprintf(ov, sizeof(ov), "%d so far   %.0f games/s", done, rate);
            else            snprintf(ov, sizeof(ov), "%d so far", done);
            // an indeterminate bar: a band sweeping back and forth
            const float t = (float)fmod(ImGui::GetTime() * 0.6, 2.0);
            const float f = t < 1.0f ? t : 2.0f - t;
            ImGui::ProgressBar(-1.0f * f, ImVec2(-FLT_MIN, 0), ov);
        }
        else
        {
            if (rate > 0.0)
                snprintf(ov, sizeof(ov), "%d / %d   %.0f games/s   %.0fs left",
                         done, total, rate, ImMax(0.0, (total - done) / rate));
            else snprintf(ov, sizeof(ov), "%d / %d", done, total);
            ImGui::ProgressBar(ImMin(1.0f, (float)done / (float)total), ImVec2(-FLT_MIN, 0), ov);
        }
    }

    // ---- the distributed row ------------------------------------------------
    if (S.compMode == MODE_SERVER)
    {
        const bool up = NetServerUp();
        if (!up && !CloudflaredPresent())
        {
            // no tunnel program, no server: say what to get and where it goes
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(Hex(Col::Warn), "Server mode needs cloudflared.exe next to app.exe.");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("cloudflared is Cloudflare's free tunnel program -- no account needed.\n"
                                  "It gives this machine a public https address for the tournament, so\n"
                                  "others join from anywhere: no router setup, and it works on campus Wi-Fi.\n\n"
                                  "Download cloudflared-windows-amd64.exe, rename it to cloudflared.exe\n"
                                  "and put it in this folder:\n  %s", ExeDir().c_str());
            ImGui::SameLine(0, 8 * g_Scale);
            if (ImGui::SmallButton("Get cloudflared..."))
                ShellExecuteA(g_Hwnd, "open", kCloudflaredPage, NULL, NULL, SW_SHOWNORMAL);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Opens Cloudflare's download page in your browser");
            ImGui::SameLine(0, 4 * g_Scale);
            if (ImGui::SmallButton("Open app folder"))
                ShellExecuteA(g_Hwnd, "open", ExeDir().c_str(), NULL, NULL, SW_SHOWNORMAL);
        }
        else
        {
            if (up)
            {
                ImGui::BeginDisabled(S.compRunning);
                if (ImGui::Button("Stop listening")) NetServerStop();
                ImGui::EndDisabled();
            }
            else if (ImGui::Button("Listen"))
            {
                std::string why;
                if (NetServerStart(S.compNetPort, why)) g_CompNetNote.clear();
                else g_CompNetNote = why;
            }
            if (!up && ImGui::IsItemHovered())
                ImGui::SetTooltip("Starts the tunnel and shows the address joiners paste. Machines that join\n"
                                  "sit in a lobby until you press Run; one that joins mid-run is handed a\n"
                                  "share straight away.");
            // who may join: set in Options, shown here
            {
                const int who = up ? g_NetServer.protect : ImClamp(S.compNetProtect, 0, 2);
                static const char* kWho[] = { "anyone may join", "password-protected", "you accept each joiner" };
                ImGui::SameLine(0, 12 * g_Scale);
                ImGui::TextColored(Hex(who == NET_OPEN ? Col::TextDim : Col::Accent2), "%s", kWho[who]);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Who may join is set under Options > Competition > Shared tournaments%s",
                                      up ? "\n(a change there counts from the next Listen)" : "");
                if (!up && who == NET_PASSWORD && !S.compNetPassword[0])
                {
                    ImGui::SameLine(0, 8 * g_Scale);
                    ImGui::TextColored(Hex(Col::Warn), "-- no password set yet");
                }
            }

            if (up)
            {
                // the address, or where the tunnel is
                const std::string url = NetTunnelUrl();
                if (!url.empty())
                {
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextColored(Hex(Col::TextDim), "joiners paste");
                    ImGui::SameLine();
                    ImGui::TextColored(Hex(Col::Accent), "%s", url.c_str());
                    ImGui::SameLine(0, 6 * g_Scale);
                    if (ImGui::SmallButton("Copy"))
                    {
                        ImGui::SetClipboardText(url.c_str());
                        S.status = "Copied the tournament's address";
                    }
                }
                else if (g_Tunnel.running.load())
                {
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextColored(Hex(Col::TextDim), "starting the tunnel... %.0fs", AppNow() - g_Tunnel.startedAt);
                }
                else
                {
                    std::string last;
                    { std::lock_guard<std::mutex> lk(g_Tunnel.mu); last = g_Tunnel.lastLine; }
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextColored(Hex(Col::Warn), "the tunnel stopped%s%s", last.empty() ? "" : ": ", last.c_str());
                    ImGui::SameLine(0, 6 * g_Scale);
                    if (ImGui::SmallButton("Restart tunnel"))
                    {
                        std::string why;
                        if (!NetTunnelStart(g_NetServer.port, why)) g_CompNetNote = why;
                    }
                }

                // who is in
                int in = 0, playing = 0;
                for (const NetPeer& p : g_NetServer.peers)
                    if (p.admitted) { in++; if (p.running) playing++; }
                ImGui::SameLine(0, 14 * g_Scale);
                if (in == 0)
                    ImGui::TextColored(Hex(Col::TextDim), "nobody has joined yet");
                else
                {
                    ImGui::TextColored(Hex(Col::Accent2), "%d joined%s", in,
                                       playing ? (", " + std::to_string(playing) + " playing").c_str() : "");
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::BeginTooltip();
                        for (const NetPeer& p : g_NetServer.peers)
                        {
                            if (!p.admitted) continue;
                            if (p.phase < 0)
                                ImGui::Text("%s  (%s)  -- no seat left", p.name.c_str(), p.addr.c_str());
                            else if (p.running || p.gamesRun)
                                ImGui::Text("%s  (%s)  phase %d  --  %d games, %.0f games/s%s",
                                            p.name.c_str(), p.addr.c_str(), p.phase, p.gamesRun, p.rate,
                                            p.running ? "" : ", done");
                            else
                                ImGui::Text("%s  (%s)  phase %d  --  waiting", p.name.c_str(), p.addr.c_str(), p.phase);
                        }
                        ImGui::EndTooltip();
                    }
                }

                // "ask me": the ones at the door
                for (size_t i = 0; i < g_NetServer.peers.size(); i++)
                {
                    NetPeer& p = g_NetServer.peers[i];
                    if (!p.pending) continue;
                    ImGui::PushID((int)i);
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextColored(Hex(Col::Accent), "%s  (%s)  wants to join", p.name.c_str(), p.addr.c_str());
                    ImGui::SameLine(0, 8 * g_Scale);
                    if (ImGui::SmallButton("Accept")) NetServerAdmit(p);
                    ImGui::SameLine(0, 4 * g_Scale);
                    if (ImGui::SmallButton("Deny")) NetServerTurnAway(p, "the host turned you away");
                    ImGui::PopID();
                }
            }
        }
        if (!up && !g_NetServer.note.empty())
        {
            ImGui::SameLine(0, 12 * g_Scale);
            ImGui::TextColored(Hex(Col::Warn), "%s", g_NetServer.note.c_str());
        }
    }
    else if (S.compMode == MODE_JOIN)
    {
        const bool linked = g_NetClient.state == NET_CONNECTING || g_NetClient.state == NET_WAITING ||
                            g_NetClient.state == NET_RUNNING;
        ImGui::BeginDisabled(linked);
        ImGui::SetNextItemWidth(300 * g_Scale);
        ImGui::InputTextWithHint("##joinaddr", "https://....trycloudflare.com", S.compNetHost, sizeof(S.compNetHost));
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("The address on the host's panel -- paste it here.\n"
                              "(host:port reaches a host on this same machine.)");
        ImGui::SameLine(0, 6 * g_Scale);
        ImGui::SetNextItemWidth(110 * g_Scale);
        ImGui::InputTextWithHint("##joinpw", "password", S.compNetPassword, sizeof(S.compNetPassword),
                                 ImGuiInputTextFlags_Password);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Only if the host set one.");
        ImGui::EndDisabled();
        ImGui::SameLine(0, 6 * g_Scale);
        if (linked)
        {
            if (ImGui::Button(g_NetClient.state == NET_CONNECTING ? "Cancel" : "Disconnect"))
            {
                CompetitionStop();
                NetClientDisconnect();
            }
        }
        else if (ImGui::Button("Connect"))
        {
            std::string why;
            if (!NetClientConnect(S.compNetHost, why)) { g_NetClient.state = NET_FAILED; g_NetClient.note = why; }
        }
        ImGui::SameLine(0, 12 * g_Scale);
        const unsigned tone = g_NetClient.state == NET_FAILED ? Col::Warn
                            : g_NetClient.state == NET_RUNNING ? Col::Accent : Col::TextDim;
        if (S.compRunning && g_NetClient.state == NET_RUNNING)
            ImGui::TextColored(Hex(tone), "playing phase %d of %d  --  %d games from here, %.0f games/s",
                               g_CompPhase, g_CompStride, g_CompLocal.gamesRun, g_CompLocal.gamesPerSecond());
        else
            ImGui::TextColored(Hex(tone), "%s", g_NetClient.note.empty() ? "not connected" : g_NetClient.note.c_str());
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Playing as \"%s\" -- the name is under Options > Competition.", S.compNetName);
        // Why the last try failed, on its own wrapped line so the window edge
        // cannot cut it off -- this is the part that says what to fix.
        if (g_NetClient.state == NET_CONNECTING && !g_NetClient.lastWhy.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, Hex(Col::Warn));
            ImGui::TextWrapped("last try: %s", g_NetClient.lastWhy.c_str());
            ImGui::PopStyleColor();
        }
    }
    if (!g_CompNetNote.empty())
    {
        ImGui::SameLine(0, 12 * g_Scale);
        ImGui::TextColored(Hex(Col::Warn), "%s", g_CompNetNote.c_str());
    }

    ImGui::Separator();

    // A joiner watches the host's merged board once it has one; its own share
    // is on the row above.
    const bool hostBoard = joined && !g_NetClient.board.empty() && g_NetClient.boardAt > 0.0;
    const cw::CompeteResult& R = hostBoard ? g_NetClient.board : S.compResult;
    if (R.empty())
    {
        if (joined)
            ImGui::TextColored(Hex(Col::TextDim),
                "Connect to a host. The field, the seed and this machine's share all come from there.");
        else
            ImGui::TextColored(Hex(Col::TextDim),
                HasWorkspace()
                    ? "Make some groups active and press Run. Each war is played from its own seed and "
                      "every survivor still standing at the end splits a point."
                    : "Open a workspace first (Ctrl+O).");
        ImGui::End();
        return;
    }

    // ---- what is being ranked --------------------------------------------
    ImGui::RadioButton("Groups", &S.compByGroup, 1);
    ImGui::SameLine();
    ImGui::RadioButton("Survivors", &S.compByGroup, 0);
    {
        // games/s only means something against the size of a battle: every
        // extra warrior -- zombies included -- is more opcodes per war
        int drawn = 0, zomb = 0, perGroup = 0;
        for (const cw::WarriorGroup& g : g_CompGroups)
        {
            if (cw::isZombieGroup(g)) zomb += (int)g.warriors.size();
            else { drawn++; perGroup = ImMax(perGroup, (int)g.warriors.size()); }
        }
        const int combo = ImMax(0, g_CompComboUsed);
        const int inBattle = (combo > 0 ? ImMin(combo, drawn) : drawn);
        ImGui::SameLine(0, 16 * g_Scale);
        ImGui::TextColored(Hex(Col::TextDim), "up to %d warriors/battle%s, %d thread(s)",
                           inBattle * perGroup + zomb,
                           zomb ? (" incl. " + std::to_string(zomb) + " zombie(s)").c_str() : "",
                           S.compThreads ? S.compThreads : (int)std::thread::hardware_concurrency());
    }
    ImGui::SameLine(0, 16 * g_Scale);
    const int others = (int)g_NetPacks.size();
    if (hostBoard)
        ImGui::TextColored(Hex(S.compRunning ? Col::Accent2 : Col::TextDim),
                           "host's board: %d war(s) %s in %.0fs  --  %.0f games/s",
                           R.gamesRun, S.compRunning ? "so far" : "scored", R.seconds, R.gamesPerSecond());
    else if (R.seconds > 0.0)
        ImGui::TextColored(Hex(S.compRunning ? Col::Accent2 : Col::TextDim),
                           "%d war(s) %s in %.2fs  --  %.0f games/s%s%s",
                           R.gamesRun, S.compRunning ? "so far" : "scored",
                           R.seconds, R.gamesPerSecond(),
                           R.gamesFailed ? "  (some could not be placed)" : "",
                           others > 0 ? ("  incl. " + std::to_string(others) + " other machine(s)").c_str() : "");
    else
        ImGui::TextColored(Hex(Col::TextDim), "%d war(s) scored%s",
                           R.gamesRun, R.gamesFailed ? "  (some could not be placed)" : "");

    if (!cw::engineOptimized())
    {
        ImGui::SameLine(0, 16 * g_Scale);
        ImGui::TextColored(Hex(Col::Warn), "engine built without -O2: about 4x slower than it should be");
    }

    // ---- a run against folders: a page per folder ---------------------------
    const bool fields = !g_CompFieldNames.empty() && !hostBoard &&
                        g_CompFieldOf.size() == R.groupName.size();
    if (fields)
    {
        if (g_CompFieldView >= (int)g_CompFieldNames.size()) g_CompFieldView = 0;
        S.compFieldShow = ImClamp(S.compFieldShow, 0, 2);
        // how the picked group(s) did in each folder: their share of its points,
        // how many groups played there, and their places on its board
        const size_t nf = g_CompFieldNames.size();
        std::vector<double> subjShare(nf, 0.0), total(nf, 0.0);
        std::vector<int> players(nf, 0), subjIn(nf, 0);
        // by identity: where a folder holds a group of the same name, both fight
        // under "Name [folder]" and the board's name is not the subject's any more
        const bool keyed = g_CompGroups.size() == R.groupName.size() && !g_CompFieldSubjectKeys.empty();
        auto isSubject = [&](size_t g) {
            if (keyed)
                return std::find(g_CompFieldSubjectKeys.begin(), g_CompFieldSubjectKeys.end(), g_CompGroups[g].key) != g_CompFieldSubjectKeys.end();
            return std::find(g_CompFieldSubjects.begin(), g_CompFieldSubjects.end(), R.groupName[g]) != g_CompFieldSubjects.end();
        };
        for (size_t g = 0; g < R.groupName.size(); g++)
        {
            const int f = g_CompFieldOf[g];
            if (f < 0 || f >= (int)nf) continue;
            total[(size_t)f] += R.groupScore[g];
            players[(size_t)f]++;
            if (isSubject(g)) { subjShare[(size_t)f] += R.groupScore[g]; subjIn[(size_t)f]++; }
        }
        // a group's place in its folder: one more than the groups that beat it
        auto placeOf = [&](size_t g) {
            int place = 1;
            for (size_t h = 0; h < R.groupName.size(); h++)
                if (h != g && g_CompFieldOf[h] == g_CompFieldOf[g] && R.groupScore[h] > R.groupScore[g]) place++;
            return place;
        };

        // the run's own controls: what each folder button shows, and the way out
        ImGui::TextColored(Hex(Col::TextDim), "Against folders, show:");
        ImGui::SameLine();
        ImGui::RadioButton("Share", &S.compFieldShow, 0);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("The picked group(s)' share of the folder's points");
        ImGui::SameLine();
        ImGui::RadioButton("vs average", &S.compFieldShow, 1);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("The share against an even split: the share times the number of groups in the folder.\n"
                              "100%% is exactly average, 200%% twice the average group's points.");
        ImGui::SameLine();
        ImGui::RadioButton("Place", &S.compFieldShow, 2);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Where the picked group(s) finished on the folder's board, of how many");
        ImGui::SameLine(0, 16 * g_Scale);
        ImGui::BeginDisabled(S.compRunning);
        if (ImGui::SmallButton("Ordinary run"))
        {
            g_FolderRun.on = false;
            g_CompFieldNames.clear();
            g_CompFieldOf.clear();
            S.status = "Run plays the active groups again";
        }
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("Leave the run against folders: Run goes back to the active groups");

        // the folders, flowing onto as many lines as they need
        const float right = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
        const ImGuiStyle& st = ImGui::GetStyle();
        if (!g_CompFieldNames.empty()) ImGui::Spacing();
        for (size_t f = 0; f < nf; f++)
        {
            const double pct = total[f] > 0.0 ? subjShare[f] / total[f] * 100.0 : 0.0;
            // against an even split: each subject's share over 1/players
            const double rel = (subjIn[f] > 0 && players[f] > 0) ? pct * players[f] / subjIn[f] : 0.0;
            std::string places;
            for (size_t g = 0; g < R.groupName.size(); g++)
                if (g_CompFieldOf[g] == (int)f && isSubject(g))
                    places += (places.empty() ? "#" : ", #") + std::to_string(placeOf(g));
            char val[96];
            if (S.compFieldShow == 1)      snprintf(val, sizeof(val), "%.0f%%", rel);
            else if (S.compFieldShow == 2) snprintf(val, sizeof(val), "%s/%d", places.empty() ? "-" : places.c_str(), players[f]);
            else                           snprintf(val, sizeof(val), "%.1f%%", pct);
            char lbl[200];
            snprintf(lbl, sizeof(lbl), "%s  %s##field%d", FolderLeaf(g_CompFieldNames[f]).c_str(), val, (int)f);

            if (f > 0)
            {
                const float w = ImGui::CalcTextSize(lbl, nullptr, true).x + st.FramePadding.x * 2.0f;
                if (ImGui::GetItemRectMax().x + st.ItemSpacing.x + w <= right) ImGui::SameLine();
            }
            const bool on = (g_CompFieldView == (int)f);
            ImGui::PushStyleColor(ImGuiCol_Button, Hex(on ? Col::Accent : Col::Bg2, on ? 0.85f : 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Hex(on ? Col::Accent : Col::Bg3));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, Hex(on ? Col::Accent : Col::Bg4, on ? 0.75f : 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, Hex(on ? 0xFFFFFF : Col::Text));
            if (ImGui::SmallButton(lbl)) g_CompFieldView = (int)f;
            ImGui::PopStyleColor(4);
            if (ImGui::IsItemHovered())
            {
                std::string who;
                for (const std::string& s : g_CompFieldSubjects) who += (who.empty() ? "" : ", ") + s;
                ImGui::SetTooltip("%s\n%s: %.1f%% of the points (%.0f%% of average), place %s of %d groups",
                                  g_CompFieldNames[f].c_str(), who.c_str(), pct, rel,
                                  places.empty() ? "-" : places.c_str(), players[f]);
            }
        }
    }
    auto inView = [&](int group) {
        if (!fields) return true;
        return group >= 0 && group < (int)g_CompFieldOf.size() && g_CompFieldOf[(size_t)group] == g_CompFieldView;
    };

    std::vector<CompRow> rows;
    if (S.compByGroup)
    {
        for (size_t i = 0; i < R.groupName.size(); i++)
            if (inView((int)i))
                rows.push_back({ R.groupName[i], R.groupScore[i], R.groupSurvived[i],
                                 CompColorOf(R, i, true) });
    }
    else
        for (size_t i = 0; i < R.warriorName.size(); i++)
            if (inView(R.warriorGroup[i]))
                rows.push_back({ R.warriorName[i], R.warriorScore[i], R.warriorSurvived[i],
                                 CompColorOf(R, i, false) });

    std::sort(rows.begin(), rows.end(),
              [](const CompRow& a, const CompRow& b) { return a.score > b.score; });

    double best = 0.0, sum = 0.0;
    for (const CompRow& r : rows) { best = ImMax(best, r.score); sum += r.score; }
    if (best <= 0.0) best = 1.0;

    // ---- bar graph, table beside it ---------------------------------------
    const float tableW = ImMin(300.0f * g_Scale, ImGui::GetContentRegionAvail().x * 0.42f);
    const float graphW = ImMax(140.0f, ImGui::GetContentRegionAvail().x - tableW - 8 * g_Scale);

    ImGui::BeginChild("##compgraphbox", ImVec2(graphW, 0), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    DrawCompBars(rows, best, R.gamesRun, S.compBarsUp, S.compBarsFit, S.compBarLabels);
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("##comptable", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
    if (ImGui::BeginTable("##board", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                                        ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 26 * g_Scale);
        ImGui::TableSetupColumn(S.compByGroup ? "Group" : "Survivor");
        ImGui::TableSetupColumn("Score", ImGuiTableColumnFlags_WidthFixed, 58 * g_Scale);
        // in a run against folders, "vs average" turns the share into a ratio
        // to an even split of the page's points
        const bool vsAvg = fields && S.compFieldShow == 1;
        ImGui::TableSetupColumn(vsAvg ? "vs avg" : "Share", ImGuiTableColumnFlags_WidthFixed, 52 * g_Scale);
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < rows.size(); i++)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextColored(Hex(Col::TextDim), "%d", (int)i + 1);
            ImGui::TableNextColumn();
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(HexU(rows[i].tint)), "%s",
                               rows[i].name.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%.2f", rows[i].score);
            ImGui::TableNextColumn();
            const double share = sum > 0.0 ? rows[i].score / sum * 100.0 : 0.0;
            if (vsAvg) ImGui::TextColored(Hex(Col::TextDim), "%.0f%%", share * (double)rows.size());
            else       ImGui::TextColored(Hex(Col::TextDim), "%.1f%%", share);
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();
    ImGui::End();
}
