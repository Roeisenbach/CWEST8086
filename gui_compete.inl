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

// Offline mode: the shard file this machine is playing, if one is open.
static cw::FieldPack                  g_CompShard;
static bool                           g_CompShardOpen = false;
static std::string                    g_CompShardPath;
static std::string                    g_CompNetNote;        // last import / export / listen outcome
static bool                           g_CompRaise = false;  // bring the panel's tab to the front once

enum { COMP_CPU = 0, COMP_GPU = 1, COMP_BOTH = 2, COMP_JAVA = 3 };
enum { MODE_LOCAL = 0, MODE_OFFLINE = 1, MODE_SERVER = 2, MODE_JOIN = 3 };

// BrowseForFolder lives in gui_options.inl, which main.cpp includes after this
static std::string BrowseForFolder(HWND owner, const char* title);

// ---- file dialogs ------------------------------------------------------------

static std::string SaveFileDialog(HWND owner, const char* filter, const char* title,
                                  const char* defExt, const std::string& suggested)
{
    char buf[MAX_PATH];
    snprintf(buf, sizeof(buf), "%s", suggested.c_str());
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = owner;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile   = buf;
    ofn.nMaxFile    = sizeof(buf);
    ofn.lpstrTitle  = title;
    ofn.lpstrDefExt = defExt;
    ofn.Flags       = OFN_EXPLORER | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetSaveFileNameA(&ofn)) return std::string();
    return buf;
}

static std::vector<std::string> OpenFilesDialog(HWND owner, const char* filter, const char* title, bool multi)
{
    std::vector<std::string> result;
    std::vector<char> buffer(64 * 1024, 0);
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = owner;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile   = buffer.data();
    ofn.nMaxFile    = (DWORD)buffer.size();
    ofn.lpstrTitle  = title;
    ofn.Flags       = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR |
                      OFN_HIDEREADONLY | (multi ? OFN_ALLOWMULTISELECT : 0);
    if (!GetOpenFileNameA(&ofn)) return result;
    const char* p = buffer.data();
    std::string first(p);
    p += first.size() + 1;
    if (*p == 0) { result.push_back(first); return result; }
    std::string dir = first;
    if (!dir.empty() && dir[dir.size() - 1] != '\\') dir += '\\';
    while (*p) { result.push_back(dir + p); p += strlen(p) + 1; }
    return result;
}

static bool ReadWholeFile(const std::string& path, std::string& out)
{
    std::ifstream f(path.c_str(), std::ios::binary);
    if (!f) return false;
    out.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return true;
}

static bool WriteWholeFile(const std::string& path, const std::string& text)
{
    std::ofstream f(path.c_str(), std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(text.data(), (std::streamsize)text.size());
    return (bool)f;
}

static std::string FileStem(const std::string& path)
{
    const size_t slash = path.find_last_of("\\/");
    std::string n = slash == std::string::npos ? path : path.substr(slash + 1);
    const size_t dot = n.find_last_of('.');
    return dot == std::string::npos ? n : n.substr(0, dot);
}

// ---- options ------------------------------------------------------------------

static void CompetitionOptionsFromApp(cw::CompeteOptions& opt)
{
    opt.firstSeed   = SeedFromText(g_App.seedText);
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
    f.seed        = SeedFromText(g_App.seedText);
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
        g_CompStopAt = ImGui::GetTime();
    }
}

// Offline mode only: a result file carrying this machine's own name is its
// saved run coming back in a merge, and stands in for the lanes rather than
// doubling them. (A joiner that happens to share the host's name is renamed by
// the host, so the rule never fires on the wire.)
static bool CompetitionLanesSuperseded()
{
    return g_App.compMode == MODE_OFFLINE && g_NetPacks.count(g_CompSender) > 0;
}

// Games finished so far, every lane and every other machine counted.
static int CompetitionGamesDone()
{
    int done = 0;
    if (!CompetitionLanesSuperseded())
        done += g_App.compProg.done.load() + g_Gpu.gamesRun + g_Java.gamesRun;
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

    // Packs from elsewhere, on top of the lanes (or instead of them, when the
    // lanes' own result file has been added back in an offline merge).
    cw::CompeteResult r = CompetitionLanesSuperseded() ? g_CompIdentity : local;
    for (const auto& kv : g_NetPacks)
    {
        cw::addResultPack(r, kv.second);
        if (kv.second.seconds > r.seconds) r.seconds = kv.second.seconds;
    }
    if (g_App.compRunning && g_CompStartAt > 0.0)
        r.seconds = ImMax(r.seconds, ImGui::GetTime() - g_CompStartAt);
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
    const bool split = (g_App.compBackend == COMP_BOTH);
    int cpuGames = openEnded ? 0 : target, gpuGames = openEnded ? 0 : target;
    int cpuStride = stride, cpuPhase = phase, gpuStride = stride, gpuPhase = phase;
    if (split)
    {
        cpuStride = gpuStride = stride * 2;
        gpuPhase  = phase + stride;
        cpuGames = gpuGames = 0;
    }

    g_App.compGpuNote.clear();
    bool gpuUp = false;
    if (g_App.compBackend == COMP_GPU || g_App.compBackend == COMP_BOTH)
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
    if (g_App.compBackend == COMP_JAVA)
    {
        cw::CompeteOptions j = opt;
        j.seedStride = stride;
        j.seedPhase  = phase;
        std::string why;
        javaUp = JavaLaneStart(g_CompGroups, j, openEnded ? 0 : target, why);
        if (!javaUp) g_App.compGpuNote = "Java: " + why;
    }

    const bool cpuLane = (g_App.compBackend == COMP_CPU || g_App.compBackend == COMP_BOTH)
                      || (g_App.compBackend == COMP_GPU && !gpuUp)
                      || (g_App.compBackend == COMP_JAVA && !javaUp);

    g_App.compProg.done.store(0);
    g_App.compProg.cancel.store(false);
    g_CompDone.store(true);                    // no CPU lane unless one starts
    g_CompSeenRev = -1;
    g_CompPacksSeen = -1;
    g_CompStopping = false;
    g_CompStartAt = ImGui::GetTime();
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

// The Run button.
static void CompetitionStart()
{
    if (g_App.compRunning) return;
    AppState& S = g_App;

    cw::CompeteOptions opt;
    switch (S.compMode)
    {
    case MODE_OFFLINE:
        if (g_CompShardOpen)
        {
            CompetitionOptionsFromField(g_CompShard, opt);
            char who[64];
            snprintf(who, sizeof(who), " shard %d/%d", g_CompShard.phase + 1, g_CompShard.stride);
            g_CompSender = NetComputerName() + who;
            CompetitionLaunch(g_CompShard.groups, opt, g_CompShard.stride, g_CompShard.phase,
                              g_CompShard.games, false);
            return;
        }
        // no shard open: a plain run of the workspace, added to whatever has
        // been merged so far
        CompetitionOptionsFromApp(opt);
        g_CompSender = NetComputerName();
        CompetitionLaunch(BuildGroups(), opt, 1, 0, S.compGames, false);
        return;

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
    const double now = ImGui::GetTime();

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
        // packs added by hand in offline mode change the board without a run
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
        if (g_NetClient.s != INVALID_SOCKET)
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

// ---- offline: shard and result files --------------------------------------------

static void CompetitionExportShards()
{
    AppState& S = g_App;
    std::vector<cw::WarriorGroup> groups = BuildGroups();
    if (groups.empty()) { S.status = "Need at least one group with an assembled survivor"; return; }
    const int n = ImClamp(S.compShards, 1, kNetStride);

    const std::string dir = BrowseForFolder(g_Hwnd, "Folder for the shard files (one per machine)");
    if (dir.empty()) return;

    cw::FieldPack f = CompetitionFieldFromApp(groups);
    f.stride = n;
    int written = 0;
    for (int p = 0; p < n; p++)
    {
        f.phase = p;
        // games p, p+n, p+2n ... below the target; 0 stays limitless
        f.games = S.compGames > 0 ? (S.compGames - p + n - 1) / n : 0;
        char name[64];
        snprintf(name, sizeof(name), "\\shard-%02d-of-%02d.cwf", p + 1, n);
        if (WriteWholeFile(dir + name, cw::writeFieldPack(f))) written++;
    }
    char buf[200];
    snprintf(buf, sizeof(buf), "%d shard file(s) written -- give one to each machine, then add their result files here", written);
    g_CompNetNote = buf;
    S.status = buf;
}

static void CompetitionOpenShard()
{
    std::vector<std::string> files = OpenFilesDialog(g_Hwnd,
        "Shard files (*.cwf)\0*.cwf\0All files (*.*)\0*.*\0", "Open a shard to play on this machine", false);
    if (files.empty()) return;
    std::string text, why;
    cw::FieldPack f;
    if (!ReadWholeFile(files[0], text)) { g_CompNetNote = "could not read " + files[0]; return; }
    if (!cw::parseFieldPack(text, f, why)) { g_CompNetNote = FileStem(files[0]) + ": " + why; return; }
    g_CompShard = f;
    g_CompShardOpen = true;
    g_CompShardPath = files[0];
    g_App.compResult = cw::CompeteResult();
    g_CompIdentity = cw::CompeteResult();
    g_CompLocal = cw::CompeteResult();
    g_CompNetNote.clear();
}

static void CompetitionCloseShard()
{
    g_CompShardOpen = false;
    g_CompShardPath.clear();
    g_CompShard = cw::FieldPack();
}

static void CompetitionSaveResults()
{
    if (g_CompLocal.empty() || g_CompLocal.gamesRun == 0) { g_CompNetNote = "nothing has been played yet"; return; }
    std::string suggested = "results-" + NetComputerName();
    if (g_CompShardOpen) suggested = "results-" + FileStem(g_CompShardPath) + "-" + NetComputerName();
    for (char& c : suggested) if (c == ' ') c = '_';
    const std::string path = SaveFileDialog(g_Hwnd, "Result files (*.cwr)\0*.cwr\0All files (*.*)\0*.*\0",
                                            "Save this machine's results", "cwr", suggested + ".cwr");
    if (path.empty()) return;
    if (WriteWholeFile(path, cw::writeResultPack(g_CompLocal, g_CompSender, g_CompRunId)))
        g_CompNetNote = "saved " + FileStem(path) + " -- add it on the host";
    else g_CompNetNote = "could not write " + path;
}

static void CompetitionAddResults()
{
    std::vector<std::string> files = OpenFilesDialog(g_Hwnd,
        "Result files (*.cwr)\0*.cwr\0All files (*.*)\0*.*\0", "Add result files from other machines", true);
    if (files.empty()) return;

    // The names have to match a field: the open shard's, or the workspace's.
    if (g_CompIdentity.empty() && !g_App.compRunning)
    {
        std::vector<cw::WarriorGroup> groups = g_CompShardOpen ? g_CompShard.groups : BuildGroups();
        if (groups.empty()) { g_CompNetNote = "load the field first, so the names have somewhere to land"; return; }
        cw::prepareResult(groups, g_CompIdentity);
        g_CompPending = g_CompIdentity;
        g_CompLocal = g_CompIdentity;
        g_CompGroups = groups;
    }

    int added = 0, strangers = 0;
    std::string bad;
    for (const std::string& path : files)
    {
        std::string text, why;
        cw::ResultPack rp;
        if (!ReadWholeFile(path, text) || !cw::parseResultPack(text, rp, why))
        {
            bad += (bad.empty() ? "" : ", ") + FileStem(path);
            continue;
        }
        if (rp.sender.empty()) rp.sender = FileStem(path);
        // count strangers against a scratch copy so a pack from another field
        // can be refused whole
        cw::CompeteResult probe = g_CompIdentity;
        const int unknown = cw::addResultPack(probe, rp);
        if (unknown > 0 && unknown == (int)rp.name.size()) { strangers++; continue; }
        g_NetPacks[rp.sender] = std::move(rp);
        added++;
    }
    g_NetPacksRev++;
    char buf[240];
    snprintf(buf, sizeof(buf), "%d result file(s) added%s%s", added,
             strangers ? ("  -- " + std::to_string(strangers) + " from a different field, skipped").c_str() : "",
             bad.empty() ? "" : ("  -- could not read " + bad).c_str());
    g_CompNetNote = buf;
}

// ---- drawing ----------------------------------------------------------------------

// The colour a name wears in the arena, matched back to the workspace so the
// board and the fight read as the same thing. A field that came over the wire
// or from a shard file has no workspace behind it, so its groups take the
// player palette in field order -- the colours a host with the same folder
// loaded would have given them.
static unsigned CompColorOf(const cw::CompeteResult& R, size_t row, bool isGroup)
{
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

// Bars from the left, best on top -- or, flipped, columns from the floor that
// climb to the right. Fitted, the bars share the whole panel between them --
// never thinner than 3 px, so a very big field scrolls -- and the stats come
// up on hover like the timeline lanes.
static void DrawCompBars(const std::vector<CompRow>& rows, double best, int gamesRun, bool vertical, bool fit)
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
    // text rides on a bar whenever the bar is tall enough to carry it
    const bool labels = !vertical && thick >= lineH + 2.0f * g_Scale;
    const bool ranks  = vertical && thick >= 12.0f * g_Scale;
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

        if (labels)
        {
            // the name rides on the bar, and steps outside it when it would not fit
            const ImVec2 ts = ImGui::CalcTextSize(r.name.c_str());
            const bool inside = len > ts.x + 16 * g_Scale;
            dl->AddText(ImVec2(a.x + (inside ? 8 * g_Scale : len + 8 * g_Scale),
                               a.y + (thick - gap - ts.y) * 0.5f),
                        HexU(inside ? Col::Bg0 : Col::Text), r.name.c_str());
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
                dl->AddText(ImVec2(a.x + (thick - gap - rs.x) * 0.5f, b.y - rs.y - 2 * g_Scale),
                            HexU(len > rs.y + 4 * g_Scale ? Col::Bg0 : Col::TextDim), rk);
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

static void DrawLeaderboardPanel()
{
    AppState& S = g_App;
    if (g_CompRaise) { ImGui::SetNextWindowFocus(); g_CompRaise = false; }
    if (!ImGui::Begin("Competition", NULL, ImGuiWindowFlags_NoCollapse)) { ImGui::End(); return; }

    // ---- controls ---------------------------------------------------------
    const bool joined   = (S.compMode == MODE_JOIN);
    const bool shardRun = (S.compMode == MODE_OFFLINE && g_CompShardOpen);
    const bool limitless = shardRun ? (g_CompShard.games <= 0) : (S.compGames <= 0);
    const int total = shardRun ? ImMax(1, g_CompShard.games) : ImMax(1, S.compGames);
    const int done  = CompetitionGamesDone();

    ImGui::BeginDisabled(S.compRunning);
    ImGui::BeginDisabled(joined || shardRun);
    ImGui::SetNextItemWidth(110 * g_Scale);
    ImGui::DragInt("games", &S.compGames, 5.0f, 0, 2000000, S.compGames ? "%d" : "limitless");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("How many battles to play. 0 keeps going until you press Stop.");
    ImGui::EndDisabled();
    ImGui::SameLine(0, 12 * g_Scale);
    ImGui::SetNextItemWidth(90 * g_Scale);
    ImGui::DragInt("threads", &S.compThreads, 0.2f, 0, 128, S.compThreads ? "%d" : "auto");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("0 spreads the run over every core the machine has (%d here)",
                          (int)std::thread::hardware_concurrency());
    ImGui::SameLine(0, 12 * g_Scale);
    ImGui::BeginDisabled(joined || shardRun);
    ImGui::SetNextItemWidth(90 * g_Scale);
    ImGui::DragInt("per battle", &S.compCombo, 0.1f, 0, 20, S.compCombo ? "%d groups" : "all");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("How many groups are drawn into each war.\n"
                          "A war holds 20 warriors, so a field of more than about nine\n"
                          "groups cannot all fight at once -- draw a few per battle and\n"
                          "let the seeds cover the combinations. 0 puts everyone in.");
    ImGui::EndDisabled();
    ImGui::SameLine(0, 12 * g_Scale);
    ImGui::SetNextItemWidth(110 * g_Scale);
    static const char* kBackends[] = { "CPU", "GPU", "CPU + GPU", "Java" };
    ImGui::Combo("engine", &S.compBackend, kBackends, 4);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("The GPU lane is contest wgpu/engine.exe -- the same engine as a\n"
                          "compute shader, run as a separate process so a driver reset\n"
                          "cannot take the editor with it.\n"
                          "CPU + GPU splits this machine's share: the CPU plays the\n"
                          "even-numbered battles and the GPU the odd ones.\n"
                          "Java plays the same battles on the CGX Java engine\n"
                          "(CGX-engine\\engine) instead. Needs Java 17 or newer.");
    ImGui::SameLine(0, 12 * g_Scale);
    ImGui::SetNextItemWidth(96 * g_Scale);
    static const char* kModes[] = { "Local", "Offline", "Server", "Join" };
    if (ImGui::Combo("mode", &S.compMode, kModes, 4))
    {
        // leaving a mode drops its wire; the board it built stays
        if (S.compMode != MODE_SERVER) NetServerStop();
        if (S.compMode != MODE_JOIN)   NetClientDisconnect();
        g_CompNetNote.clear();
    }
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Local    this machine only.\n"
                          "Offline  write one shard file per machine, carry them over,\n"
                          "         bring the result files back and add them up here.\n"
                          "Server   host: other machines join over the network and play\n"
                          "         their share the moment you press Run.\n"
                          "Join     play a share of a tournament somebody else is hosting.\n"
                          "\n"
                          "Every machine plays its own phase of the game index, so the\n"
                          "battles never overlap and the standings are a plain sum.");
    ImGui::EndDisabled();

    ImGui::SameLine(0, 12 * g_Scale);
    if (S.compRunning)
    {
        if (ImGui::Button("Stop", ImVec2(90 * g_Scale, 0))) CompetitionStop();
    }
    else if (!joined)
    {
        if (ImGui::Button("Run", ImVec2(90 * g_Scale, 0))) CompetitionStart();
    }

    ImGui::SameLine(0, 12 * g_Scale);
    if (shardRun)
        ImGui::TextColored(Hex(Col::TextDim), "seed %lld", (long long)g_CompShard.seed);
    else if (joined && S.compRunning)
        ImGui::TextColored(Hex(Col::TextDim), "seed %lld", (long long)g_NetClient.field.seed);
    else
        ImGui::TextColored(Hex(Col::TextDim), "seed %s", S.seedText);

    if (!S.compGpuNote.empty())
    {
        ImGui::SameLine(0, 12 * g_Scale);
        ImGui::TextColored(Hex(Col::Warn), "%s", S.compGpuNote.c_str());
    }
    else if (g_Gpu.result.gamesRun > 0 || g_Gpu.running)
    {
        ImGui::SameLine(0, 12 * g_Scale);
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
    if (S.compMode == MODE_OFFLINE)
    {
        ImGui::BeginDisabled(S.compRunning);
        if (g_CompShardOpen)
        {
            ImGui::TextColored(Hex(Col::Accent), "shard %d of %d", g_CompShard.phase + 1, g_CompShard.stride);
            ImGui::SameLine(0, 8 * g_Scale);
            int warriors = 0;
            for (const cw::WarriorGroup& g : g_CompShard.groups) warriors += (int)g.warriors.size();
            if (g_CompShard.games > 0)
                ImGui::TextColored(Hex(Col::TextDim), "%d groups, %d warriors, %d games  --  %s",
                                   (int)g_CompShard.groups.size(), warriors, g_CompShard.games, g_CompShard.title.c_str());
            else
                ImGui::TextColored(Hex(Col::TextDim), "%d groups, %d warriors, until Stop  --  %s",
                                   (int)g_CompShard.groups.size(), warriors, g_CompShard.title.c_str());
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", g_CompShardPath.c_str());
            ImGui::SameLine(0, 8 * g_Scale);
            if (ImGui::SmallButton("Close shard")) CompetitionCloseShard();
        }
        else
        {
            ImGui::SetNextItemWidth(80 * g_Scale);
            ImGui::DragInt("shards", &S.compShards, 0.1f, 1, kNetStride);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("One shard file per machine. Each plays every %dth battle, offset by its number.",
                                  ImMax(1, S.compShards));
            ImGui::SameLine(0, 6 * g_Scale);
            if (ImGui::Button("Export shards...")) CompetitionExportShards();
            ImGui::SameLine(0, 12 * g_Scale);
            if (ImGui::Button("Open shard...")) CompetitionOpenShard();
        }
        ImGui::SameLine(0, 12 * g_Scale);
        ImGui::BeginDisabled(g_CompLocal.empty() || g_CompLocal.gamesRun == 0);
        if (ImGui::Button("Save results...")) CompetitionSaveResults();
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("What this machine played, as a file to carry back to the host.");
        ImGui::SameLine(0, 6 * g_Scale);
        if (ImGui::Button("Add results...")) CompetitionAddResults();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Result files from the other machines. Each is added into the board;\n"
                              "adding the same one twice replaces it, never doubles it.");
        if (!g_NetPacks.empty())
        {
            ImGui::SameLine(0, 8 * g_Scale);
            int games = 0;
            for (const auto& kv : g_NetPacks) games += kv.second.gamesRun;
            ImGui::TextColored(Hex(Col::Accent2), "%d file(s) merged, %d games", (int)g_NetPacks.size(), games);
            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                for (const auto& kv : g_NetPacks)
                    ImGui::Text("%s  --  %d games in %.0fs", kv.first.c_str(), kv.second.gamesRun, kv.second.seconds);
                ImGui::EndTooltip();
            }
            ImGui::SameLine(0, 6 * g_Scale);
            if (ImGui::SmallButton("Clear")) { g_NetPacks.clear(); g_NetPacksRev++; }
        }
        ImGui::EndDisabled();
    }
    else if (S.compMode == MODE_SERVER)
    {
        ImGui::BeginDisabled(NetServerUp());
        ImGui::SetNextItemWidth(70 * g_Scale);
        ImGui::InputInt("port", &S.compNetPort, 0, 0);
        S.compNetPort = ImClamp(S.compNetPort, 1, 65535);
        ImGui::EndDisabled();
        ImGui::SameLine(0, 6 * g_Scale);
        if (NetServerUp())
        {
            ImGui::BeginDisabled(S.compRunning);
            if (ImGui::Button("Stop listening")) NetServerStop();
            ImGui::EndDisabled();
            ImGui::SameLine(0, 12 * g_Scale);
            static std::string addrs;
            static double addrsAt = -100.0;
            if (ImGui::GetTime() - addrsAt > 30.0) { addrs = NetLocalAddresses(); addrsAt = ImGui::GetTime(); }
            ImGui::TextColored(Hex(Col::Accent), "listening on port %d", g_NetServer.port);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Joiners type one of this machine's addresses:\n%s\n\nport %d must be let through the firewall.",
                                  addrs.empty() ? "(no address found)" : addrs.c_str(), g_NetServer.port);
            ImGui::SameLine(0, 12 * g_Scale);
            if (g_NetServer.peers.empty())
                ImGui::TextColored(Hex(Col::TextDim), "nobody has joined yet");
            else
            {
                int playing = 0;
                for (const NetPeer& p : g_NetServer.peers) if (p.running) playing++;
                ImGui::TextColored(Hex(Col::Accent2), "%d joined%s", (int)g_NetServer.peers.size(),
                                   playing ? (", " + std::to_string(playing) + " playing").c_str() : "");
                if (ImGui::IsItemHovered())
                {
                    ImGui::BeginTooltip();
                    for (const NetPeer& p : g_NetServer.peers)
                    {
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
        }
        else if (ImGui::Button("Listen"))
        {
            std::string why;
            if (NetServerStart(S.compNetPort, why)) g_CompNetNote.clear();
            else g_CompNetNote = why;
        }
        if (!NetServerUp() && !g_NetServer.note.empty())
        {
            ImGui::SameLine(0, 12 * g_Scale);
            ImGui::TextColored(Hex(Col::Warn), "%s", g_NetServer.note.c_str());
        }
        if (ImGui::IsItemHovered() && !NetServerUp())
            ImGui::SetTooltip("Open the port. Machines that join sit in a lobby until you press Run;\n"
                              "one that joins mid-run is handed a share straight away.");
    }
    else if (S.compMode == MODE_JOIN)
    {
        const bool linked = (g_NetClient.s != INVALID_SOCKET) || g_NetClient.state == NET_CONNECTING;
        ImGui::BeginDisabled(linked);
        ImGui::SetNextItemWidth(160 * g_Scale);
        ImGui::InputText("host", S.compNetHost, sizeof(S.compNetHost));
        ImGui::SameLine(0, 6 * g_Scale);
        ImGui::SetNextItemWidth(70 * g_Scale);
        ImGui::InputInt("port##join", &S.compNetPort, 0, 0);
        S.compNetPort = ImClamp(S.compNetPort, 1, 65535);
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
            if (!NetClientConnect(S.compNetHost, S.compNetPort, why)) g_NetClient.note = why;
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
        else if (S.compMode == MODE_OFFLINE)
            ImGui::TextColored(Hex(Col::TextDim),
                "Export a shard file per machine, or open one to play here. Result files from every "
                "machine add up on the host.");
        else
            ImGui::TextColored(Hex(Col::TextDim),
                "Enable some groups and press Run. Each war is played from its own seed and every "
                "survivor still standing at the end splits a point.");
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
    const int others = (int)g_NetPacks.size() - (CompetitionLanesSuperseded() ? 1 : 0);
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

    std::vector<CompRow> rows;
    if (S.compByGroup)
        for (size_t i = 0; i < R.groupName.size(); i++)
            rows.push_back({ R.groupName[i], R.groupScore[i], R.groupSurvived[i],
                             CompColorOf(R, i, true) });
    else
        for (size_t i = 0; i < R.warriorName.size(); i++)
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
    DrawCompBars(rows, best, R.gamesRun, S.compBarsUp, S.compBarsFit);
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("##comptable", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
    if (ImGui::BeginTable("##board", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                                        ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 26 * g_Scale);
        ImGui::TableSetupColumn(S.compByGroup ? "Group" : "Survivor");
        ImGui::TableSetupColumn("Score", ImGuiTableColumnFlags_WidthFixed, 58 * g_Scale);
        ImGui::TableSetupColumn("Share", ImGuiTableColumnFlags_WidthFixed, 52 * g_Scale);
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
            ImGui::TextColored(Hex(Col::TextDim), "%.1f%%",
                               sum > 0.0 ? rows[i].score / sum * 100.0 : 0.0);
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();
    ImGui::End();
}
