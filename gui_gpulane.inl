// =============================================================================
//  GPU lane
//
//  "contest wgpu/engine.exe" is the same engine as a Vulkan compute shader --
//  one invocation per war. It runs here as a child process rather than linked
//  in, for two reasons: the app then has no Vulkan dependency at all and still
//  starts on a machine without a driver, and a shader that overruns the GPU
//  watchdog loses the device, which would take the whole editor down with any
//  unsaved warriors in it. As a child it just dies and the CPU lane carries on.
//
//  The two lanes split the tournament by parity of the global game index: the
//  CPU plays 0, 2, 4..., the GPU 1, 3, 5.... Both the battle seed and the
//  combination of groups are drawn from that index, so neither lane has to be
//  told what the other is doing, and the two results simply add up.
//
//  Included by main.cpp before gui_compete.inl.
// =============================================================================

struct GpuLane
{
    PROCESS_INFORMATION proc{};
    bool     running = false;
    std::string dir;                  // where the field and the scores file live
    std::string scoresPath;
    int      games = 0;               // what this lane was asked to play

    // The GPU reports scores against the file name it read a warrior from, so
    // that is what maps its numbers back onto our rows.
    std::map<std::string, int> slotOfFile;

    cw::CompeteResult result;         // this lane's contribution, names included
    int      gamesRun = 0;
    double   seconds = 0.0;
    std::string note;

    // The host's own output goes to a log beside the field. Its last line is
    // shown in the panel so a lane that has not scored yet still reads as alive
    // -- "compiling pipeline..." rather than nothing at all.
    std::string logPath;
    HANDLE   logHandle = INVALID_HANDLE_VALUE;
    std::string lastLine;
    double   lastLogRead = 0.0;

    // where the host is between finished games, from the scores file
    unsigned passes = 0, inFlight = 0, budget = 0;
};
static GpuLane g_Gpu;

// engine.exe and its shader, looked for beside the app and in the source tree.
static bool GpuLaneFind(std::string& exe, std::string& why)
{
    char buf[MAX_PATH] = "";
    GetModuleFileNameA(NULL, buf, MAX_PATH);
    std::string base = buf;
    const size_t slash = base.find_last_of("\\/");
    base = (slash == std::string::npos) ? std::string() : base.substr(0, slash + 1);

    const char* rel[] = { "contest wgpu\\engine.exe", "..\\contest wgpu\\engine.exe" };
    for (const char* r : rel)
    {
        const std::string p = base + r;
        if (!FileExists(p)) continue;
        const std::string spv = p.substr(0, p.find_last_of("\\/") + 1) + "shaders\\compute.spv";
        if (!FileExists(spv)) { why = "found engine.exe but not shaders/compute.spv"; return false; }
        exe = p;
        return true;
    }
    why = "contest wgpu\\engine.exe not found beside the app";
    return false;
}

// Small filesystem helpers of its own: this is included ahead of the options
// panel, where the general-purpose ones live.
static bool GpuWriteBinary(const std::string& path, const std::vector<unsigned char>& b)
{
    std::ofstream f(path.c_str(), std::ios::binary | std::ios::trunc);
    if (!f) return false;
    if (!b.empty()) f.write((const char*)b.data(), (std::streamsize)b.size());
    return (bool)f;
}

static void GpuClearDir(const std::string& dir)
{
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((dir + "*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        DeleteFileA((dir + fd.cFileName).c_str());
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

static std::string GpuLaneTempDir()
{
    char t[MAX_PATH] = "";
    GetTempPathA(MAX_PATH, t);
    return std::string(t) + "cwest_gpu\\";
}

// Write the field where the GPU host can read it. The names carry a fixed-width
// ordinal so that the host's sorted directory walk reproduces exactly the order
// the CPU lane was handed -- the combination draw depends on it. Survivor pairs
// keep the trailing 1 / 2 the host pairs on; a zombie's type is its last letter.
static bool GpuLaneExportField(const std::vector<cw::WarriorGroup>& groups, GpuLane& L)
{
    const std::string sdir = L.dir + "survivors\\";
    const std::string zdir = L.dir + "zombies\\";
    CreateDirectoryA(L.dir.c_str(), NULL);
    CreateDirectoryA(sdir.c_str(), NULL);
    CreateDirectoryA(zdir.c_str(), NULL);

    // clear anything a previous run left, or the field would grow every time
    GpuClearDir(sdir);
    GpuClearDir(zdir);

    L.slotOfFile.clear();
    int slot = 0, gi = 0, zi = 0;
    for (const cw::WarriorGroup& g : groups)
    {
        const bool zomb = cw::isZombieGroup(g);
        int within = 0;
        for (const cw::WarriorData& w : g.warriors)
        {
            char name[128];
            if (zomb)
            {
                // the host reads a trailing 'h' as the heavy kind
                snprintf(name, sizeof(name), "z%04d_%c", zi++,
                         w.type == cw::WarriorType::ZOMBIE_H ? 'h' : 'n');
            }
            else if (g.warriors.size() >= 2)
            {
                snprintf(name, sizeof(name), "g%04d_%d", gi, ++within);
            }
            else
            {
                // a lone survivor gets no trailing digit: the host drops an
                // unpaired "...1", but takes a plain name as a group of one
                snprintf(name, sizeof(name), "g%04d_s", gi);
            }

            const std::vector<unsigned char> bin(w.code.begin(), w.code.end());
            if (!GpuWriteBinary((zomb ? zdir : sdir) + name, bin)) return false;
            if (!zomb) L.slotOfFile[name] = slot++;   // scoring rows skip zombies
        }
        if (!zomb) ++gi;
    }
    return slot > 0;
}

// The process, log and scores-file handling below is shared by every child
// lane -- the Java lane (gui_javalane.inl) is the same shape -- so it takes the
// lane it works on.
static void LaneCloseLog(GpuLane& L)
{
    if (L.logHandle != INVALID_HANDLE_VALUE) CloseHandle(L.logHandle);
    L.logHandle = INVALID_HANDLE_VALUE;
}
static void GpuLaneCloseLog() { LaneCloseLog(g_Gpu); }

static void LaneStop(GpuLane& L)
{
    if (!L.running) return;
    TerminateProcess(L.proc.hProcess, 1);
    WaitForSingleObject(L.proc.hProcess, 2000);
    CloseHandle(L.proc.hProcess);
    CloseHandle(L.proc.hThread);
    LaneCloseLog(L);
    L.running = false;
}
static void GpuLaneStop() { LaneStop(g_Gpu); }

// The tail of the host's log, read no more than twice a second.
static void LaneReadLog(GpuLane& L)
{
    const double now = ImGui::GetTime();
    if (now - L.lastLogRead < 0.5) return;
    L.lastLogRead = now;

    HANDLE h = CreateFileA(L.logPath.c_str(), GENERIC_READ,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    LARGE_INTEGER size{};
    GetFileSizeEx(h, &size);
    const DWORD want = (DWORD)ImMin<long long>(size.QuadPart, 4096);
    if (want > 0)
    {
        LARGE_INTEGER at{};
        at.QuadPart = size.QuadPart - want;
        SetFilePointerEx(h, at, NULL, FILE_BEGIN);
        std::string buf((size_t)want, '\0');
        DWORD got = 0;
        if (ReadFile(h, &buf[0], want, &got, NULL) && got > 0)
        {
            buf.resize(got);
            // last line that says anything
            size_t end = buf.find_last_not_of("\r\n ");
            if (end != std::string::npos)
            {
                size_t start = buf.find_last_of("\r\n", end);
                start = (start == std::string::npos) ? 0 : start + 1;
                L.lastLine = buf.substr(start, end - start + 1);
                // trim the leading whitespace the host indents progress with
                const size_t ws = L.lastLine.find_first_not_of(" \t");
                if (ws != std::string::npos) L.lastLine = L.lastLine.substr(ws);
            }
        }
    }
    CloseHandle(h);
}

// Start a lane's process with its output going to the lane's log, so a lane
// that fails or stalls leaves a record instead of vanishing.
static bool LaneSpawn(GpuLane& L, const std::string& cmdLine, const std::string& workDir)
{
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    L.logHandle = CreateFileA(L.logPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                              &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    {
        // keep the command line at the top of the log, for reproducing by hand
        DWORD w = 0;
        std::string head = cmdLine + "\r\n\r\n";
        if (L.logHandle != INVALID_HANDLE_VALUE)
            WriteFile(L.logHandle, head.data(), (DWORD)head.size(), &w, NULL);
    }

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    if (L.logHandle != INVALID_HANDLE_VALUE)
    {
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = L.logHandle;
        si.hStdError  = L.logHandle;
        si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
    }
    std::vector<char> cmd(cmdLine.begin(), cmdLine.end());
    cmd.push_back('\0');
    if (!CreateProcessA(NULL, cmd.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL,
                        workDir.c_str(), &si, &L.proc))
    {
        LaneCloseLog(L);
        return false;
    }
    L.running = true;
    return true;
}

// Start the lane. `games` is how many battles this lane itself plays; 0 means
// open-ended -- keep going until told to stop -- which is how a lane of a split
// runs, since it cannot know how many the other lane will get through.
static bool GpuLaneStart(const std::vector<cw::WarriorGroup>& groups,
                         const cw::CompeteOptions& opt, int games, std::string& why)
{
    std::string exe;
    if (!GpuLaneFind(exe, why)) return false;

    GpuLaneCloseLog();
    g_Gpu = GpuLane();
    g_Gpu.dir = GpuLaneTempDir();
    g_Gpu.scoresPath = g_Gpu.dir + "scores.txt";
    g_Gpu.logPath = g_Gpu.dir + "gpu.log";
    g_Gpu.games = games;
    g_Gpu.lastLine = "starting...";
    DeleteFileA(g_Gpu.scoresPath.c_str());

    if (!GpuLaneExportField(groups, g_Gpu)) { why = "could not write the field for the GPU"; return false; }
    cw::prepareResult(groups, g_Gpu.result);

    // Open-ended: a count the run will never reach, and a chunk that fits the
    // card well without being so large that the first finished game is a long
    // time coming. 4096 resident is where this shader's throughput plateaus.
    const bool openEnded = (games <= 0);
    const int  askGames  = openEnded ? 1000000000 : games;
    const char* resident = openEnded ? " --resident 4096" : "";

    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "\"%s\" --survivors \"%ssurvivors\" --zombies \"%szombies\" --scores \"%s\""
             " --seed %lld --pair-seed %lld --combo %d --seed-stride %d --seed-phase %d%s %d %d",
             exe.c_str(), g_Gpu.dir.c_str(), g_Gpu.dir.c_str(), g_Gpu.scoresPath.c_str(),
             (long long)opt.firstSeed, (long long)opt.pairingSeed, opt.comboSize,
             opt.seedStride, opt.seedPhase, resident, askGames, opt.maxRounds);

    const std::string workDir = exe.substr(0, exe.find_last_of("\\/"));
    if (!LaneSpawn(g_Gpu, cmd, workDir))
    {
        why = "could not start engine.exe";
        return false;
    }
    return true;
}

// Read whatever the lane has scored so far. Returns true while it is alive.
// `what` names the lane in the note left when it dies early.
static bool LanePoll(GpuLane& L, const char* what)
{
    if (!L.running) return false;
    LaneReadLog(L);

    // The host writes to a temporary and renames, so this is either the whole
    // of one update or the whole of the previous one -- never half of either.
    std::string text;
    if (ReadTextFile(L.scoresPath, text))
    {
        std::istringstream in(text);
        std::string tag;
        std::vector<double> score(L.result.warriorScore.size(), 0.0);
        std::vector<int>    surv(L.result.warriorSurvived.size(), 0);
        int run = 0, failed = 0;
        double secs = 0.0;
        bool sawGames = false;

        while (in >> tag)
        {
            if (tag == "games") { in >> run >> failed >> secs; sawGames = true; }
            else if (tag == "pass") { in >> L.passes >> L.inFlight >> L.budget; }
            else if (tag == "w")
            {
                std::string nm; double sc = 0.0; int sv = 0;
                in >> nm >> sc >> sv;
                auto it = L.slotOfFile.find(nm);
                if (it == L.slotOfFile.end()) continue;
                if ((size_t)it->second >= score.size()) continue;
                score[(size_t)it->second] = sc;
                surv[(size_t)it->second]  = sv;
            }
            else { std::string skip; std::getline(in, skip); }
        }

        if (sawGames)
        {
            L.result.warriorScore.swap(score);
            L.result.warriorSurvived.swap(surv);
            L.result.gamesRun = run;
            L.result.gamesFailed = failed;
            L.result.seconds = secs;
            L.gamesRun = run;
            L.seconds = secs;
        }
    }

    DWORD code = STILL_ACTIVE;
    GetExitCodeProcess(L.proc.hProcess, &code);
    if (code == STILL_ACTIVE) return true;

    CloseHandle(L.proc.hProcess);
    CloseHandle(L.proc.hThread);
    LaneCloseLog(L);
    L.running = false;
    if (code != 0 && (L.games <= 0 || L.gamesRun < L.games))
        L.note = std::string("the ") + what + " lane stopped early -- see " + L.logPath
                 + ": " + L.lastLine;
    return false;
}
static bool GpuLanePoll() { return LanePoll(g_Gpu, "GPU"); }
