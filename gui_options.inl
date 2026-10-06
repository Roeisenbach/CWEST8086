// =============================================================================
//  gui_options.inl -- options window, the open-folder flow, and zombies
//
//  Included once from main.cpp, after the editor helpers so it can assemble and
//  disassemble while it walks a folder.
// =============================================================================
#include <shlobj.h>
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

static void ClearWorkspace()
{
    g_App.players.clear();
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

    int moved = 0;
    for (const std::string& p : ListDirFiles(root))
    {
        const std::string fn  = FileNameOf(p);
        const std::string ext = ExtOf(fn);
        const std::string low = ToLower(fn);

        const char* dst = NULL;
        if (ext == ".asm")      dst = kAsmDir;
        else if (ext.empty())   dst = (low.compare(0, 3, "zom") == 0) ? kZombDir : kSurvDir;
        if (!dst) continue;                       // .txt, .png, whatever -- leave it alone

        if (MoveFileA(p.c_str(), JoinPath(JoinPath(root, dst), fn).c_str())) moved++;
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
static bool SaveBinarySurvivor(Survivor& sv)
{
    if (sv.path.empty() || !ExtOf(FileNameOf(sv.path)).empty()) return false;

    const std::string dir  = sv.path.substr(0, sv.path.find_last_of("\\/"));
    const std::string stem = StemOf(FileNameOf(sv.path));
    const std::string sib  = JoinPath(JoinPath(dir.substr(0, dir.find_last_of("\\/") + 1), kAsmDir),
                                      stem + ".asm");
    const std::string asmOut = DirExists(JoinPath(dir.substr(0, dir.find_last_of("\\/") + 1), kAsmDir))
                             ? sib : (sv.path + ".asm");

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

    int assembled = 0, disassembled = 0;
    if (DirExists(aDir) && DirExists(sDir)) SyncFolderPair(aDir, sDir, assembled, disassembled);

    // Binaries define who runs; the matching source, if any, replaces the
    // disassembly so the editor shows what the author actually wrote.
    if (DirExists(sDir))
    {
        std::vector<std::string> bins;
        for (const std::string& p : ListDirFiles(sDir))
            if (ExtOf(FileNameOf(p)).empty()) bins.push_back(p);
        LoadBinarySurvivorFiles(bins);
    }

    if (DirExists(aDir))
    {
        for (const std::string& p : ListDirFiles(aDir))
        {
            if (ExtOf(FileNameOf(p)) != ".asm") continue;
            std::string text;
            if (!ReadTextFile(p, text)) continue;

            const std::string stem = StemOf(FileNameOf(p));
            Survivor* found = NULL;
            for (Player& pl : g_App.players)
                for (int k = 0; k < kSlotCount && !found; k++)
                    if (pl.slots[k].present && StemOf(pl.slots[k].name) == stem) found = &pl.slots[k];

            if (found)
            {
                found->code = text;
                found->path = p;                   // saving edits the source, not the binary
                found->dirty = false;
                ReassembleSurvivor(*found);
            }
            else
            {
                LoadSurvivorFiles(std::vector<std::string>(1, p));
            }
        }
    }

    if (DirExists(zDir)) LoadZombieFolder(zDir);

    // Every source-only survivor still needs assembling before it can run.
    for (Player& pl : g_App.players)
        for (int k = 0; k < kSlotCount; k++)
            if (pl.slots[k].present && pl.slots[k].bin.empty()) ReassembleSurvivor(pl.slots[k]);

    if (!g_App.players.empty())
    {
        Ref r; r.player = 0; r.slot = g_App.players[0].FirstLoaded();
        if (r.slot >= 0) { g_App.selection.assign(1, r); g_App.anchor = r; OpenInEditor(r); }
    }

    char buf[192];
    snprintf(buf, sizeof(buf), "%s: %d group(s), %d zombie(s)%s%s",
             FileNameOf(root).c_str(), (int)g_App.players.size(), (int)g_App.zombies.size(),
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
// Options window
// -----------------------------------------------------------------------------
static void PaletteEditor()
{
    bool edited = false;
    for (int i = 0; i < kPalCount; i++)
    {
        unsigned* slot = PalSlot(i);
        float c[3] = { ((*slot >> 16) & 0xFF) / 255.0f,
                       ((*slot >>  8) & 0xFF) / 255.0f,
                       ( *slot        & 0xFF) / 255.0f };
        ImGui::PushID(i);
        if (ImGui::ColorEdit3(PalName(i), c, ImGuiColorEditFlags_NoInputs))
        {
            *slot = ((unsigned)(c[0] * 255.0f + 0.5f) << 16) |
                    ((unsigned)(c[1] * 255.0f + 0.5f) <<  8) |
                     (unsigned)(c[2] * 255.0f + 0.5f);
            edited = true;
        }
        ImGui::PopID();
        if ((i % 2) == 0 && i + 1 < kPalCount) ImGui::SameLine(160 * g_Scale);
    }
    if (edited) { g_App.themeIndex = kThemeCount; ApplyTheme(); }
}

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
                       "Everything else is left where it is.");
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

// A section heading: the label in the accent colour with a rule running out to
// the right edge, matching the panel headers elsewhere in the app.
static void OptHeader(const char* label)
{
    ImGui::Dummy(ImVec2(0, 4 * g_Scale));
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float  w = ImGui::GetContentRegionAvail().x;
    const ImVec2 ts = ImGui::CalcTextSize(label);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddText(p, HexU(Col::Accent, 0.9f), label);
    dl->AddLine(ImVec2(p.x + ts.x + 8 * g_Scale, p.y + ts.y * 0.5f),
                ImVec2(p.x + w, p.y + ts.y * 0.5f), HexU(Col::Line));
    ImGui::Dummy(ImVec2(w, ts.y + 4 * g_Scale));
}

static void OptNote(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    ImGui::PushStyleColor(ImGuiCol_Text, Hex(Col::TextDim));
    ImGui::TextWrappedV(fmt, args);
    ImGui::PopStyleColor();
    va_end(args);
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

    ImGui::SetNextWindowSize(ImVec2(560 * g_Scale, 470 * g_Scale), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(430 * g_Scale, 260 * g_Scale), ImVec2(FLT_MAX, FLT_MAX));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    const bool shown = ImGui::Begin("Options", &S.showOptions, ImGuiWindowFlags_NoCollapse);
    ImGui::PopStyleVar();
    if (!shown) { ImGui::End(); return; }

    // ---- left rail --------------------------------------------------------
    const float railW = 132.0f * g_Scale;
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Hex(Col::Bg2));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8 * g_Scale, 10 * g_Scale));
    ImGui::BeginChild("##optnav", ImVec2(railW, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
    for (int i = 0; i < kSectionCount; i++)
    {
        const bool on = (section == i);
        ImGui::PushStyleColor(ImGuiCol_Header, Hex(Col::Accent, 0.22f));
        ImGui::PushStyleColor(ImGuiCol_Text, Hex(on ? Col::Text : Col::TextDim));
        if (ImGui::Selectable(kSections[i], on, 0, ImVec2(0, 24 * g_Scale))) section = i;
        ImGui::PopStyleColor(2);
        if (on)
        {
            const ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(mn.x - 4 * g_Scale, mn.y + 3 * g_Scale),
                                                      ImVec2(mn.x - 1 * g_Scale, mx.y - 3 * g_Scale),
                                                      HexU(Col::Accent), 1.5f * g_Scale);
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImGui::SameLine(0, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16 * g_Scale, 12 * g_Scale));
    ImGui::BeginChild("##optbody", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);

    switch (section)
    {
    // ------------------------------------------------------------ appearance
    case 0:
    {
        OptHeader("THEME");
        for (int i = 0; i < kThemeCount; i++)
        {
            if (i) ImGui::SameLine(0, 6 * g_Scale);
            // a swatch of the theme's own accent makes the list scannable
            const ImVec4 sw = Hex(kThemes[i].c[8]);
            ImGui::PushStyleColor(ImGuiCol_Button, Hex(kThemes[i].c[1]));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Hex(kThemes[i].c[3]));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, Hex(kThemes[i].c[4]));
            ImGui::PushStyleColor(ImGuiCol_Text, Hex(kThemes[i].c[6]));
            ImGui::PushID(i);
            const bool hit = ImGui::Button("Aa", ImVec2(46 * g_Scale, 34 * g_Scale));
            ImGui::PopID();
            ImGui::PopStyleColor(4);
            const ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(mn.x + 4 * g_Scale, mx.y - 6 * g_Scale),
                                                      ImVec2(mx.x - 4 * g_Scale, mx.y - 3 * g_Scale),
                                                      ImGui::GetColorU32(sw), 1.5f);
            if (S.themeIndex == i)
                ImGui::GetWindowDrawList()->AddRect(mn, mx, HexU(Col::Accent), 6 * g_Scale, 0, 2.0f);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", kThemes[i].name);
            if (hit) SetTheme(i);
        }
        ImGui::TextColored(Hex(Col::TextDim), "%s",
                           S.themeIndex == kThemeCount ? "Custom" : kThemes[S.themeIndex].name);
        if (S.themeIndex == kThemeCount)
        {
            ImGui::SameLine();
            if (ImGui::SmallButton("Reset to Midnight")) SetTheme(0);
        }

        OptHeader("COLOURS");
        OptNote("Editing any colour switches the theme to Custom.");
        PaletteEditor();

        OptHeader("PANELS");
        ImGui::Checkbox("Editor", &S.showEditor);            ImGui::SameLine(180 * g_Scale);
        ImGui::Checkbox("Debug", &S.showArena);
        ImGui::Checkbox("Competition", &S.showBoard);        ImGui::SameLine(180 * g_Scale);
        ImGui::Checkbox("Timeline", &S.showTimeline);
        ImGui::Checkbox("Survivor inspector", &S.showInspector); ImGui::SameLine(180 * g_Scale);
        ImGui::Checkbox("Arena offsets", &S.arenaRulers);
        ImGui::Checkbox("Inspector on the left of the arena", &S.inspectorLeft);
        ImGui::Checkbox("Collapse groups to survivors only", &S.groupsCollapsed);
        OptNote("The stack and shared columns always take the edge nearest the arena.");
        ImGui::Spacing();
        ImGui::SetNextItemWidth(200 * g_Scale);
        ImGui::SliderFloat("Inspector width", &S.inspectorWidth, 240.0f, 900.0f, "%.0f px");

        OptHeader("ARENA");
        ImGui::Checkbox("Own colour for untouched core", &S.arenaBgCustom);
        if (S.arenaBgCustom)
        {
            ImGui::SameLine();
            float ab[3] = { ((S.arenaBg >> 16) & 0xFF) / 255.0f,
                            ((S.arenaBg >>  8) & 0xFF) / 255.0f,
                            ( S.arenaBg        & 0xFF) / 255.0f };
            if (ImGui::ColorEdit3("##arenabg", ab, ImGuiColorEditFlags_NoInputs))
                S.arenaBg = ((unsigned)(ab[0] * 255.0f + 0.5f) << 16) |
                            ((unsigned)(ab[1] * 255.0f + 0.5f) <<  8) |
                             (unsigned)(ab[2] * 255.0f + 0.5f);
        }
        OptNote("The cells nobody has written to yet. The sheet behind the grid follows the theme.");

        OptHeader("TEXT SIZE");
        ImGui::SetNextItemWidth(200 * g_Scale);
        ImGui::SliderFloat("Panel text", &S.fontMonoSize, 9.0f, 26.0f, "%.0f px");
        ImGui::SetNextItemWidth(200 * g_Scale);
        ImGui::SliderFloat("Editor text", &S.fontEditorSize, 8.0f, 40.0f, "%.0f px");
        OptNote("Panel text covers the arena hex, the inspector and the timeline. The editor "
                "has its own size, also on Ctrl + and Ctrl -.");
        ImGui::Spacing();
        if (ImGui::Button("Reset panel layout")) g_ResetLayout = true;
        ImGui::SameLine();
        ImGui::Checkbox("ImGui demo", &S.showDemo);
        break;
    }

    // ----------------------------------------------------------------- files
    case 1:
    {
        OptHeader("WORKSPACE");
        ImGui::TextColored(Hex(Col::TextDim), "%s", S.lastFolder.empty() ? "(no folder open)"
                                                                         : S.lastFolder.c_str());
        ImGui::Spacing();
        if (ImGui::Button("Open folder...", ImVec2(130 * g_Scale, 0))) OpenWorkspaceFolder();
        ImGui::SameLine();
        if (ImGui::Button("Reload") && !S.lastFolder.empty()) LoadWorkspaceFolder(S.lastFolder);
        ImGui::SameLine();
        if (ImGui::Button("Import files...")) ImportLooseFiles();

        ImGui::Spacing();
        if (ImGui::Button("New group", ImVec2(130 * g_Scale, 0))) NewPlayer();
        ImGui::SameLine();
        if (ImGui::Button("New zombie")) NewZombie();
        ImGui::SameLine();
        if (ImGui::Button("Save all")) SaveAll();
        ImGui::SameLine();
        if (ImGui::Button("Close all") && !(S.players.empty() && S.zombies.empty())) ClearWorkspace();

        OptHeader("FOLDER LAYOUT");
        OptNote("A workspace folder holds assembly, survivors and zombies subfolders. "
                "When none of them exist:");
        ImGui::RadioButton("Ask whether to sort it", &S.folderSortMode, 0);
        ImGui::RadioButton("Always sort it",         &S.folderSortMode, 1);
        ImGui::RadioButton("Never sort it",          &S.folderSortMode, 2);

        OptHeader("KEEP BOTH FORMS IN STEP");
        ImGui::Checkbox("Assemble  assembly/*.asm  into  survivors/", &S.folderAssemble);
        ImGui::Checkbox("Disassemble  survivors/*  into  assembly/",  &S.folderDisassemble);
        OptNote("Source wins: a binary is only rewritten from source that assembles cleanly, "
                "and source is only written for a binary that has none.");
        break;
    }

    // ---------------------------------------------------------------- editor
    case 2:
    {
        OptHeader("EDITOR");
        ImGui::Checkbox("Assembled bytes gutter", &S.showAsmGutter);
        OptNote("The address and machine code of every line, down the left of the source.");
        ImGui::Spacing();
        ImGui::Checkbox("Double-click a byte to re-read the code from it", &S.dblClickDisasm);
        OptNote("Cuts the instruction that straddles that byte down to a db and decodes "
                "everything after it afresh. This rewrites the source.");
        ImGui::Spacing();
        if (ImGui::Checkbox("Uppercase disassembly", &S.asmUpperCase)) RelistAllSurvivors();
        bool ws = S.editor.IsShowingWhitespaces();
        if (ImGui::Checkbox("Show whitespace", &ws)) S.editor.SetShowWhitespaces(ws);
        ImGui::Spacing();
        OptNote("Source is assembled as you type -- there is nothing to press.");
        break;
    }

    // -------------------------------------------------------------- debugger
    case 3:
    {
        OptHeader("MEMORY VIEWS");
        ImGui::Checkbox("Stack follows SS",  &S.followSS);
        ImGui::Checkbox("Shared follows ES", &S.followES);
        ImGui::Checkbox("Code follows CS",   &S.followCS);
        OptNote("Each view decides for itself, and its heading in the survivor panel is the same "
                "switch. Off, a view stays on the block the survivor was given -- which is what "
                "you want once a warrior starts throwing its segment registers around.");

        OptHeader("BREAKPOINTS");
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##bp", "CD 20, F4, EB 00", S.breakPatterns, sizeof(S.breakPatterns));
        OptNote("Hex bytes, comma separated. Playback stops on the cycle whose instruction matches.");
        int bmode = S.breakStartsWith ? 1 : 0;
        if (ImGui::RadioButton("Instruction contains the bytes", &bmode, 0)) S.breakStartsWith = false;
        if (ImGui::RadioButton("Instruction starts with them",   &bmode, 1)) S.breakStartsWith = true;
        ImGui::Checkbox("Mark them on the timeline", &S.breakOnTimeline);

        OptHeader("POINTER RINGS IN THE ARENA");
        OptNote("Rings the arena cell each pointer of the inspected survivor lands on. Only the "
                "offset picks the cell -- the segment half never leaves the arena.");
        ImGui::Checkbox("Base (BX BP)",  &S.hlBase);  ImGui::SameLine(180 * g_Scale);
        ImGui::Checkbox("Index (SI DI)", &S.hlIndex);
        ImGui::Checkbox("Stack (SP)",    &S.hlPointer);
        float hc[3] = { ((S.hlColor >> 16) & 0xFF) / 255.0f,
                        ((S.hlColor >>  8) & 0xFF) / 255.0f,
                        ( S.hlColor        & 0xFF) / 255.0f };
        if (ImGui::ColorEdit3("Ring colour", hc, ImGuiColorEditFlags_NoInputs))
            S.hlColor = ((unsigned)(hc[0] * 255.0f + 0.5f) << 16) |
                        ((unsigned)(hc[1] * 255.0f + 0.5f) <<  8) |
                         (unsigned)(hc[2] * 255.0f + 0.5f);
        break;
    }

    // ------------------------------------------------------------ simulation
    case 4:
    {
        OptHeader("WAR");
        ImGui::SetNextItemWidth(200 * g_Scale);
        ImGui::InputText("Seed", S.seedText, sizeof(S.seedText));
        OptNote("Resolved exactly the way the reference does: a plain number is taken as-is, "
                "anything else goes through Java's string hash. Currently %lld.",
                (long long)SeedFromText(S.seedText));
        ImGui::SetNextItemWidth(200 * g_Scale);
        ImGui::DragInt("Max rounds", &S.maxRounds, 100.0f, 100, cw::War::MAX_ROUND);
        ImGui::SetNextItemWidth(200 * g_Scale);
        ImGui::SliderInt("Default zombie speed", &S.zombieSpeed, 1, 8);
        OptNote("Only what a newly loaded zombie starts at -- each one carries its own speed, "
                "set on its row in the groups bar.");
        ImGui::Checkbox("Advance the seed on every recompute", &S.advanceSeed);
        OptNote("What the reference does between wars: the seed is bumped and written back, so "
                "each run is a fresh fight rather than the same one again.");
        ImGui::Spacing();
        if (ImGui::Button("New war (re-precompute)")) GameArm();

        OptHeader("PLAYBACK");
        ImGui::SetNextItemWidth(200 * g_Scale);
        ImGui::SliderFloat("Speed", &S.speedExp, 0.0f, 30.0f, "%.1f");
        ImGui::SameLine();
        ImGui::TextColored(Hex(Col::Accent2), "%.0f r/s", RoundsPerSec());
        ImGui::Checkbox("Stop on events", &S.game.stopOnEvents); ImGui::SameLine(180 * g_Scale);
        ImGui::Checkbox("Play in reverse", &S.game.reverse);
        break;
    }

    // ----------------------------------------------------------- competition
    case 5:
    {
        OptHeader("BAR GRAPH");
        ImGui::Checkbox("Columns that climb", &S.compBarsUp);
        OptNote("Flips the graph: columns rise from the floor and climb to the right, the "
                "winner at the far end. Hover a column for its name and score.");
        ImGui::Checkbox("Fit the whole field", &S.compBarsFit);
        OptNote("The bars share the whole panel between them, never thinner than 3 px, so a "
                "field of hundreds is one picture. Names stay while the bars are tall enough "
                "to carry them; either way the stats come up on hover, the way the timeline "
                "lanes work.");

        OptHeader("SHARED TOURNAMENTS");
        ImGui::SetNextItemWidth(200 * g_Scale);
        ImGui::InputText("This machine's name", S.compNetName, sizeof(S.compNetName));
        OptNote("How this machine is listed on a host's board and named in a result file. "
                "Two machines with the same name get numbered by the host.");
        ImGui::SetNextItemWidth(200 * g_Scale);
        ImGui::InputInt("Port", &S.compNetPort, 0, 0);
        S.compNetPort = ImClamp(S.compNetPort, 1, 65535);
        OptNote("The port a host listens on and a joiner connects to. The same box is on the "
                "Competition panel; hosting needs it let through the firewall.");
        OptNote("A hosted tournament is cut into %d phases of the game index: the host plays "
                "phase 0 and each joiner gets one of its own, so up to %d machines can share "
                "a run. Shard files work the same way with as many phases as shards.",
                kNetStride, kNetStride - 1);
        break;
    }

    // ----------------------------------------------------------------- about
    default:
    {
        OptHeader("CWEST 8086");
        OptNote("Core Wars Editor, Simulator and Tournaments for the CodeGuru Xtreme 8086 arena.");

        OptHeader("SHORTCUTS");
        struct K { const char* k; const char* what; };
        static const K keys[] = {
            { "Ctrl+O",       "open a workspace folder" },
            { "Ctrl+B",       "import loose files" },
            { "Ctrl+N",       "new group" },
            { "Ctrl+S",       "save the open survivor" },
            { "Ctrl+Shift+S", "save everything" },
            { "Ctrl+,",       "these options" },
            { "Ctrl+Q",       "disable / enable the selection" },
            { "F5",           "play or pause" },
            { "Ctrl+F5",      "new war (re-precompute)" },
            { "Shift+F5",     "rewind" },
            { "F10",          "step one clock cycle" },
            { "Shift+F10",    "one clock cycle back" },
            { "F11",          "forward one round" },
            { "Shift+F11",    "back one round" },
            { "F8",           "continue to the next event" },
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

        OptHeader("MOUSE");
        OptNote("Groups bar: click opens and selects, Ctrl / Shift click multi-selects, hold to "
                "disable, right-click to rename and recolour.\n"
                "Arena and timeline: wheel zooms, drag pans, and the bar under the timeline is "
                "the viewport -- drag its ends to widen or narrow what you see.");
        break;
    }
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::End();
}
