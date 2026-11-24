// =============================================================================
//  gui_workspace.inl -- the workspace: one folder on disk that holds everything
//
//  Like an editor's open folder. Nothing works without one: every group,
//  zombie and folder of groups is a file or a directory inside it, and every
//  change made in the app is a change on disk.
//
//    <workspace>\assembly\<folder...>\<name>1.asm   source -- what the editor edits
//    <workspace>\survivors\<folder...>\<name>1      machine code -- what fights
//    <workspace>\zombies\<name>  (+ <name>.asm)     zombies, never in folders
//    <workspace>\.cwest\workspace.ini               which groups are active, their
//                                                   colours, which folders are shut
//
//  A folder of groups is a directory under both trees, nested as deep as you
//  like. The workspace is watched, so files added, removed or edited outside the
//  app (in Explorer, by another editor) show up on their own.
//
//  Included by main.cpp after the editor plumbing and FindOrCreatePlayer.
// =============================================================================
#include <shlobj.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <cstdarg>

// -----------------------------------------------------------------------------
// Filesystem helpers
// -----------------------------------------------------------------------------
static std::string JoinPath(const std::string& a, const std::string& b)
{
    if (a.empty()) return b;
    const char last = a[a.size() - 1];
    return (last == '\\' || last == '/') ? a + b : a + "\\" + b;
}

static std::string ParentDir(const std::string& p)
{
    const size_t s = p.find_last_of("\\/");
    return (s == std::string::npos) ? std::string() : p.substr(0, s);
}

static bool DirExists(const std::string& p)
{
    const DWORD a = GetFileAttributesA(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static bool PathExists(const std::string& p)
{
    return GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES;
}

static bool EnsureDir(const std::string& p)
{
    return DirExists(p) || CreateDirectoryA(p.c_str(), NULL) != 0;
}

// Creates every missing directory down to p.
static bool EnsureDirs(const std::string& p)
{
    if (p.empty()) return false;
    if (DirExists(p)) return true;
    const std::string up = ParentDir(p);
    if (!up.empty() && up != p && !DirExists(up) && !EnsureDirs(up)) return false;
    return CreateDirectoryA(p.c_str(), NULL) != 0 || DirExists(p);
}

static bool IsHiddenAttr(DWORD a)
{
    return (a & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) != 0;
}

// Every visible plain file directly inside dir, as full paths, sorted.
static std::vector<std::string> ListDirFiles(const std::string& dir)
{
    std::vector<std::string> out;
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(JoinPath(dir, "*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return out;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (IsHiddenAttr(fd.dwFileAttributes)) continue;          // desktop.ini, Thumbs.db
        out.push_back(JoinPath(dir, fd.cFileName));
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    std::sort(out.begin(), out.end(), [](const std::string& a, const std::string& b) {
        return ToLower(a) < ToLower(b);
    });
    return out;
}

// The names of the visible directories directly inside dir, sorted.
static std::vector<std::string> ListSubDirs(const std::string& dir)
{
    std::vector<std::string> out;
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(JoinPath(dir, "*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return out;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (IsHiddenAttr(fd.dwFileAttributes)) continue;
        const std::string n = fd.cFileName;
        if (n == "." || n == ".." || n[0] == '.') continue;      // .git, .cwest and friends
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

static bool ReadFileBytes(const std::string& path, std::string& out)
{
    std::ifstream f(path.c_str(), std::ios::binary);
    if (!f) return false;
    out.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return true;
}

static bool WriteFileBytes(const std::string& path, const std::string& bytes)
{
    std::ofstream f(path.c_str(), std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(bytes.data(), (std::streamsize)bytes.size());
    return (bool)f;
}

// A directory's files with their write times, from the listing itself -- a
// separate GetFileAttributesEx per file made reading a big workspace (and the
// re-read after every save) take most of a second.
struct TimedFile { std::string path; unsigned long long time; };
static std::vector<TimedFile> ListDirFilesTimed(const std::string& dir)
{
    std::vector<TimedFile> out;
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileExA(JoinPath(dir, "*").c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, NULL,
                                FIND_FIRST_EX_LARGE_FETCH);
    if (h == INVALID_HANDLE_VALUE) return out;
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        if (IsHiddenAttr(fd.dwFileAttributes)) continue;
        out.push_back({ JoinPath(dir, fd.cFileName),
                        ((unsigned long long)fd.ftLastWriteTime.dwHighDateTime << 32) | fd.ftLastWriteTime.dwLowDateTime });
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return out;
}

static unsigned long long FileTimeOf(const std::string& p)
{
    WIN32_FILE_ATTRIBUTE_DATA d;
    if (p.empty() || !GetFileAttributesExA(p.c_str(), GetFileExInfoStandard, &d)) return 0;
    return ((unsigned long long)d.ftLastWriteTime.dwHighDateTime << 32) | d.ftLastWriteTime.dwLowDateTime;
}

static long long FileSizeOf(const std::string& p)
{
    WIN32_FILE_ATTRIBUTE_DATA d;
    if (!GetFileAttributesExA(p.c_str(), GetFileExInfoStandard, &d)) return -1;
    return ((long long)d.nFileSizeHigh << 32) | d.nFileSizeLow;
}

// Sends files and directories to the Windows Recycle Bin, so even a deletion
// the app has forgotten about can still be fished out from there.
static bool SendToRecycleBin(const std::vector<std::string>& paths, HWND owner = NULL)
{
    if (paths.empty()) return true;
    std::string list;
    for (const std::string& p : paths) { list += p; list.push_back('\0'); }
    list.push_back('\0');
    SHFILEOPSTRUCTA op;
    memset(&op, 0, sizeof(op));
    op.hwnd   = owner;
    op.wFunc  = FO_DELETE;
    op.pFrom  = list.c_str();
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;
    const int rc = SHFileOperationA(&op);
    return rc == 0 && !op.fAnyOperationsAborted;
}

// The Recycle Bin can take a second or more, so a deletion hands it the files
// on a thread of its own and the lists change at once. Anything that needs the
// disk settled (bringing files back, a rescan, closing) waits for it first.
static std::thread       g_RecycleThread;
static std::atomic<bool> g_RecycleBusy{false};
static std::atomic<bool> g_RecycleFailed{false};

static void RecycleWait()
{
    if (g_RecycleThread.joinable()) g_RecycleThread.join();
}

static void RecycleInBackground(const std::vector<std::string>& paths)
{
    RecycleWait();
    if (paths.empty()) return;
    g_RecycleBusy.store(true);
    g_RecycleThread = std::thread([paths]() {
        const HRESULT init = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        if (!SendToRecycleBin(paths, NULL)) g_RecycleFailed.store(true);
        if (SUCCEEDED(init)) CoUninitialize();
        g_RecycleBusy.store(false);
    });
}

static std::wstring AcpToWide(const std::string& s)
{
    const int n = MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, NULL, 0);
    std::wstring w(n > 0 ? (size_t)(n - 1) : 0, L'\0');
    if (n > 1) MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, &w[0], n);
    return w;
}

static std::string WideToAcp(const wchar_t* w)
{
    const int n = WideCharToMultiByte(CP_ACP, 0, w, -1, NULL, 0, NULL, NULL);
    std::string s(n > 0 ? (size_t)(n - 1) : 0, '\0');
    if (n > 1) WideCharToMultiByte(CP_ACP, 0, w, -1, &s[0], n, NULL, NULL);
    return s;
}

// The folder picker: the same Explorer dialog the file pickers use (with its
// address bar, favourites and "New folder"), in folder mode. The GUIDs are
// spelled out here so the build needs no extra import library.
static std::string BrowseForFolder(HWND owner, const char* title, const std::string& startIn)
{
    static const CLSID kClsidFileOpenDialog = { 0xDC1C5A9C, 0xE88A, 0x4DDE, { 0xA5, 0xA1, 0x60, 0xF8, 0x2A, 0x20, 0xAE, 0xF7 } };
    static const IID   kIidFileOpenDialog   = { 0xD57C7288, 0xD4AD, 0x4768, { 0xBE, 0x02, 0x9D, 0x96, 0x95, 0x32, 0xD9, 0x60 } };
    static const IID   kIidShellItem        = { 0x43826D1E, 0xE718, 0x42EE, { 0xBC, 0x55, 0xA1, 0xE2, 0x61, 0xC3, 0x7B, 0xFE } };

    const HRESULT init = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    std::string result;
    IFileOpenDialog* dlg = NULL;
    if (SUCCEEDED(CoCreateInstance(kClsidFileOpenDialog, NULL, CLSCTX_INPROC_SERVER, kIidFileOpenDialog, (void**)&dlg)) && dlg)
    {
        DWORD opts = 0;
        dlg->GetOptions(&opts);
        dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
        const std::wstring wt = AcpToWide(title);
        dlg->SetTitle(wt.c_str());
        if (!startIn.empty() && DirExists(startIn))
        {
            IShellItem* at = NULL;
            const std::wstring ws = AcpToWide(startIn);
            if (SUCCEEDED(SHCreateItemFromParsingName(ws.c_str(), NULL, kIidShellItem, (void**)&at)) && at)
            {
                dlg->SetFolder(at);
                at->Release();
            }
        }
        if (SUCCEEDED(dlg->Show(owner)))
        {
            IShellItem* item = NULL;
            if (SUCCEEDED(dlg->GetResult(&item)) && item)
            {
                PWSTR p = NULL;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &p)) && p)
                {
                    result = WideToAcp(p);
                    CoTaskMemFree(p);
                }
                item->Release();
            }
        }
        dlg->Release();
    }
    else
    {
        // very old Windows: the tree dialog
        char buf[MAX_PATH] = "";
        BROWSEINFOA bi;
        memset(&bi, 0, sizeof(bi));
        bi.hwndOwner = owner;
        bi.pszDisplayName = buf;
        bi.lpszTitle = title;
        bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_EDITBOX;
        LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
        if (pidl)
        {
            char path[MAX_PATH] = "";
            if (SHGetPathFromIDListA(pidl, path)) result = path;
            CoTaskMemFree(pidl);
        }
    }
    if (SUCCEEDED(init)) CoUninitialize();
    return result;
}

// -----------------------------------------------------------------------------
// Layout and names
// -----------------------------------------------------------------------------
static const char* kAsmDir   = "assembly";
static const char* kSurvDir  = "survivors";
static const char* kZombDir  = "zombies";
static const char* kStateDir = ".cwest";

static bool IsLayoutDirName(const std::string& name)
{
    const std::string n = ToLower(name);
    return n == kAsmDir || n == kSurvDir || n == kZombDir;
}

// A name that has to be a file or directory name: no path separators or the
// characters Windows refuses, no leading/trailing blanks or dots.
static std::string CleanFileName(const std::string& raw)
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
    if (n.size() > 60) n.resize(60);
    return n;
}

// A folder of groups is a directory, so it cannot be named after one of the
// layout's own three. Empty = unusable.
static std::string CleanFolderName(const std::string& raw)
{
    std::string n = CleanFileName(raw);
    if (n.empty() || n[0] == '.' || IsLayoutDirName(n)) return std::string();
    return n;
}

// A group's files are <name>1 and <name>2, and a name is read back off a file
// by cutting the number and any separator before it -- so a group name cannot
// end in one of those separators, or it would not survive the trip.
static std::string CleanGroupName(const std::string& raw)
{
    std::string n = CleanFileName(raw);
    while (!n.empty() && (n.back() == '_' || n.back() == '-' || n.back() == ' ' || n.back() == '.'))
        n.pop_back();
    if (!n.empty() && n[0] == '.') n.erase(0, 1);
    return n;
}

static bool HasWorkspace() { return !g_App.lastFolder.empty(); }

static std::string WsRoot()
{
    std::string r = g_App.lastFolder;
    while (r.size() > 3 && (r.back() == '\\' || r.back() == '/')) r.pop_back();
    return r;
}

static std::string NativeRel(std::string f)
{
    for (char& c : f) if (c == '/') c = '\\';
    return f;
}

static std::string TreeDir(const char* kind, const std::string& folder)
{
    const std::string d = JoinPath(WsRoot(), kind);
    return folder.empty() ? d : JoinPath(d, NativeRel(folder));
}

static std::string AsmPathFor(const std::string& folder, const std::string& stem)
{
    return JoinPath(TreeDir(kAsmDir, folder), stem + ".asm");
}

static std::string BinPathFor(const std::string& folder, const std::string& stem)
{
    return JoinPath(TreeDir(kSurvDir, folder), stem);
}

static std::string ZombieDir() { return JoinPath(WsRoot(), kZombDir); }

// The file stem of a group's survivor: "Phoenix" -> "Phoenix1", but "Bot7" ->
// "Bot7_1", because "Bot71" would read back as survivor 71 of "Bot".
static std::string SurvivorStem(const std::string& group, int slot)
{
    const bool digit = !group.empty() && isdigit((unsigned char)group.back());
    return group + (digit ? "_" : "") + std::to_string(slot + 1);
}

// -----------------------------------------------------------------------------
// Selection kept across changes to the lists
//
// Refs are indices; anything that adds, removes or reorders groups, zombies or
// folders remembers the selection by identity first and finds it again after.
// -----------------------------------------------------------------------------
static RefKey KeyOfRef(const Ref& r)
{
    RefKey k;
    if (IsFolderRef(r))
    {
        if (r.slot < (int)g_App.folders.size()) { k.kind = 2; k.folder = g_App.folders[r.slot].path; }
        return k;
    }
    if (IsZombieRef(r))
    {
        if (r.slot < (int)g_App.zombies.size()) { k.kind = 1; k.name = g_App.zombies[r.slot].sv.name; }
        return k;
    }
    if (r.player >= 0 && r.player < (int)g_App.players.size())
    {
        k.kind = 0;
        k.folder = g_App.players[r.player].folder;
        k.name = g_App.players[r.player].name;
        k.slot = r.slot;
    }
    return k;
}

static Ref RefOfKey(const RefKey& k)
{
    Ref r;
    if (k.kind == 2)
    {
        const int fi = FolderIndex(k.folder);
        if (fi >= 0) { r.player = kFolderPlayer; r.slot = fi; }
    }
    else if (k.kind == 1)
    {
        for (int zi = 0; zi < (int)g_App.zombies.size(); zi++)
            if (ToLower(g_App.zombies[zi].sv.name) == ToLower(k.name)) { r.player = kZombiePlayer; r.slot = zi; break; }
    }
    else if (k.kind == 0)
    {
        for (int p = 0; p < (int)g_App.players.size(); p++)
            if (ToLower(g_App.players[p].name) == ToLower(k.name) &&
                ToLower(g_App.players[p].folder) == ToLower(k.folder))
            { r.player = p; r.slot = k.slot; break; }
    }
    return r;
}

struct SelectionMemo { std::vector<RefKey> sel; RefKey anchor, open; };

static SelectionMemo RememberSelection()
{
    SelectionMemo m;
    for (const Ref& r : g_App.selection) m.sel.push_back(KeyOfRef(r));
    m.anchor = KeyOfRef(g_App.anchor);
    m.open = KeyOfRef(g_App.open);
    return m;
}

static void CloseEditorBuffer()
{
    g_App.open = Ref();
    g_App.editor.SetText("");
    g_App.editor.SetReadOnly(true);
}

static void RestoreSelection(const SelectionMemo& m)
{
    g_App.selection.clear();
    for (const RefKey& k : m.sel)
    {
        const Ref r = RefOfKey(k);
        if (r.Valid() && !IsSelected(r)) g_App.selection.push_back(r);
    }
    g_App.anchor = RefOfKey(m.anchor);
    if (m.open.kind < 0) return;
    const Ref open = RefOfKey(m.open);
    const Survivor* s = SurvivorAt(open);
    if (open.Valid() && s && s->present) g_App.open = open;
    else                                 CloseEditorBuffer();
}

// Selects exactly these, and opens the first survivor among them.
static void SelectKeys(const std::vector<RefKey>& keys, bool openFirst)
{
    g_App.selection.clear();
    for (const RefKey& k : keys)
    {
        const Ref r = RefOfKey(k);
        if (r.Valid() && !IsSelected(r)) g_App.selection.push_back(r);
    }
    if (g_App.selection.empty()) return;
    g_App.anchor = g_App.selection.back();
    if (!openFirst) return;
    for (const Ref& r : g_App.selection)
    {
        if (IsFolderRef(r)) continue;
        Ref t = r;
        if (!IsZombieRef(t) && t.slot < 0) t.slot = g_App.players[t.player].FirstLoaded();
        if (SurvivorAt(t)) { OpenInEditor(t); break; }
    }
}

// Unfolds a folder and every folder it is inside, so whatever is in it shows.
static void RevealFolder(const std::string& path)
{
    for (std::string p = path; !p.empty(); p = FolderParent(p))
    {
        const int fi = FolderIndex(p);
        if (fi >= 0) g_App.folders[fi].collapsed = false;
    }
}

// Starts typing a new name into a row of the groups bar (F2).
static void BeginInlineRename(const Ref& r)
{
    const RefKey k = KeyOfRef(r);
    if (k.kind < 0 || (k.kind == 0 && k.slot >= 0)) { g_Rename = InlineRename(); return; }
    g_Rename = InlineRename();
    g_Rename.key = k;
    g_Rename.active = true;
    g_Rename.focus = true;
    const std::string cur = k.kind == 2 ? FolderLeaf(k.folder) : k.name;
    snprintf(g_Rename.buf, sizeof(g_Rename.buf), "%s", cur.c_str());
}

static bool RenamingRef(const Ref& r)
{
    if (!g_Rename.active) return false;
    const RefKey k = KeyOfRef(r);
    return k.kind == g_Rename.key.kind && ToLower(k.folder) == ToLower(g_Rename.key.folder) &&
           ToLower(k.name) == ToLower(g_Rename.key.name);
}

// Deletions kept for Ctrl+Z: the bytes of every file, and the rows as they were.
struct FileBlob { std::string path; std::string bytes; };
struct GhostRow
{
    int         kind = 0;          // 0 group, 1 folder, 2 zombie
    std::string folder;            // the folder the row was in (a folder's parent)
    std::string label;
    int         pos = 0;           // rows before it that are still there
    unsigned    tint = 0x808080;
};
struct DeleteBatch
{
    double at = 0.0;
    std::vector<FileBlob>    files;
    std::vector<std::string> dirs;                     // recreated even when empty
    std::vector<std::pair<int, Player>> groups;        // original index, the whole group
    std::vector<GroupFolder> folders;
    std::vector<std::pair<int, Zombie>> zombies;
    struct Slot { std::string folder, group; int slot = 0; Survivor sv; unsigned tint = 0, tint2 = 0; bool twoTone = false; };
    std::vector<Slot>     slots;                       // single survivors whose group stayed
    std::vector<GhostRow> ghosts;
    std::string what;
};
static std::vector<DeleteBatch> g_Trash;
static const double kGhostSecs = 5.0;

// How visible a deleted row still is: 1 while it lingers, fading to 0.
static float GhostAlpha(const DeleteBatch& b)
{
    const double age = glfwGetTime() - b.at;
    if (age >= kGhostSecs) return 0.0f;
    if (age <= kGhostSecs - 0.8) return 1.0f;
    return (float)((kGhostSecs - age) / 0.8);
}

static void ClearWorkspace()
{
    g_App.players.clear();
    g_App.folders.clear();
    MarkFoldersChanged();
    MarkPlayersChanged();
    g_App.zombies.clear();
    g_App.selection.clear();
    g_App.anchor = Ref();
    CloseEditorBuffer();
    g_App.inspectWarrior = -1;
    g_App.game.clear();
    g_Rename = InlineRename();
}

// -----------------------------------------------------------------------------
// Reading the workspace
// -----------------------------------------------------------------------------
struct DiskEntry
{
    std::string folder, stem, asmPath, binPath;
    unsigned long long asmTime = 0, binTime = 0;
};

// One directory of both trees: its warriors (paired by file stem), then every
// directory under it, as folders of groups.
static void ScanTree(const std::string& folder, std::vector<std::string>& folders,
                     std::vector<DiskEntry>& out, int depth)
{
    const std::string aDir = TreeDir(kAsmDir, folder), sDir = TreeDir(kSurvDir, folder);
    std::map<std::string, DiskEntry> here;               // by lower-case stem, so sorted
    for (const TimedFile& f : ListDirFilesTimed(sDir))
    {
        const std::string fn = FileNameOf(f.path);
        if (!ExtOf(fn).empty()) continue;                // machine code has no extension
        DiskEntry& e = here[ToLower(fn)];
        if (e.stem.empty()) { e.stem = fn; e.folder = folder; }
        e.binPath = f.path;
        e.binTime = f.time;
    }
    for (const TimedFile& f : ListDirFilesTimed(aDir))
    {
        const std::string fn = FileNameOf(f.path);
        if (ExtOf(fn) != ".asm") continue;
        const std::string stem = StemOf(fn);
        DiskEntry& e = here[ToLower(stem)];
        if (e.stem.empty()) { e.stem = stem; e.folder = folder; }
        e.asmPath = f.path;
        e.asmTime = f.time;
    }
    for (auto& kv : here) out.push_back(kv.second);
    if (depth >= 12) return;

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
        if (CleanFolderName(sub) != sub) continue;       // a name we could not make again
        const std::string child = FolderJoin(folder, sub);
        folders.push_back(child);
        ScanTree(child, folders, out, depth + 1);
    }
}

static void ScanZombies(std::vector<DiskEntry>& out)
{
    const std::string z = ZombieDir();
    std::map<std::string, DiskEntry> here;
    for (const std::string& p : ListDirFiles(z))
    {
        const std::string fn = FileNameOf(p), ext = ExtOf(fn);
        if (ext.empty())
        {
            DiskEntry& e = here[ToLower(fn)];
            if (e.stem.empty()) e.stem = fn;
            e.binPath = p;
            e.binTime = FileTimeOf(p);
        }
        else if (ext == ".asm")
        {
            DiskEntry& e = here[ToLower(StemOf(fn))];
            if (e.stem.empty()) e.stem = StemOf(fn);
            e.asmPath = p;
            e.asmTime = FileTimeOf(p);
        }
    }
    for (auto& kv : here) out.push_back(kv.second);
}

// Fills a survivor from its files. The source is what a human edits, so where
// both exist it wins -- but machine code is only ever rewritten from source
// that assembles cleanly, and source is only written for machine code that
// has none, and only when it reads back to exactly the same bytes.
static void LoadSurvivorFromDisk(Survivor& sv, const DiskEntry& e,
                                 const std::string& asmIfNone, const std::string& binIfNone, bool allowWrites)
{
    const bool keepEnabled = sv.present ? sv.enabled : true;
    sv = Survivor();
    sv.name    = e.stem;
    sv.asmPath = e.asmPath.empty() ? asmIfNone : e.asmPath;
    sv.binPath = e.binPath.empty() ? binIfNone : e.binPath;
    sv.present = true;
    sv.enabled = keepEnabled;
    sv.dirty   = false;

    std::vector<unsigned char> disk;
    const bool haveBin = !e.binPath.empty() && ReadBinaryFile(e.binPath, disk);
    if (haveBin && disk.size() > 512) disk.resize(512);      // WarriorRepository.MAX_WARRIOR_SIZE

    std::string text;
    if (!e.asmPath.empty() && ReadTextFile(e.asmPath, text))
    {
        sv.code = text;
        sv.asmTime = e.asmTime;
        ReassembleSurvivor(sv);
        if (haveBin) sv.binTime = e.binTime;
        if (allowWrites && g_App.folderAssemble && sv.asmOk && !sv.bin.empty() && (!haveBin || disk != sv.bin))
            if (EnsureDirs(ParentDir(sv.binPath)) && WriteBinaryFile(sv.binPath, sv.bin))
                sv.binTime = FileTimeOf(sv.binPath);
    }
    else if (haveBin)
    {
        ListSurvivor(sv, disk);                 // real disassembly, labels and all
        sv.binTime = e.binTime;
        if (allowWrites && g_App.folderDisassemble && sv.asmOk && sv.bin == disk)
            if (EnsureDirs(ParentDir(sv.asmPath)) && WriteTextFile(sv.asmPath, sv.code))
                sv.asmTime = FileTimeOf(sv.asmPath);
    }
}

// Where a survivor file of `folder` lands among the groups: "<base>1" and
// "<base>2" are one group's two survivors. `loaded` is the survivor already
// read from the entry's files.
static void PlaceDiskEntry(const DiskEntry& e, Survivor&& loaded)
{
    std::string base; int index = 0;
    SplitIndexedStem(e.stem, base, index);
    int pi = FindOrCreatePlayer(base, e.folder);
    int slot = (index >= 1 && index <= kSlotCount) ? index - 1 : -1;
    if (slot < 0 || g_App.players[pi].slots[slot].present)
    {
        slot = -1;
        for (int s = 0; s < kSlotCount; s++)
            if (!g_App.players[pi].slots[s].present) { slot = s; break; }
    }
    if (slot < 0)
    {
        // both slots taken: an overflow group, so nothing is silently dropped
        pi = FindOrCreatePlayer(base + " (2)", e.folder);
        slot = 0;
        for (int s = 0; s < kSlotCount; s++)
            if (!g_App.players[pi].slots[s].present) { slot = s; break; }
    }
    g_App.players[pi].slots[slot] = std::move(loaded);
}

// Reads every entry -- the files, and the assembling or listing that makes the
// other half -- on all cores at once. That is nearly all of the time it takes
// to open a workspace, and each file is its own business: nothing here touches
// the groups, only the entry's own files.
static std::vector<Survivor> ReadDiskEntries(const std::vector<DiskEntry>& entries, bool allowWrites)
{
    std::vector<Survivor> out(entries.size());
    std::atomic<size_t> next(0);
    auto work = [&]() {
        for (;;)
        {
            const size_t i = next.fetch_add(1);
            if (i >= entries.size()) break;
            const DiskEntry& e = entries[i];
            LoadSurvivorFromDisk(out[i], e, AsmPathFor(e.folder, e.stem), BinPathFor(e.folder, e.stem), allowWrites);
        }
    };
    int n = (int)std::thread::hardware_concurrency();
    n = ImClamp(n, 1, 32);
    if (entries.size() < 16) n = 1;
    std::vector<std::thread> helpers;
    for (int t = 1; t < n && t < (int)entries.size(); t++) helpers.emplace_back(work);
    work();
    for (std::thread& t : helpers) t.join();
    return out;
}

static void PlaceDiskEntries(const std::vector<DiskEntry>& entries, bool allowWrites)
{
    std::vector<Survivor> loaded;
    { PERF("o.read"); loaded = ReadDiskEntries(entries, allowWrites); }
    // numbered files claim their slots before unnumbered ones take what is free
    for (int pass = 0; pass < 2; pass++)
        for (size_t i = 0; i < entries.size(); i++)
        {
            const DiskEntry& e = entries[i];
            std::string base; int index = 0;
            SplitIndexedStem(e.stem, base, index);
            const bool numbered = (index >= 1 && index <= kSlotCount);
            if (numbered == (pass == 0)) PlaceDiskEntry(e, std::move(loaded[i]));
        }
}

static void LoadZombieEntry(const DiskEntry& e, bool allowWrites)
{
    Zombie z;
    z.speed = g_App.zombieSpeed;
    z.name  = e.stem;
    z.tint  = kPlayerTints[(g_App.zombies.size() + 3) % (sizeof(kPlayerTints) / sizeof(kPlayerTints[0]))];
    LoadSurvivorFromDisk(z.sv, e, JoinPath(ZombieDir(), e.stem + ".asm"), JoinPath(ZombieDir(), e.stem), allowWrites);
    // a trailing "h" is how the reference repository marks the heavy kind
    const std::string low = ToLower(e.stem);
    z.heavy = !low.empty() && low.back() == 'h';
    g_App.zombies.push_back(std::move(z));
}

// -----------------------------------------------------------------------------
// Writing a survivor
// -----------------------------------------------------------------------------
// Source to its .asm, and -- when it assembles -- machine code to its binary.
// Source that does not assemble is saved, but the machine code on disk is left
// alone, so the survivor still counts as having unsaved changes.
static bool WriteSurvivorFiles(Survivor& sv)
{
    if (sv.asmPath.empty() && sv.binPath.empty()) return false;
    bool ok = true;
    if (!sv.asmPath.empty())
    {
        // Source on disk this survivor never read is somebody's own file: keep
        // a copy before writing over it.
        if (sv.asmTime == 0 && PathExists(sv.asmPath))
        {
            std::string existing;
            if (ReadTextFile(sv.asmPath, existing) && existing != sv.code && !PathExists(sv.asmPath + ".bak"))
                WriteTextFile(sv.asmPath + ".bak", existing);
        }
        ok = EnsureDirs(ParentDir(sv.asmPath)) && WriteTextFile(sv.asmPath, sv.code);
        if (ok) sv.asmTime = FileTimeOf(sv.asmPath);
    }
    const bool binToo = sv.asmOk && !sv.bin.empty() && !sv.binPath.empty();
    if (ok && binToo)
    {
        ok = EnsureDirs(ParentDir(sv.binPath)) && WriteBinaryFile(sv.binPath, sv.bin);
        if (ok) sv.binTime = FileTimeOf(sv.binPath);
    }
    sv.dirty = !ok || !binToo;
    return ok;
}

// -----------------------------------------------------------------------------
// Workspace state: .cwest\workspace.ini
// -----------------------------------------------------------------------------
static std::string g_WsStateWritten;      // what is on disk, so an unchanged state is not rewritten
static double      g_WsStateCheckAt = 0.0;

static std::string WsStatePath()
{
    return JoinPath(JoinPath(WsRoot(), kStateDir), "workspace.ini");
}

static std::string NoTabs(std::string s)
{
    for (char& c : s) if (c == '\t' || c == '\n' || c == '\r') c = ' ';
    return s;
}

static std::string WsStateText()
{
    std::string s = "# CWEST8086 workspace: which groups are active, their colours, folded folders\n";
    char buf[96];
    for (const GroupFolder& f : g_App.folders)
        s += "folder\t" + f.path + "\t" + (f.collapsed ? "1" : "0") + "\n";
    for (const Player& pl : g_App.players)
    {
        int bits = 0;
        for (int k = 0; k < kSlotCount; k++) if (pl.slots[k].enabled) bits |= 1 << k;
        snprintf(buf, sizeof(buf), "\t%d\t%d\t%u\t%u\t%d\n", pl.enabled ? 1 : 0, bits, pl.tint, pl.tint2, pl.twoTone ? 1 : 0);
        s += "group\t" + pl.folder + "\t" + pl.name + buf;
    }
    for (const Zombie& z : g_App.zombies)
    {
        snprintf(buf, sizeof(buf), "\t%d\t%d\t%d\t%u\n", z.speed, z.heavy ? 1 : 0, z.sv.enabled ? 1 : 0, z.tint);
        s += "zombie\t" + z.sv.name + buf;
    }
    return s;
}

static std::vector<std::string> SplitTabs(const std::string& line)
{
    std::vector<std::string> out;
    size_t pos = 0;
    for (;;)
    {
        const size_t t = line.find('\t', pos);
        out.push_back(line.substr(pos, t == std::string::npos ? std::string::npos : t - pos));
        if (t == std::string::npos) break;
        pos = t + 1;
    }
    return out;
}

// Laid over a freshly read workspace. Groups the file does not know (new ones,
// or a workspace opened for the first time) stay active with their own colour.
static void ApplyWsState()
{
    std::string text;
    if (!ReadTextFile(WsStatePath(), text)) return;
    g_WsStateWritten = text;
    // folder|name -> group, once (a scan per line was quadratic)
    std::unordered_map<std::string, int> groupAt;
    for (int i = 0; i < (int)g_App.players.size(); i++)
        groupAt.emplace(ToLower(g_App.players[(size_t)i].folder) + "|" + ToLower(g_App.players[(size_t)i].name), i);
    size_t pos = 0;
    while (pos < text.size())
    {
        size_t nl = text.find('\n', pos);
        if (nl == std::string::npos) nl = text.size();
        const std::string line = text.substr(pos, nl - pos);
        pos = nl + 1;
        if (line.empty() || line[0] == '#') continue;
        const std::vector<std::string> f = SplitTabs(line);
        if (f[0] == "folder" && f.size() >= 3)
        {
            const int fi = FolderIndex(f[1]);
            if (fi >= 0) g_App.folders[fi].collapsed = (f[2] == "1");
        }
        else if (f[0] == "group" && f.size() >= 8)
        {
            auto at = groupAt.find(ToLower(f[1]) + "|" + ToLower(f[2]));
            if (at != groupAt.end())
            {
                Player& pl = g_App.players[(size_t)at->second];
                const int bits = atoi(f[4].c_str());
                for (int k = 0; k < kSlotCount; k++)
                    if (pl.slots[k].present) pl.slots[k].enabled = (bits >> k) & 1;
                pl.enabled = (f[3] == "1");
                pl.tint    = (unsigned)strtoul(f[5].c_str(), NULL, 10);
                pl.tint2   = (unsigned)strtoul(f[6].c_str(), NULL, 10);
                pl.twoTone = (f[7] == "1");
            }
        }
        else if (f[0] == "zombie" && f.size() >= 6)
        {
            for (Zombie& z : g_App.zombies)
            {
                if (ToLower(z.sv.name) != ToLower(f[1])) continue;
                z.speed      = ImClamp(atoi(f[2].c_str()), 1, 8);
                z.heavy      = (f[3] == "1");
                z.sv.enabled = (f[4] == "1");
                if (f.size() >= 6) z.tint = (unsigned)strtoul(f[5].c_str(), NULL, 10);
                break;
            }
        }
    }
}

static void WriteWsStateNow()
{
    if (!HasWorkspace() || !DirExists(WsRoot())) return;
    const std::string text = WsStateText();
    if (text == g_WsStateWritten) return;
    const std::string dir = JoinPath(WsRoot(), kStateDir);
    if (!DirExists(dir))
    {
        if (!EnsureDir(dir)) return;
        SetFileAttributesA(dir.c_str(), FILE_ATTRIBUTE_HIDDEN);
    }
    if (WriteTextFile(WsStatePath(), text)) g_WsStateWritten = text;
}

// Twice a second: whatever changed (an active switch, a colour, a folded
// folder) is written out.
static void WsStateTick()
{
    const double now = ImGui::GetTime();
    if (now < g_WsStateCheckAt) return;
    g_WsStateCheckAt = now + 0.5;
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) return;   // mid-drag on a colour wheel
    WriteWsStateNow();
}

// -----------------------------------------------------------------------------
// Watching the folder
// -----------------------------------------------------------------------------
static HANDLE g_WsWatch = INVALID_HANDLE_VALUE;
static double g_WsRescanAt = -1.0;

static void StopWatch()
{
    RecycleWait();
    if (g_WsWatch != INVALID_HANDLE_VALUE) FindCloseChangeNotification(g_WsWatch);
    g_WsWatch = INVALID_HANDLE_VALUE;
    g_WsRescanAt = -1.0;
}

static void StartWatch()
{
    StopWatch();
    if (!HasWorkspace()) return;
    g_WsWatch = FindFirstChangeNotificationA(WsRoot().c_str(), TRUE,
        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE);
}

// -----------------------------------------------------------------------------
// Loose files at the root
// -----------------------------------------------------------------------------
static bool LooksLikeWarriorFile(const std::string& p)
{
    const std::string ext = ExtOf(FileNameOf(p));
    if (ext == ".asm") return true;
    if (!ext.empty()) return false;
    const long long n = FileSizeOf(p);
    return n > 0 && n <= 512;
}

// Only a directory that holds warriors and nothing else is taken for a folder
// of survivors: one stray document, or a directory inside it, and it is
// somebody's own folder -- not read as warriors, and never sorted.
static bool LooksLikeSurvivorFolder(const std::string& dir)
{
    const std::vector<std::string> inside = ListDirFiles(dir);
    if (inside.empty() || !ListSubDirs(dir).empty()) return false;
    for (const std::string& p : inside)
        if (!LooksLikeWarriorFile(p)) return false;
    return true;
}

static int LooseWarriorCount(const std::string& root)
{
    int n = 0;
    for (const std::string& p : ListDirFiles(root)) if (LooksLikeWarriorFile(p)) n++;
    for (const std::string& sub : ListSubDirs(root))
        if (!IsLayoutDirName(sub) && !CleanFolderName(sub).empty() && LooksLikeSurvivorFolder(JoinPath(root, sub)))
            n += (int)ListDirFiles(JoinPath(root, sub)).size();
    return n;
}

// Moves a flat pile into assembly / survivors / zombies. ".asm" is source; a
// small file without an extension is machine code, and machine code whose
// name starts with "zom" is a zombie. A directory of nothing but warriors is
// sorted the same way into a folder of the same name.
static int SortFolderIntoSubfolders(const std::string& root)
{
    int moved = 0;
    auto sortDir = [&](const std::string& from, const std::string& sub) {
        for (const std::string& p : ListDirFiles(from))
        {
            if (!LooksLikeWarriorFile(p)) continue;
            const std::string fn  = FileNameOf(p);
            const std::string ext = ExtOf(fn);
            const char* dst = (ext == ".asm") ? kAsmDir
                            : (ToLower(fn).compare(0, 3, "zom") == 0 ? kZombDir : kSurvDir);
            std::string to = JoinPath(root, dst);
            if (!sub.empty() && dst != kZombDir) to = JoinPath(to, sub);
            if (!EnsureDirs(to)) continue;
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

static int g_WsLooseFiles = 0;            // warriors lying at the root of an open workspace

// -----------------------------------------------------------------------------
// Opening, closing, reloading
// -----------------------------------------------------------------------------
static std::string g_SortPromptRoot;      // non-empty while the sort popup is up

static void AddRecentWorkspace(const std::string& root)
{
    std::vector<std::string>& r = g_App.recentWorkspaces;
    for (size_t i = 0; i < r.size(); i++)
        if (ToLower(r[i]) == ToLower(root)) { r.erase(r.begin() + i); break; }
    r.insert(r.begin(), root);
    if (r.size() > 8) r.resize(8);
}

static std::string FullPathOf(const std::string& p)
{
    char buf[MAX_PATH * 2] = "";
    const DWORD n = GetFullPathNameA(p.c_str(), sizeof(buf), buf, NULL);
    std::string out = (n > 0 && n < sizeof(buf)) ? std::string(buf) : p;
    while (out.size() > 3 && (out.back() == '\\' || out.back() == '/')) out.pop_back();
    return out;
}

static void SelectFirstRow()
{
    const std::vector<Ref> rows = FlatRows();
    for (const Ref& r : rows)
    {
        if (IsFolderRef(r) || r.slot != -1) continue;
        Ref t = r;
        t.slot = g_App.players[r.player].FirstLoaded();
        if (t.slot < 0) continue;
        g_App.selection.assign(1, r);
        g_App.anchor = r;
        OpenInEditor(t);
        return;
    }
}

// Reads the whole workspace into the lists, replacing what was there.
static void LoadWorkspaceContents()
{
    { PERF("o.clear"); ClearWorkspace(); }
    std::vector<std::string> folders;
    std::vector<DiskEntry> entries;
    { PERF("o.scan"); ScanTree(std::string(), folders, entries, 0); }
    { PERF("o.folders"); for (const std::string& f : folders) EnsureFolder(f); }
    { PERF("o.place"); PlaceDiskEntries(entries, true); }

    std::vector<DiskEntry> zs;
    ScanZombies(zs);
    for (const DiskEntry& e : zs) LoadZombieEntry(e, true);

    g_WsStateWritten.clear();
    { PERF("o.state"); ApplyWsState(); }
    SortFolders();
    { PERF("o.loose"); g_WsLooseFiles = LooseWarriorCount(WsRoot()); }
}

// Saves what is unsaved and lets go of the folder.
static void CloseWorkspace()
{
    if (!HasWorkspace()) return;
    FlushEditorToModel();
    SaveDirty();
    WriteWsStateNow();
    StopWatch();
    ClearWorkspace();
    g_WsStateWritten.clear();
    g_Trash.clear();
    g_App.lastFolder.clear();
    g_WsLooseFiles = 0;
    g_App.status = "Workspace closed";
}

static void OpenWorkspace(const std::string& rootIn)
{
    if (rootIn.empty()) return;
    const std::string root = FullPathOf(rootIn);
    if (!DirExists(root)) { g_App.status = "No such folder: " + root; return; }

    // Warriors lying loose in a folder with no layout yet: nothing is moved
    // without asking.
    const bool structured = DirExists(JoinPath(root, kAsmDir)) || DirExists(JoinPath(root, kSurvDir)) ||
                            DirExists(JoinPath(root, kZombDir));
    if (!structured && LooseWarriorCount(root) > 0)
    {
        if (g_App.folderSortMode == 1) SortFolderIntoSubfolders(root);
        else { g_SortPromptRoot = root; return; }
    }

    { PERF("o.close"); CloseWorkspace(); }
    g_App.lastFolder = root;
    AddRecentWorkspace(root);
    LoadWorkspaceContents();
    { PERF("o.watch"); StartWatch(); }
    { PERF("o.select"); SelectFirstRow(); }

    int active = 0;
    for (const Player& pl : g_App.players) if (pl.enabled) active++;
    char buf[256];
    snprintf(buf, sizeof(buf), "%s: %d group(s), %d active, %d folder(s), %d zombie(s)",
             FileNameOf(root).c_str(), (int)g_App.players.size(), active,
             (int)g_App.folders.size(), (int)g_App.zombies.size());
    g_App.status = buf;
}

static void PickWorkspace()
{
    const std::string start = HasWorkspace() ? ParentDir(WsRoot()) : std::string();
    const std::string dir = BrowseForFolder(g_Hwnd, "Open a workspace folder (or make a new one)", start);
    if (!dir.empty()) OpenWorkspace(dir);
}

static void RevealInExplorer(const std::string& path)
{
    if (path.empty()) return;
    if (DirExists(path)) ShellExecuteA(g_Hwnd, "open", path.c_str(), NULL, NULL, SW_SHOWNORMAL);
    else
    {
        const std::string args = "/select,\"" + path + "\"";
        ShellExecuteA(g_Hwnd, "open", "explorer.exe", args.c_str(), NULL, SW_SHOWNORMAL);
    }
}

// Brings the lists in line with the disk after something changed it outside
// the app: new files come in, files that are gone take their survivors with
// them (unless there are edits in hand, which recreate the file), and a file
// that was edited elsewhere is read again.
static void ReconcileWorkspace()
{
    if (!HasWorkspace()) return;
    if (!DirExists(WsRoot())) { g_App.status = "The workspace folder is gone: " + WsRoot(); return; }
    RecycleWait();
    FlushEditorToModel();
    const SelectionMemo memo = RememberSelection();
    const Survivor* openBefore = SurvivorAt(g_App.open);

    std::vector<std::string> folders;
    std::vector<DiskEntry> entries;
    ScanTree(std::string(), folders, entries, 0);
    std::map<std::string, const DiskEntry*> disk;
    for (const DiskEntry& e : entries) disk[ToLower(e.folder) + "|" + ToLower(e.stem)] = &e;
    std::set<std::string> matched;

    int added = 0, gone = 0, reread = 0;
    for (int p = (int)g_App.players.size() - 1; p >= 0; p--)
    {
        Player& pl = g_App.players[p];
        for (int k = 0; k < kSlotCount; k++)
        {
            Survivor& sv = pl.slots[k];
            if (!sv.present) continue;
            const std::string key = ToLower(pl.folder) + "|" + ToLower(sv.name);
            auto it = disk.find(key);
            if (it == disk.end())
            {
                if (!sv.dirty) { const bool en = sv.enabled; sv = Survivor(); sv.enabled = en; gone++; }
                continue;
            }
            matched.insert(key);
            const DiskEntry& e = *it->second;
            if (sv.dirty) continue;                       // edits in hand win
            const bool asmMoved = !e.asmPath.empty() && e.asmTime != sv.asmTime;
            const bool binMoved = e.asmPath.empty() && !e.binPath.empty() && e.binTime != sv.binTime;
            if (asmMoved || binMoved)
            {
                LoadSurvivorFromDisk(sv, e, AsmPathFor(pl.folder, sv.name), BinPathFor(pl.folder, sv.name), false);
                RefreshEditorIfOpen(&sv);
                reread++;
            }
            else
            {
                sv.asmTime = e.asmTime ? e.asmTime : sv.asmTime;
                if (!e.binPath.empty()) sv.binTime = e.binTime;
            }
        }
        if (pl.LoadedCount() == 0) { g_App.players.erase(g_App.players.begin() + p); MarkPlayersChanged(); }
    }

    std::vector<DiskEntry> fresh;
    for (const DiskEntry& e : entries)
        if (!matched.count(ToLower(e.folder) + "|" + ToLower(e.stem))) fresh.push_back(e);
    for (const std::string& f : folders) if (FolderIndex(f) < 0) { EnsureFolder(f); added++; }
    PlaceDiskEntries(fresh, true);
    added += (int)fresh.size();

    // folders whose directories are gone, with nothing left in them
    for (int fi = (int)g_App.folders.size() - 1; fi >= 0; fi--)
    {
        const std::string& path = g_App.folders[fi].path;
        if (DirExists(TreeDir(kSurvDir, path)) || DirExists(TreeDir(kAsmDir, path))) continue;
        if (!GroupsUnder(path).empty()) continue;
        bool child = false;
        for (const GroupFolder& g : g_App.folders)
            if (ToLower(FolderParent(g.path)) == ToLower(path)) child = true;
        if (child) continue;
        g_App.folders.erase(g_App.folders.begin() + fi);
        MarkFoldersChanged();
        gone++;
    }

    // zombies, the same way
    {
        std::vector<DiskEntry> zs;
        ScanZombies(zs);
        std::map<std::string, const DiskEntry*> zd;
        for (const DiskEntry& e : zs) zd[ToLower(e.stem)] = &e;
        std::set<std::string> zm;
        for (int zi = (int)g_App.zombies.size() - 1; zi >= 0; zi--)
        {
            Zombie& z = g_App.zombies[zi];
            auto it = zd.find(ToLower(z.sv.name));
            if (it == zd.end())
            {
                if (!z.sv.dirty) { g_App.zombies.erase(g_App.zombies.begin() + zi); gone++; }
                continue;
            }
            zm.insert(it->first);
            const DiskEntry& e = *it->second;
            if (z.sv.dirty) continue;
            if ((!e.asmPath.empty() && e.asmTime != z.sv.asmTime) ||
                (e.asmPath.empty() && !e.binPath.empty() && e.binTime != z.sv.binTime))
            {
                LoadSurvivorFromDisk(z.sv, e, JoinPath(ZombieDir(), e.stem + ".asm"), JoinPath(ZombieDir(), e.stem), false);
                RefreshEditorIfOpen(&z.sv);
                reread++;
            }
        }
        for (const DiskEntry& e : zs)
            if (!zm.count(ToLower(e.stem))) { LoadZombieEntry(e, true); added++; }
    }

    SortFolders();
    g_WsLooseFiles = LooseWarriorCount(WsRoot());
    if (added || gone)
    {
        RestoreSelection(memo);
        if (SurvivorAt(g_App.open) != openBefore && g_App.open.Valid()) RefreshEditorIfOpen(SurvivorAt(g_App.open));
    }
    if (added || gone || reread)
    {
        std::string s = "Workspace changed on disk:";
        if (added)  s += " " + std::to_string(added) + " new";
        if (gone)   s += std::string(added ? "," : "") + " " + std::to_string(gone) + " gone";
        if (reread) s += std::string(added || gone ? "," : "") + " " + std::to_string(reread) + " re-read";
        g_App.status = s;
    }
}

// Once a frame: has anything under the folder changed? Changes come in bursts
// (and include our own saves), so the rescan waits for a quiet moment.
static void WsWatchTick()
{
    if (g_WsWatch == INVALID_HANDLE_VALUE) return;
    const double now = ImGui::GetTime();
    if (WaitForSingleObject(g_WsWatch, 0) == WAIT_OBJECT_0)
    {
        FindNextChangeNotification(g_WsWatch);
        g_WsRescanAt = now + 0.35;
    }
    if (g_RecycleFailed.exchange(false))
    {
        g_App.status = "Some files could not be deleted (in use, or protected) -- they stay in the workspace";
        g_WsRescanAt = now + 0.35;
    }
    // a deletion still on its way to the Recycle Bin is not a disk to read yet
    if (g_RecycleBusy.load()) { if (g_WsRescanAt > 0.0) g_WsRescanAt = ImMax(g_WsRescanAt, now + 0.2); return; }
    if (!g_RecycleBusy.load() && g_RecycleThread.joinable()) g_RecycleThread.join();
    if (g_WsRescanAt > 0.0 && now >= g_WsRescanAt &&
        !ImGui::IsMouseDown(ImGuiMouseButton_Left) && !ImGui::IsAnyItemActive() && !ImGui::IsDragDropActive())
    {
        g_WsRescanAt = -1.0;
        ReconcileWorkspace();
    }
}

// -----------------------------------------------------------------------------
// Making things
// -----------------------------------------------------------------------------
// Group names (lower case) already used in a folder -- in the lists, and by any
// file on disk the lists do not have yet.
static std::set<std::string> TakenGroupNames(const std::string& folder)
{
    std::set<std::string> s;
    for (int p : PlayersInFolder(folder)) s.insert(ToLower(g_App.players[p].name));
    for (const char* kind : { kSurvDir, kAsmDir })
        for (const std::string& p : ListDirFiles(TreeDir(kind, folder)))
        {
            std::string base; int index = 0;
            SplitIndexedStem(StemOf(FileNameOf(p)), base, index);
            s.insert(ToLower(base));
        }
    return s;
}

static std::set<std::string> TakenZombieStems()
{
    std::set<std::string> s;
    for (const Zombie& z : g_App.zombies) s.insert(ToLower(z.sv.name));
    for (const std::string& p : ListDirFiles(ZombieDir())) s.insert(ToLower(StemOf(FileNameOf(p))));
    return s;
}

static std::string UniqueName(const std::string& want, const std::set<std::string>& taken, const char* sep)
{
    if (!taken.count(ToLower(want))) return want;
    for (int n = 2; n < 1000; n++)
    {
        const std::string c = want + sep + std::to_string(n);
        if (!taken.count(ToLower(c))) return c;
    }
    return want + sep + "x";
}

// The folder new things go into: the selected folder, or the folder of the
// selected group, or the top.
static std::string TargetFolder()
{
    const Ref r = g_App.anchor.Valid() ? g_App.anchor
                : (g_App.selection.empty() ? Ref() : g_App.selection.back());
    if (IsFolderRef(r) && r.slot < (int)g_App.folders.size()) return g_App.folders[r.slot].path;
    if (r.player >= 0 && r.player < (int)g_App.players.size()) return g_App.players[r.player].folder;
    return std::string();
}

static void NewFolderIn(const std::string& parent)
{
    if (!HasWorkspace()) { g_App.status = "Open a workspace first"; return; }
    std::set<std::string> taken;
    for (const GroupFolder& f : g_App.folders)
        if (ToLower(FolderParent(f.path)) == ToLower(parent)) taken.insert(ToLower(FolderLeaf(f.path)));
    for (const char* kind : { kSurvDir, kAsmDir })
        for (const std::string& d : ListSubDirs(TreeDir(kind, parent))) taken.insert(ToLower(d));
    const std::string path = FolderJoin(parent, UniqueName("New folder", taken, " "));
    if (!EnsureDirs(TreeDir(kSurvDir, path)) || !EnsureDirs(TreeDir(kAsmDir, path)))
    {
        g_App.status = "Could not create " + TreeDir(kSurvDir, path);
        return;
    }
    const int fi = EnsureFolder(path);
    (void)fi;
    SortFolders();
    RevealFolder(parent);
    Ref r; r.player = kFolderPlayer; r.slot = FolderIndex(path);
    g_App.selection.assign(1, r);
    g_App.anchor = r;
    BeginInlineRename(r);
    g_App.status = "Created " + path + " -- type a name, Enter keeps it";
}

static void NewGroupIn(const std::string& folder)
{
    if (!HasWorkspace()) { g_App.status = "Open a workspace first"; return; }
    if (!folder.empty() && FolderIndex(folder) < 0) EnsureFolder(folder);
    const std::set<std::string> taken = TakenGroupNames(folder);
    std::string name;
    for (int i = 0; i < 26 && name.empty(); i++)
    {
        const std::string c = std::string("Group ") + (char)('A' + i);
        if (!taken.count(ToLower(c))) name = c;
    }
    if (name.empty()) name = UniqueName("Group", taken, " ");

    FlushEditorToModel();
    const int pi = FindOrCreatePlayer(name, folder);
    g_App.players[pi].tint  = NextPaletteTint();      // made here: the palette in turn
    g_App.players[pi].tint2 = SlotTint(g_App.players[pi].tint, 1);
    bool ok = true;
    for (int s = 0; s < kSlotCount; s++)
    {
        Survivor& sv = g_App.players[pi].slots[s];
        sv.name    = SurvivorStem(name, s);
        sv.asmPath = AsmPathFor(folder, sv.name);
        sv.binPath = BinPathFor(folder, sv.name);
        sv.code    = kTemplate;
        sv.present = true;
        sv.enabled = true;
        ReassembleSurvivor(sv);
        ok = WriteSurvivorFiles(sv) && ok;
    }
    RevealFolder(folder);
    Ref r; r.player = pi; r.slot = -1;
    g_App.selection.assign(1, r);
    g_App.anchor = r;
    Ref open = r; open.slot = 0;
    OpenInEditor(open);
    BeginInlineRename(r);
    g_App.status = ok ? "Created " + name + " -- type a name, Enter keeps it"
                      : "Created " + name + ", but could not write its files";
}

static void NewZombie()
{
    if (!HasWorkspace()) { g_App.status = "Open a workspace first"; return; }
    const std::string stem = UniqueName("zombie", TakenZombieStems(), " ");
    Zombie z;
    z.name  = stem;
    z.tint  = kPlayerTints[(g_App.zombies.size() + 3) % (sizeof(kPlayerTints) / sizeof(kPlayerTints[0]))];
    z.speed = g_App.zombieSpeed;
    z.sv.name    = stem;
    z.sv.asmPath = JoinPath(ZombieDir(), stem + ".asm");
    z.sv.binPath = JoinPath(ZombieDir(), stem);
    z.sv.code    = kTemplate;
    z.sv.present = true;
    ReassembleSurvivor(z.sv);
    WriteSurvivorFiles(z.sv);
    FlushEditorToModel();
    g_App.zombies.push_back(std::move(z));

    Ref r; r.player = kZombiePlayer; r.slot = (int)g_App.zombies.size() - 1;
    g_App.selection.assign(1, r);
    g_App.anchor = r;
    OpenInEditor(r);
    BeginInlineRename(r);
    g_App.status = "Created " + stem;
}

// -----------------------------------------------------------------------------
// Renaming and moving
// -----------------------------------------------------------------------------
struct FileMove { std::string from, to; };

// All or nothing: if one move fails, the ones already done are put back.
static bool DoMoves(const std::vector<FileMove>& moves, std::string& err)
{
    for (const FileMove& m : moves)
        if (ToLower(m.from) != ToLower(m.to) && PathExists(m.to))
        { err = FileNameOf(m.to) + " is already there"; return false; }
    std::vector<FileMove> done;
    for (const FileMove& m : moves)
    {
        if (m.from == m.to) continue;
        EnsureDirs(ParentDir(m.to));
        if (!MoveFileA(m.from.c_str(), m.to.c_str()))
        {
            err = "could not move " + FileNameOf(m.from);
            for (size_t i = done.size(); i-- > 0; ) MoveFileA(done[i].to.c_str(), done[i].from.c_str());
            return false;
        }
        done.push_back(m);
    }
    return true;
}

static bool RenameGroup(int pi, const std::string& raw)
{
    if (pi < 0 || pi >= (int)g_App.players.size()) return false;
    Player& pl = g_App.players[pi];
    const std::string name = CleanGroupName(raw);
    if (name.empty()) { g_App.status = "\"" + raw + "\" can't be a group name"; return false; }
    if (name == pl.name) return true;
    for (int q = 0; q < (int)g_App.players.size(); q++)
        if (q != pi && ToLower(g_App.players[q].name) == ToLower(name) &&
            ToLower(g_App.players[q].folder) == ToLower(pl.folder))
        {
            g_App.status = (pl.folder.empty() ? std::string("There is already a group") : pl.folder + " already has a group")
                         + " called " + name;
            return false;
        }

    FlushEditorToModel();
    std::vector<FileMove> moves;
    for (int k = 0; k < kSlotCount; k++)
    {
        const Survivor& sv = pl.slots[k];
        if (!sv.present) continue;
        const std::string stem = SurvivorStem(name, k);
        if (PathExists(sv.asmPath)) moves.push_back({ sv.asmPath, AsmPathFor(pl.folder, stem) });
        if (PathExists(sv.binPath)) moves.push_back({ sv.binPath, BinPathFor(pl.folder, stem) });
    }
    std::string err;
    if (!DoMoves(moves, err)) { g_App.status = "Could not rename " + pl.name + ": " + err; return false; }
    for (int k = 0; k < kSlotCount; k++)
    {
        Survivor& sv = pl.slots[k];
        if (!sv.present) continue;
        sv.name    = SurvivorStem(name, k);
        sv.asmPath = AsmPathFor(pl.folder, sv.name);
        sv.binPath = BinPathFor(pl.folder, sv.name);
    }
    g_App.status = "Renamed " + pl.name + " to " + name;
    pl.name = name;
    MarkPlayersChanged();
    return true;
}

static bool RenameZombie(int zi, const std::string& raw)
{
    if (zi < 0 || zi >= (int)g_App.zombies.size()) return false;
    Zombie& z = g_App.zombies[zi];
    const std::string stem = CleanFileName(raw);
    if (stem.empty() || stem.find('.') != std::string::npos)
    { g_App.status = "\"" + raw + "\" can't be a zombie's file name"; return false; }
    if (stem == z.sv.name) return true;
    std::set<std::string> taken = TakenZombieStems();
    taken.erase(ToLower(z.sv.name));
    if (taken.count(ToLower(stem))) { g_App.status = "There is already a zombie called " + stem; return false; }

    FlushEditorToModel();
    std::vector<FileMove> moves;
    const std::string na = JoinPath(ZombieDir(), stem + ".asm"), nb = JoinPath(ZombieDir(), stem);
    if (PathExists(z.sv.asmPath)) moves.push_back({ z.sv.asmPath, na });
    if (PathExists(z.sv.binPath)) moves.push_back({ z.sv.binPath, nb });
    std::string err;
    if (!DoMoves(moves, err)) { g_App.status = "Could not rename " + z.name + ": " + err; return false; }
    z.sv.asmPath = na;
    z.sv.binPath = nb;
    z.sv.name = stem;
    g_App.status = "Renamed " + z.name + " to " + stem;
    z.name = stem;
    return true;
}

// Moves (or renames: same thing) a folder and everything under it.
static bool MoveFolderTo(const std::string& oldPath, const std::string& newPath)
{
    if (oldPath.empty() || newPath.empty() || oldPath == newPath) return true;
    const bool caseOnly = ToLower(oldPath) == ToLower(newPath);
    if (!caseOnly && FolderUnder(newPath, oldPath))
    { g_App.status = "A folder can't go inside itself"; return false; }
    if (!caseOnly && (FolderIndex(newPath) >= 0 || DirExists(TreeDir(kSurvDir, newPath)) ||
                      DirExists(TreeDir(kAsmDir, newPath))))
    { g_App.status = newPath + " already exists"; return false; }

    FlushEditorToModel();
    SelectionMemo memo = RememberSelection();
    std::vector<FileMove> moves;
    for (const char* kind : { kSurvDir, kAsmDir })
        if (DirExists(TreeDir(kind, oldPath))) moves.push_back({ TreeDir(kind, oldPath), TreeDir(kind, newPath) });
    std::string err;
    if (!DoMoves(moves, err)) { g_App.status = "Could not move " + oldPath + ": " + err; return false; }
    // both trees keep the same shape
    EnsureDirs(TreeDir(kSurvDir, newPath));
    EnsureDirs(TreeDir(kAsmDir, newPath));

    auto rebase = [&](std::string& p) {
        if (FolderUnder(p, oldPath)) p = newPath + p.substr(oldPath.size());
    };
    for (GroupFolder& f : g_App.folders) rebase(f.path);
    for (Player& pl : g_App.players)
    {
        if (!FolderUnder(pl.folder, oldPath)) continue;
        rebase(pl.folder);
        for (Survivor& sv : pl.slots)
        {
            if (!sv.present) continue;
            sv.asmPath = AsmPathFor(pl.folder, sv.name);
            sv.binPath = BinPathFor(pl.folder, sv.name);
        }
    }
    for (RefKey& k : memo.sel) if (k.kind == 0 || k.kind == 2) rebase(k.folder);
    if (memo.anchor.kind >= 0) rebase(memo.anchor.folder);
    if (memo.open.kind == 0) rebase(memo.open.folder);
    SortFolders();
    RestoreSelection(memo);
    RevealFolder(FolderParent(newPath));
    return true;
}

static bool RenameFolder(const std::string& path, const std::string& raw)
{
    const std::string leaf = CleanFolderName(raw);
    if (leaf.empty()) { g_App.status = "\"" + raw + "\" can't be a folder name"; return false; }
    const std::string to = FolderJoin(FolderParent(path), leaf);
    if (!MoveFolderTo(path, to)) return false;
    if (to != path) g_App.status = "Renamed " + FolderLeaf(path) + " to " + leaf;
    return true;
}

static void MoveGroupsTo(const std::vector<int>& groups, const std::string& folderRaw)
{
    if (!HasWorkspace()) return;
    std::string folder = folderRaw;
    if (!folder.empty())
    {
        const int fi = FolderIndex(folder);
        folder = fi >= 0 ? g_App.folders[fi].path : folder;     // an existing folder keeps its spelling
        EnsureFolder(folder);
        EnsureDirs(TreeDir(kSurvDir, folder));
        EnsureDirs(TreeDir(kAsmDir, folder));
    }
    FlushEditorToModel();
    int moved = 0;
    std::string firstErr;
    for (int pi : groups)
    {
        if (pi < 0 || pi >= (int)g_App.players.size()) continue;
        Player& pl = g_App.players[pi];
        if (ToLower(pl.folder) == ToLower(folder)) continue;
        std::string why;
        for (int q = 0; q < (int)g_App.players.size() && why.empty(); q++)
            if (q != pi && ToLower(g_App.players[q].name) == ToLower(pl.name) &&
                ToLower(g_App.players[q].folder) == ToLower(folder))
                why = (folder.empty() ? std::string("there is already a loose group")
                                      : FolderLeaf(folder) + " already has a group") + " called " + pl.name;
        std::vector<FileMove> moves;
        for (const Survivor& sv : pl.slots)
        {
            if (!sv.present) continue;
            if (PathExists(sv.asmPath)) moves.push_back({ sv.asmPath, AsmPathFor(folder, sv.name) });
            if (PathExists(sv.binPath)) moves.push_back({ sv.binPath, BinPathFor(folder, sv.name) });
        }
        if (why.empty()) DoMoves(moves, why);
        if (!why.empty()) { if (firstErr.empty()) firstErr = why; continue; }
        pl.folder = folder;
        MarkPlayersChanged();
        for (Survivor& sv : pl.slots)
        {
            if (!sv.present) continue;
            sv.asmPath = AsmPathFor(folder, sv.name);
            sv.binPath = BinPathFor(folder, sv.name);
        }
        moved++;
    }
    RevealFolder(folder);
    std::string s = "Moved " + std::to_string(moved) + " group(s) " +
                    (folder.empty() ? std::string("to the top") : "into " + folder);
    if (!firstErr.empty()) s += (moved ? "  -- skipped some: " : "  -- ") + firstErr;
    g_App.status = (moved || !firstErr.empty()) ? s : "Already there";
}

// A folder's groups and folders go up one level, and the folder goes.
static void UngroupFolder(const std::string& path)
{
    const int fi = FolderIndex(path);
    if (fi < 0) return;
    const std::string parent = FolderParent(path);
    MoveGroupsTo(GroupsIn(path), parent);
    std::vector<std::string> kids;
    for (int c : FolderChildren(path)) kids.push_back(g_App.folders[c].path);
    for (const std::string& c : kids) MoveFolderTo(c, FolderJoin(parent, FolderLeaf(c)));
    if (!GroupsUnder(path).empty() || !FolderChildren(path).empty()) return;    // something would not move
    bool empty = true;
    for (const char* kind : { kSurvDir, kAsmDir })
        if (!ListDirFiles(TreeDir(kind, path)).empty() || !ListSubDirs(TreeDir(kind, path)).empty()) empty = false;
    if (!empty) { g_App.status = path + " still has other files in it, so it stays"; return; }
    for (const char* kind : { kSurvDir, kAsmDir }) RemoveDirectoryA(TreeDir(kind, path).c_str());
    const int at = FolderIndex(path);
    if (at >= 0) g_App.folders.erase(g_App.folders.begin() + at);
    MarkFoldersChanged();
    g_App.selection.clear();
    g_App.status = "Ungrouped " + path;
}

// -----------------------------------------------------------------------------
// Deleting, and bringing back
//
// Deleting sends the files to the Recycle Bin and keeps their bytes here, so
// Ctrl+Z can write them back exactly. The rows stay on screen, struck through,
// for a few seconds before they go.
// -----------------------------------------------------------------------------
static bool SnapshotFile(const std::string& p, DeleteBatch& b)
{
    FileBlob f;
    f.path = p;
    if (!ReadFileBytes(p, f.bytes)) return false;
    b.files.push_back(std::move(f));
    return true;
}

static void SnapshotTree(const std::string& dir, DeleteBatch& b, int depth = 0)
{
    b.dirs.push_back(dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(JoinPath(dir, "*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    std::vector<std::string> subs;
    do {
        const std::string n = fd.cFileName;
        if (n == "." || n == "..") continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) subs.push_back(n);
        else SnapshotFile(JoinPath(dir, n), b);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    if (depth < 16) for (const std::string& s : subs) SnapshotTree(JoinPath(dir, s), b, depth + 1);
}

static void DeleteRefs(const std::vector<Ref>& refs)
{
    if (!HasWorkspace()) return;
    FlushEditorToModel();
    AppState& S = g_App;

    // what goes: folders (not counting one inside another that goes), groups
    // (picked, or inside a folder that goes), lone survivors, zombies
    std::vector<std::string> folders;
    for (const Ref& r : refs)
        if (IsFolderRef(r) && r.slot < (int)S.folders.size()) folders.push_back(S.folders[r.slot].path);
    std::vector<std::string> topFolders;
    for (const std::string& f : folders)
    {
        bool inside = false;
        for (const std::string& g : folders) if (ToLower(g) != ToLower(f) && FolderUnder(f, g)) inside = true;
        bool dup = false;
        for (const std::string& t : topFolders) if (ToLower(t) == ToLower(f)) dup = true;
        if (!inside && !dup) topFolders.push_back(f);
    }
    auto underGone = [&](const std::string& folder) {
        for (const std::string& f : topFolders) if (FolderUnder(folder, f)) return true;
        return false;
    };

    std::set<int> groups;
    for (const Ref& r : refs)
        if (!IsFolderRef(r) && !IsZombieRef(r) && r.player >= 0 && r.player < (int)S.players.size() && r.slot == -1)
            groups.insert(r.player);
    for (int p = 0; p < (int)S.players.size(); p++) if (underGone(S.players[p].folder)) groups.insert(p);

    std::vector<std::pair<int, int>> lone;     // survivors whose group stays
    for (const Ref& r : refs)
        if (!IsFolderRef(r) && !IsZombieRef(r) && r.player >= 0 && r.player < (int)S.players.size() &&
            r.slot >= 0 && r.slot < kSlotCount && !groups.count(r.player) && S.players[r.player].slots[r.slot].present)
            lone.push_back(std::make_pair(r.player, r.slot));
    std::set<int> zombies;
    for (const Ref& r : refs) if (IsZombieRef(r) && r.slot < (int)S.zombies.size()) zombies.insert(r.slot);

    if (topFolders.empty() && groups.empty() && lone.empty() && zombies.empty())
    { S.status = "Nothing selected to delete"; return; }

    DeleteBatch b;
    b.at = ImGui::GetTime();
    std::vector<std::string> recycle;

    for (const std::string& f : topFolders)
    {
        const std::string parent = FolderParent(f);
        int pos = 0;
        for (int c : FolderChildren(parent))
        {
            if (ToLower(S.folders[c].path) == ToLower(f)) break;
            bool goes = false;
            for (const std::string& t : topFolders) if (ToLower(t) == ToLower(S.folders[c].path)) goes = true;
            if (!goes) pos++;
        }
        GhostRow g; g.kind = 1; g.folder = parent; g.label = FolderLeaf(f); g.pos = pos;
        b.ghosts.push_back(g);
        for (const char* kind : { kSurvDir, kAsmDir })
            if (DirExists(TreeDir(kind, f))) { SnapshotTree(TreeDir(kind, f), b); recycle.push_back(TreeDir(kind, f)); }
        for (const GroupFolder& gf : S.folders) if (FolderUnder(gf.path, f)) b.folders.push_back(gf);
    }

    for (int p : groups)
    {
        const Player& pl = S.players[p];
        b.groups.push_back(std::make_pair(p, pl));
        if (underGone(pl.folder)) continue;          // its files went with the folder
        int pos = 0;
        for (int q : GroupsIn(pl.folder)) { if (q == p) break; if (!groups.count(q)) pos++; }
        GhostRow g; g.kind = 0; g.folder = pl.folder; g.label = pl.name; g.pos = pos; g.tint = pl.tint;
        b.ghosts.push_back(g);
        for (const Survivor& sv : pl.slots)
        {
            if (!sv.present) continue;
            for (const std::string& path : { sv.asmPath, sv.binPath })
                if (!path.empty() && PathExists(path) && SnapshotFile(path, b)) recycle.push_back(path);
        }
    }

    for (const std::pair<int, int>& ls : lone)
    {
        const Player& pl = S.players[ls.first];
        const Survivor& sv = pl.slots[ls.second];
        DeleteBatch::Slot s;
        s.folder = pl.folder; s.group = pl.name; s.slot = ls.second; s.sv = sv;
        s.tint = pl.tint; s.tint2 = pl.tint2; s.twoTone = pl.twoTone;
        b.slots.push_back(s);
        for (const std::string& path : { sv.asmPath, sv.binPath })
            if (!path.empty() && PathExists(path) && SnapshotFile(path, b)) recycle.push_back(path);
    }

    for (int zi : zombies)
    {
        const Zombie& z = S.zombies[zi];
        b.zombies.push_back(std::make_pair(zi, z));
        GhostRow g; g.kind = 2; g.label = z.name; g.pos = zi; g.tint = z.tint;
        for (int o : zombies) if (o < zi) g.pos--;
        b.ghosts.push_back(g);
        for (const std::string& path : { z.sv.asmPath, z.sv.binPath })
            if (!path.empty() && PathExists(path) && SnapshotFile(path, b)) recycle.push_back(path);
    }

    // the lists
    SelectionMemo memo = RememberSelection();
    for (auto it = zombies.rbegin(); it != zombies.rend(); ++it) S.zombies.erase(S.zombies.begin() + *it);
    for (const std::pair<int, int>& ls : lone)
    {
        Survivor& sv = S.players[ls.first].slots[ls.second];
        sv = Survivor();
        sv.enabled = false;
    }
    std::set<int> drop = groups;
    for (const std::pair<int, int>& ls : lone)
        if (S.players[ls.first].LoadedCount() == 0) drop.insert(ls.first);
    for (auto it = drop.rbegin(); it != drop.rend(); ++it) S.players.erase(S.players.begin() + *it);
    for (int fi = (int)S.folders.size() - 1; fi >= 0; fi--)
        if (underGone(S.folders[fi].path)) S.folders.erase(S.folders.begin() + fi);
    MarkFoldersChanged();
    MarkPlayersChanged();
    RestoreSelection(memo);
    g_Rename = InlineRename();

    std::string what;
    auto part = [&what](int n, const char* one, const char* many) {
        if (!n) return;
        if (!what.empty()) what += ", ";
        what += std::to_string(n) + " " + (n == 1 ? one : many);
    };
    part((int)topFolders.size(), "folder", "folders");
    part((int)groups.size(), "group", "groups");
    part((int)lone.size(), "survivor", "survivors");
    part((int)zombies.size(), "zombie", "zombies");
    b.what = what;
    RecycleInBackground(recycle);
    b.at = glfwGetTime();                    // the rows linger from now
    g_Trash.push_back(std::move(b));
    if (g_Trash.size() > 30) g_Trash.erase(g_Trash.begin());
    g_GroupsFocus = true;
    S.status = "Deleted " + what + "  --  Ctrl+Z brings it back (it is in the Recycle Bin too)";
}

static void DeleteSelection()
{
    if (g_App.selection.empty()) { g_App.status = "Nothing selected"; return; }
    DeleteRefs(g_App.selection);
}

static void UndoDelete()
{
    AppState& S = g_App;
    if (g_Trash.empty()) { S.status = "Nothing deleted to bring back"; return; }
    RecycleWait();                           // the files have to be gone before they come back
    const DeleteBatch& b = g_Trash.back();
    // never write over something that has taken a deleted file's place since
    for (const FileBlob& f : b.files)
        if (PathExists(f.path)) { S.status = "Can't bring it back: " + FileNameOf(f.path) + " is there again"; return; }

    FlushEditorToModel();
    SelectionMemo memo = RememberSelection();
    for (const std::string& d : b.dirs) EnsureDirs(d);
    int written = 0;
    for (const FileBlob& f : b.files)
        if (EnsureDirs(ParentDir(f.path)) && WriteFileBytes(f.path, f.bytes)) written++;

    auto retime = [](Survivor& sv) {
        if (!sv.present) return;
        sv.asmTime = FileTimeOf(sv.asmPath);
        sv.binTime = FileTimeOf(sv.binPath);
    };
    std::vector<RefKey> back;
    for (const GroupFolder& f : b.folders)
    {
        const int fi = EnsureFolder(f.path);
        S.folders[fi].collapsed = f.collapsed;
    }
    for (const GroupFolder& f : b.folders)
    {
        bool top = true;
        for (const GroupFolder& g : b.folders) if (ToLower(g.path) == ToLower(FolderParent(f.path))) top = false;
        if (top) { RefKey k; k.kind = 2; k.folder = f.path; back.push_back(k); }
    }
    std::vector<std::pair<int, Player>> groups = b.groups;
    std::sort(groups.begin(), groups.end(),
              [](const std::pair<int, Player>& x, const std::pair<int, Player>& y) { return x.first < y.first; });
    for (std::pair<int, Player>& g : groups)
    {
        Player pl = g.second;
        for (Survivor& sv : pl.slots) retime(sv);
        if (!pl.folder.empty()) EnsureFolder(pl.folder);
        const int at = ImMin(g.first, (int)S.players.size());
        S.players.insert(S.players.begin() + at, pl);
        MarkPlayersChanged();
        bool inFolder = false;
        for (const GroupFolder& f : b.folders) if (ToLower(f.path) == ToLower(pl.folder)) inFolder = true;
        if (!inFolder) { RefKey k; k.kind = 0; k.folder = pl.folder; k.name = pl.name; k.slot = -1; back.push_back(k); }
    }
    for (const DeleteBatch::Slot& s : b.slots)
    {
        int pi = -1;
        for (int p = 0; p < (int)S.players.size(); p++)
            if (ToLower(S.players[p].name) == ToLower(s.group) && ToLower(S.players[p].folder) == ToLower(s.folder)) pi = p;
        if (pi < 0)
        {
            pi = FindOrCreatePlayer(s.group, s.folder);
            S.players[pi].tint = s.tint; S.players[pi].tint2 = s.tint2; S.players[pi].twoTone = s.twoTone;
        }
        Survivor& slot = S.players[pi].slots[s.slot];
        if (slot.present) continue;
        slot = s.sv;
        retime(slot);
        RefKey k; k.kind = 0; k.folder = s.folder; k.name = s.group; k.slot = s.slot; back.push_back(k);
    }
    std::vector<std::pair<int, Zombie>> zs = b.zombies;
    std::sort(zs.begin(), zs.end(),
              [](const std::pair<int, Zombie>& x, const std::pair<int, Zombie>& y) { return x.first < y.first; });
    for (std::pair<int, Zombie>& z : zs)
    {
        retime(z.second.sv);
        S.zombies.insert(S.zombies.begin() + ImMin(z.first, (int)S.zombies.size()), z.second);
        RefKey k; k.kind = 1; k.name = z.second.sv.name; back.push_back(k);
    }
    const std::string what = b.what;
    g_Trash.pop_back();
    SortFolders();
    RestoreSelection(memo);
    for (const RefKey& k : back) if (k.kind == 0 || k.kind == 2) RevealFolder(k.kind == 2 ? FolderParent(k.folder) : k.folder);
    SelectKeys(back, false);
    S.status = "Brought back " + what;
}

// -----------------------------------------------------------------------------
// Bringing files in: pasted, dropped, or copied out
// -----------------------------------------------------------------------------
struct Incoming
{
    std::string src, folder, stem, base;
    int  index = 0;
    bool source = false;
    bool zombie = false;
};

static void CollectIncoming(const std::string& pathIn, const std::string& folder, bool zombie,
                            std::vector<Incoming>& out, int depth, int& skipped)
{
    std::string path = pathIn;
    while (path.size() > 3 && (path.back() == '\\' || path.back() == '/')) path.pop_back();
    const DWORD a = GetFileAttributesA(path.c_str());
    if (a == INVALID_FILE_ATTRIBUTES) { skipped++; return; }
    if (a & FILE_ATTRIBUTE_DIRECTORY)
    {
        if (depth > 8) return;
        const std::string leaf = FileNameOf(path), low = ToLower(leaf);
        std::string sub = folder;
        bool z = zombie;
        if (low == kZombDir) z = true;
        else if (low == kSurvDir || low == kAsmDir) { /* its contents go where it was dropped */ }
        else if (!zombie)
        {
            const std::string clean = CleanFolderName(leaf);
            if (clean.empty() || clean[0] == '.') return;
            sub = FolderJoin(folder, clean);
        }
        for (const std::string& f : ListDirFiles(path)) CollectIncoming(f, sub, z, out, depth + 1, skipped);
        for (const std::string& d : ListSubDirs(path)) CollectIncoming(JoinPath(path, d), sub, z, out, depth + 1, skipped);
        return;
    }
    if (IsHiddenAttr(a)) return;
    const std::string fn = FileNameOf(path), ext = ExtOf(fn);
    Incoming in;
    in.src = path; in.folder = folder; in.zombie = zombie;
    if (ext == ".asm" || ext == ".s")
    {
        if (FileSizeOf(path) > 256 * 1024) { skipped++; return; }
        in.source = true;
    }
    else if (ext.empty())
    {
        const long long n = FileSizeOf(path);
        if (n <= 0 || n > 512) { skipped++; return; }      // a warrior is at most 512 bytes
    }
    else { skipped++; return; }
    in.stem = StemOf(fn);
    SplitIndexedStem(in.stem, in.base, in.index);
    out.push_back(in);
}

// Copies warrior files and folders into the workspace. Files arrive the way
// the workspace keeps them -- source under assembly, machine code under
// survivors -- and a group whose name is taken where it lands gets "copy" added.
static void ImportIntoWorkspace(const std::vector<std::string>& paths, const std::string& folder, bool zombies)
{
    if (!HasWorkspace()) { g_App.status = "Open a workspace first"; return; }
    std::vector<Incoming> in;
    int skipped = 0;
    for (const std::string& p : paths) CollectIncoming(p, folder, zombies, in, 0, skipped);
    if (in.empty())
    {
        g_App.status = skipped ? "Nothing to bring in: " + std::to_string(skipped) + " file(s) are not warriors"
                               : "Nothing to bring in";
        return;
    }

    std::map<std::string, std::set<std::string>> taken;     // per folder
    std::map<std::string, std::string> renamed;             // folder|base -> new base
    std::set<std::string> zTaken = TakenZombieStems();
    std::map<std::string, std::string> zRenamed;            // stem -> new stem
    std::vector<RefKey> landed;
    int copied = 0;
    for (const Incoming& f : in)
    {
        std::string dst;
        if (f.zombie)
        {
            const std::string key = ToLower(f.stem);
            if (!zRenamed.count(key))
            {
                const std::string s = UniqueName(f.stem, zTaken, " copy ");
                zTaken.insert(ToLower(s));
                zRenamed[key] = s;
                RefKey k; k.kind = 1; k.name = s; landed.push_back(k);
            }
            dst = JoinPath(ZombieDir(), zRenamed[key] + (f.source ? ".asm" : ""));
        }
        else
        {
            const std::string fkey = ToLower(f.folder);
            if (!taken.count(fkey)) taken[fkey] = TakenGroupNames(f.folder);
            const std::string key = fkey + "|" + ToLower(f.base);
            if (!renamed.count(key))
            {
                std::string nb = f.base;
                if (taken[fkey].count(ToLower(nb)))
                {
                    nb = f.base + " copy";
                    nb = UniqueName(nb, taken[fkey], " ");
                }
                taken[fkey].insert(ToLower(nb));
                renamed[key] = nb;
                RefKey k; k.kind = 0; k.folder = f.folder; k.name = nb; k.slot = -1; landed.push_back(k);
            }
            const std::string nb = renamed[key];
            const std::string stem = (nb == f.base) ? f.stem
                                   : (f.index >= 1 && f.index <= kSlotCount) ? SurvivorStem(nb, f.index - 1) : nb;
            dst = f.source ? AsmPathFor(f.folder, stem) : BinPathFor(f.folder, stem);
            if (!f.folder.empty())
            {
                EnsureDirs(TreeDir(kSurvDir, f.folder));
                EnsureDirs(TreeDir(kAsmDir, f.folder));
            }
        }
        if (!EnsureDirs(ParentDir(dst)) || PathExists(dst)) { skipped++; continue; }
        if (CopyFileA(f.src.c_str(), dst.c_str(), TRUE)) copied++;
        else skipped++;
    }

    ReconcileWorkspace();
    for (const RefKey& k : landed) if (k.kind == 0) RevealFolder(k.folder);
    SelectKeys(landed, true);
    g_App.status = "Brought in " + std::to_string(copied) + " file(s)" +
                   (folder.empty() || zombies ? std::string() : " into " + folder) +
                   (skipped ? "  -- skipped " + std::to_string(skipped) + " that are not warriors" : std::string());
}

static void PasteFromClipboard(const std::string& folder, bool zombies)
{
    std::vector<std::string> paths;
#ifdef CWEST_TEST_HOOKS
    // test builds: the "clipboard" is testpaste.txt beside the exe, a path per line
    {
        std::string list;
        if (ReadTextFile(ExeDir() + "testpaste.txt", list))
        {
            size_t pos = 0;
            while (pos < list.size())
            {
                size_t nl = list.find('\n', pos);
                if (nl == std::string::npos) nl = list.size();
                const std::string p = list.substr(pos, nl - pos);
                if (!p.empty()) paths.push_back(p);
                pos = nl + 1;
            }
            ImportIntoWorkspace(paths, folder, zombies);
            return;
        }
    }
#endif
    if (OpenClipboard(g_Hwnd))
    {
        HANDLE h = GetClipboardData(CF_HDROP);
        if (h)
        {
            HDROP drop = (HDROP)h;
            const UINT n = DragQueryFileA(drop, 0xFFFFFFFF, NULL, 0);
            for (UINT i = 0; i < n; i++)
            {
                char buf[MAX_PATH * 2] = "";
                if (DragQueryFileA(drop, i, buf, sizeof(buf))) paths.push_back(buf);
            }
        }
        CloseClipboard();
    }
    if (paths.empty())
    {
        g_App.status = "Nothing to paste -- copy warrior files in Explorer (or groups here) first";
        return;
    }
    ImportIntoWorkspace(paths, folder, zombies);
}

// The selection's files onto the clipboard, the way Explorer puts them there --
// so they paste back in here as copies, or into any Explorer window.
static void CopySelectionToClipboard()
{
    std::vector<std::string> paths;
    auto add = [&paths](const std::string& p) {
        if (p.empty() || !PathExists(p)) return;
        for (const std::string& q : paths) if (ToLower(q) == ToLower(p)) return;
        paths.push_back(p);
    };
    for (const Ref& r : g_App.selection)
    {
        if (IsFolderRef(r))
        {
            if (r.slot >= (int)g_App.folders.size()) continue;
            add(TreeDir(kSurvDir, g_App.folders[r.slot].path));
            add(TreeDir(kAsmDir, g_App.folders[r.slot].path));
            continue;
        }
        if (IsZombieRef(r))
        {
            if (r.slot >= (int)g_App.zombies.size()) continue;
            add(g_App.zombies[r.slot].sv.binPath);
            add(g_App.zombies[r.slot].sv.asmPath);
            continue;
        }
        if (r.player < 0 || r.player >= (int)g_App.players.size()) continue;
        const Player& pl = g_App.players[r.player];
        for (int k = 0; k < kSlotCount; k++)
        {
            if (!pl.slots[k].present || (r.slot >= 0 && r.slot != k)) continue;
            add(pl.slots[k].binPath);
            add(pl.slots[k].asmPath);
        }
    }
    if (paths.empty()) { g_App.status = "Nothing selected to copy"; return; }

    size_t chars = 0;
    for (const std::string& p : paths) chars += p.size() + 1;
    HGLOBAL mem = GlobalAlloc(GHND, sizeof(DROPFILES) + chars + 1);
    if (!mem) return;
    DROPFILES* df = (DROPFILES*)GlobalLock(mem);
    df->pFiles = sizeof(DROPFILES);
    df->fWide = FALSE;
    char* at = (char*)df + sizeof(DROPFILES);
    for (const std::string& p : paths) { memcpy(at, p.c_str(), p.size() + 1); at += p.size() + 1; }
    *at = 0;
    GlobalUnlock(mem);
    if (OpenClipboard(g_Hwnd))
    {
        EmptyClipboard();
        if (!SetClipboardData(CF_HDROP, mem)) GlobalFree(mem);
        CloseClipboard();
        g_App.status = "Copied " + std::to_string((int)paths.size()) + " file(s) -- Ctrl+V pastes them as copies";
    }
    else GlobalFree(mem);
}

// -----------------------------------------------------------------------------
// The queue: changes asked for while the lists are being drawn
// -----------------------------------------------------------------------------
static std::vector<int> GroupIndicesOf(const std::vector<RefKey>& keys)
{
    std::vector<int> out;
    for (const RefKey& k : keys)
    {
        if (k.kind != 0) continue;
        RefKey g = k; g.slot = -1;
        const Ref r = RefOfKey(g);
        if (r.player >= 0 && std::find(out.begin(), out.end(), r.player) == out.end()) out.push_back(r.player);
    }
    return out;
}

static void RunWsOps()
{
    if (g_WsOps.empty()) return;
    std::vector<WsOp> ops;
    ops.swap(g_WsOps);
    for (const WsOp& op : ops)
    {
        switch (op.kind)
        {
        case WsOp::None: break;
        case WsOp::OpenWorkspace: OpenWorkspace(op.folder); break;
        case WsOp::PickWorkspace: PickWorkspace(); break;
        case WsOp::CloseWorkspace: CloseWorkspace(); break;
        case WsOp::MoveGroups:
        {
            MoveGroupsTo(GroupIndicesOf(op.items), op.folder);
            // folders picked along with the groups go too
            for (const RefKey& k : op.items)
                if (k.kind == 2 && ToLower(FolderParent(k.folder)) != ToLower(op.folder))
                    MoveFolderTo(k.folder, FolderJoin(op.folder, FolderLeaf(k.folder)));
            break;
        }
        case WsOp::MoveFolder:
            MoveFolderTo(op.name, FolderJoin(op.folder, FolderLeaf(op.name)));
            break;
        case WsOp::Rename:
        {
            if (op.items.empty()) break;
            const RefKey& k = op.items[0];
            if (k.kind == 2) RenameFolder(k.folder, op.name);
            else
            {
                const Ref r = RefOfKey(k);
                if (IsZombieRef(r)) RenameZombie(r.slot, op.name);
                else if (r.player >= 0) RenameGroup(r.player, op.name);
            }
            break;
        }
        case WsOp::NewFolder: NewFolderIn(op.folder); break;
        case WsOp::NewGroup:  NewGroupIn(op.folder); break;
        case WsOp::NewZombie: NewZombie(); break;
        case WsOp::Ungroup:   UngroupFolder(op.folder); break;
        case WsOp::Delete:
        {
            std::vector<Ref> refs;
            for (const RefKey& k : op.items) { const Ref r = RefOfKey(k); if (r.Valid()) refs.push_back(r); }
            DeleteRefs(refs);
            break;
        }
        case WsOp::Undo:    UndoDelete(); break;
        case WsOp::Import:  ImportIntoWorkspace(op.paths, op.folder, op.zombies); break;
        case WsOp::Paste:   PasteFromClipboard(op.folder, op.zombies); break;
        case WsOp::RunFolders:
        {
            std::vector<int> groups = GroupIndicesOf(op.items);
            CompetitionStartFolders(groups, op.paths);
            break;
        }
        case WsOp::SortLoose:
        {
            const int moved = SortFolderIntoSubfolders(WsRoot());
            ReconcileWorkspace();
            g_App.status = "Sorted " + std::to_string(moved) + " loose file(s) into the workspace";
            break;
        }
        }
    }
}

// -----------------------------------------------------------------------------
// The sort prompt
// -----------------------------------------------------------------------------
static void DrawSortPrompt()
{
    if (!g_SortPromptRoot.empty() && !ImGui::IsPopupOpen("Sort this folder?"))
        ImGui::OpenPopup("Sort this folder?");

    ImGui::SetNextWindowSize(ImVec2(440 * g_Scale, 0), ImGuiCond_Always);
    if (!ImGui::BeginPopupModal("Sort this folder?", NULL, ImGuiWindowFlags_NoResize)) return;

    ImGui::TextWrapped("%s has warrior files lying loose in it.", FileNameOf(g_SortPromptRoot).c_str());
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, Hex(Col::TextDim));
    ImGui::TextWrapped("A workspace keeps source in assembly, machine code in survivors and zombies in "
                       "zombies. Sorting moves .asm files into assembly, small files without an extension "
                       "into survivors (or zombies, when the name starts with \"zom\"), and a directory of "
                       "nothing but warriors in as a folder of groups. Everything else stays where it is.");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    static bool dontAsk = false;
    ImGui::Checkbox("Always sort, don't ask", &dontAsk);
    ImGui::Separator();

    const std::string root = g_SortPromptRoot;
    if (ImGui::Button("Sort and open", ImVec2(150 * g_Scale, 0)))
    {
        if (dontAsk) g_App.folderSortMode = 1;
        g_SortPromptRoot.clear();
        const int moved = SortFolderIntoSubfolders(root);
        OpenWorkspace(root);
        g_App.status += "  (sorted " + std::to_string(moved) + " file(s))";
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100 * g_Scale, 0)))
    {
        g_SortPromptRoot.clear();
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}
