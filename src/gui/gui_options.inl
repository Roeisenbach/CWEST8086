// =============================================================================
//  gui_options.inl -- options window, the open-folder flow, and zombies
//
//  Included once from main.cpp, after the editor helpers so it can assemble and
//  disassemble while it walks a folder.
// =============================================================================
#include <shlobj.h>
#include <shellapi.h>   // ShellExecuteA -- "Open fonts folder"
#include <cstdarg>

// -----------------------------------------------------------------------------
// Small filesystem helpers (Win32 -- the project already depends on it heavily)
// -----------------------------------------------------------------------------
static std::string JoinPath(const std::string& a, const std::string& b)
{
    if (a.empty()) return b;
    char last = a[a.size() - 1];
    return (last == '\\' || last == '/') ? a + b : a + "\\" + b;
}

static bool DirExists(const std::string& p)
{
    const DWORD a = GetFileAttributesA(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static bool EnsureDir(const std::string& p)
{
    return DirExists(p) || CreateDirectoryA(p.c_str(), NULL) != 0;
}

// Every plain file directly inside dir, as full paths.
static std::vector<std::string> ListDirFiles(const std::string& dir)
{
    std::vector<std::string> out;
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(JoinPath(dir, "*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return out;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        out.push_back(JoinPath(dir, fd.cFileName));
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    std::sort(out.begin(), out.end());
    return out;
}

// The names of the directories directly inside dir, sorted.
static std::vector<std::string> ListSubDirs(const std::string& dir)
{
    std::vector<std::string> out;
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(JoinPath(dir, "*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return out;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (fd.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) continue;
        const std::string n = fd.cFileName;
        if (n == "." || n == ".." || n[0] == '.') continue;      // .git and friends are not folders of survivors
        out.push_back(n);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    std::sort(out.begin(), out.end(), [](const std::string& a, const std::string& b) {
        return ToLower(a) < ToLower(b);
    });
    return out;
}

static std::string ExtOf(const std::string& fileName)
{
    const size_t d = fileName.find_last_of('.');
    return (d == std::string::npos) ? std::string() : ToLower(fileName.substr(d));
}

static bool WriteBinaryFile(const std::string& path, const std::vector<unsigned char>& b)
{
    std::ofstream f(path.c_str(), std::ios::binary | std::ios::trunc);
    if (!f) return false;
    if (!b.empty()) f.write((const char*)b.data(), (std::streamsize)b.size());
    return (bool)f;
}

// SHBrowseForFolder is the one folder picker that needs no COM object lifetime
// management beyond initialising the apartment.
static std::string BrowseForFolder(HWND owner, const char* title)
{
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    char buf[MAX_PATH] = "";
    BROWSEINFOA bi;
    memset(&bi, 0, sizeof(bi));
    bi.hwndOwner = owner;
    bi.pszDisplayName = buf;
    bi.lpszTitle = title;
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_EDITBOX;

    std::string result;
    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (pidl)
    {
        char path[MAX_PATH] = "";
        if (SHGetPathFromIDListA(pidl, path)) result = path;
        CoTaskMemFree(pidl);
    }
    return result;
}

// -----------------------------------------------------------------------------
// Open folder
// -----------------------------------------------------------------------------
static const char* kAsmDir  = "assembly";
static const char* kSurvDir = "survivors";
static const char* kZombDir = "zombies";

static std::string g_SortPromptRoot;    // non-empty while the sort popup is up

static bool IsLayoutDirName(const std::string& name)
{
    const std::string n = ToLower(name);
    return n == kAsmDir || n == kSurvDir || n == kZombDir;
}

// A folder of groups is a directory on disk, so its name has to be one: no
// path separators or the characters Windows refuses, no trailing dots, and not
// one of the three names the workspace layout itself uses. Empty = unusable.
static std::string CleanFolderName(const std::string& raw)
{
    std::string n;
    for (char ch : raw)
    {
        const unsigned char c = (unsigned char)ch;
        if (c < 32 || strchr("\\/:*?\"<>|", ch)) continue;
        n.push_back(ch);
    }
    while (!n.empty() && (n.back() == ' ' || n.back() == '.')) n.pop_back();
    size_t lead = 0;
    while (lead < n.size() && n[lead] == ' ') lead++;
    n.erase(0, lead);
    if (n.empty() || n[0] == '.' || IsLayoutDirName(n)) return std::string();
    if (n.size() > 60) n.resize(60);
    return n;
}

// Only a directory that holds warriors and nothing else is taken for a folder
// of survivors: one stray document, or a directory inside it, and it is
// somebody's own folder -- not read as warriors, and never sorted.
static bool LooksLikeSurvivorFolder(const std::string& dir)
{
    const std::vector<std::string> inside = ListDirFiles(dir);
    if (inside.empty() || !ListSubDirs(dir).empty()) return false;
    for (const std::string& p : inside)
    {
        const std::string ext = ExtOf(FileNameOf(p));
        if (!ext.empty() && ext != ".asm" && ext != ".s" && ext != ".txt") return false;
    }
    return true;
}

static void ClearWorkspace()
{
    g_App.players.clear();
    g_App.folders.clear();
    g_App.zombies.clear();
    g_App.selection.clear();
    g_App.anchor = Ref();
    g_App.open = Ref();
    g_App.editor.SetText("");
    g_App.editor.SetReadOnly(true);
    g_App.inspectWarrior = -1;
    g_App.game.clear();
}

// Move a flat pile of files into assembly / survivors / zombies. ".asm" is
// source; anything without an extension is machine code, and a machine-code
// file whose name starts with "zom" is a zombie.
static int SortFolderIntoSubfolders(const std::string& root)
{
    if (!EnsureDir(JoinPath(root, kAsmDir)) ||
        !EnsureDir(JoinPath(root, kSurvDir)) ||
        !EnsureDir(JoinPath(root, kZombDir)))
    {
        g_App.status = "Could not create the assembly / survivors / zombies folders";
        return 0;
    }

    // `sub` is a folder of survivors inside the pile: its files are sorted the
    // same way, into a folder of the same name under assembly / survivors, so
    // the grouping survives the sort. Zombies are not kept in folders.
    int moved = 0;
    auto sortDir = [&](const std::string& from, const std::string& sub) {
        for (const std::string& p : ListDirFiles(from))
        {
            const std::string fn  = FileNameOf(p);
            const std::string ext = ExtOf(fn);
            const std::string low = ToLower(fn);

            const char* dst = NULL;
            if (ext == ".asm")      dst = kAsmDir;
            else if (ext.empty())   dst = (low.compare(0, 3, "zom") == 0) ? kZombDir : kSurvDir;
            if (!dst) continue;                       // .txt, .png, whatever -- leave it alone

            std::string to = JoinPath(root, dst);
            if (!sub.empty() && dst != kZombDir)
            {
                to = JoinPath(to, sub);
                if (!EnsureDir(to)) continue;
            }
            if (MoveFileA(p.c_str(), JoinPath(to, fn).c_str())) moved++;
        }
    };
    sortDir(root, std::string());
    for (const std::string& sub : ListSubDirs(root))
    {
        if (IsLayoutDirName(sub) || CleanFolderName(sub).empty()) continue;
        if (!LooksLikeSurvivorFolder(JoinPath(root, sub))) continue;
        sortDir(JoinPath(root, sub), sub);
        RemoveDirectoryA(JoinPath(root, sub).c_str());   // goes only if the sort emptied it
    }
    return moved;
}

// Keep the two representations of one warrior in step. The .asm wins wherever
// both exist -- it is the file a human edits -- so a binary is only written
// from source, and source is only written for a binary that has none.
static void SyncFolderPair(const std::string& asmDir, const std::string& binDir,
                           int& assembled, int& disassembled)
{
    if (g_App.folderAssemble)
    {
        for (const std::string& p : ListDirFiles(asmDir))
        {
            if (ExtOf(FileNameOf(p)) != ".asm") continue;
            std::string text;
            if (!ReadTextFile(p, text)) continue;

            Survivor tmp;
            tmp.code = text;
            ReassembleSurvivor(tmp);
            if (!tmp.asmOk || tmp.bin.empty()) continue;

            const std::string out = JoinPath(binDir, StemOf(FileNameOf(p)));
            std::vector<unsigned char> old;
            if (ReadBinaryFile(out, old) && old == tmp.bin) continue;   // already current
            if (WriteBinaryFile(out, tmp.bin)) assembled++;
        }
    }

    if (g_App.folderDisassemble)
    {
        for (const std::string& p : ListDirFiles(binDir))
        {
            if (!ExtOf(FileNameOf(p)).empty()) continue;
            const std::string out = JoinPath(asmDir, StemOf(FileNameOf(p)) + ".asm");
            if (FileExists(out)) continue;             // never overwrite hand-written source

            std::vector<unsigned char> bytes;
            if (!ReadBinaryFile(p, bytes)) continue;
            if (bytes.size() > 512) bytes.resize(512);

            Survivor tmp;
            ListSurvivor(tmp, bytes);
            if (WriteTextFile(out, tmp.code)) disassembled++;
        }
    }
}

// Saving a survivor that came from a binary: the machine code goes back to the
// file it was loaded from, and the source lands beside it in assembly/ (or next
// to it as <name>.asm when there is no such folder). Returns false for a plain
// .asm survivor, which the normal text save handles.
static std::string ParentDir(const std::string& p)
{
    const size_t s = p.find_last_of("\\/");
    return (s == std::string::npos) ? std::string() : p.substr(0, s);
}

static bool SaveBinarySurvivor(Survivor& sv)
{
    if (sv.path.empty() || !ExtOf(FileNameOf(sv.path)).empty()) return false;

    const std::string dir  = ParentDir(sv.path);
    const std::string up   = ParentDir(dir);
    const std::string stem = StemOf(FileNameOf(sv.path));
    std::string asmOut = sv.path + ".asm";
    if (DirExists(JoinPath(up, kAsmDir)))
        asmOut = JoinPath(JoinPath(up, kAsmDir), stem + ".asm");
    else if (ToLower(FileNameOf(up)) == kSurvDir && DirExists(JoinPath(ParentDir(up), kAsmDir)))
    {
        // survivors/<folder>/name: the source goes to assembly/<folder>/name.asm
        const std::string adir = JoinPath(JoinPath(ParentDir(up), kAsmDir), FileNameOf(dir));
        if (EnsureDir(adir)) asmOut = JoinPath(adir, stem + ".asm");
    }

    bool ok = WriteTextFile(asmOut, sv.code);
    if (ok && sv.asmOk && !sv.bin.empty()) ok = WriteBinaryFile(sv.path, sv.bin);

    sv.dirty = !ok;
    g_App.status = ok ? ("Saved " + sv.name + " + " + FileNameOf(asmOut))
                      : ("FAILED to save " + sv.name);
    return true;
}

static void LoadZombieFolder(const std::string& dir)
{
    for (const std::string& p : ListDirFiles(dir))
    {
        const std::string fn = FileNameOf(p);
        if (!ExtOf(fn).empty()) continue;             // binaries only

        std::vector<unsigned char> bytes;
        if (!ReadBinaryFile(p, bytes)) continue;
        if (bytes.size() > 512) bytes.resize(512);

        Zombie z;
        z.speed = g_App.zombieSpeed;
        z.name  = StemOf(fn);
        z.tint  = kPlayerTints[(g_App.zombies.size() + 3) % (sizeof(kPlayerTints) / sizeof(kPlayerTints[0]))];
        z.sv.name = fn;
        z.sv.path = p;
        ListSurvivor(z.sv, bytes);
        z.sv.present = true;
        z.sv.enabled = true;
        z.sv.dirty   = false;
        // A trailing "h" is how the reference repository marks the heavy kind.
        const std::string stem = ToLower(StemOf(fn));
        z.heavy = !stem.empty() && stem[stem.size() - 1] == 'h';
        g_App.zombies.push_back(std::move(z));
    }
}

static void LoadWorkspaceFolder(const std::string& root)
{
    if (root.empty() || !DirExists(root)) { g_App.status = "No such folder"; return; }

    const std::string aDir = JoinPath(root, kAsmDir);
    const std::string sDir = JoinPath(root, kSurvDir);
    const std::string zDir = JoinPath(root, kZombDir);
    const bool structured = DirExists(aDir) || DirExists(sDir) || DirExists(zDir);

    if (!structured)
    {
        // Nothing to load from yet. Ask, sort, or fall back to a flat read.
        if (g_App.folderSortMode == 0) { g_SortPromptRoot = root; return; }
        if (g_App.folderSortMode == 1) SortFolderIntoSubfolders(root);
    }

    g_App.lastFolder = root;
    ClearWorkspace();

    if (!structured && g_App.folderSortMode == 2)
    {
        // "Open as-is": the same as selecting every file in the folder and
        // dropping it on the window -- nothing moved, assembled or written.
        // Each directory inside it comes in as a folder of survivors.
        std::vector<std::string> files = ListDirFiles(root);
        size_t count = files.size();
        ImportFiles(files);
        for (const std::string& sub : ListSubDirs(root))
        {
            if (CleanFolderName(sub).empty() || !LooksLikeSurvivorFolder(JoinPath(root, sub))) continue;
            const std::vector<std::string> inside = ListDirFiles(JoinPath(root, sub));
            EnsureFolder(sub);
            ImportFiles(inside, sub);
            count += inside.size();
        }
        if (!g_App.players.empty())
        {
            const std::vector<Ref> rows = FlatRows();
            Ref r = rows.empty() ? Ref() : rows[0];
            if (r.Valid()) { r.slot = g_App.players[r.player].FirstLoaded(); }
            if (r.Valid() && r.slot >= 0) { g_App.selection.assign(1, r); g_App.anchor = r; OpenInEditor(r); }
        }
        g_App.status = FileNameOf(root) + ": opened as-is, " + std::to_string((int)g_App.players.size())
                     + " group(s)" + (g_App.folders.empty() ? std::string()
                                      : " in " + std::to_string((int)g_App.folders.size()) + " folder(s)")
                     + " from " + std::to_string((int)count) + " file(s)";
        return;
    }

    int assembled = 0, disassembled = 0;

    // One level of the tree: the top of assembly / survivors when `sub` is
    // empty, otherwise the folder of that name inside each of them.
    auto loadLevel = [&](const std::string& sub) {
        const std::string aAt = sub.empty() ? aDir : JoinPath(aDir, sub);
        const std::string sAt = sub.empty() ? sDir : JoinPath(sDir, sub);

        // A folder that exists on one side only gets its twin when the option
        // that would fill it is on, so the two trees keep the same shape.
        if (!sub.empty())
        {
            if (DirExists(aDir) && DirExists(sAt) && !DirExists(aAt) && g_App.folderDisassemble) EnsureDir(aAt);
            if (DirExists(sDir) && DirExists(aAt) && !DirExists(sAt) && g_App.folderAssemble)    EnsureDir(sAt);
        }
        if (DirExists(aAt) && DirExists(sAt)) SyncFolderPair(aAt, sAt, assembled, disassembled);

        // Binaries define who runs; the matching source, if any, replaces the
        // disassembly so the editor shows what the author actually wrote.
        if (DirExists(sAt))
        {
            std::vector<std::string> bins;
            for (const std::string& p : ListDirFiles(sAt))
                if (ExtOf(FileNameOf(p)).empty()) bins.push_back(p);
            LoadBinarySurvivorFiles(bins, sub);
        }

        if (DirExists(aAt))
        {
            for (const std::string& p : ListDirFiles(aAt))
            {
                if (ExtOf(FileNameOf(p)) != ".asm") continue;
                std::string text;
                if (!ReadTextFile(p, text)) continue;

                const std::string stem = StemOf(FileNameOf(p));
                Survivor* found = NULL;
                for (Player& pl : g_App.players)
                {
                    if (ToLower(pl.folder) != ToLower(sub)) continue;   // the same name in another folder is another warrior
                    for (int k = 0; k < kSlotCount && !found; k++)
                        if (pl.slots[k].present && StemOf(pl.slots[k].name) == stem) found = &pl.slots[k];
                }

                if (found)
                {
                    found->code = text;
                    found->path = p;                   // saving edits the source, not the binary
                    found->dirty = false;
                    ReassembleSurvivor(*found);
                }
                else
                {
                    LoadSurvivorFiles(std::vector<std::string>(1, p), sub);
                }
            }
        }
    };

    loadLevel(std::string());

    // Folders of survivors: every directory inside survivors/ or assembly/.
    {
        std::vector<std::string> subs = ListSubDirs(sDir);
        for (const std::string& s : ListSubDirs(aDir))
        {
            bool have = false;
            for (const std::string& t : subs) if (ToLower(t) == ToLower(s)) have = true;
            if (!have) subs.push_back(s);
        }
        std::sort(subs.begin(), subs.end(), [](const std::string& a, const std::string& b) {
            return ToLower(a) < ToLower(b);
        });
        for (const std::string& sub : subs)
        {
            if (CleanFolderName(sub).empty()) continue;
            EnsureFolder(sub);
            loadLevel(sub);
        }
    }

    if (DirExists(zDir)) LoadZombieFolder(zDir);

    // Every source-only survivor still needs assembling before it can run.
    for (Player& pl : g_App.players)
        for (int k = 0; k < kSlotCount; k++)
            if (pl.slots[k].present && pl.slots[k].bin.empty()) ReassembleSurvivor(pl.slots[k]);

    if (!g_App.players.empty())
    {
        // the first row the groups bar shows, which with folders is not player 0
        const std::vector<Ref> rows = FlatRows();
        Ref r; r.player = rows.empty() ? 0 : rows[0].player;
        r.slot = g_App.players[r.player].FirstLoaded();
        if (r.slot >= 0) { g_App.selection.assign(1, r); g_App.anchor = r; OpenInEditor(r); }
    }

    char buf[224];
    snprintf(buf, sizeof(buf), "%s: %d group(s)%s, %d zombie(s)%s%s",
             FileNameOf(root).c_str(), (int)g_App.players.size(),
             g_App.folders.empty() ? "" : (" in " + std::to_string((int)g_App.folders.size()) + " folder(s)").c_str(),
             (int)g_App.zombies.size(),
             assembled    ? (" - assembled "    + std::to_string(assembled)).c_str()    : "",
             disassembled ? (" - disassembled " + std::to_string(disassembled)).c_str() : "");
    g_App.status = buf;
}

static void OpenWorkspaceFolder()
{
    const std::string dir = BrowseForFolder(g_Hwnd, "Open a CoreWars folder");
    if (!dir.empty()) LoadWorkspaceFolder(dir);
}

// The old two-dialog flow, kept as an escape hatch for files that are not laid
// out as a folder.
static void ImportLooseFiles()
{
    std::vector<std::string> paths = OpenBinaryDialog(g_Hwnd);
    std::vector<std::string> src, bin;
    for (const std::string& p : paths)
        (ExtOf(FileNameOf(p)) == ".asm" ? src : bin).push_back(p);
    if (!bin.empty()) LoadBinarySurvivorFiles(bin);
    if (!src.empty())
    {
        LoadSurvivorFiles(src);
        for (Player& pl : g_App.players)
            for (int k = 0; k < kSlotCount; k++)
                if (pl.slots[k].present && pl.slots[k].bin.empty()) ReassembleSurvivor(pl.slots[k]);
    }
}

// -----------------------------------------------------------------------------
// Folders of groups, on disk
//
// A folder in the groups bar is a directory: survivors/<folder> and
// assembly/<folder> in a sorted workspace, <folder> itself in a flat one. Filing
// a group therefore moves its files, so the arrangement is still there the next
// time the folder is opened. Survivors that are not files of the open workspace
// -- new ones, or ones imported from somewhere else -- are only labelled.
// -----------------------------------------------------------------------------
struct WsPlace
{
    bool        ok = false;
    std::string root;      // the open workspace
    std::string base;      // the directory folders hang off: root, or root\survivors, root\assembly
    std::string kind;      // "assembly" / "survivors", or empty in a flat workspace
    std::string sub;       // the folder the file is in now, or empty
    std::string file;
};

static WsPlace PlaceOf(const std::string& path)
{
    WsPlace w;
    std::string root = g_App.lastFolder;
    while (!root.empty() && (root.back() == '\\' || root.back() == '/')) root.pop_back();
    if (root.empty() || path.size() < root.size() + 2) return w;
    if (ToLower(path.substr(0, root.size())) != ToLower(root)) return w;
    if (path[root.size()] != '\\' && path[root.size()] != '/') return w;

    std::vector<std::string> parts;
    std::string cur;
    for (size_t i = root.size() + 1; i <= path.size(); i++)
    {
        if (i == path.size() || path[i] == '\\' || path[i] == '/')
        {
            if (!cur.empty()) parts.push_back(cur);
            cur.clear();
        }
        else cur.push_back(path[i]);
    }
    if (parts.empty()) return w;

    size_t at = 0;
    w.root = root;
    w.base = root;
    const std::string first = ToLower(parts[0]);
    if (parts.size() >= 2 && (first == kAsmDir || first == kSurvDir))
    {
        w.kind = first;
        w.base = JoinPath(root, parts[0]);
        at = 1;
    }
    // In a sorted workspace only what is under assembly / survivors is loaded
    // from it, so a file lying anywhere else there is not part of the layout.
    if (w.kind.empty() && (DirExists(JoinPath(root, kAsmDir)) || DirExists(JoinPath(root, kSurvDir)) ||
                           DirExists(JoinPath(root, kZombDir))))
        return w;
    const size_t depth = parts.size() - 1 - at;       // directories between the base and the file
    if (depth > 1) return w;                          // deeper than one folder: not ours to move
    if (depth == 1) w.sub = parts[at];
    if (IsLayoutDirName(w.sub)) return w;             // zombies\x in a sorted workspace
    w.file = parts.back();
    w.ok = true;
    return w;
}

static bool PathExists(const std::string& p)
{
    return GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES;
}

// Where the survivor's file would go, or empty when filing it moves nothing.
static std::string RefileTarget(const Survivor& sv, const std::string& folder)
{
    if (sv.path.empty()) return std::string();
    const WsPlace at = PlaceOf(sv.path);
    if (!at.ok || ToLower(at.sub) == ToLower(folder)) return std::string();
    return JoinPath(folder.empty() ? at.base : JoinPath(at.base, folder), at.file);
}

static bool RefileSurvivor(Survivor& sv, const std::string& folder, std::string& err)
{
    const std::string dst = RefileTarget(sv, folder);
    if (dst.empty()) return true;
    const WsPlace at = PlaceOf(sv.path);

    if (!EnsureDir(ParentDir(dst))) { err = "could not create " + ParentDir(dst); return false; }
    if (!MoveFileA(sv.path.c_str(), dst.c_str())) { err = "could not move " + at.file; return false; }
    const std::string oldDir = ParentDir(sv.path);
    sv.path = dst;

    // The other form of the same warrior -- the binary beside its source --
    // goes with it, so the two trees keep the same shape.
    if (!at.kind.empty())
    {
        const bool isAsm = (at.kind == kAsmDir);
        const std::string otherBase = JoinPath(at.root, isAsm ? kSurvDir : kAsmDir);
        const std::string otherName = isAsm ? StemOf(at.file) : StemOf(at.file) + ".asm";
        const std::string otherOldDir = at.sub.empty() ? otherBase : JoinPath(otherBase, at.sub);
        const std::string otherNewDir = folder.empty() ? otherBase : JoinPath(otherBase, folder);
        const std::string from = JoinPath(otherOldDir, otherName), to = JoinPath(otherNewDir, otherName);
        if (PathExists(from) && !PathExists(to) && EnsureDir(otherNewDir))
            MoveFileA(from.c_str(), to.c_str());
        if (!at.sub.empty()) RemoveDirectoryA(otherOldDir.c_str());   // only goes once it is empty
    }
    if (!at.sub.empty()) RemoveDirectoryA(oldDir.c_str());
    return true;
}

// The directories a folder of the open workspace can exist under.
static std::vector<std::string> FolderBases()
{
    std::vector<std::string> out;
    const std::string root = g_App.lastFolder;
    if (root.empty() || !DirExists(root)) return out;
    const std::string a = JoinPath(root, kAsmDir), s = JoinPath(root, kSurvDir);
    if (DirExists(a)) out.push_back(a);
    if (DirExists(s)) out.push_back(s);
    if (out.empty()) out.push_back(root);
    return out;
}

static void MovePlayersToFolder(const std::vector<int>& players, const std::string& folderRaw)
{
    FlushEditorToModel();
    std::string folder;
    if (!folderRaw.empty())
    {
        folder = CleanFolderName(folderRaw);
        if (folder.empty()) { g_App.status = "\"" + folderRaw + "\" is not a usable folder name"; return; }
        folder = g_App.folders[EnsureFolder(folder)].name;       // an existing folder keeps its spelling
    }

    int moved = 0, files = 0;
    std::string firstErr;
    for (int pi : players)
    {
        if (pi < 0 || pi >= (int)g_App.players.size()) continue;
        Player& pl = g_App.players[pi];
        if (ToLower(pl.folder) == ToLower(folder)) { pl.folder = folder; continue; }

        // Refuse rather than half-move: a group of that name already there, or
        // a file in the way, and this group stays where it is.
        std::string why;
        for (int q = 0; q < (int)g_App.players.size() && why.empty(); q++)
            if (q != pi && ToLower(g_App.players[q].name) == ToLower(pl.name) &&
                ToLower(g_App.players[q].folder) == ToLower(folder))
                why = (folder.empty() ? std::string("there is already a loose group")
                                      : folder + " already has a group") + " called " + pl.name;
        for (const Survivor& sv : pl.slots)
        {
            if (!sv.present || !why.empty()) continue;
            const std::string dst = RefileTarget(sv, folder);
            if (!dst.empty() && PathExists(dst)) why = FileNameOf(dst) + " already exists in " + FileNameOf(ParentDir(dst));
        }
        if (!why.empty()) { if (firstErr.empty()) firstErr = why; continue; }

        for (Survivor& sv : pl.slots)
        {
            if (!sv.present) continue;
            const std::string before = sv.path;
            std::string e;
            if (!RefileSurvivor(sv, folder, e)) { if (firstErr.empty()) firstErr = e; }
            else if (sv.path != before) files++;
        }
        pl.folder = folder;
        moved++;
    }

    std::string s = "Moved " + std::to_string(moved) + " group(s) " +
                    (folder.empty() ? std::string("out of their folder") : "into " + folder);
    if (files) s += "  (" + std::to_string(files) + " file(s) moved on disk)";
    if (!firstErr.empty()) s += (moved ? "  -- skipped some: " : "  -- ") + firstErr;
    g_App.status = (moved || !firstErr.empty()) ? s : "Already there";
}

// Forget a folder that has nobody left in it, and its (empty) directories.
static void DropFolderIfEmpty(const std::string& name)
{
    const int fi = FolderIndex(name);
    if (fi < 0 || !PlayersInFolder(name).empty()) return;
    for (const std::string& base : FolderBases())
        RemoveDirectoryA(JoinPath(base, name).c_str());      // refuses unless empty, which is the point
    g_App.folders.erase(g_App.folders.begin() + fi);
}

static void RenameGroupFolder(int fi, const std::string& raw)
{
    if (fi < 0 || fi >= (int)g_App.folders.size()) return;
    const std::string name = CleanFolderName(raw);
    if (name.empty()) { g_App.status = "\"" + raw + "\" is not a usable folder name"; return; }
    const std::string old = g_App.folders[fi].name;
    if (name == old) return;
    const std::vector<int> members = PlayersInFolder(old);

    const int other = FolderIndex(name);
    if (other >= 0 && other != fi)
    {
        // renamed onto a folder that exists: the two become one
        MovePlayersToFolder(members, g_App.folders[other].name);
        DropFolderIfEmpty(old);
        return;
    }
    if (ToLower(name) == ToLower(old))
    {
        // only the capitals changed; Windows sees the same directory
        g_App.folders[fi].name = name;
        for (int p : members) g_App.players[p].folder = name;
        g_App.status = "Renamed to " + name;
        return;
    }

    // An empty directory moves in one go; anything with files in it is refiled
    // survivor by survivor so each one's path is known afterwards.
    if (members.empty())
        for (const std::string& base : FolderBases())
            if (DirExists(JoinPath(base, old)) && !PathExists(JoinPath(base, name)))
                MoveFileA(JoinPath(base, old).c_str(), JoinPath(base, name).c_str());
    g_App.folders[fi].name = name;
    MovePlayersToFolder(members, name);
    for (const std::string& base : FolderBases())
        RemoveDirectoryA(JoinPath(base, old).c_str());
    // anyone who could not be moved is still in the old folder, on disk and here
    if (!PlayersInFolder(old).empty()) EnsureFolder(old);
    else                               g_App.status = "Renamed " + old + " to " + name;
}

static void RunFolderOp()
{
    FolderOp op = g_FolderOp;
    g_FolderOp = FolderOp();
    AppState& S = g_App;
    switch (op.kind)
    {
    case FolderOp::None: return;

    case FolderOp::Move:
        MovePlayersToFolder(op.players, op.name);
        if (!op.name.empty())
        {
            const int fi = FolderIndex(CleanFolderName(op.name));
            if (fi >= 0) S.folders[fi].collapsed = false;      // show where they went
        }
        break;

    case FolderOp::Rename:
        RenameGroupFolder(op.folder, op.name);
        break;

    case FolderOp::New:
    {
        std::string name = CleanFolderName(op.name);
        for (int n = 1; name.empty() || FolderIndex(name) >= 0; n++)
            name = "Folder " + std::to_string(n);
        EnsureFolder(name);
        S.status = "Created " + name + " -- drag groups onto it, or right-click it to rename";
        break;
    }

    case FolderOp::Ungroup:
    {
        if (op.folder < 0 || op.folder >= (int)S.folders.size()) break;
        const std::string name = S.folders[op.folder].name;
        MovePlayersToFolder(PlayersInFolder(name), std::string());
        DropFolderIfEmpty(name);
        break;
    }

    case FolderOp::Close:
    {
        // Out of the workspace, like closing a group: nothing on disk is touched.
        if (op.folder < 0 || op.folder >= (int)S.folders.size()) break;
        const std::string name = S.folders[op.folder].name;
        std::vector<int> members = PlayersInFolder(name);
        FlushEditorToModel();
        for (size_t i = members.size(); i-- > 0; )
            S.players.erase(S.players.begin() + members[i]);
        S.folders.erase(S.folders.begin() + op.folder);
        S.selection.clear();
        S.anchor = Ref();
        S.open = Ref();
        S.editor.SetText("");
        S.editor.SetReadOnly(true);
        S.status = "Closed " + name + "  (" + std::to_string((int)members.size()) + " group(s))";
        break;
    }
    }
}

// -----------------------------------------------------------------------------
// Zombie section of the groups bar
// -----------------------------------------------------------------------------
static void AddZombieFiles()
{
    for (const std::string& p : OpenBinaryDialog(g_Hwnd))
    {
        std::vector<unsigned char> bytes;
        if (!ReadBinaryFile(p, bytes)) continue;
        if (bytes.size() > 512) bytes.resize(512);
        Zombie z;
        z.speed = g_App.zombieSpeed;          // the default; each row can differ
        z.name  = StemOf(FileNameOf(p));
        z.tint  = kPlayerTints[(g_App.zombies.size() + 3) % (sizeof(kPlayerTints) / sizeof(kPlayerTints[0]))];
        z.sv.name = FileNameOf(p);
        z.sv.path = p;
        ListSurvivor(z.sv, bytes);
        z.sv.present = true;
        g_App.zombies.push_back(std::move(z));
    }
}

static float g_ZombieDragAcc = 0.0f;   // pixels banked while dragging a speed pill

// A zombie written from scratch, exactly the way New player seeds a survivor.
static void NewZombie()
{
    Zombie z;
    char n[48];
    snprintf(n, sizeof(n), "Zombie %d", (int)g_App.zombies.size() + 1);
    z.name  = n;
    z.tint  = kPlayerTints[(g_App.zombies.size() + 3) % (sizeof(kPlayerTints) / sizeof(kPlayerTints[0]))];
    z.speed = g_App.zombieSpeed;
    z.sv.name    = "zombie.asm";
    z.sv.code    = kTemplate;
    z.sv.present = true;
    z.sv.dirty   = true;
    ReassembleSurvivor(z.sv);
    g_App.zombies.push_back(std::move(z));

    Ref r; r.player = kZombiePlayer; r.slot = (int)g_App.zombies.size() - 1;
    g_App.selection.assign(1, r);
    g_App.anchor = r;
    OpenInEditor(r);
    g_App.status = std::string("Created ") + n;
}

// One zombie as a single chip, for the collapsed groups strip. Zombies have no
// second survivor, so the row is one chip wide with the same gestures.
static void ZombieChip(int zi, float size)
{
    AppState& S = g_App;
    if (zi < 0 || zi >= (int)S.zombies.size()) return;
    Zombie& z = S.zombies[zi];
    Ref ref; ref.player = kZombiePlayer; ref.slot = zi;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::PushID(4000 + zi);
    BtnState b = BareButton("##zchip", ImVec2(size, size));

    bool clicked = b.clicked;
    if (CheckLongPress(clicked)) ToggleRefEnabled(ref);

    const bool selected = IsSelected(ref);
    ImU32 bg, fg, border = 0;
    if (!z.sv.enabled)      { bg = HexU(Col::Bg2, 0.55f); fg = HexU(Col::TextDim); }
    else if (selected)      { bg = HexU(b.held ? Col::Bg4 : (b.hovered ? Col::Bg3 : Col::Bg2));
                              fg = HexU(z.tint); border = HexU(z.tint, 0.95f); }
    else                    { bg = HexU(z.tint, b.hovered ? 1.0f : 0.82f); fg = HexU(Col::Bg0); }

    const float r = 6.0f * g_Scale;
    dl->AddRectFilled(b.mn, b.mx, bg, r);
    if (border) dl->AddRect(b.mn, b.mx, border, r, 0, 1.4f);
    const char* lbl = z.heavy ? "H" : "Z";
    const ImVec2 ts = ImGui::CalcTextSize(lbl);
    dl->AddText(ImVec2((b.mn.x + b.mx.x - ts.x) * 0.5f, (b.mn.y + b.mx.y - ts.y) * 0.5f), fg, lbl);
    DrawLongPressHint(b.mn, b.mx);

    if (b.hovered) ImGui::SetTooltip("%s  x%d%s", z.name.c_str(), z.speed,
                                     z.heavy ? "  (heavy)" : "");
    if (clicked) HandleRowClick(ref, true);
    ImGui::PopID();
}

static void DrawZombieSection()
{
    AppState& S = g_App;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImGui::Dummy(ImVec2(0, 8 * g_Scale));

    // --- header ------------------------------------------------------------
    {
        const ImVec2 o = ImGui::GetCursorScreenPos();
        const float  w = ImGui::GetContentRegionAvail().x;
        const float  h = 20.0f * g_Scale;
        dl->AddLine(ImVec2(o.x, o.y), ImVec2(o.x + w, o.y), HexU(Col::Line));
        dl->AddText(ImVec2(o.x + 2 * g_Scale, o.y + 5 * g_Scale), HexU(Col::TextDim), "ZOMBIES");

        const float bs = 16.0f * g_Scale;
        ImGui::SetCursorScreenPos(ImVec2(o.x + w - bs, o.y + 4 * g_Scale));
        BtnState b = BareButton("##znew", ImVec2(bs, bs));
        if (b.hovered) dl->AddRectFilled(b.mn, b.mx, HexU(Col::Bg3), 4 * g_Scale);
        DrawGlyphPlus(dl, ImVec2((b.mn.x + b.mx.x) * 0.5f, (b.mn.y + b.mx.y) * 0.5f), 4 * g_Scale,
                      HexU(Col::TextDim));
        if (b.hovered) ImGui::SetTooltip("New zombie");
        if (b.clicked) NewZombie();

        ImGui::SetCursorScreenPos(ImVec2(o.x + w - bs * 2 - 4 * g_Scale, o.y + 4 * g_Scale));
        b = BareButton("##zadd", ImVec2(bs, bs));
        if (b.hovered) dl->AddRectFilled(b.mn, b.mx, HexU(Col::Bg3), 4 * g_Scale);
        DrawGlyphFolder(dl, ImVec2((b.mn.x + b.mx.x) * 0.5f, (b.mn.y + b.mx.y) * 0.5f), 4 * g_Scale,
                        HexU(Col::TextDim));
        if (b.hovered) ImGui::SetTooltip("Load zombie binaries");
        if (b.clicked) AddZombieFiles();

        ImGui::SetCursorScreenPos(ImVec2(o.x, o.y + h + 3 * g_Scale));
    }

    if (S.zombies.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, Hex(Col::TextDim));
        ImGui::TextWrapped("None yet.");
        ImGui::PopStyleColor();
        return;
    }

    // --- rows --------------------------------------------------------------
    const float rowH = 24.0f * g_Scale;
    for (int zi = 0; zi < (int)S.zombies.size(); zi++)
    {
        Zombie& z = S.zombies[zi];
        Ref ref; ref.player = kZombiePlayer; ref.slot = zi;

        ImGui::PushID(2000 + zi);
        const float avail = ImGui::GetContentRegionAvail().x;
        const ImVec2 rowStart = ImGui::GetCursorScreenPos();

        bool clicked = ImGui::Selectable("##zrow", IsSelected(ref), ImGuiSelectableFlags_AllowOverlap,
                                         ImVec2(avail, rowH));
        const ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
        if (CheckLongPress(clicked)) ToggleRefEnabled(ref);
        DrawLongPressHint(mn, mx);

        // Attached here, not further down: BeginPopupContextItem hangs off
        // whatever item was submitted last, and below this point that is the
        // speed pill rather than the row.
        if (ImGui::BeginPopupContextItem("##zctx"))
        {
            if (ImGui::IsWindowAppearing())
                snprintf(g_RenameBuf, sizeof(g_RenameBuf), "%s", z.name.c_str());
            ImGui::SetNextItemWidth(180 * g_Scale);
            if (ImGui::InputText("##zrename", g_RenameBuf, sizeof(g_RenameBuf),
                                 ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
            {
                if (g_RenameBuf[0]) { z.name = g_RenameBuf; S.status = "Renamed to " + z.name; }
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere(-1);

            float zc[3] = { ((z.tint >> 16) & 0xFF) / 255.0f,
                            ((z.tint >>  8) & 0xFF) / 255.0f,
                            ( z.tint        & 0xFF) / 255.0f };
            if (ImGui::ColorEdit3("Colour", zc,
                                  ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueWheel))
                z.tint = ((unsigned)(zc[0] * 255.0f + 0.5f) << 16) |
                         ((unsigned)(zc[1] * 255.0f + 0.5f) <<  8) |
                          (unsigned)(zc[2] * 255.0f + 0.5f);
            ImGui::Separator();
            ImGui::MenuItem("Heavy (double speed)", NULL, &z.heavy);
            if (ImGui::MenuItem(z.sv.enabled ? "Disable" : "Enable")) ToggleRefEnabled(ref);
            if (ImGui::MenuItem("Remove"))
            {
                if (S.open == ref) { S.open = Ref(); S.editor.SetText(""); S.editor.SetReadOnly(true); }
                S.zombies.erase(S.zombies.begin() + zi);
                ImGui::EndPopup();
                ImGui::PopID();
                break;
            }
            ImGui::EndPopup();
        }


        dl->AddRectFilled(ImVec2(mn.x, mn.y + 3 * g_Scale), ImVec2(mn.x + 3 * g_Scale, mx.y - 3 * g_Scale),
                          HexU(z.tint, z.sv.enabled ? 1.0f : 0.30f), 1.5f * g_Scale);

        // Speed reads as a small pill rather than a framed slider -- a 24px row
        // has no room for one, and drag-to-change is the same gesture anyway.
        char sp[8];
        snprintf(sp, sizeof(sp), "x%d", z.speed);
        const float pillW = ImMax(ImGui::CalcTextSize("x8").x + 12 * g_Scale, 26.0f * g_Scale);
        const float pillH = ImGui::GetTextLineHeight() + 4 * g_Scale;

        const ImVec2 nsz = ImGui::CalcTextSize(z.name.c_str());
        const float tx = mn.x + 12 * g_Scale, tyy = mn.y + (rowH - nsz.y) * 0.5f;
        dl->PushClipRect(mn, ImVec2(mx.x - pillW - 12 * g_Scale, mx.y), true);
        dl->AddText(ImVec2(tx, tyy), z.sv.enabled ? HexU(Col::Text) : HexU(Col::TextDim), z.name.c_str());
        if (!z.sv.enabled)
            dl->AddLine(ImVec2(tx, tyy + nsz.y * 0.55f), ImVec2(tx + nsz.x, tyy + nsz.y * 0.55f),
                        HexU(Col::Danger, 0.85f), 1.2f);
        dl->PopClipRect();

        if (ImGui::IsItemHovered() && !ImGui::IsAnyItemActive())
            ImGui::SetTooltip("%s%s\n\nClick: open in the editor\nRight-click: rename, colour, heavy\nHold: %s",
                              z.name.c_str(), z.heavy ? "  (heavy - double speed)" : "",
                              z.sv.enabled ? "disable" : "enable");

        // submitted after the row so it wins the click
        ImGui::SetCursorScreenPos(ImVec2(mx.x - pillW - 4 * g_Scale, mn.y + (rowH - pillH) * 0.5f));
        BtnState pb = BareButton("##zsp", ImVec2(pillW, pillH));
        dl->AddRectFilled(pb.mn, pb.mx, HexU(z.heavy ? Col::Warn : Col::Bg3, pb.hovered ? 0.9f : 0.6f),
                          pillH * 0.5f);
        const ImVec2 psz = ImGui::CalcTextSize(sp);
        dl->AddText(ImVec2((pb.mn.x + pb.mx.x - psz.x) * 0.5f, (pb.mn.y + pb.mx.y - psz.y) * 0.5f),
                    HexU(z.heavy ? Col::Bg0 : Col::Text), sp);
        if (ImGui::IsItemActive())
        {
            g_ZombieDragAcc += ImGui::GetIO().MouseDelta.x;
            const float stepPx = 8.0f * g_Scale;
            while (g_ZombieDragAcc >=  stepPx) { z.speed = ImMin(8, z.speed + 1); g_ZombieDragAcc -= stepPx; }
            while (g_ZombieDragAcc <= -stepPx) { z.speed = ImMax(1, z.speed - 1); g_ZombieDragAcc += stepPx; }
        }
        else if (ImGui::IsItemDeactivated()) g_ZombieDragAcc = 0.0f;
        if (pb.hovered)
            ImGui::SetTooltip("Cycles this zombie gets per turn: %d%s\n\nDrag sideways to change",
                              z.speed, z.heavy ? "   (heavy doubles it)" : "");

        if (clicked)
        {
            HandleRowClick(ref, true);
            // zombies are warriors like any other, so the inspector follows too
            if (S.game.ready())
                for (int k = 0; k < S.game.numWarriors(); k++)
                    if (S.game.replay().warriorName[(size_t)k] == z.name)
                    { S.inspectWarrior = k; S.status = "Inspecting " + z.name; break; }
        }
        ImGui::PopID();
        ImGui::SetCursorScreenPos(ImVec2(rowStart.x, mx.y + 3 * g_Scale));
    }
    ImGui::Dummy(ImVec2(0.0f, 0.0f));
}

// -----------------------------------------------------------------------------
// Helpers the options window and the open-folder flow share
// -----------------------------------------------------------------------------
static void RelistAllSurvivors()
{
    for (Player& pl : g_App.players)
        for (int k = 0; k < kSlotCount; k++)
        {
            Survivor& sv = pl.slots[k];
            if (sv.present && !sv.bin.empty() && !sv.dirty) ListSurvivor(sv, sv.bin);
        }
    for (Zombie& z : g_App.zombies)
        if (z.sv.present && !z.sv.bin.empty() && !z.sv.dirty) ListSurvivor(z.sv, z.sv.bin);
    Ref keep = g_App.open; g_App.open = Ref(); OpenInEditor(keep);
}

static void DrawSortPrompt()
{
    if (!g_SortPromptRoot.empty() && !ImGui::IsPopupOpen("Sort this folder?"))
        ImGui::OpenPopup("Sort this folder?");

    ImGui::SetNextWindowSize(ImVec2(430 * g_Scale, 0), ImGuiCond_Always);
    if (!ImGui::BeginPopupModal("Sort this folder?", NULL, ImGuiWindowFlags_NoResize)) return;

    ImGui::TextWrapped("%s has no assembly / survivors / zombies folders.",
                       FileNameOf(g_SortPromptRoot).c_str());
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, Hex(Col::TextDim));
    ImGui::TextWrapped("Sorting moves .asm files into assembly, extension-less files into survivors, "
                       "and extension-less files whose name starts with \"zom\" into zombies. "
                       "A folder inside it that holds nothing but warriors is sorted the same way and "
                       "stays a folder of survivors. Everything else is left where it is.");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    static bool dontAsk = false;
    ImGui::Checkbox("Do not ask again", &dontAsk);
    ImGui::Separator();

    const std::string root = g_SortPromptRoot;
    if (ImGui::Button("Sort and open", ImVec2(150 * g_Scale, 0)))
    {
        if (dontAsk) g_App.folderSortMode = 1;
        g_SortPromptRoot.clear();
        const int moved = SortFolderIntoSubfolders(root);
        LoadWorkspaceFolder(root);
        g_App.status += "  (sorted " + std::to_string(moved) + " file(s))";
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Open as-is", ImVec2(120 * g_Scale, 0)))
    {
        if (dontAsk) g_App.folderSortMode = 2;
        g_SortPromptRoot.clear();
        const int keep = g_App.folderSortMode;
        g_App.folderSortMode = 2;                 // one-shot: read the folder flat
        LoadWorkspaceFolder(root);
        g_App.folderSortMode = keep;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(90 * g_Scale, 0)))
    {
        g_SortPromptRoot.clear();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

// =============================================================================
//  Options window
//
//  A rail of sections down the left and the chosen section on the right, as a
//  column of cards. Every setting is one row of a card: what it is -- and,
//  dimmer underneath, what it does -- on the left, its control on the right.
//  The helpers below are that vocabulary; DrawOptionsWindow only spells the
//  settings out with it.
// =============================================================================

// Theme changes asked for while the window is being drawn are applied after it
// is closed: ApplyTheme() rebuilds the whole style, which must not happen with
// this window's own style pushes still on the stack.
static int g_OptThemeAction = -2;          // -2 nothing, -1 re-apply the palette, >= 0 that preset

static bool EditColour(const char* id, unsigned* v, ImGuiColorEditFlags flags = 0)
{
    float c[3] = { ((*v >> 16) & 0xFF) / 255.0f,
                   ((*v >>  8) & 0xFF) / 255.0f,
                   ( *v        & 0xFF) / 255.0f };
    if (!ImGui::ColorEdit3(id, c, flags)) return false;
    *v = ((unsigned)(c[0] * 255.0f + 0.5f) << 16) |
         ((unsigned)(c[1] * 255.0f + 0.5f) <<  8) |
          (unsigned)(c[2] * 255.0f + 0.5f);
    return true;
}

// Explanatory text: dim, a little smaller than the labels, wrapped.
static void OptNote(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    ImGui::PushFont(NULL, ImGui::GetFontSize() * 0.9f);
    ImGui::PushStyleColor(ImGuiCol_Text, Hex(Col::TextDim));
    ImGui::TextWrappedV(fmt, args);
    ImGui::PopStyleColor();
    ImGui::PopFont();
    va_end(args);
}

// The heading of a whole section, at the top of the right-hand side.
static void OptSectionTitle(const char* title, const char* blurb)
{
    ImGui::PushFont(NULL, ImGui::GetFontSize() * 1.45f);
    ImGui::TextUnformatted(title);
    ImGui::PopFont();
    OptNote("%s", blurb);
    ImGui::Dummy(ImVec2(0, 2 * g_Scale));
}

// ---- cards ------------------------------------------------------------------
static int g_OptRows = 0;                  // rows drawn so far in the open card

// A titled, rounded panel that grows to fit what is put in it. Always paired
// with OptCardEnd().
static void OptCard(const char* title)
{
    ImGui::Dummy(ImVec2(0, 5 * g_Scale));
    ImGui::PushFont(NULL, ImGui::GetFontSize() * 0.86f);
    ImGui::TextColored(Hex(Col::TextDim), "%s", title);
    ImGui::PopFont();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, Hex(Col::Bg1));
    ImGui::PushStyleColor(ImGuiCol_Border, Hex(Col::Line));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f * g_Scale);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14 * g_Scale, 10 * g_Scale));
    ImGui::BeginChild(title, ImVec2(0, 0),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY |
                      ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    // all four are read when the child begins; nothing inside should inherit them
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
    g_OptRows = 0;
}

static void OptCardEnd()
{
    ImGui::EndChild();
}

// The hairline between two rows of a card; nothing above the first.
static void OptDivide()
{
    if (g_OptRows++ == 0) return;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float  w = ImGui::GetContentRegionAvail().x;
    ImGui::GetWindowDrawList()->AddLine(ImVec2(p.x, p.y + 2 * g_Scale), ImVec2(p.x + w, p.y + 2 * g_Scale),
                                        HexU(Col::Line, 0.65f));
    ImGui::Dummy(ImVec2(0, 5 * g_Scale));
}

// One setting. Draws the text half and leaves the cursor where a control
// `ctrlW` wide belongs, level with the middle of the text. Returns true when
// the text half was clicked, so a switch can be flipped from its label.
static bool OptRow(const char* label, const char* note, float ctrlW)
{
    OptDivide();
    const float  full   = ImGui::GetContentRegionAvail().x;
    const ImVec2 start  = ImGui::GetCursorScreenPos();
    const float  textW  = ImMax(90.0f * g_Scale, full - ctrlW - 18.0f * g_Scale);
    const float  frameH = ImGui::GetFrameHeight();
    const bool   hasNote = note && note[0];

    ImGui::BeginGroup();
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + textW);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 2 * g_Scale));
    // with nothing under it, the label sits level with its control
    if (!hasNote) ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    if (hasNote)
    {
        ImGui::PushFont(NULL, ImGui::GetFontSize() * 0.9f);
        ImGui::PushStyleColor(ImGuiCol_Text, Hex(Col::TextDim));
        ImGui::TextUnformatted(note);
        ImGui::PopStyleColor();
        ImGui::PopFont();
    }
    ImGui::PopStyleVar();
    ImGui::PopTextWrapPos();
    ImGui::EndGroup();
    const bool  clicked = ImGui::IsItemClicked();
    const float groupH  = ImGui::GetItemRectSize().y;

    ImGui::SameLine();
    ImGui::SetCursorScreenPos(ImVec2(start.x + full - ctrlW,
                                     start.y + ImMax(0.0f, (groupH - frameH) * 0.5f)));
    ImGui::SetNextItemWidth(ctrlW);
    return clicked;
}

// ---- controls ---------------------------------------------------------------
static float ToggleSwitchWidth()
{
    return ImFloor(ImFloor(ImGui::GetFrameHeight() * 0.72f) * 1.85f);
}

// A sliding on/off switch. `flip` flips it from outside -- its label was clicked.
static bool ToggleSwitch(const char* id, bool* v, bool flip = false)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float fh = ImGui::GetFrameHeight();
    const float h  = ImFloor(fh * 0.72f), w = ToggleSwitchWidth();
    const ImVec2 at = ImGui::GetCursorScreenPos();

    const bool changed = ImGui::InvisibleButton(id, ImVec2(w, fh)) || flip;
    if (changed) *v = !*v;
    const bool hovered = ImGui::IsItemHovered();

    // the knob eases across rather than jumping
    ImGuiStorage* st = ImGui::GetStateStorage();
    const ImGuiID key = ImGui::GetItemID();
    float t = st->GetFloat(key, *v ? 1.0f : 0.0f);
    t = ImClamp(t + (*v ? 1.0f : -1.0f) * ImGui::GetIO().DeltaTime * 9.0f, 0.0f, 1.0f);
    st->SetFloat(key, t);

    const ImVec2 p(at.x, at.y + ImFloor((fh - h) * 0.5f));
    const ImVec4 track = ImLerp(Hex(Col::Bg4), Hex(Col::Accent), t);
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), ImGui::GetColorU32(track), h * 0.5f);
    if (hovered)
        dl->AddRect(p, ImVec2(p.x + w, p.y + h), HexU(Col::Accent, 0.6f), h * 0.5f, 0, 1.5f);
    dl->AddCircleFilled(ImVec2(p.x + h * 0.5f + t * (w - h), p.y + h * 0.5f),
                        h * 0.5f - 2.5f * g_Scale, HexU(0xFFFFFF), 24);
    return changed;
}

static bool OptToggle(const char* label, bool* v, const char* note = NULL)
{
    const bool flip = OptRow(label, note, ToggleSwitchWidth());
    ImGui::PushID(label);
    const bool changed = ToggleSwitch("##sw", v, flip);
    ImGui::PopID();
    return changed;
}

static bool OptSliderF(const char* label, float* v, float lo, float hi, const char* fmt, const char* note = NULL)
{
    OptRow(label, note, 200 * g_Scale);
    ImGui::PushID(label);
    const bool changed = ImGui::SliderFloat("##v", v, lo, hi, fmt);
    ImGui::PopID();
    return changed;
}

static bool OptSliderI(const char* label, int* v, int lo, int hi, const char* note = NULL)
{
    OptRow(label, note, 200 * g_Scale);
    ImGui::PushID(label);
    const bool changed = ImGui::SliderInt("##v", v, lo, hi);
    ImGui::PopID();
    return changed;
}

static bool OptColour(const char* label, unsigned* v, const char* note = NULL)
{
    OptRow(label, note, ImGui::GetFrameHeight() * 1.9f);
    ImGui::PushID(label);
    const bool changed = EditColour("##c", v, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
    ImGui::PopID();
    return changed;
}

// A row whose control is a button: the label says what, the button does it.
static bool OptAction(const char* label, const char* button, const char* note = NULL)
{
    const float w = ImGui::CalcTextSize(button).x + ImGui::GetStyle().FramePadding.x * 2.0f + 8 * g_Scale;
    OptRow(label, note, w);
    ImGui::PushID(label);
    const bool hit = ImGui::Button(button, ImVec2(w, 0));
    ImGui::PopID();
    return hit;
}

// One-of-several as a strip of joined buttons, which is what a short list of
// radio buttons really is.
static float SegmentedWidth(const char* const* items, int n)
{
    float w = 0.0f;
    for (int i = 0; i < n; i++) w += ImGui::CalcTextSize(items[i]).x + 22 * g_Scale;
    return w;
}

static bool Segmented(const char* id, int* v, const char* const* items, int n)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float h = ImGui::GetFrameHeight();
    const ImVec2 at = ImGui::GetCursorScreenPos();
    const float r = 6 * g_Scale;
    dl->AddRectFilled(at, ImVec2(at.x + SegmentedWidth(items, n), at.y + h), HexU(Col::Bg2), r);

    bool changed = false;
    ImGui::PushID(id);
    float x = at.x;
    for (int i = 0; i < n; i++)
    {
        const ImVec2 ts = ImGui::CalcTextSize(items[i]);
        const float  w  = ts.x + 22 * g_Scale;
        ImGui::SetCursorScreenPos(ImVec2(x, at.y));
        ImGui::PushID(i);
        const bool hit = ImGui::InvisibleButton("##seg", ImVec2(w, h));
        const bool hov = ImGui::IsItemHovered();
        ImGui::PopID();
        if (hit && *v != i) { *v = i; changed = true; }
        const bool on = (*v == i);
        if (on)       dl->AddRectFilled(ImVec2(x + 2, at.y + 2), ImVec2(x + w - 2, at.y + h - 2), HexU(Col::Accent, 0.9f), r - 1);
        else if (hov) dl->AddRectFilled(ImVec2(x + 2, at.y + 2), ImVec2(x + w - 2, at.y + h - 2), HexU(Col::Bg3), r - 1);
        dl->AddText(ImVec2(x + (w - ts.x) * 0.5f, at.y + (h - ts.y) * 0.5f),
                    HexU(on ? Col::Bg0 : (hov ? Col::Text : Col::TextDim)), items[i]);
        x += w;
    }
    ImGui::PopID();
    return changed;
}

static bool OptSegmented(const char* label, int* v, const char* const* items, int n, const char* note = NULL)
{
    OptRow(label, note, SegmentedWidth(items, n));
    return Segmented(label, v, items, n);
}

// A combo over the fonts folder. `file` holds a file name; empty is the
// built-in choice, shown as `builtIn`. Picking one only flags the swap --
// ReloadFonts() runs it before the next frame.
static void FontPicker(const char* id, char* file, size_t cap, const char* builtIn)
{
    const std::string preview = file[0] ? FontDisplayName(file) : std::string(builtIn) + " (default)";
    if (!ImGui::BeginCombo(id, preview.c_str())) return;
    char def[96];
    snprintf(def, sizeof(def), "%s (default)", builtIn);
    if (ImGui::Selectable(def, file[0] == 0) && file[0])
    {
        file[0] = 0;
        g_FontsDirty = true;
    }
    for (const std::string& f : ListFontFiles())
    {
        const bool cur = (f == file);
        const std::string name = FontDisplayName(f) + "##" + f;
        if (ImGui::Selectable(name.c_str(), cur) && !cur)
        {
            snprintf(file, cap, "%s", f.c_str());
            g_FontsDirty = true;
        }
        if (cur) ImGui::SetItemDefaultFocus();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", f.c_str());
    }
    ImGui::EndCombo();
}

// A theme as a thumbnail of itself: its background, two panels, a few lines of
// its text and its accent colours, with the name underneath.
static bool ThemeTile(int i, bool selected, float w)
{
    const ThemeDef& t = kThemes[i];
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float u = g_Scale;
    const float h = ImFloor(w * 0.58f);
    ImGui::PushID(i);
    const BtnState b = BareButton("##theme", ImVec2(w, h + ImGui::GetTextLineHeight() + 6 * u));
    ImGui::PopID();

    const ImVec2 mn = b.mn, mx(b.mn.x + w, b.mn.y + h);
    const float r = 8 * u;
    dl->AddRectFilled(mn, mx, HexU(t.c[0]), r);
    dl->AddRectFilled(ImVec2(mn.x + 6 * u, mn.y + 6 * u), ImVec2(mn.x + 30 * u, mx.y - 6 * u), HexU(t.c[1]), 4 * u);
    dl->AddRectFilled(ImVec2(mn.x + 34 * u, mn.y + 6 * u), ImVec2(mx.x - 6 * u, mx.y - 6 * u), HexU(t.c[1]), 4 * u);
    for (int k = 0; k < 3; k++)
        dl->AddRectFilled(ImVec2(mn.x + 10 * u, mn.y + (12 + k * 10) * u),
                          ImVec2(mn.x + 26 * u, mn.y + (18 + k * 10) * u),
                          HexU(t.c[k == 0 ? 8 : (k == 1 ? 9 : 10)]), 2 * u);
    const float px = mn.x + 40 * u;
    dl->AddRectFilled(ImVec2(px, mn.y + 13 * u), ImVec2(px + 34 * u, mn.y + 17 * u), HexU(t.c[6]), 2 * u);
    dl->AddRectFilled(ImVec2(px, mn.y + 23 * u), ImVec2(px + 52 * u, mn.y + 27 * u), HexU(t.c[7]), 2 * u);
    // a scrap of the theme's arena: its lines, its core, and two warriors
    {
        const float c = 7 * u, g = 1.5f * u;
        const ImVec2 o(px, mx.y - 6 * u - 2 * c - g * 3);
        dl->AddRectFilled(o, ImVec2(o.x + 4 * c + g * 5, o.y + 2 * c + g * 3), HexU(t.c[14]), 2 * u);
        for (int yy = 0; yy < 2; yy++)
            for (int xx = 0; xx < 4; xx++)
            {
                const unsigned cc = (yy == 0 && xx == 1) ? t.c[8] : (yy == 1 && xx == 2) ? t.c[9] : t.c[13];
                const ImVec2 a(o.x + g + xx * (c + g), o.y + g + yy * (c + g));
                dl->AddRectFilled(a, ImVec2(a.x + c, a.y + c), HexU(cc), 1.5f * u);
            }
    }
    dl->AddRectFilled(ImVec2(mx.x - 30 * u, mx.y - 19 * u), ImVec2(mx.x - 12 * u, mx.y - 12 * u), HexU(t.c[8]), 3.5f * u);
    dl->AddRect(mn, mx, HexU(selected ? Col::Accent : (b.hovered ? Col::TextDim : Col::Line)), r, 0,
                selected ? 2.0f * u : 1.0f);

    const ImVec2 ns = ImGui::CalcTextSize(t.name);
    dl->PushClipRect(ImVec2(mn.x, mx.y), ImVec2(mx.x, b.mx.y), true);
    dl->AddText(ImVec2(mn.x + ImMax(0.0f, (w - ns.x) * 0.5f), mx.y + 4 * u),
                HexU(selected ? Col::Text : Col::TextDim), t.name);
    dl->PopClipRect();
    return b.clicked;
}

// Every palette slot as a swatch with its name, in as many columns as fit.
static void PaletteEditor()
{
    const float colW = 150 * g_Scale;
    const float baseX = ImGui::GetCursorPosX();
    const int   cols = ImMax(1, (int)(ImGui::GetContentRegionAvail().x / colW));
    bool edited = false;
    for (int i = 0; i < kPalCount; i++)
    {
        if (i % cols) ImGui::SameLine(baseX + (float)(i % cols) * colW);
        ImGui::PushID(i);
        if (EditColour(PalName(i), PalSlot(i), ImGuiColorEditFlags_NoInputs)) edited = true;
        ImGui::PopID();
    }
    if (edited) { g_App.themeIndex = kThemeCount; g_OptThemeAction = -1; }
}

// The segment:register pairs as a grid of pills, one row per segment the way
// they are usually read: CS:IP / DS:AX..DX / DS:SI DI ES:SI DI / SS:BP SP.
static bool PointerPill(int i, float w)
{
    AppState& S = g_App;
    const bool on = (S.hlMask & (1u << i)) != 0;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float h = ImGui::GetFrameHeight();
    ImGui::PushID(i);
    const BtnState b = BareButton("##pp", ImVec2(w, h));
    ImGui::PopID();
    const float r = h * 0.5f;
    if (on)             dl->AddRectFilled(b.mn, b.mx, HexU(S.hlColor, b.hovered ? 1.0f : 0.9f), r);
    else                dl->AddRectFilled(b.mn, b.mx, HexU(b.hovered ? Col::Bg3 : Col::Bg2), r);
    if (!on)            dl->AddRect(b.mn, b.mx, HexU(Col::Line), r);
    const char* n = kPointerPairs[i].name;
    const ImVec2 ts = ImGui::CalcTextSize(n);
    if (g_App.fontMono) ImGui::PushFont(g_App.fontMono, 0.0f);
    const ImVec2 ms = ImGui::CalcTextSize(n);
    dl->AddText(ImVec2((b.mn.x + b.mx.x - ms.x) * 0.5f, (b.mn.y + b.mx.y - ms.y) * 0.5f),
                HexU(on ? Col::Bg0 : Col::TextDim), n);
    if (g_App.fontMono) ImGui::PopFont();
    (void)ts;
    if (b.clicked) S.hlMask ^= (1u << i);
    return b.clicked;
}

static void PointerPairPicker()
{
    AppState& S = g_App;
    static const int rows[][4] = {
        { 0, -1, -1, -1 },
        { 1,  2,  3,  4 },
        { 5,  6,  7,  8 },
        { 9, 10, -1, -1 },
    };
    static const char* rowName[] = { "Code", "Data", "Index", "Stack" };
    const float gap = 8 * g_Scale;
    const float capW = 64 * g_Scale;
    const float pillW = ImClamp((ImGui::GetContentRegionAvail().x - capW - gap * 3.0f) / 4.0f,
                                70 * g_Scale, 110 * g_Scale);
    for (int r = 0; r < 4; r++)
    {
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(Hex(Col::TextDim), "%s", rowName[r]);
        float x = ImGui::GetCursorPosX() + capW;
        for (int c = 0; c < 4; c++)
        {
            if (rows[r][c] < 0) break;
            ImGui::SameLine(x);
            PointerPill(rows[r][c], pillW);
            x += pillW + gap;
        }
    }
    ImGui::Dummy(ImVec2(0, 2 * g_Scale));
    if (ImGui::SmallButton("All"))  S.hlMask = (1u << kPointerPairCount) - 1u;
    ImGui::SameLine();
    if (ImGui::SmallButton("None")) S.hlMask = 0;
    ImGui::SameLine(0, 12 * g_Scale);
    int n = 0;
    for (int i = 0; i < kPointerPairCount; i++) if (S.hlMask & (1u << i)) n++;
    ImGui::TextColored(Hex(Col::TextDim), n ? "%d shown" : "nothing shown", n);
}

// The little drawing beside each section's name in the rail.
static void OptRailIcon(ImDrawList* dl, int which, const ImVec2& c, float s, ImU32 col)
{
    const float t = 1.5f * g_Scale;
    switch (which)
    {
    case 0:   // appearance: a disc, half of it filled
        dl->AddCircle(c, s, col, 24, t);
        dl->PathArcTo(c, s, -IM_PI * 0.5f, IM_PI * 0.5f, 16);
        dl->PathFillConvex(col);
        break;
    case 1:   // files
        DrawGlyphFolder(dl, ImVec2(c.x, c.y - s * 0.1f), s, col);
        break;
    case 2:   // editor: lines of source
        dl->AddLine(ImVec2(c.x - s, c.y - s * 0.7f), ImVec2(c.x + s * 0.5f, c.y - s * 0.7f), col, t);
        dl->AddLine(ImVec2(c.x - s * 0.4f, c.y), ImVec2(c.x + s, c.y), col, t);
        dl->AddLine(ImVec2(c.x - s * 0.4f, c.y + s * 0.7f), ImVec2(c.x + s * 0.3f, c.y + s * 0.7f), col, t);
        break;
    case 3:   // debugger: a crosshair
        dl->AddCircle(c, s * 0.62f, col, 20, t);
        dl->AddLine(ImVec2(c.x - s, c.y), ImVec2(c.x - s * 0.3f, c.y), col, t);
        dl->AddLine(ImVec2(c.x + s * 0.3f, c.y), ImVec2(c.x + s, c.y), col, t);
        dl->AddLine(ImVec2(c.x, c.y - s), ImVec2(c.x, c.y - s * 0.3f), col, t);
        dl->AddLine(ImVec2(c.x, c.y + s * 0.3f), ImVec2(c.x, c.y + s), col, t);
        break;
    case 4:   // simulation
        DrawGlyphPlay(dl, c, s * 0.85f, col);
        break;
    case 5:   // competition: columns that climb
        dl->AddRectFilled(ImVec2(c.x - s, c.y + s * 0.1f), ImVec2(c.x - s * 0.45f, c.y + s), col, 1.0f);
        dl->AddRectFilled(ImVec2(c.x - s * 0.27f, c.y - s * 0.4f), ImVec2(c.x + s * 0.27f, c.y + s), col, 1.0f);
        dl->AddRectFilled(ImVec2(c.x + s * 0.45f, c.y - s), ImVec2(c.x + s, c.y + s), col, 1.0f);
        break;
    default:  // about: an i in a ring
        dl->AddCircle(c, s, col, 24, t);
        dl->AddCircleFilled(ImVec2(c.x, c.y - s * 0.45f), t * 0.75f, col, 8);
        dl->AddLine(ImVec2(c.x, c.y - s * 0.1f), ImVec2(c.x, c.y + s * 0.55f), col, t);
        break;
    }
}

static void DrawOptionsWindow()
{
    AppState& S = g_App;
    DrawSortPrompt();
    if (!S.showOptions) return;

    static const char* kSections[] = { "Appearance", "Files", "Editor", "Debugger", "Simulation", "Competition", "About" };
    static const int   kSectionCount = (int)(sizeof(kSections) / sizeof(kSections[0]));
    static int section = 0;
    if (g_OptionsTab >= 1 && g_OptionsTab <= kSectionCount) section = g_OptionsTab - 1;
    g_OptionsTab = -1;

    ImGui::SetNextWindowSize(ImVec2(760 * g_Scale, 580 * g_Scale), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(600 * g_Scale, 340 * g_Scale), ImVec2(FLT_MAX, FLT_MAX));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    const bool shown = ImGui::Begin("Options", &S.showOptions, ImGuiWindowFlags_NoCollapse);
    ImGui::PopStyleVar();
    if (!shown) { ImGui::End(); return; }

    // ---- left rail --------------------------------------------------------
    const float railW = 172.0f * g_Scale;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Hex(Col::Bg1));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10 * g_Scale, 14 * g_Scale));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 3 * g_Scale));
    ImGui::BeginChild("##optnav", ImVec2(railW, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float rowH = 34.0f * g_Scale;
        for (int i = 0; i < kSectionCount; i++)
        {
            const bool on = (section == i);
            ImGui::PushID(i);
            const BtnState b = BareButton("##sec", ImVec2(ImGui::GetContentRegionAvail().x, rowH));
            ImGui::PopID();
            if (on)             dl->AddRectFilled(b.mn, b.mx, HexU(Col::Accent, 0.16f), 8 * g_Scale);
            else if (b.hovered) dl->AddRectFilled(b.mn, b.mx, HexU(Col::Bg3, 0.75f), 8 * g_Scale);
            if (on)
                dl->AddRectFilled(ImVec2(b.mn.x, b.mn.y + 9 * g_Scale), ImVec2(b.mn.x + 3 * g_Scale, b.mx.y - 9 * g_Scale),
                                  HexU(Col::Accent), 1.5f * g_Scale);
            const float cy = (b.mn.y + b.mx.y) * 0.5f;
            OptRailIcon(dl, i, ImVec2(b.mn.x + 22 * g_Scale, cy), 6.0f * g_Scale,
                        HexU(on ? Col::Accent : Col::TextDim));
            dl->AddText(ImVec2(b.mn.x + 40 * g_Scale, cy - ImGui::GetTextLineHeight() * 0.5f),
                        HexU((on || b.hovered) ? Col::Text : Col::TextDim), kSections[i]);
            if (b.clicked) section = i;
        }

        // the shortcut that brings this window back, parked at the foot of the rail
        const char* hint = "Ctrl + ,  opens this";
        ImGui::PushFont(NULL, ImGui::GetFontSize() * 0.86f);
        const ImVec2 hs = ImGui::CalcTextSize(hint);
        const ImVec2 wp = ImGui::GetWindowPos(), ws = ImGui::GetWindowSize();
        if (ImGui::GetCursorScreenPos().y + hs.y * 2.0f < wp.y + ws.y)
            dl->AddText(ImVec2(wp.x + 14 * g_Scale, wp.y + ws.y - hs.y - 12 * g_Scale),
                        HexU(Col::TextDim, 0.7f), hint);
        ImGui::PopFont();

        dl->AddLine(ImVec2(wp.x + ws.x - 1, wp.y), ImVec2(wp.x + ws.x - 1, wp.y + ws.y), HexU(Col::Line));
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    // ---- the section --------------------------------------------------------
    ImGui::SameLine(0, 0);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Hex(Col::Bg0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20 * g_Scale, 16 * g_Scale));
    ImGui::BeginChild("##optbody", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    ImGui::PushID(section);

    switch (section)
    {
    // ------------------------------------------------------------ appearance
    case 0:
    {
        OptSectionTitle("Appearance", "The theme and its colours, the arena, fonts, and which panels are on screen.");

        OptCard("THEME");
        {
            const float tileW = 116 * g_Scale, gapX = 10 * g_Scale;
            const float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
            for (int i = 0; i < kThemeCount; i++)
            {
                if (ThemeTile(i, S.themeIndex == i, tileW)) g_OptThemeAction = i;
                if (i + 1 < kThemeCount && ImGui::GetItemRectMax().x + gapX + tileW <= right)
                    ImGui::SameLine(0, gapX);
            }
            if (S.themeIndex == kThemeCount)
            {
                ImGui::AlignTextToFramePadding();
                ImGui::TextColored(Hex(Col::TextDim), "Custom palette");
                ImGui::SameLine();
                if (ImGui::SmallButton("Reset to Midnight")) g_OptThemeAction = 0;
            }
        }
        OptCardEnd();

        OptCard("COLOURS");
        OptNote("Editing any colour switches the theme to Custom.");
        ImGui::Dummy(ImVec2(0, 2 * g_Scale));
        PaletteEditor();
        OptCardEnd();

        OptCard("ARENA");
        // part of the theme: changing either makes the palette Custom
        if (OptColour("Arena", &Col::ArenaCore,
                      "The cells nobody has written to yet. Part of the theme."))
            S.themeIndex = kThemeCount;
        if (OptColour("Arena lines", &Col::ArenaGrid,
                      "The lines between the cells once the arena is zoomed in. Part of the theme."))
            S.themeIndex = kThemeCount;
        OptSliderF("Grid line thickness", &S.arenaGridWidth, 0.0f, 3.0f, "%.1f x",
                   "The lines and the cells' rounded corners both scale with it, and grow in "
                   "gradually as you zoom in. 0 is no lines and square corners: one solid sheet.");
        OptToggle("Offsets along the edges", &S.arenaRulers,
                  "Row and column addresses pinned to the top and left of the arena.");
        OptCardEnd();

        OptCard("PANELS");
        OptToggle("Editor", &S.showEditor);
        OptToggle("Debug", &S.showArena);
        OptToggle("Competition", &S.showBoard);
        OptToggle("Timeline", &S.showTimeline);
        OptToggle("Survivor inspector", &S.showInspector);
        OptToggle("Inspector on the left of the arena", &S.inspectorLeft,
                  "The stack and shared columns always take the edge nearest the arena.");
        OptSliderF("Inspector width", &S.inspectorWidth, 240.0f, 900.0f, "%.0f px");
        OptToggle("Collapse groups to survivors only", &S.groupsCollapsed,
                  "The groups bar shrinks to a strip of 1 / 2 chips.");
        if (OptAction("Panel layout", "Reset", "Puts every panel back into one tabbed group."))
            g_ResetLayout = true;
        OptCardEnd();

        OptCard("FONTS");
        OptRow("Interface font", NULL, 220 * g_Scale);
        FontPicker("##fontui", S.fontUIFile, sizeof(S.fontUIFile), "Rubik");
        OptRow("Code font", "The editor, the arena hex, the inspector and the timeline.", 220 * g_Scale);
        FontPicker("##fontmono", S.fontMonoFile, sizeof(S.fontMonoFile), "IBM Plex Mono");
        if (OptAction("Fonts folder", "Open", "Any .ttf or .otf file put in it can be picked above."))
        {
            const std::vector<std::string> dirs = FontDirs();
            if (!dirs.empty()) ShellExecuteA(NULL, "open", dirs[0].c_str(), NULL, NULL, SW_SHOWNORMAL);
        }
        OptSliderF("Panel text", &S.fontMonoSize, 9.0f, 26.0f, "%.0f px",
                   "The arena hex, the inspector and the timeline.");
        OptSliderF("Editor text", &S.fontEditorSize, 8.0f, 40.0f, "%.0f px",
                   "Also on Ctrl + and Ctrl -.");
        OptCardEnd();
        break;
    }

    // ----------------------------------------------------------------- files
    case 1:
    {
        OptSectionTitle("Files", "The workspace folder, folders of survivors, and how both are kept on disk.");

        OptCard("WORKSPACE");
        ImGui::PushStyleColor(ImGuiCol_Text, Hex(S.lastFolder.empty() ? Col::TextDim : Col::Text));
        ImGui::TextWrapped("%s", S.lastFolder.empty() ? "No folder open" : S.lastFolder.c_str());
        ImGui::PopStyleColor();
        OptNote("%d group(s) in %d folder(s), %d zombie(s)", (int)S.players.size(),
                (int)S.folders.size(), (int)S.zombies.size());
        ImGui::Dummy(ImVec2(0, 2 * g_Scale));
        if (ImGui::Button("Open folder...")) OpenWorkspaceFolder();
        ImGui::SameLine();
        if (ImGui::Button("Reload") && !S.lastFolder.empty()) LoadWorkspaceFolder(S.lastFolder);
        ImGui::SameLine();
        if (ImGui::Button("Import files...")) ImportLooseFiles();
        ImGui::SameLine();
        if (ImGui::Button("Save all")) SaveAll();
        ImGui::SameLine();
        if (ImGui::Button("Close all") && !(S.players.empty() && S.zombies.empty() && S.folders.empty()))
            ClearWorkspace();
        g_OptRows = 1;
        if (OptAction("New group", "Create", "Two survivors from the template, ready to edit.")) NewPlayer();
        if (OptAction("New zombie", "Create")) NewZombie();
        OptCardEnd();

        OptCard("FOLDERS OF SURVIVORS");
        OptNote("Groups can be filed into folders in the groups bar: drag a group onto a folder, or "
                "right-click it and pick Move to folder. Clicking a folder folds it away; holding "
                "it turns every group in it on or off.");
        g_OptRows = 1;
        if (OptAction("New folder", "Create", "Empty, at the top of the groups bar."))
        { g_FolderOp = FolderOp(); g_FolderOp.kind = FolderOp::New; }
        OptDivide();
        OptNote("A folder is a directory on disk -- survivors/<folder> and assembly/<folder> in a "
                "sorted workspace, <folder> itself in one opened as-is -- so filing a group moves "
                "its files and the arrangement is still there the next time the folder is opened. "
                "Dropping a directory of warriors on the window brings it in as a folder too.");
        OptCardEnd();

        OptCard("FOLDER LAYOUT");
        {
            static const char* kSort[] = { "Ask", "Always sort", "Never sort" };
            OptSegmented("When a folder is not sorted yet", &S.folderSortMode, kSort, 3,
                         "A workspace holds assembly, survivors and zombies subfolders. This is "
                         "what happens when a folder has none of them.");
        }
        OptCardEnd();

        OptCard("KEEP BOTH FORMS IN STEP");
        OptToggle("Assemble  assembly/*.asm  into  survivors/", &S.folderAssemble);
        OptToggle("Disassemble  survivors/*  into  assembly/", &S.folderDisassemble);
        OptDivide();
        OptNote("Source wins: a binary is only rewritten from source that assembles cleanly, "
                "and source is only written for a binary that has none.");
        OptCardEnd();
        break;
    }

    // ---------------------------------------------------------------- editor
    case 2:
    {
        OptSectionTitle("Editor", "Source is assembled as you type -- there is nothing to press.");

        OptCard("SOURCE");
        OptToggle("Assembled bytes gutter", &S.showAsmGutter,
                  "The address and machine code of every line, down the left of the source.");
        OptToggle("Double-click a byte to re-read the code from it", &S.dblClickDisasm,
                  "Cuts the instruction that straddles that byte down to a db and decodes "
                  "everything after it afresh. This rewrites the source.");
        if (OptToggle("Uppercase disassembly", &S.asmUpperCase)) RelistAllSurvivors();
        {
            bool ws = S.editor.IsShowingWhitespaces();
            if (OptToggle("Show whitespace", &ws)) S.editor.SetShowWhitespaces(ws);
        }
        OptSliderF("Text size", &S.fontEditorSize, 8.0f, 40.0f, "%.0f px", "Also on Ctrl + and Ctrl -.");
        OptCardEnd();
        break;
    }

    // -------------------------------------------------------------- debugger
    case 3:
    {
        OptSectionTitle("Debugger", "What the inspector follows, where playback stops, and what the arena marks.");

        OptCard("MEMORY VIEWS");
        OptToggle("Stack follows SS",  &S.followSS);
        OptToggle("Shared follows ES", &S.followES);
        OptToggle("Code follows CS",   &S.followCS);
        OptDivide();
        OptNote("Each view decides for itself, and its heading in the survivor panel is the same "
                "switch. Off, a view stays on the block the survivor was given -- which is what "
                "you want once a warrior starts throwing its segment registers around.");
        OptCardEnd();

        OptCard("EDIT TOOL");
        OptToggle("Edit mode", &S.editMode,
                  "Double-click a byte in the arena, the stack or the shared page, a register, "
                  "or a flag to change it. The war is played out again with the change on the "
                  "next move.");
        OptCardEnd();

        OptCard("BREAKPOINTS");
        ImGui::TextUnformatted("Byte patterns");
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##bp", "CD 20, F4, EB 00", S.breakPatterns, sizeof(S.breakPatterns));
        OptNote("Hex bytes, comma separated. Playback stops on the cycle whose instruction matches.");
        g_OptRows = 1;
        {
            static const char* kMatch[] = { "Contains them", "Starts with them" };
            int bmode = S.breakStartsWith ? 1 : 0;
            if (OptSegmented("The instruction", &bmode, kMatch, 2)) S.breakStartsWith = (bmode == 1);
        }
        OptToggle("Mark them on the timeline", &S.breakOnTimeline);
        OptCardEnd();

        OptCard("POINTERS IN THE ARENA");
        OptNote("Rings the arena cell the inspected survivor's segment:register pairs point at, "
                "tagged with the pair once the arena is zoomed in. Pick any of them.");
        ImGui::Dummy(ImVec2(0, 4 * g_Scale));
        PointerPairPicker();
        g_OptRows = 1;
        OptColour("Ring colour", &S.hlColor);
        OptCardEnd();

        OptCard("INSTRUCTIONS IN THE ARENA");
        OptToggle("Shade every second instruction", &S.arenaInsnBands,
                  "Around IP, back and forth through the survivor's own stretch of code, every "
                  "other instruction's bytes are shaded a little -- so where one instruction ends "
                  "and the next begins can be read straight off the arena. Counted from the start "
                  "of the survivor's code, so the pattern stays put as IP moves.");
        if (S.arenaInsnBands)
        {
            static const char* kWho[] = { "Inspected survivor", "Every live survivor" };
            OptSegmented("Around the IP of", &S.arenaInsnWho, kWho, 2);
            OptSliderI("Reach", &S.arenaInsnReach, 8, 512, "Bytes each way from IP, at most.");
        }
        OptCardEnd();
        break;
    }

    // ------------------------------------------------------------ simulation
    case 4:
    {
        OptSectionTitle("Simulation", "The war the Debug panel records, and how it is played back.");

        OptCard("WAR");
        {
            char note[200];
            snprintf(note, sizeof(note), "Resolved the way the reference does: a plain number is taken "
                     "as-is, anything else goes through Java's string hash. Currently %lld.",
                     (long long)SeedFromText(S.seedText));
            OptRow("Seed", note, 200 * g_Scale);
            ImGui::InputText("##seed", S.seedText, sizeof(S.seedText));
        }
        OptToggle("Advance the seed on every recompute", &S.advanceSeed,
                  "What the reference does between wars: the seed is bumped and written back, so "
                  "each run is a fresh fight rather than the same one again.");
        OptRow("Max rounds", NULL, 200 * g_Scale);
        ImGui::DragInt("##maxrounds", &S.maxRounds, 100.0f, 100, cw::War::MAX_ROUND);
        OptSliderI("Default zombie speed", &S.zombieSpeed, 1, 8,
                   "Only what a newly loaded zombie starts at -- each one carries its own speed, "
                   "set on its row in the groups bar.");
        if (OptAction("New war", "Recompute", "Plays the whole war out again from the seed.")) GameArm();
        OptCardEnd();

        OptCard("PLAYBACK");
        {
            char note[64];
            snprintf(note, sizeof(note), "%.0f rounds a second", RoundsPerSec());
            OptSliderF("Speed", &S.speedExp, 0.0f, 30.0f, "%.1f", note);
        }
        OptToggle("Stop on events", &S.game.stopOnEvents, "A death or a breakpoint pauses playback.");
        OptToggle("Play in reverse", &S.game.reverse);
        OptCardEnd();
        break;
    }

    // ----------------------------------------------------------- competition
    case 5:
    {
        OptSectionTitle("Competition", "The score graph, and how this machine appears in a shared tournament.");

        OptCard("TOURNAMENT");
        {
            char note[200];
            snprintf(note, sizeof(note), "The seed the first war is fought from; war k uses it + k. "
                     "Empty follows the Debug panel's seed. Currently %lld.",
                     (long long)SeedFromText(CompSeedText()));
            OptRow("Initial seed", note, 200 * g_Scale);
            char hint[96];
            snprintf(hint, sizeof(hint), "Debug's: %s", S.seedText);
            ImGui::InputTextWithHint("##compseedopt", hint, S.compSeedText, sizeof(S.compSeedText));
        }
        OptRow("Pairing seed", "Which groups are drawn into each battle, independent of the fight seed.",
               200 * g_Scale);
        ImGui::InputInt("##pairseed", &S.compPairSeed, 0, 0);
        OptCardEnd();

        OptCard("BAR GRAPH");
        OptToggle("Columns that climb", &S.compBarsUp,
                  "Flips the graph: columns rise from the floor and climb to the right, the "
                  "winner at the far end.");
        OptToggle("Names and scores on the bars", &S.compBarLabels,
                  "Written along a bar that lies down, and turned on its side on a column that "
                  "stands up. A bar too thin to carry a line of text shows them on hover instead.");
        OptToggle("Fit the whole field", &S.compBarsFit,
                  "The bars share the whole panel between them, never thinner than 3 px, so a "
                  "field of hundreds is one picture.");
        OptCardEnd();

        OptCard("SHARED TOURNAMENTS");
        OptRow("This machine's name",
               "How it is listed on a host's board and named in a result file. Two machines "
               "with the same name get numbered by the host.", 200 * g_Scale);
        ImGui::InputText("##netname", S.compNetName, sizeof(S.compNetName));
        OptRow("Port",
               "What a host listens on and a joiner connects to. Hosting needs it let through "
               "the firewall.", 200 * g_Scale);
        ImGui::InputInt("##netport", &S.compNetPort, 0, 0);
        S.compNetPort = ImClamp(S.compNetPort, 1, 65535);
        OptDivide();
        OptNote("A hosted tournament is cut into %d phases of the game index: the host plays "
                "phase 0 and each joiner gets one of its own, so up to %d machines can share "
                "a run. Shard files work the same way with as many phases as shards.",
                kNetStride, kNetStride - 1);
        OptCardEnd();
        break;
    }

    // ----------------------------------------------------------------- about
    default:
    {
        OptSectionTitle("CWEST 8086", "Core Wars Editor, Simulator and Tournaments for the CodeGuru Xtreme 8086 arena.");

        OptCard("SHORTCUTS");
        {
            struct K { const char* k; const char* what; };
            static const K keys[] = {
                { "Ctrl+O",       "open a workspace folder" },
                { "Ctrl+B",       "import loose files" },
                { "Ctrl+N",       "new group" },
                { "Ctrl+S",       "save the open survivor" },
                { "Ctrl+Shift+S", "save everything" },
                { "Ctrl+,",       "these options" },
                { "Ctrl+Q",       "disable / enable the selection" },
                { "Delete",       "close the selected groups" },
                { "F5",           "play or pause" },
                { "Ctrl+F5",      "new war (re-precompute)" },
                { "Shift+F5",     "rewind" },
                { "F11",          "forward one round" },
                { "Shift+F11",    "back one round" },
                { "F8",           "continue to the next event" },
                { "Alt",          "next encoding of the instruction on the cursor line" },
                { "Esc",          "clear the selection" },
            };
            if (ImGui::BeginTable("##keys", 2, ImGuiTableFlags_SizingFixedFit))
            {
                for (const K& k : keys)
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextColored(Hex(Col::Accent2), "%s", k.k);
                    ImGui::TableNextColumn();
                    ImGui::TextColored(Hex(Col::TextDim), "%s", k.what);
                }
                ImGui::EndTable();
            }
        }
        OptCardEnd();

        OptCard("MOUSE");
        OptNote("Groups bar: click opens and selects, Ctrl / Shift click multi-selects, hold to "
                "disable, right-click to rename, recolour and move to a folder. Drag a group onto "
                "a folder to file it there.");
        g_OptRows = 1;
        OptDivide();
        OptNote("Arena and timeline: wheel zooms, drag pans, and the bar under the timeline is "
                "the viewport -- drag its ends to widen or narrow what you see.");
        OptDivide();
        OptNote("Edit mode: double-click a byte, a register or a flag to change it.");
        OptCardEnd();
        break;
    }
    }

    ImGui::PopID();
    ImGui::Dummy(ImVec2(0, 6 * g_Scale));
    ImGui::EndChild();
    ImGui::End();

    if (g_OptThemeAction >= 0)       SetTheme(g_OptThemeAction);
    else if (g_OptThemeAction == -1) ApplyTheme();
    g_OptThemeAction = -2;
}
