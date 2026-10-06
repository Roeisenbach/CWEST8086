// =============================================================================
//  Java lane
//
//  The CGX Java engine (CGX-engine/engine, the class files proven bit-identical
//  to the official corewars8086 jar) playing a competition for the app. It runs
//  as a child process exactly like the GPU lane -- same log, same scores file,
//  same poll -- so the board cannot tell which engine filled it.
//
//  The driver is CGX-engine/lane/JavaLane.java. It does not play wars itself:
//  every battle is Tourney.runBattle. It only follows this app's contract for
//  which battles those are -- game g is fought by cw::drawCombo's groups, in
//  the same order, on seed firstSeed + g, and the lane plays phase + k*stride --
//  so a Java run is the same tournament a CPU run is, war for war.
//
//  Included by main.cpp after gui_gpulane.inl, whose lane helpers it shares.
// =============================================================================

static GpuLane g_Java;

static std::string JavaLaneAppDir()
{
    char buf[MAX_PATH] = "";
    GetModuleFileNameA(NULL, buf, MAX_PATH);
    std::string base = buf;
    const size_t slash = base.find_last_of("\\/");
    return (slash == std::string::npos) ? std::string() : base.substr(0, slash + 1);
}

// The newest JDK/JRE under `parent` whose name starts with `prefix`.
static std::string JavaLaneNewestUnder(const std::string& parent, const char* prefix)
{
    std::string best;
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((parent + prefix + "*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return best;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        const std::string java = parent + fd.cFileName + "\\bin\\java.exe";
        // names sort by version closely enough ("jdk-26..." after "jdk-17...")
        if (FileExists(java) && (best.empty() || java > best)) best = java;
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return best;
}

// java.exe: JAVA_HOME first, then PATH, then the usual install folders -- the
// same order CGX-engine/RUN.bat looks in.
static std::string JavaLaneFindJava(const char* exeName)
{
    char buf[MAX_PATH] = "";
    if (GetEnvironmentVariableA("JAVA_HOME", buf, MAX_PATH) > 0)
    {
        std::string home = buf;
        if (!home.empty() && home.back() != '\\' && home.back() != '/') home += '\\';
        if (FileExists(home + "bin\\" + exeName)) return home + "bin\\" + exeName;
    }
    char* part = nullptr;
    if (SearchPathA(NULL, exeName, NULL, MAX_PATH, buf, &part) > 0) return buf;

    const char* roots[] = { "C:\\Program Files\\Java\\", "C:\\Program Files\\Eclipse Adoptium\\",
                            "C:\\Program Files\\Microsoft\\" };
    for (const char* r : roots)
    {
        std::string j = JavaLaneNewestUnder(r, "jdk");
        if (j.empty()) j = JavaLaneNewestUnder(r, "jre");
        if (!j.empty())
        {
            if (strcmp(exeName, "java.exe") == 0) return j;
            const std::string other = j.substr(0, j.size() - strlen("java.exe")) + exeName;
            if (FileExists(other)) return other;
        }
    }
    return std::string();
}

static ULONGLONG JavaLaneWriteTime(const std::string& path)
{
    WIN32_FILE_ATTRIBUTE_DATA a;
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &a)) return 0;
    return ((ULONGLONG)a.ftLastWriteTime.dwHighDateTime << 32) | a.ftLastWriteTime.dwLowDateTime;
}

// Runs a command to completion with no window. Only used for the one-off javac.
static bool JavaLaneRunAndWait(const std::string& cmdLine, DWORD timeoutMs)
{
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::vector<char> cmd(cmdLine.begin(), cmdLine.end());
    cmd.push_back('\0');
    if (!CreateProcessA(NULL, cmd.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
        return false;
    const bool finished = WaitForSingleObject(pi.hProcess, timeoutMs) == WAIT_OBJECT_0;
    DWORD code = 1;
    if (finished) GetExitCodeProcess(pi.hProcess, &code);
    else TerminateProcess(pi.hProcess, 1);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return finished && code == 0;
}

// Finds java.exe and the CGX folder, and builds the lane driver if its class
// is missing or older than its source. Fills the classpath to run it with.
static bool JavaLaneFind(std::string& java, std::string& classPath, std::string& why)
{
    const std::string base = JavaLaneAppDir();
    std::string cgx;
    const char* rel[] = { "CGX-engine\\", "..\\CGX-engine\\" };
    for (const char* r : rel)
        if (FileExists(base + r + "engine\\Tourney.class")) { cgx = base + r; break; }
    if (cgx.empty()) { why = "CGX-engine\\engine not found beside the app"; return false; }

    const std::string jar = cgx + "lib\\corewars8086-6.0.0.jar";
    if (!FileExists(jar)) { why = "CGX-engine\\lib\\corewars8086-6.0.0.jar is missing"; return false; }

    java = JavaLaneFindJava("java.exe");
    if (java.empty()) { why = "no Java found -- install a JDK 17 or newer, or set JAVA_HOME"; return false; }

    const std::string src = cgx + "lane\\JavaLane.java";
    const std::string build = cgx + "lane\\build\\";
    const std::string cls = build + "JavaLane.class";
    if (!FileExists(cls) || (FileExists(src) && JavaLaneWriteTime(src) > JavaLaneWriteTime(cls)))
    {
        if (!FileExists(src)) { why = "CGX-engine\\lane\\JavaLane.java is missing"; return false; }
        // without a javac, a stale build still runs; no build at all cannot
        const std::string javac = JavaLaneFindJava("javac.exe");
        if (javac.empty() && !FileExists(cls))
        {
            why = "the Java lane needs building and no javac was found";
            return false;
        }
        if (!javac.empty())
        {
            CreateDirectoryA(build.c_str(), NULL);
            const std::string cmd = "\"" + javac + "\" -nowarn -cp \"" + cgx + "engine;" + jar +
                                    "\" -d \"" + build.substr(0, build.size() - 1) + "\" \"" + src + "\"";
            if (!JavaLaneRunAndWait(cmd, 60000) || !FileExists(cls))
            {
                why = "could not compile CGX-engine\\lane\\JavaLane.java";
                return false;
            }
        }
    }

    classPath = cgx + "engine;" + build.substr(0, build.size() - 1) + ";" + jar;
    return true;
}

static std::string JavaLaneTempDir()
{
    char t[MAX_PATH] = "";
    GetTempPathA(MAX_PATH, t);
    return std::string(t) + "cwest_java\\";
}

// The field as the driver reads it: one `g` line per group in the order the
// CPU lane is handed them, then its warriors' code in hex. Names are replaced
// by short labels -- the scores come back keyed by them -- but two groups that
// share a name keep sharing a label, since the engine treats them as one.
static bool JavaLaneExportField(const std::vector<cw::WarriorGroup>& groups, GpuLane& L,
                                const cw::CompeteOptions& opt, std::string& why)
{
    CreateDirectoryA(L.dir.c_str(), NULL);
    std::ostringstream out;
    out << "cwest-java-field 1\n";

    std::map<std::string, std::string> labelOf;
    L.slotOfFile.clear();
    int slot = 0, zi = 0;
    static const char hex[] = "0123456789abcdef";
    for (size_t gi = 0; gi < groups.size(); gi++)
    {
        const cw::WarriorGroup& g = groups[gi];
        auto it = labelOf.find(g.name);
        if (it == labelOf.end()) it = labelOf.emplace(g.name, "g" + std::to_string(gi)).first;
        out << "g " << it->second << " " << g.warriors.size() << "\n";

        for (const cw::WarriorData& w : g.warriors)
        {
            int type = 0;
            std::string name;
            if (cw::isZombie(w.type))
            {
                // the CGX engine has one zombie speed for the whole war
                if (w.speed > 0 && w.speed != opt.zombieSpeed)
                {
                    why = "the Java engine cannot give one zombie its own speed";
                    return false;
                }
                type = (w.type == cw::WarriorType::ZOMBIE_H) ? 2 : 1;
                name = "z" + std::to_string(zi++);
            }
            else
            {
                name = "s" + std::to_string(slot);
                L.slotOfFile[name] = slot++;     // scoring rows skip zombies
            }
            std::string code;
            code.reserve(w.code.size() * 2);
            for (unsigned char c : w.code) { code += hex[c >> 4]; code += hex[c & 15]; }
            out << "w " << name << " " << type << " " << code << "\n";
        }
    }
    if (slot == 0) { why = "no scoring groups -- only zombies"; return false; }
    const std::string text = out.str();
    if (!GpuWriteBinary(L.dir + "field.txt", std::vector<unsigned char>(text.begin(), text.end())))
    {
        why = "could not write the field for Java";
        return false;
    }
    return true;
}

static void JavaLaneStop() { LaneStop(g_Java); }
static bool JavaLanePoll() { return LanePoll(g_Java, "Java"); }

// Start the lane on `opt`'s phase of its stride. `games` 0 is open-ended.
static bool JavaLaneStart(const std::vector<cw::WarriorGroup>& groups,
                          const cw::CompeteOptions& opt, int games, std::string& why)
{
    std::string java, cp;
    if (!JavaLaneFind(java, cp, why)) return false;

    LaneCloseLog(g_Java);
    g_Java = GpuLane();
    g_Java.dir = JavaLaneTempDir();
    g_Java.scoresPath = g_Java.dir + "scores.txt";
    g_Java.logPath = g_Java.dir + "java.log";
    g_Java.games = games;
    g_Java.lastLine = "starting...";
    CreateDirectoryA(g_Java.dir.c_str(), NULL);
    DeleteFileA(g_Java.scoresPath.c_str());

    if (!JavaLaneExportField(groups, g_Java, opt, why)) return false;
    cw::prepareResult(groups, g_Java.result);

    char args[1024];
    snprintf(args, sizeof(args),
             " --seed %lld --pair-seed %lld --combo %d --stride %d --phase %d"
             " --games %d --rounds %d --zspeed %d --threads %d --parent %lu",
             (long long)opt.firstSeed, (long long)opt.pairingSeed, opt.comboSize,
             opt.seedStride, opt.seedPhase, ImMax(0, games), opt.maxRounds,
             opt.zombieSpeed, opt.threads, (unsigned long)GetCurrentProcessId());

    const std::string cmd = "\"" + java + "\" -Xmx1g -cp \"" + cp + "\" JavaLane"
                            " --field \"" + g_Java.dir + "field.txt\" --scores \"" +
                            g_Java.scoresPath + "\"" + args;
    if (!LaneSpawn(g_Java, cmd, g_Java.dir.substr(0, g_Java.dir.size() - 1)))
    {
        why = "could not start java.exe";
        return false;
    }
    return true;
}
