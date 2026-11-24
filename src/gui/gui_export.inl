// =============================================================================
//  gui_export.inl -- Export: copies of survivors, as .asm source or as the
//  binaries the engine loads, into any folder you pick.
//
//  Before anything is written the dialog says what would go wrong in a real
//  tournament:
//    * int 0x89 is this app's own. Here it assembles to INT 87h, but any other
//      assembler makes CD 89 of it, and CD 89 kills the survivor that runs it.
//    * brk (EB 00) is a breakpoint for the debugger. In a real war it is a jump
//      to the next instruction: a turn thrown away every time it runs.
//    * a survivor whose source does not assemble has no binary to export.
//  Files already in the folder are only replaced when you say so.
//
//  Included by main.cpp after gui_workspace.inl.
// =============================================================================

struct ExportItem
{
    std::string name;                  // file stem, e.g. "Phoenix1"
    std::string folder;                // the group's folder ("" at the top); zombies: "zombies"
    std::string code;                  // the source as it stands, or the listing of a binary
    std::vector<unsigned char> bin;
    bool        assembles = true;
    bool        listingOnly = false;   // no source of its own: its .asm is the disassembly
    int         int89 = 0;             // int 0x89 places
    int         brks = 0;              // brk lines
};

struct ExportState
{
    bool        wantOpen = false;
    std::vector<RefKey> keys;          // what was asked for
    std::vector<ExportItem> items;     // what that comes to
    std::string pendingDir;            // picked, but some files are already there
    std::vector<std::string> clashes;
};
static ExportState g_Export;

// Lines whose instruction is brk, in source or in a listing (the
// disassembler writes EB 00 as BRK too).
static int ExportCountBrk(const std::string& code)
{
    int n = 0;
    size_t pos = 0;
    while (pos < code.size())
    {
        size_t nl = code.find('\n', pos);
        if (nl == std::string::npos) nl = code.size();
        std::string line = code.substr(pos, nl - pos);
        pos = nl + 1;
        const size_t sc = line.find(';');
        if (sc != std::string::npos) line.resize(sc);
        // the words of the line, a leading "label:" dropped
        std::vector<std::string> words;
        std::string w;
        for (char c : line)
        {
            if (c == ' ' || c == '\t' || c == '\r') { if (!w.empty()) { words.push_back(w); w.clear(); } }
            else w += c;
        }
        if (!w.empty()) words.push_back(w);
        size_t i = 0;
        if (i < words.size() && words[i].size() > 1 && words[i].back() == ':') i++;
        if (i < words.size() && ToLower(words[i]) == "brk") n++;
    }
    return n;
}

static void ExportAdd(std::vector<ExportItem>& out, std::set<const Survivor*>& seen, const Survivor& sv,
                      const std::string& folder)
{
    if (!sv.present || !seen.insert(&sv).second) return;
    ExportItem it;
    it.name = sv.name;
    it.folder = folder;
    it.code = sv.code;
    it.bin = sv.bin;
    it.assembles = sv.asmOk && !sv.bin.empty();
    it.listingOnly = (sv.asmTime == 0 && !sv.dirty);
    it.int89 = (int)sv.smartSites.size();
    it.brks = ExportCountBrk(sv.code);
    out.push_back(std::move(it));
}

// What the asked-for rows come to: every survivor of a group, every group in
// a folder (and its folders), a single survivor, a zombie.
static void ExportCollect()
{
    ExportState& E = g_Export;
    FlushEditorToModel();
    E.items.clear();
    std::set<const Survivor*> seen;
    auto addGroup = [&](int p, int slot) {
        if (p < 0 || p >= (int)g_App.players.size()) return;
        const Player& pl = g_App.players[(size_t)p];
        for (int s = 0; s < kSlotCount; s++)
            if (slot < 0 || slot == s) ExportAdd(E.items, seen, pl.slots[s], pl.folder);
    };
    for (const RefKey& k : E.keys)
    {
        if (k.kind == 2)
            for (int p : GroupsUnder(k.folder)) addGroup(p, -1);
        else if (k.kind == 1)
        {
            const Ref r = RefOfKey(k);
            if (r.slot >= 0 && r.slot < (int)g_App.zombies.size())
                ExportAdd(E.items, seen, g_App.zombies[(size_t)r.slot].sv, "zombies");
        }
        else if (k.kind == 0)
        {
            const Ref r = RefOfKey(k);
            addGroup(r.player, k.slot);
        }
    }
}

// The rows a menu's Export acts on: the whole selection when the row is part
// of it (survivors picked one by one included), otherwise just the row.
static std::vector<RefKey> ExportKeysFor(const Ref& row)
{
    std::vector<RefKey> out;
    if (IsSelected(row) && g_App.selection.size() > 1)
        for (const Ref& r : g_App.selection) { RefKey k = KeyOfRef(r); if (k.kind >= 0) out.push_back(k); }
    if (out.empty()) out.push_back(KeyOfRef(row));
    return out;
}

static void ExportOpen(const std::vector<RefKey>& keys)
{
    g_Export.keys = keys;
    g_Export.wantOpen = true;
}

// Where each item goes: names that meet in the same folder are numbered.
static std::vector<std::string> ExportTargets(const std::string& dir)
{
    const ExportState& E = g_Export;
    const bool bin = g_App.exportFormat == 1;
    std::vector<std::string> out;
    std::set<std::string> taken;
    for (const ExportItem& it : E.items)
    {
        if (bin && !it.assembles) { out.push_back(std::string()); continue; }
        const std::string sub = g_App.exportKeepFolders && !it.folder.empty() ? JoinPath(dir, it.folder) : dir;
        std::string stem = it.name;
        std::string path;
        for (int n = 2; ; n++)
        {
            path = JoinPath(sub, stem + (bin ? "" : ".asm"));
            if (taken.insert(ToLower(path)).second) break;
            stem = it.name + " (" + std::to_string(n) + ")";
        }
        out.push_back(path);
    }
    return out;
}

// Writes everything; `replace` says what to do with a file already there.
static void ExportWrite(const std::string& dir, bool replace)
{
    ExportState& E = g_Export;
    const bool bin = g_App.exportFormat == 1;
    const std::vector<std::string> targets = ExportTargets(dir);
    int written = 0, failed = 0, replaced = 0, kept = 0;
    for (size_t i = 0; i < E.items.size(); i++)
    {
        std::string path = targets[i];
        if (path.empty()) continue;                         // nothing to write (does not assemble)
        if (PathExists(path))
        {
            if (replace) replaced++;
            else
            {
                // keep both: the copy gets the next free number
                const std::string base = bin ? path : path.substr(0, path.size() - 4);
                for (int n = 2; PathExists(path); n++)
                    path = base + " (" + std::to_string(n) + ")" + (bin ? "" : ".asm");
                kept++;
            }
        }
        if (!EnsureDirs(ParentDir(path))) { failed++; continue; }
        std::string text = E.items[i].code;
        if (!text.empty() && text.back() != '\n') text += '\n';
        const bool ok = bin ? WriteBinaryFile(path, E.items[i].bin) : WriteTextFile(path, text);
        if (ok) written++; else failed++;
    }
    snprintf(g_App.exportDir, sizeof(g_App.exportDir), "%s", dir.c_str());
    std::string s = "Exported " + std::to_string(written) + (bin ? " binar" + std::string(written == 1 ? "y" : "ies")
                                                                 : " .asm file" + std::string(written == 1 ? "" : "s")) +
                    " to " + dir;
    if (replaced) s += " (" + std::to_string(replaced) + " replaced)";
    if (kept)     s += " (" + std::to_string(kept) + " kept beside the old ones)";
    if (failed)   s += " -- " + std::to_string(failed) + " could not be written";
    g_App.status = s;
}

static void DrawExportWindow()
{
    ExportState& E = g_Export;
    AppState& S = g_App;
#ifdef CWEST_TEST_HOOKS
    // test builds only: CWEST_EXPORT_RUN=<dir>|<0 asm, 1 binary>|<keep folders 0/1>
    // exports everything once, with no dialog, and writes what the dialog would
    // warn about to report.txt in <dir>
    {
        static bool ran = false;
        const char* run = getenv("CWEST_EXPORT_RUN");
        if (!ran && run && HasWorkspace() && !S.players.empty())
        {
            ran = true;
            std::string a = run;
            const size_t b1 = a.find('|'), b2 = a.find('|', b1 + 1);
            const std::string dir = a.substr(0, b1);
            S.exportFormat = atoi(a.c_str() + b1 + 1);
            S.exportKeepFolders = atoi(a.c_str() + b2 + 1) != 0;
            E.keys.clear();
            for (const Player& pl : S.players) { RefKey k; k.kind = 0; k.folder = pl.folder; k.name = pl.name; E.keys.push_back(k); }
            for (const Zombie& z : S.zombies) { RefKey k; k.kind = 1; k.name = z.sv.name; E.keys.push_back(k); }
            ExportCollect();
            std::string rep;
            for (const ExportItem& it : E.items)
                rep += it.name + "  folder=" + it.folder + "  int89=" + std::to_string(it.int89) + "  brk=" +
                       std::to_string(it.brks) + "  assembles=" + (it.assembles ? "1" : "0") +
                       "  listing=" + (it.listingOnly ? "1" : "0") + "  bytes=" + std::to_string(it.bin.size()) + "\n";
            const std::vector<std::string> t = ExportTargets(dir);
            for (const std::string& x : t) rep += "target " + (x.empty() ? std::string("(left out)") : x) + "\n";
            int clashes = 0;
            for (const std::string& x : t) if (!x.empty() && PathExists(x)) clashes++;
            rep += "clashes " + std::to_string(clashes) + "\n";
            ExportWrite(dir, false);
            rep += "status " + S.status + "\n";
            EnsureDirs(dir);
            WriteTextFile(JoinPath(dir, "report.txt"), rep);
        }
    }
    // test builds only: F7 exports every group and zombie
    if (ImGui::IsKeyPressed(ImGuiKey_F7, false) && !ImGui::IsPopupOpen("Export###exportdlg"))
    {
        std::vector<RefKey> all;
        for (const Player& pl : S.players) { RefKey k; k.kind = 0; k.folder = pl.folder; k.name = pl.name; all.push_back(k); }
        for (const Zombie& z : S.zombies) { RefKey k; k.kind = 1; k.name = z.sv.name; all.push_back(k); }
        ExportOpen(all);
    }
#endif
    if (E.wantOpen)
    {
        E.wantOpen = false;
        E.pendingDir.clear();
        E.clashes.clear();
        ExportCollect();
        ImGui::OpenPopup("Export###exportdlg");
    }
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(460 * g_Scale, 0), ImVec2(760 * g_Scale, 640 * g_Scale));
    if (!ImGui::BeginPopupModal("Export###exportdlg", NULL, ImGuiWindowFlags_AlwaysAutoResize)) return;

    const bool bin = S.exportFormat == 1;
    int zombies = 0, groups = 0;
    {
        std::set<std::string> g;
        for (const ExportItem& it : E.items)
        {
            if (it.folder == "zombies") zombies++;
            else g.insert(ToLower(it.folder) + "|" + it.name.substr(0, it.name.size() > 1 ? it.name.size() - 1 : 0));
        }
        groups = (int)g.size();
    }
    if (E.items.empty())
    {
        ImGui::TextUnformatted("Nothing to export: those rows hold no survivors.");
        if (ImGui::Button("Close")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }
    ImGui::Text("%d survivor%s%s", (int)E.items.size() - zombies, (int)E.items.size() - zombies == 1 ? "" : "s",
                zombies ? (" and " + std::to_string(zombies) + " zombie" + (zombies == 1 ? "" : "s")).c_str() : "");
    (void)groups;

    ImGui::BeginDisabled(!E.pendingDir.empty());
    ImGui::RadioButton("Assembly source (.asm)", &S.exportFormat, 0);
    ImGui::SameLine(0, 16 * g_Scale);
    ImGui::RadioButton("Binary (what the engine loads)", &S.exportFormat, 1);
    ImGui::Checkbox("Keep their folders", &S.exportKeepFolders);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Each survivor goes into a subfolder named like its folder here.\n"
                          "Off: all of them side by side in the folder you pick.");
    ImGui::EndDisabled();
    ImGui::Separator();

    // what would go wrong in a real tournament
    std::vector<const ExportItem*> int89, brk, broken, listing;
    for (const ExportItem& it : E.items)
    {
        if (it.int89) int89.push_back(&it);
        if (it.brks) brk.push_back(&it);
        if (!it.assembles) broken.push_back(&it);
        if (it.listingOnly) listing.push_back(&it);
    }
    auto names = [](const std::vector<const ExportItem*>& v, bool counts) {
        std::string s;
        for (size_t i = 0; i < v.size(); i++)
        {
            if (i) s += ", ";
            if (i == 8) { s += "and " + std::to_string(v.size() - 8) + " more"; break; }
            s += v[i]->name;
            if (counts && v[i]->brks > 1) s += " (" + std::to_string(v[i]->brks) + ")";
        }
        return s;
    };
    const float wrap = 600 * g_Scale;
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrap);
    bool any = false;
    if (!int89.empty())
    {
        any = true;
        ImGui::TextColored(Hex(Col::Warn), "int 0x89 in %d: %s", (int)int89.size(), names(int89, false).c_str());
        ImGui::TextColored(Hex(Col::TextDim), bin
            ? "The binary has INT 87h there (CD 87), which runs -- but anyone who assembles the source elsewhere gets "
              "CD 89, and CD 89 kills the survivor that runs it. Write int 0x87 in the source before sharing it."
            : "int 0x89 is this app's own: every other assembler makes it CD 89, and CD 89 kills the survivor that "
              "runs it. Write int 0x87 instead before it goes to a real tournament.");
    }
    if (!brk.empty())
    {
        any = true;
        ImGui::TextColored(Hex(Col::Warn), "brk in %d: %s", (int)brk.size(), names(brk, true).c_str());
        ImGui::TextColored(Hex(Col::TextDim),
            "brk is a breakpoint for the debugger. In a real war it is a jump to the next instruction (EB 00): "
            "a turn thrown away every time it runs. Take it out for a tournament.");
    }
    if (bin && !broken.empty())
    {
        any = true;
        ImGui::TextColored(Hex(Col::Danger), "Left out -- %d do not assemble: %s", (int)broken.size(), names(broken, false).c_str());
    }
    else if (!bin && !broken.empty())
    {
        any = true;
        ImGui::TextColored(Hex(Col::Warn), "%d do not assemble: %s", (int)broken.size(), names(broken, false).c_str());
    }
    if (!bin && !listing.empty())
        ImGui::TextColored(Hex(Col::TextDim), "%d have no source of their own (%s): their .asm is the disassembly.",
                           (int)listing.size(), names(listing, false).c_str());
    if (!any) ImGui::TextColored(Hex(Col::Ok), "Nothing to warn about.");
    ImGui::PopTextWrapPos();
    ImGui::Separator();

    if (!E.pendingDir.empty())
    {
        // the folder already holds some of these
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrap);
        ImGui::TextColored(Hex(Col::Warn), "%d of these are already in %s: %s%s", (int)E.clashes.size(),
                           E.pendingDir.c_str(), E.clashes[0].c_str(),
                           E.clashes.size() > 1 ? (" and " + std::to_string(E.clashes.size() - 1) + " more").c_str() : "");
        ImGui::PopTextWrapPos();
        if (ImGui::Button("Replace them")) { ExportWrite(E.pendingDir, true); ImGui::CloseCurrentPopup(); }
        ImGui::SameLine();
        if (ImGui::Button("Keep both")) { ExportWrite(E.pendingDir, false); ImGui::CloseCurrentPopup(); }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("The new copies get a number: \"Phoenix1 (2)\"");
        ImGui::SameLine();
        if (ImGui::Button("Back")) { E.pendingDir.clear(); E.clashes.clear(); }
    }
    else
    {
        if (ImGui::Button("Export to a folder..."))
        {
            std::string dir;
#ifdef CWEST_TEST_HOOKS
            if (const char* to = getenv("CWEST_EXPORT_TO")) dir = to;          // test builds: no picker
            else
#endif
            dir = BrowseForFolder(g_Hwnd, "Export to which folder?", S.exportDir);
            if (!dir.empty())
            {
                const std::vector<std::string> t = ExportTargets(dir);
                E.clashes.clear();
                for (const std::string& p : t)
                    if (!p.empty() && PathExists(p)) E.clashes.push_back(FileNameOf(p));
                if (E.clashes.empty()) { ExportWrite(dir, false); ImGui::CloseCurrentPopup(); }
                else E.pendingDir = dir;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}
