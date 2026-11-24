// =============================================================================
//  gui_options.inl -- options window, the open-folder flow, and zombies
//
//  Included once from main.cpp, after the editor helpers so it can assemble and
//  disassemble while it walks a folder.
// =============================================================================
// The folder flow lives in gui_workspace.inl now (included from main.cpp).
// -----------------------------------------------------------------------------
// Zombie section of the groups bar
// -----------------------------------------------------------------------------
static float g_ZombieDragAcc = 0.0f;   // pixels banked while dragging a speed pill

// The cycles a zombie really gets per turn: its speed, twice over when it is
// heavy (the engine doubles a ZOMBIE_H's speed -- see War's zombie turn).
static int ZombieCycles(const Zombie& z) { return z.speed * (z.heavy ? 2 : 1); }

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
    if (!z.sv.enabled)      { bg = HexU(Col::Bg2, 0.7f); fg = HexU(Col::TextDim); border = HexU(z.tint, selected ? 0.95f : 0.55f); }
    else if (selected)      { bg = HexU(b.held ? Col::Bg4 : (b.hovered ? Col::Bg3 : Col::Bg2));
                              fg = HexU(z.tint); border = HexU(z.tint, 0.95f); }
    else                    { bg = HexU(z.tint, b.hovered ? 1.0f : 0.85f); fg = HexU(Col::Bg0); }

    const float r = 6.0f * g_Scale;
    dl->AddRectFilled(b.mn, b.mx, bg, r);
    if (border) dl->AddRect(b.mn, b.mx, border, r, 0, 1.4f);
    const char* lbl = z.heavy ? "H" : "Z";
    const ImVec2 ts = ImGui::CalcTextSize(lbl);
    dl->AddText(ImVec2((b.mn.x + b.mx.x - ts.x) * 0.5f, (b.mn.y + b.mx.y - ts.y) * 0.5f), fg, lbl);
    DrawLongPressHint(b.mn, b.mx);

    if (b.hovered) ImGui::SetTooltip("%s  x%d%s  -  %s", z.name.c_str(), ZombieCycles(z),
                                     z.heavy ? "  (heavy)" : "", z.sv.enabled ? "active" : "not active");
    if (clicked) HandleRowClick(ref, true);
    ImGui::PopID();
}

static void DrawZombieSection()
{
    AppState& S = g_App;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImGui::Dummy(ImVec2(0, 10 * g_Scale));
    const ImVec2 sectionTop = ImGui::GetCursorScreenPos();

    // --- header ------------------------------------------------------------
    {
        const ImVec2 o = ImGui::GetCursorScreenPos();
        const float  w = ImGui::GetContentRegionAvail().x;
        const float  bs = 22.0f * g_Scale;
        const float  h = bs + 12 * g_Scale;
        dl->AddLine(ImVec2(o.x, o.y), ImVec2(o.x + w, o.y), HexU(Col::Line));
        const ImVec2 ts = ImGui::CalcTextSize("Zombies");
        dl->AddText(ImVec2(o.x + 4 * g_Scale, o.y + (h - ts.y) * 0.5f + 2 * g_Scale), HexU(Col::Text), "Zombies");
        {
            int active = 0;
            for (const Zombie& z : S.zombies) if (z.sv.enabled) active++;
            char n[24];
            if (active == (int)S.zombies.size()) snprintf(n, sizeof(n), "%d", (int)S.zombies.size());
            else                                  snprintf(n, sizeof(n), "%d/%d", active, (int)S.zombies.size());
            ImGui::PushFont(NULL, ImGui::GetFontSize() * 0.82f);
            const ImVec2 ns = ImGui::CalcTextSize(n);
            const float ph = ns.y + 4 * g_Scale, pw = ImMax(ph, ns.x + 10 * g_Scale);
            const ImVec2 pmn(o.x + 4 * g_Scale + ts.x + 7 * g_Scale, o.y + (h - ph) * 0.5f + 2 * g_Scale);
            dl->AddRectFilled(pmn, ImVec2(pmn.x + pw, pmn.y + ph), HexU(Col::Bg3), ph * 0.5f);
            dl->AddText(ImVec2(pmn.x + (pw - ns.x) * 0.5f, pmn.y + 2 * g_Scale), HexU(Col::TextDim), n);
            ImGui::PopFont();
        }

        // new zombie
        ImGui::SetCursorScreenPos(ImVec2(o.x + w - bs, o.y + (h - bs) * 0.5f + 2 * g_Scale));
        BtnState b = BareButton("##znew", ImVec2(bs, bs));
        dl->AddRectFilled(b.mn, b.mx, HexU(b.held ? Col::Bg4 : b.hovered ? Col::Bg3 : Col::Bg2), 6 * g_Scale);
        DrawGlyphPlus(dl, ImVec2((b.mn.x + b.mx.x) * 0.5f, (b.mn.y + b.mx.y) * 0.5f), 4.5f * g_Scale,
                      HexU(b.hovered ? Col::Text : Col::TextDim));
        if (b.hovered) ImGui::SetTooltip("New zombie\nZombie files you paste or drop here land in zombies too");
        if (b.clicked) QueueWsOp(WsOp::NewZombie);

        ImGui::SetCursorScreenPos(ImVec2(o.x, o.y + h + 4 * g_Scale));
    }

    // the zombies deleted a moment ago, by where they were
    std::vector<GhostRef> ghosts;
    for (size_t bi = 0; bi < g_Trash.size(); bi++)
    {
        const float a = GhostAlpha(g_Trash[bi]);
        if (a <= 0.0f) continue;
        for (const GhostRow& g : g_Trash[bi].ghosts)
            if (g.kind == 2) ghosts.push_back(GhostRef{ &g, a, bi + 1 == g_Trash.size() });
    }
    const float rowH = 28.0f * g_Scale;

    if (S.zombies.empty() && ghosts.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, Hex(Col::TextDim));
        ImGui::TextWrapped("None yet.");
        ImGui::PopStyleColor();
    }

    // --- rows --------------------------------------------------------------
    for (int zi = 0; zi < (int)S.zombies.size(); zi++)
    {
        for (const GhostRef& g : ghosts) if (g.row->pos == zi) DrawGhostRow(*g.row, g.alpha, g.newest, rowH);
        Zombie& z = S.zombies[zi];
        Ref ref; ref.player = kZombiePlayer; ref.slot = zi;

        ImGui::PushID(2000 + zi);
        const float avail = ImGui::GetContentRegionAvail().x;
        const ImVec2 rowStart = ImGui::GetCursorScreenPos();

        const bool selected = IsSelected(ref);
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0, 0, 0, 0));
        bool clicked = ImGui::Selectable("##zrow", selected, ImGuiSelectableFlags_AllowOverlap,
                                         ImVec2(avail, rowH));
        ImGui::PopStyleColor(3);
        const ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
        const bool rowHovered = ImGui::IsItemHovered(), rowHeld = ImGui::IsItemActive();
        if (CheckLongPress(clicked)) ToggleRefEnabled(ref);
        if (selected)
        {
            dl->AddRectFilled(mn, mx, HexU(Col::Accent, rowHovered ? 0.20f : 0.15f), 7 * g_Scale);
            dl->AddRect(mn, mx, HexU(Col::Accent, 0.35f), 7 * g_Scale, 0, 1.0f);
        }
        else if (rowHovered || rowHeld)
            dl->AddRectFilled(mn, mx, HexU(rowHeld ? Col::Bg3 : Col::Bg2), 7 * g_Scale);
        DrawLongPressHint(mn, mx);
        const bool renaming = RenamingRef(ref);

        // Attached here, not further down: BeginPopupContextItem hangs off
        // whatever item was submitted last, and below this point that is the
        // speed pill rather than the row.
        if (ImGui::BeginPopupContextItem("##zctx"))
        {
            if (MultiSelected(ref)) SharedContextMenu(ref);
            else
            {
                ImGui::TextDisabled("%s", z.name.c_str());
                ImGui::Separator();
                if (ImGui::MenuItem("Rename", "F2")) BeginInlineRename(ref);
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
                if (ImGui::MenuItem("Active", "Ctrl+Q", z.sv.enabled)) ToggleRefEnabled(ref);
                if (ImGui::MenuItem("Export...")) ExportOpen(ExportKeysFor(ref));
                if (ImGui::MenuItem("Reveal in Explorer"))
                    RevealInExplorer(PathExists(z.sv.binPath) ? z.sv.binPath : z.sv.asmPath);
                ImGui::Separator();
                if (ImGui::MenuItem("Delete", "Del")) QueueDelete(std::vector<RefKey>(1, KeyOfRef(ref)));
            }
            ImGui::EndPopup();
        }

        // Speed reads as a small pill rather than a framed slider -- a 24px row
        // has no room for one, and drag-to-change is the same gesture anyway.
        // It shows what the zombie really gets: a heavy x2 is x4.
        char sp[8];
        snprintf(sp, sizeof(sp), "x%d", ZombieCycles(z));
        const float pillW = ImMax(ImGui::CalcTextSize("x16").x + 12 * g_Scale, 26.0f * g_Scale);
        const float pillH = ImGui::GetTextLineHeight() + 4 * g_Scale;

        const float av = 18.0f * g_Scale;
        const ImVec2 amn(mn.x + 6 * g_Scale, mn.y + (rowH - av) * 0.5f), amx(amn.x + av, amn.y + av);
        const ImVec2 nsz = ImGui::CalcTextSize(z.name.c_str());
        const float tx = amx.x + 9 * g_Scale, tyy = mn.y + (rowH - nsz.y) * 0.5f;
        if (!renaming)
        {
            dl->PushClipRect(mn, ImVec2(mx.x - pillW - 12 * g_Scale, mx.y), true);
            dl->AddText(ImVec2(tx, tyy), z.sv.enabled ? HexU(Col::Text) : HexU(Col::TextDim), z.name.c_str());
            dl->PopClipRect();
        }

        if (rowHovered && !ImGui::IsAnyItemActive())
            ImGui::SetTooltip("%s%s - %s\n\nClick: open in the editor\nClick the square or hold the row: %s\n"
                              "F2: rename   Del: delete   right-click: colour, heavy",
                              z.name.c_str(), z.heavy ? "  (heavy - double speed)" : "",
                              z.sv.enabled ? "active" : "not active",
                              z.sv.enabled ? "take it out" : "make it active");

        // avatar, like a group's: solid when active, an outline when not, and
        // a click on it is the switch
        {
            ImGui::SetCursorScreenPos(amn);
            BtnState ab = BareButton("##zavatar", ImVec2(av, av));
            if (z.sv.enabled) dl->AddRectFilled(amn, amx, HexU(z.tint, ab.hovered ? 1.0f : 0.92f), 5 * g_Scale);
            else
            {
                dl->AddRectFilled(amn, amx, HexU(ab.hovered ? Col::Bg3 : Col::Bg2), 5 * g_Scale);
                dl->AddRect(amn, amx, HexU(z.tint, ab.hovered ? 0.9f : 0.6f), 5 * g_Scale, 0, 1.5f);
            }
            const char* zl = z.heavy ? "H" : "Z";
            const ImVec2 zs = ImGui::CalcTextSize(zl);
            dl->AddText(ImVec2((amn.x + amx.x - zs.x) * 0.5f, (amn.y + amx.y - zs.y) * 0.5f),
                        z.sv.enabled ? HexU(Col::Bg0, 0.9f) : HexU(z.tint, 0.85f), zl);
            if (ab.hovered)
                ImGui::SetTooltip(z.sv.enabled ? "Active - in every war\nClick: take it out"
                                               : "Not active - sits out\nClick: make it active");
            if (ab.clicked) ToggleRefEnabled(ref);
        }

        if (renaming) InlineRenameField(ImVec2(tx - 4 * g_Scale, mn.y), ImVec2(mx.x - pillW - 12 * g_Scale, mx.y));

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
        {
            if (z.heavy)
                ImGui::SetTooltip("Cycles this zombie gets per turn: %d\n(x%d, doubled because it is heavy)\n\n"
                                  "Drag sideways to change", ZombieCycles(z), z.speed);
            else
                ImGui::SetTooltip("Cycles this zombie gets per turn: %d\n\nDrag sideways to change", z.speed);
        }

        if (clicked && !renaming)
        {
            HandleRowClick(ref, true);
            g_GroupsFocus = true;
            // zombies are warriors like any other, so the inspector follows too
            if (S.game.ready())
                for (int k = 0; k < S.game.numWarriors(); k++)
                    if (S.game.replay().warriorName[(size_t)k] == z.name)
                    { S.inspectWarrior = k; S.status = "Inspecting " + z.name; break; }
        }
        ImGui::PopID();
        ImGui::SetCursorScreenPos(ImVec2(rowStart.x, mx.y + 3 * g_Scale));
    }
    for (const GhostRef& g : ghosts) if (g.row->pos >= (int)S.zombies.size()) DrawGhostRow(*g.row, g.alpha, g.newest, rowH);
    ImGui::Dummy(ImVec2(0.0f, 0.0f));

    // files dropped on this part of the list are zombies
    const ImVec2 end = ImGui::GetCursorScreenPos();
    AddDropZone(sectionTop, ImVec2(sectionTop.x + ImGui::GetContentRegionAvail().x + 8 * g_Scale,
                                   ImMax(end.y, ImGui::GetWindowPos().y + ImGui::GetWindowHeight())),
                std::string(), true);
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
            // only a listing nobody has written down: real source is left alone
            if (sv.present && !sv.bin.empty() && !sv.dirty && sv.asmTime == 0) ListSurvivor(sv, sv.bin);
        }
    for (Zombie& z : g_App.zombies)
        if (z.sv.present && !z.sv.bin.empty() && !z.sv.dirty && z.sv.asmTime == 0) ListSurvivor(z.sv, z.sv.bin);
    Ref keep = g_App.open; g_App.open = Ref(); OpenInEditor(keep);
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
// `ctrlW` wide belongs, level with the middle of the text -- a control `ctrlH`
// tall when it is not one frame high. Returns true when the text half was
// clicked, so a switch can be flipped from its label.
static bool OptRow(const char* label, const char* note, float ctrlW, float ctrlH = 0.0f)
{
    OptDivide();
    const float  full   = ImGui::GetContentRegionAvail().x;
    const ImVec2 start  = ImGui::GetCursorScreenPos();
    const float  textW  = ImMax(90.0f * g_Scale, full - ctrlW - 18.0f * g_Scale);
    const float  frameH = ctrlH > 0.0f ? ctrlH : ImGui::GetFrameHeight();
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
    const float w = ImGui::CalcTextSize(button, NULL, true).x + ImGui::GetStyle().FramePadding.x * 2.0f + 8 * g_Scale;
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

// The strip is one item, and the segment under the mouse is worked out from
// where it is. A button per segment, each placed by hand, left the layout's
// cursor under the strip instead of under the row -- so beside a note longer
// than the strip is tall, the next row started on top of the note's last lines.
static bool Segmented(const char* id, int* v, const char* const* items, int n)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float h = ImGui::GetFrameHeight();
    const ImVec2 at = ImGui::GetCursorScreenPos();
    const float r = 6 * g_Scale;
    const float total = SegmentedWidth(items, n);

    ImGui::PushID(id);
    const bool hit = ImGui::InvisibleButton("##seg", ImVec2(total, h));
    const bool hovStrip = ImGui::IsItemHovered();
    ImGui::PopID();
    const float mouseX = ImGui::GetIO().MousePos.x;
    auto segW = [&](int i) { return ImGui::CalcTextSize(items[i]).x + 22 * g_Scale; };

    int under = -1;
    if (hovStrip)
    {
        float x = at.x;
        for (int i = 0; i < n; i++)
        {
            const float w = segW(i);
            if (mouseX >= x && mouseX < x + w) { under = i; break; }
            x += w;
        }
    }
    bool changed = false;
    if (hit && under >= 0 && *v != under) { *v = under; changed = true; }

    dl->AddRectFilled(at, ImVec2(at.x + total, at.y + h), HexU(Col::Bg2), r);
    float x = at.x;
    for (int i = 0; i < n; i++)
    {
        const ImVec2 ts = ImGui::CalcTextSize(items[i]);
        const float  w  = segW(i);
        const bool on = (*v == i), hov = (under == i);
        if (on)       dl->AddRectFilled(ImVec2(x + 2, at.y + 2), ImVec2(x + w - 2, at.y + h - 2), HexU(Col::Accent, 0.9f), r - 1);
        else if (hov) dl->AddRectFilled(ImVec2(x + 2, at.y + 2), ImVec2(x + w - 2, at.y + h - 2), HexU(Col::Bg3), r - 1);
        dl->AddText(ImVec2(x + (w - ts.x) * 0.5f, at.y + (h - ts.y) * 0.5f),
                    HexU(on ? Col::Bg0 : (hov ? Col::Text : Col::TextDim)), items[i]);
        x += w;
    }
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
#ifdef CWEST_TEST_HOOKS
    // test builds only: CWEST_OPTSECTION=<n> opens on that section
    {
        static bool picked = false;
        if (!picked) { picked = true; if (const char* e = getenv("CWEST_OPTSECTION")) section = atoi(e); }
    }
#endif
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
#ifdef CWEST_TEST_HOOKS
    // test builds only: CWEST_OPTSCROLL=<pixels> holds the section scrolled there
    if (const char* sc = getenv("CWEST_OPTSCROLL")) ImGui::SetScrollY((float)atof(sc));
#endif
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

        OptCard("GROUP COLOURS");
        {
            const float sw = ImGui::GetFrameHeight(), gap = 6 * g_Scale;
            OptRow("Palette",
                   "Groups made here take these in turn. A group read in from disk gets a colour "
                   "from its name instead. Either way it keeps it until you change it.",
                   5 * sw + 4 * gap);
            // each square placed outright, in one group: after OptRow, SameLine
            // would line the second square up with the label's top, not the first
            unsigned* pal[5] = { &S.groupPal0, &S.groupPal1, &S.groupPal2, &S.groupPal3, &S.groupPal4 };
            const ImVec2 at = ImGui::GetCursorScreenPos();
            ImGui::BeginGroup();
            for (int i = 0; i < 5; i++)
            {
                ImGui::SetCursorScreenPos(ImVec2(at.x + i * (sw + gap), at.y));
                ImGui::PushID(i);
                EditColour("##pal", pal[i], ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
                ImGui::PopID();
            }
            ImGui::EndGroup();

            if (OptAction("Default palette", "Reset",
                          "Back to the five colours the palette started with. Groups keep their colours."))
            {
                S.groupPal0 = 0xFF3B3B; S.groupPal1 = 0x2EDB4B; S.groupPal2 = 0x3D6BFF;
                S.groupPal3 = 0xFFD83A; S.groupPal4 = 0xD54BFF;
            }
            // asks first, in a dialog of its own: this replaces colours chosen by
            // hand, and there is no undo for it
            if (OptAction("Recolour every group", "Recolour all",
                          "Gives every group in the workspace the colour its name picks, as if it were "
                          "read in for the first time. Two-colour groups go back to one. "
                          "Colours you picked by hand are lost."))
                ImGui::OpenPopup("Recolour every group?");
            ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            if (ImGui::BeginPopupModal("Recolour every group?", NULL, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::Text("Give all %d group(s) the colour their name picks?", (int)S.players.size());
                ImGui::TextColored(Hex(Col::TextDim), "Colours picked by hand are replaced, and this cannot be undone.");
                ImGui::Dummy(ImVec2(0, 4 * g_Scale));
                if (ImGui::Button("Recolour all"))
                {
                    for (Player& pl : S.players)
                    {
                        pl.tint    = DefaultGroupTint(pl.name);
                        pl.tint2   = SlotTint(pl.tint, 1);
                        pl.twoTone = false;
                    }
                    S.status = "Recoloured " + std::to_string((int)S.players.size()) + " group(s) by their names";
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
        }
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
                   "The lines and the cells' rounded corners both scale with it and with the zoom. "
                   "Zooming in, they fade in from transparent rather than snapping on. "
                   "0 is no lines and square corners: one solid sheet.");
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
        OptToggle("Windows can leave the main window", &S.popOutWindows,
                  "Drag a panel by its tab -- or Options, an inspector, any window -- past the edge of "
                  "the app and it becomes a window of its own on the desktop. Drag it back over the "
                  "app to put it back in.");
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
        ImGui::PushStyleColor(ImGuiCol_Text, Hex(HasWorkspace() ? Col::Text : Col::TextDim));
        ImGui::TextWrapped("%s", HasWorkspace() ? WsRoot().c_str() : "No workspace open");
        ImGui::PopStyleColor();
        if (HasWorkspace())
            OptNote("%d group(s), %d folder(s), %d zombie(s). Everything is a file in this folder; "
                    "files added, removed or edited there from outside show up on their own.",
                    (int)S.players.size(), (int)S.folders.size(), (int)S.zombies.size());
        ImGui::Dummy(ImVec2(0, 2 * g_Scale));
        if (ImGui::Button("Open workspace...")) QueueWsOp(WsOp::PickWorkspace);
        ImGui::BeginDisabled(!HasWorkspace());
        ImGui::SameLine();
        if (ImGui::Button("Reveal")) RevealInExplorer(WsRoot());
        ImGui::SameLine();
        if (ImGui::Button("Read again")) ReconcileWorkspace();
        ImGui::SameLine();
        if (ImGui::Button("Save all")) SaveAll();
        ImGui::SameLine();
        if (ImGui::Button("Close")) QueueWsOp(WsOp::CloseWorkspace);
        ImGui::EndDisabled();
        g_OptRows = 1;
        OptToggle("Autosave", &S.autoSave,
                  "Edits are written to disk a moment after you stop typing. A survivor's machine "
                  "code is only rewritten once its source assembles cleanly. Off, a yellow dot on a "
                  "survivor marks unsaved changes (Ctrl+S saves).");
        ImGui::BeginDisabled(!HasWorkspace());
        if (OptAction("New group", "Create", "Two survivors from the template, in the selected folder."))
            QueueWsOp(WsOp::NewGroup, TargetFolder());
        if (OptAction("New zombie", "Create")) QueueWsOp(WsOp::NewZombie);
        ImGui::EndDisabled();
        OptCardEnd();

        OptCard("FOLDERS OF GROUPS");
        OptNote("A folder is a directory, the same one under survivors and under assembly, and "
                "folders go inside folders as deep as you like. Drag groups or folders onto a "
                "folder to move them in, or onto empty space to bring them to the top. F2 renames, "
                "Delete deletes (Ctrl+Z brings it back), Ctrl+A selects everything.");
        g_OptRows = 1;
        ImGui::BeginDisabled(!HasWorkspace());
        if (OptAction("New folder", "Create", "Inside the selected folder."))
            QueueWsOp(WsOp::NewFolder, TargetFolder());
        ImGui::EndDisabled();
        OptDivide();
        OptNote("To bring warriors in, paste them (Ctrl+V after copying them in Explorer), drop them "
                "on the window -- onto a folder to land them there -- or put them in the workspace's "
                "survivors or assembly folder yourself.");
        OptCardEnd();

        OptCard("FOLDER LAYOUT");
        {
            static const char* kSort[] = { "Ask", "Always sort" };
            OptSegmented("Warriors loose in a folder you open", &S.folderSortMode, kSort, 2,
                         "A workspace keeps them in assembly, survivors and zombies. Sorting moves them "
                         "there; nothing is moved without this.");
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
                  "Double-click a byte in the arena, the stack or the shared page, or a register, "
                  "and type the new value right there -- Enter or clicking away keeps it, Esc drops "
                  "it. Double-click a flag to flip it. The war is played out again with the change "
                  "on the next move.");
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
        OptToggle("Seed field on the toolbar", &S.compSeedInBar,
                  "Shown only while the panel is wide enough for it; the seed is always here.");
        OptToggle("Seed search on the toolbar", &S.compSeekTool,
                  "A tool that plays random wars between the groups you pick and keeps the seeds "
                  "of the ones the survivors you pick won -- so a game like that can be opened "
                  "in Debug.");
        OptCardEnd();

        OptCard("ENGINES");
        OptNote("C++ is this app's own engine on every core; Java is the CGX reference engine, "
                "playing the same battles.");
        g_OptRows = 1;
        if (OptToggle("Offer the Busy (C++) engine", &S.compAllowBusy,
                      "The C++ engine and the graphics card (gpu-engine) at the same time, each "
                      "playing half the battles.") && !S.compAllowBusy && S.compBackend == COMP_BOTH)
            S.compBackend = COMP_CPU;
        {
            // the hazard: a striped sign and the plain words
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 o = ImGui::GetCursorScreenPos();
            const float w = ImGui::GetContentRegionAvail().x;
            const float s = 18.0f * g_Scale;
            const char* warn = "Fire hazard. Busy runs the processor and the graphics card flat out "
                               "together for as long as the tournament lasts. A laptop, a dusty "
                               "case or a weak cooler can overheat -- it can throttle, shut down or, "
                               "in the worst case, fry the computer. Watch the temperatures, and "
                               "don't leave it running unattended.";
            const float textW = w - s - 28 * g_Scale;
            const ImVec2 ts = ImGui::CalcTextSize(warn, NULL, false, textW);
            const float h = ImMax(ts.y, s) + 14 * g_Scale;
            dl->AddRectFilled(o, ImVec2(o.x + w, o.y + h), HexU(Col::Danger, 0.12f), 7 * g_Scale);
            dl->AddRect(o, ImVec2(o.x + w, o.y + h), HexU(Col::Danger, 0.55f), 7 * g_Scale, 0, 1.0f);
            const ImVec2 c(o.x + 8 * g_Scale + s * 0.5f, o.y + 7 * g_Scale + s * 0.5f);
            dl->AddTriangleFilled(ImVec2(c.x, c.y - s * 0.5f), ImVec2(c.x + s * 0.55f, c.y + s * 0.45f),
                                  ImVec2(c.x - s * 0.55f, c.y + s * 0.45f), HexU(Col::Warn));
            dl->AddTriangle(ImVec2(c.x, c.y - s * 0.5f), ImVec2(c.x + s * 0.55f, c.y + s * 0.45f),
                            ImVec2(c.x - s * 0.55f, c.y + s * 0.45f), HexU(0x000000, 0.8f), 1.2f);
            dl->AddLine(ImVec2(c.x, c.y - s * 0.18f), ImVec2(c.x, c.y + s * 0.16f), HexU(0x000000), 2.0f * g_Scale);
            dl->AddCircleFilled(ImVec2(c.x, c.y + s * 0.30f), 1.2f * g_Scale, HexU(0x000000));
            ImGui::SetCursorScreenPos(ImVec2(o.x + s + 16 * g_Scale, o.y + 7 * g_Scale));
            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + textW);      // window-local, like ImGui wants it
            ImGui::PushStyleColor(ImGuiCol_Text, Hex(S.compAllowBusy ? Col::Danger : Col::TextDim));
            ImGui::TextUnformatted(warn);
            ImGui::PopStyleColor();
            ImGui::PopTextWrapPos();
            ImGui::SetCursorScreenPos(ImVec2(o.x, o.y + h + 4 * g_Scale));
            ImGui::Dummy(ImVec2(0, 0));
        }
        OptCardEnd();

        OptCard("BAR GRAPH");
        OptToggle("Columns that climb", &S.compBarsUp,
                  "Flips the graph: columns rise from the floor and climb to the right, the "
                  "winner at the far end.");
        OptToggle("Names and scores on the bars", &S.compBarLabels,
                  "Written along a bar that lies down, and turned on its side on a column that "
                  "stands up. A bar too thin to carry a line of text shows them on hover instead.");
        OptToggle("Shadow under names on the bars", &S.compBarShadow,
                  "A one-pixel drop shadow in the opposite tone, so a name stays readable on a bright "
                  "bar as well as a dark one.");
        OptToggle("Fit the whole field", &S.compBarsFit,
                  "The bars share the whole panel between them, never thinner than 3 px, so a "
                  "field of hundreds is one picture.");
        OptCardEnd();

        OptCard("SHARED TOURNAMENTS");
        OptRow("This machine's name",
               "How it is listed on a host's board and named in a result file. Two machines "
               "with the same name get numbered by the host.", 200 * g_Scale);
        ImGui::InputText("##netname", S.compNetName, sizeof(S.compNetName));
        {
            static const char* kWho[] = { "Anyone", "Password", "Ask me" };
            OptSegmented("Who may join when you host", &S.compNetProtect, kWho, 3,
                         "Anyone with the address; whoever types the password -- every message is then "
                         "sealed with it, so the tunnel carries the tournament but cannot read it; or "
                         "whoever you accept on the Competition panel when they knock.");
            S.compNetProtect = ImClamp(S.compNetProtect, 0, 2);
            if (S.compNetProtect == NET_PASSWORD)
            {
                OptRow("Password", "Joiners type the same one next to the address. It never crosses the wire.",
                       200 * g_Scale);
                ImGui::InputTextWithHint("##hostpw", "password", S.compNetPassword, sizeof(S.compNetPassword),
                                         ImGuiInputTextFlags_Password);
            }
            if (NetServerUp())
            {
                OptDivide();
                OptNote("You are listening now: a change here counts from the next time you press Listen.");
            }
        }
        OptRow("Port",
               "The local port the tunnel hands joiners to -- this machine only, nothing to open in "
               "a firewall. Change it only if it is taken. (host:port joins a host on this same machine.)",
               200 * g_Scale);
        ImGui::InputInt("##netport", &S.compNetPort, 0, 0);
        S.compNetPort = ImClamp(S.compNetPort, 1, 65535);
        OptDivide();
        OptNote("A hosted tournament is cut into %d phases of the game index: the host plays "
                "phase 0 and each joiner gets one of its own, so up to %d machines can share "
                "a run.",
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
                { "Ctrl+O",       "open a workspace" },
                { "Ctrl+N",       "new group" },
                { "Ctrl+S",       "save the open survivor" },
                { "Ctrl+Shift+S", "save everything" },
                { "Ctrl+,",       "these options" },
                { "Ctrl+Q",       "make the selection active / take it out" },
                { "F2",           "rename the selected group, folder or zombie" },
                { "Ctrl+A",       "select every row of the groups bar" },
                { "Ctrl+C / V",   "copy the selection's files / paste files in" },
                { "Delete",       "delete the selection (to the Recycle Bin)" },
                { "Ctrl+Z",       "bring back what was deleted (groups bar)" },
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
        OptNote("Groups bar: click opens and selects, Ctrl / Shift click multi-selects. A group's "
                "square is solid when it is active and an outline when it sits out -- click it (or "
                "hold the row) to switch. Right-click for colours, folders and runs against folders; "
                "with several rows selected it shows only what applies to all of them. Drag rows "
                "onto a folder to move them in.");
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
