// =============================================================================
//  Warrior inspector  --  registers, FLAGS bit by bit, the code around CS:IP,
//  and the private stack / group shared memory shown as 16-bit words.
//
//  Included by main.cpp after AppState, the palette helpers and WarriorColor
//  are in scope. Rendered either in the pane beside the arena or, once popped
//  out, in its own window (several may be open at once).
// =============================================================================

static cw::u8 MemAt(const cw::Player& p, cw::s16 seg, int off)
{
    return p.readByte(cw::RMAddr(seg, (cw::s16)off).linear);
}
static cw::u16 WordAt(const cw::Player& p, cw::s16 seg, int off)
{
    return (cw::u16)(MemAt(p, seg, off) | (MemAt(p, seg, off + 1) << 8));
}

// One "NAME  value" register cell.
// One "NAME value" register cell. The width follows the text rather than being
// fixed, so a longer label such as NRG keeps a gap before its value and nothing
// runs together once the interface font is turned up.
static void RegCell(const char* name, cw::u16 v, unsigned tint = Col::Text,
                    int wi = -1, int regSlot = -1)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float h = ImGui::GetTextLineHeight() + 6 * g_Scale;
    char buf[8];
    snprintf(buf, sizeof(buf), "%04X", v);
    const ImVec2 ns = ImGui::CalcTextSize(name);
    const ImVec2 ts = ImGui::CalcTextSize(buf);
    const float w = ImMax(72.0f * g_Scale, ns.x + ts.x + 22.0f * g_Scale);

    const ImVec2 p = ImGui::GetCursorScreenPos();
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), HexU(Col::Bg2), 4 * g_Scale);
    dl->AddText(ImVec2(p.x + 6 * g_Scale, p.y + 3 * g_Scale), HexU(Col::TextDim), name);
    dl->AddText(ImVec2(p.x + w - ts.x - 6 * g_Scale, p.y + 3 * g_Scale), HexU(tint), buf);
    ImGui::Dummy(ImVec2(w, h));
    if (g_App.editMode && regSlot >= 0 && ImGui::IsItemHovered())
    {
        dl->AddRect(p, ImVec2(p.x + w, p.y + h), HexU(Col::Accent2, 0.8f), 4 * g_Scale, 0, 1.4f);
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) RequestRegEdit(wi, regSlot, name, v);
    }
}

// FLAGS, one lit chip per bit.
static void FlagBits(cw::u16 flags)
{
    struct B { const char* n; cw::u16 m; const char* full; };
    static const B bits[] = {
        { "O", 0x0800, "Overflow"  }, { "D", 0x0400, "Direction" },
        { "I", 0x0200, "Interrupt" }, { "T", 0x0100, "Trap"      },
        { "S", 0x0080, "Sign"      }, { "Z", 0x0040, "Zero"      },
        { "A", 0x0010, "Aux carry" }, { "P", 0x0004, "Parity"    },
        { "C", 0x0001, "Carry"     }
    };
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float h = ImGui::GetTextLineHeight() + 6 * g_Scale;
    const float w = 21.0f * g_Scale;
    for (int i = 0; i < IM_ARRAYSIZE(bits); i++)
    {
        if (i) ImGui::SameLine(0, 3 * g_Scale);
        const bool on = (flags & bits[i].m) != 0;
        const ImVec2 p = ImGui::GetCursorScreenPos();
        dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h),
                          HexU(on ? Col::Accent : Col::Bg2, on ? 0.9f : 1.0f), 4 * g_Scale);
        const ImVec2 ts = ImGui::CalcTextSize(bits[i].n);
        dl->AddText(ImVec2(p.x + (w - ts.x) * 0.5f, p.y + 3 * g_Scale),
                    HexU(on ? Col::Bg0 : Col::TextDim), bits[i].n);
        ImGui::Dummy(ImVec2(w, h));
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s = %d", bits[i].full, on ? 1 : 0);
    }
}

// Disassembly around CS:IP. x86 is variable length, so we start a little way
// back and decode forward until the stream lands exactly on IP; that offset is
// the one whose decoding agrees with what the CPU is about to execute.
// ---------------------------------------------------------------------------
// Walking the instruction stream
//
// x86 cannot be decoded backwards, so the boundary before an address is found
// by decoding forward from far enough back that the stream re-syncs onto it.
// ---------------------------------------------------------------------------
static int NextInsn(const cw::Player& pl, cw::s16 cs, int at)
{
    cw::u8 b[cw::kOpBytes];
    for (int k = 0; k < cw::kOpBytes; k++) b[k] = MemAt(pl, cs, at + k);
    const int len = cw::disasm(b, cw::kOpBytes).len;
    return (int)(cw::u16)(at + (len > 0 ? len : 1));
}

static int PrevInsn(const cw::Player& pl, cw::s16 cs, int at)
{
    for (int back = 24; back >= 1; back--)
    {
        int cur = (int)(cw::u16)(at - back), prev = cur, guard = 0;
        while ((cw::u16)cur != (cw::u16)at && guard++ < 32)
        {
            prev = cur;
            cur = NextInsn(pl, cs, cur);
        }
        if ((cw::u16)cur == (cw::u16)at && (cw::u16)prev != (cw::u16)at) return prev;
    }
    return (int)(cw::u16)(at - 1);
}

static int WalkInsn(const cw::Player& pl, cw::s16 cs, int at, int n)
{
    for (; n > 0; n--) at = NextInsn(pl, cs, at);
    for (; n < 0; n++) at = PrevInsn(pl, cs, at);
    return (int)(cw::u16)at;
}

// The first instruction boundary at or after `at`, found by decoding forward
// from far enough back that the stream re-syncs onto the framing.
static int SyncBoundary(const cw::Player& pl, cw::s16 cs, int at)
{
    int cur = (int)(cw::u16)(at - 24);
    for (int guard = 0; guard < 48; guard++)
    {
        // (cur - at) staying in the low half means cur has reached or passed at
        if ((cw::u16)(cur - at) < 0x8000u) return (int)(cw::u16)cur;
        cur = NextInsn(pl, cs, cur);
    }
    return (int)(cw::u16)at;
}

// ---------------------------------------------------------------------------
// Code view -- a window onto the whole 0000..FFFF stream, not just around IP.
//
// The scrollbar spans the entire address space. A step jumps it so the
// instruction about to run sits a third of the way down; between steps it stays
// where it was left. While the war is playing it is pinned, so it cannot be
// scrolled out from under the survivor.
// ---------------------------------------------------------------------------
struct CodeViewState {
    cw::u64 seenCycle = ~(cw::u64)0;
    int     top = 0;              // the address of the first row -- authoritative
    float   setScroll = -1.0f;    // what we last asked the scrollbar for
    int     seenCs = -1;
    bool    primed = false;
    float   rawEase = 0.0f;
};
static std::map<int, CodeViewState> g_CodeViews;

static void CodeView(cw::GuiSession& g, const cw::CpuState& stIn, int wi, float height)
{
    const cw::Player& pl = g.player();
    // A local copy so the view can read through the arena segment instead of a
    // CS the warrior has thrown somewhere odd.
    cw::CpuState st = stIn;
    if (!g_App.followCS) st.cs = (cw::s16)cw::War::ARENA_SEGMENT;
    CodeViewState& V = g_CodeViews[wi];
    const bool running = g.running();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, Hex(Col::Bg0));
    ImGui::BeginChild("##code", ImVec2(0, height), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_AlwaysVerticalScrollbar |
                      (running ? ImGuiWindowFlags_NoScrollWithMouse : 0));
    if (g_App.fontMono) ImGui::PushFont(g_App.fontMono, g_App.fontMonoSize * g_Scale);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    const float rowH = ImGui::GetTextLineHeightWithSpacing();
    const ImVec2 wpos = ImGui::GetWindowPos();
    const ImVec2 wsz  = ImGui::GetWindowSize();
    // one past what fits, clipped, so the list runs to the bottom edge
    const int  rows   = ImMax(3, (int)(wsz.y / rowH) + 1);
    (void)0;   // IP is chased only when it leaves the band, not pinned to a row

    // One scroll row per byte: fine enough that a wheel notch glides rather than
    // jumping, and the thumb still says where in the 64K you are.
    ImGui::Dummy(ImVec2(1.0f, 0x10000 * rowH));

    // The address is the state; the scrollbar is only an input device. Reading
    // the position back out of ImGui lagged a frame, which made the listing
    // thrash whenever CS moved.
    const float scrollNow = ImGui::GetScrollY();
    if (!running && V.setScroll >= 0.0f && fabsf(scrollNow - V.setScroll) > 0.5f)
        V.top = SyncBoundary(pl, st.cs, ImClamp((int)(scrollNow / rowH), 0, 0xFFFF));

    if (!V.primed || V.seenCs != (int)(cw::u16)st.cs)
    {
        // first look, or the code segment moved: nothing on screen relates to
        // what is running now, so start clean with IP at the top
        V.primed = true;
        V.seenCs = (int)(cw::u16)st.cs;
        V.top = (int)(cw::u16)st.ip;
    }
    else if (V.seenCycle != g.cycle())
    {
        // Only chase IP when it has left the comfortable band: within two rows
        // of the bottom, or gone off the top. Otherwise the listing sits still
        // and the highlight walks down it.
        V.seenCycle = g.cycle();
        int at = V.top, found = -1;
        for (int r = 0; r < rows; r++)
        {
            if ((cw::u16)at == (cw::u16)st.ip) { found = r; break; }
            at = NextInsn(pl, st.cs, at);
        }
        if (found < 0)                       V.top = (int)(cw::u16)st.ip;         // above or past
        else if (found >= rows - 3)          V.top = WalkInsn(pl, st.cs, V.top, found - (rows - 3));
    }
    V.seenCycle = g.cycle();

    const float want = ImClamp((float)V.top * rowH, 0.0f, ImGui::GetScrollMaxY());
    ImGui::SetScrollY(want);
    V.setScroll = want;

    const int top = V.top;

    // The raw bytes ride on the right. Three wide is enough to read the opcode
    // without eating the disassembly; hovering opens it to a full instruction.
    const float byte1 = ImGui::CalcTextSize("00 ").x;
    const float rawFull = byte1 * 6.0f + 6.0f * g_Scale;
    const float rawNarrow = byte1 * 3.0f + 6.0f * g_Scale;
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const bool overRaw = ImGui::IsWindowHovered() &&
                         mouse.x >= wpos.x + wsz.x - rawFull - ImGui::GetStyle().ScrollbarSize;
    float& rawEase = V.rawEase;
    rawEase = ImClamp(rawEase + (overRaw ? 1.0f : -1.0f) * ImGui::GetIO().DeltaTime * 7.0f, 0.0f, 1.0f);
    const float rawW = rawNarrow + (rawFull - rawNarrow) * rawEase;
    const float rawRight = wpos.x + wsz.x - ImGui::GetStyle().ScrollbarSize - 2.0f * g_Scale;
    const float codeRight = rawRight - rawW;

    dl->PushClipRect(wpos, ImVec2(wpos.x + wsz.x, wpos.y + wsz.y), true);
    dl->AddLine(ImVec2(codeRight - 3 * g_Scale, wpos.y),
                ImVec2(codeRight - 3 * g_Scale, wpos.y + wsz.y), HexU(Col::Line, 0.7f));

    const float addrW = ImGui::CalcTextSize("0000  ").x;
    // Anything the snapshot does not cover is not code; decoding the zeros the
    // reader hands back would spell out instructions that are not there.
    const cw::s32 memLo = cw::SNAP_LO, memHiA = g.replay().memHi;
    auto inBounds = [&](int off) {
        const cw::s32 lin = cw::RMAddr(st.cs, (cw::s16)off).linear;
        return lin >= memLo && lin < memHiA;
    };

    int at = top;
    for (int row = 0; row < rows; row++)
    {
        cw::u8 b[cw::kOpBytes];
        for (int k = 0; k < cw::kOpBytes; k++) b[k] = MemAt(pl, st.cs, at + k);
        cw::Insn in = cw::disasm(b, cw::kOpBytes, g_App.asmUpperCase, at);
        const bool oob = !inBounds(at);
        if (oob)
        {
            snprintf(in.text, sizeof(in.text), "out of bounds");
            in.len = 1;
            in.invalid = true;
        }
        const bool here = ((cw::u16)at == (cw::u16)st.ip);
        const float y = wpos.y + row * rowH + 2.0f;

        if (here)
        {
            dl->AddRectFilled(ImVec2(wpos.x, y - 1), ImVec2(wpos.x + wsz.x, y + rowH - 2),
                              HexU(Col::Accent, 0.20f), 3 * g_Scale);
            // the marker is a boundary: above it has run, this row has not
            dl->AddLine(ImVec2(wpos.x, y - 1), ImVec2(wpos.x + wsz.x, y - 1),
                        HexU(Col::Accent, 0.9f), 1.4f);
        }

        char addr[8];
        snprintf(addr, sizeof(addr), "%04X", (unsigned)(cw::u16)at);
        dl->AddText(ImVec2(wpos.x + 4 * g_Scale, y),
                    HexU(here ? Col::Accent : Col::TextDim, here ? 1.0f : 0.6f), addr);

        const unsigned tc = in.invalid ? Col::Danger
                          : in.unsupported ? Col::Warn
                          : (here ? Col::Text : Col::TextDim);
        dl->PushClipRect(ImVec2(wpos.x, y), ImVec2(codeRight - 5 * g_Scale, y + rowH), true);
        dl->AddText(ImVec2(wpos.x + 4 * g_Scale + addrW, y), HexU(tc, here ? 1.0f : 0.85f), in.text);
        dl->PopClipRect();

        const int shown = ImMin(in.len, (int)(rawW / byte1));
        char raw[32];
        raw[0] = 0;
        for (int k = 0; k < shown && k < 6; k++)
            snprintf(raw + k * 3, sizeof(raw) - (size_t)(k * 3), "%02X ", b[k]);
        if (raw[0])
        {
            const ImVec2 rs = ImGui::CalcTextSize(raw);
            dl->AddText(ImVec2(rawRight - rs.x, y),
                        HexU(here ? Col::Text : Col::TextDim, here ? 0.95f : 0.6f), raw);
            if (shown < in.len)
                dl->AddText(ImVec2(rawRight - rs.x - ImGui::CalcTextSize("+").x, y),
                            HexU(Col::TextDim, 0.5f), "+");
        }

        at = (int)(cw::u16)(at + (in.len > 0 ? in.len : 1));
    }
    dl->PopClipRect();

    if (g_App.fontMono) ImGui::PopFont();
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// A pointer register worth marking inside a memory block.
struct PtrMark { int off; const char* name; unsigned col; };

// ---------------------------------------------------------------------------
// Two bytes per row, the whole block. The stack and the shared page are shown
// this way, stacked in a narrow column on the arena-facing edge.
// ---------------------------------------------------------------------------
static void ByteColumn(const char* id, cw::GuiSession& g, cw::s16 seg, int byteCount,
                       const PtrMark* marks, int markCount, float height,
                       bool startAtEnd = false)
{
    const cw::Player& pl = g.player();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Hex(Col::Bg0));
    ImGui::BeginChild(id, ImVec2(0, height), ImGuiChildFlags_Borders);
    // The stack grows downwards, so its interesting end is the bottom.
    if (startAtEnd)
    {
        static std::map<std::string, bool> seen;
        bool& done = seen[id];
        if (!done) { done = true; ImGui::SetScrollY(ImGui::GetScrollMaxY()); }
    }
    if (g_App.fontMono) ImGui::PushFont(g_App.fontMono, g_App.fontMonoSize * g_Scale);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    const float rowH = ImGui::GetTextLineHeightWithSpacing();
    ImGuiListClipper clip;
    clip.Begin((byteCount + 1) / 2, rowH);
    while (clip.Step())
    {
        for (int row = clip.DisplayStart; row < clip.DisplayEnd; row++)
        {
            const int off = row * 2;
            const PtrMark* hit = NULL;
            for (int m = 0; m < markCount; m++)
                if (marks[m].off == off || marks[m].off == off + 1) { hit = &marks[m]; break; }

            const ImVec2 p = ImGui::GetCursorScreenPos();
            if (hit)
                dl->AddRectFilled(ImVec2(p.x - 3, p.y - 1),
                                  ImVec2(p.x + ImGui::GetContentRegionAvail().x, p.y + ImGui::GetTextLineHeight() + 1),
                                  HexU(hit->col, 0.28f), 3 * g_Scale);
            ImGui::TextColored(Hex(Col::TextDim, 0.6f), "%04X", (unsigned)off);
            for (int k = 0; k < 2 && off + k < byteCount; k++)
            {
                ImGui::SameLine(0, 6 * g_Scale);
                const bool on = hit && hit->off == off + k;
                const unsigned bv = (unsigned)MemAt(pl, seg, off + k);
                ImGui::TextColored(Hex(on ? hit->col : Col::Text, on ? 1.0f : 0.9f), "%02X", bv);
                if (g_App.editMode && ImGui::IsItemHovered())
                {
                    dl->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
                                HexU(Col::Accent2, 0.8f), 2.0f);
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        RequestByteEdit(cw::RMAddr(seg, (cw::s16)(off + k)).linear, bv, id + 2);
                }
            }
            if (hit)
            {
                ImGui::SameLine(0, 5 * g_Scale);
                ImGui::TextColored(Hex(hit->col), "%s", hit->name);
            }
        }
    }
    clip.End();

    if (g_App.fontMono) ImGui::PopFont();
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

static void DrawWarriorInspector(cw::GuiSession& g, int wi, bool popped)
{
    if (!g.ready())
    {
        ImGui::TextColored(Hex(Col::TextDim), "No war recorded yet.");
        return;
    }
    // with nothing picked, follow whoever is executing at the current cycle
    if (wi < 0 || wi >= g.numWarriors()) wi = g.player().actingWarrior();
    if (wi < 0 || wi >= g.numWarriors())
    {
        ImGui::TextColored(Hex(Col::TextDim), "No survivor selected.");
        ImGui::TextColored(Hex(Col::TextDim), "Click a 1 / 2 button in GROUPS.");
        return;
    }
    const cw::CpuState& st = g.player().cpu(wi);
    const bool alive = g.warriorAlive(wi);

    // The stack and the shared page sit on the edge nearest the arena, so the
    // two memory columns and the arena read as one continuous surface.
    const bool sideRight = !g_App.inspectorLeft;
    // Wide enough for "ADDR BB BB" plus a pointer tag. Measured with the mono
    // font pushed: sizing it in the interface font made the rows clip.
    if (g_App.fontMono) ImGui::PushFont(g_App.fontMono, g_App.fontMonoSize * g_Scale);
    const float colW = ImGui::CalcTextSize("0000  00 00  SP").x + 22.0f * g_Scale;
    if (g_App.fontMono) ImGui::PopFont();
    const ImVec2 whole = ImGui::GetContentRegionAvail();
    const bool wide = whole.x > colW * 1.9f;
    const float mainW = wide ? whole.x - colW - ImGui::GetStyle().ItemSpacing.x : whole.x;

    // ---- the memory columns ------------------------------------------------
    const cw::s16 stackSeg  = g_App.followSS ? st.ss : (cw::s16)g.homeSS(wi);
    const cw::s16 sharedSeg = g_App.followES ? st.es : (cw::s16)g.homeES(wi);

    // Clicking a heading decides whether that one view chases its segment
    // register or stays on the block the survivor started with.
    auto followToggle = [](const char* label, bool& flag, const char* reg) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Hex(Col::Bg3, 0.6f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, Hex(Col::Bg4, 0.6f));
        ImGui::PushStyleColor(ImGuiCol_Text, Hex(flag ? Col::Warn : Col::TextDim));
        ImGui::SmallButton(label);
        ImGui::PopStyleColor(4);
        char id[24];
        snprintf(id, sizeof(id), "##fl%s", reg);
        if (ImGui::BeginPopupContextItem(id))
        {
            char mi[40];
            snprintf(mi, sizeof(mi), "Follow %s", reg);
            ImGui::MenuItem(mi, NULL, &flag);
            ImGui::EndPopup();
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s\nRight-click: %s %s", label,
                              flag ? "stop following" : "follow", reg);
    };

    // A pointer belongs in a block when its own segment register points there:
    // SP lives in SS, DI in ES, SI in DS.
    auto marksFor = [&](cw::s16 blockSeg, PtrMark* out) {
        int n = 0;
        if ((cw::u16)st.ss == (cw::u16)blockSeg) out[n++] = { (int)(cw::u16)st.sp, "SP", Col::Accent2 };
        if ((cw::u16)st.es == (cw::u16)blockSeg) out[n++] = { (int)(cw::u16)st.di, "DI", Col::Ok };
        if ((cw::u16)st.ds == (cw::u16)blockSeg) out[n++] = { (int)(cw::u16)st.si, "SI", Col::Warn };
        return n;
    };

    auto drawColumns = [&](float w, float h) {
        (void)w;
        // Split what is actually left after the two headings and the gaps
        // between the four items, so neither list can spill past the panel.
        const float spacing = ImGui::GetStyle().ItemSpacing.y;
        const float half = ImMax(40.0f,
                                 (h - ImGui::GetTextLineHeight() * 2.0f - spacing * 3.0f) * 0.5f);

        char lab[40];
        PtrMark sm[3]; const int smN = marksFor(stackSeg, sm);
        snprintf(lab, sizeof(lab), "STACK %04X%s", (unsigned)(cw::u16)stackSeg,
                 g_App.followSS ? " SS" : "");
        followToggle(lab, g_App.followSS, "SS");
        ByteColumn("##stack", g, stackSeg, cw::War::STACK_SIZE, sm, smN, half, true);

        PtrMark hm[3]; const int hmN = marksFor(sharedSeg, hm);
        snprintf(lab, sizeof(lab), "SHARED %04X%s", (unsigned)(cw::u16)sharedSeg,
                 g_App.followES ? " ES" : "");
        followToggle(lab, g_App.followES, "ES");
        ByteColumn("##shared", g, sharedSeg, cw::War::GROUP_SHARED_MEMORY_SIZE, hm, hmN, half);
    };

    // ---- the main column ---------------------------------------------------
    if (wide && !sideRight) { ImGui::BeginChild("##memcol", ImVec2(colW, whole.y), 0,
                                              ImGuiWindowFlags_NoScrollbar); drawColumns(colW, whole.y); ImGui::EndChild(); ImGui::SameLine(); }

    ImGui::BeginChild("##maincol", ImVec2(wide ? mainW : 0.0f, whole.y));
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(WarriorColor(wi, 1.0f)));
        ImGui::TextUnformatted(g.replay().warriorName[(size_t)wi].c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::TextColored(Hex(alive ? Col::Ok : Col::Danger), "%s", alive ? "ALIVE" : "DEAD");
        if (!popped)
        {
            ImGui::SameLine(0, 10 * g_Scale);
            if (ImGui::SmallButton("pop out"))
            {
                bool have = false;
                for (int x : g_App.poppedInspectors) if (x == wi) have = true;
                if (!have) g_App.poppedInspectors.push_back(wi);
            }
        }
        ImGui::Separator();

        const float gap = 4 * g_Scale;
        RegCell("AX", (cw::u16)st.ax, Col::Text, wi, cw::PR_AX); ImGui::SameLine(0, gap);
        RegCell("BX", (cw::u16)st.bx, Col::Text, wi, cw::PR_BX); ImGui::SameLine(0, gap);
        RegCell("CX", (cw::u16)st.cx, Col::Text, wi, cw::PR_CX); ImGui::SameLine(0, gap);
        RegCell("DX", (cw::u16)st.dx, Col::Text, wi, cw::PR_DX);
        RegCell("SI", (cw::u16)st.si, Col::Text, wi, cw::PR_SI); ImGui::SameLine(0, gap);
        RegCell("DI", (cw::u16)st.di, Col::Text, wi, cw::PR_DI); ImGui::SameLine(0, gap);
        RegCell("BP", (cw::u16)st.bp, Col::Text, wi, cw::PR_BP); ImGui::SameLine(0, gap);
        RegCell("SP", (cw::u16)st.sp, Col::Accent2, wi, cw::PR_SP);
        RegCell("DS", (cw::u16)st.ds, Col::TextDim, wi, cw::PR_DS); ImGui::SameLine(0, gap);
        RegCell("ES", (cw::u16)st.es, Col::TextDim, wi, cw::PR_ES); ImGui::SameLine(0, gap);
        RegCell("SS", (cw::u16)st.ss, Col::TextDim, wi, cw::PR_SS); ImGui::SameLine(0, gap);
        RegCell("CS", (cw::u16)st.cs, Col::TextDim, wi, cw::PR_CS);
        RegCell("IP",  (cw::u16)st.ip, Col::Accent, wi, cw::PR_IP); ImGui::SameLine(0, gap);
        RegCell("NRG", (cw::u16)st.energy, Col::Warn, wi, cw::PR_NRG);
        ImGui::SameLine(0, 8 * g_Scale);
        ImGui::TextColored(Hex(Col::TextDim), "bombs %d / %d",
                           (int)(cw::u8)st.bomb1, (int)(cw::u8)st.bomb2);

        ImGui::Dummy(ImVec2(0, 3 * g_Scale));
        ImGui::TextColored(Hex(Col::TextDim), "FLAGS  %04X", (unsigned)(cw::u16)st.flags);
        FlagBits((cw::u16)st.flags);

        ImGui::Dummy(ImVec2(0, 4 * g_Scale));
        {
            // The code view can chase CS too. Left off, it stays in the arena,
            // which is what you want when a warrior throws CS somewhere odd.
            char lab[48];
            snprintf(lab, sizeof(lab), "CODE %04X:%04X%s",
                     (unsigned)(cw::u16)(g_App.followCS ? st.cs : (cw::s16)cw::War::ARENA_SEGMENT),
                     (unsigned)(cw::u16)st.ip, g_App.followCS ? " CS" : "");
            followToggle(lab, g_App.followCS, "CS");
            if (g.running()) { ImGui::SameLine(); ImGui::TextColored(Hex(Col::TextDim), "(pinned)"); }
        }
        // whatever is left of the panel belongs to the code
        CodeView(g, st, wi, ImMax(60.0f, ImGui::GetContentRegionAvail().y));
    }
    ImGui::EndChild();

    if (wide && sideRight) { ImGui::SameLine(); ImGui::BeginChild("##memcol", ImVec2(colW, whole.y), 0,
                                              ImGuiWindowFlags_NoScrollbar); drawColumns(colW, whole.y); ImGui::EndChild(); }
}

// Popped-out inspectors: one window per survivor, closable independently.
static void DrawPoppedInspectors(cw::GuiSession& g)
{
    for (size_t i = 0; i < g_App.poppedInspectors.size(); )
    {
        const int wi = g_App.poppedInspectors[i];
        bool open = true;
        char title[96];
        snprintf(title, sizeof(title), "%s##insp%d",
                 (g.ready() && wi < g.numWarriors()) ? g.replay().warriorName[(size_t)wi].c_str()
                                                     : "survivor", wi);
        ImGui::SetNextWindowSize(ImVec2(400 * g_Scale, 780 * g_Scale), ImGuiCond_FirstUseEver);
        if (ImGui::Begin(title, &open, ImGuiWindowFlags_NoCollapse))
        {
            ImGui::PushID(wi);
            DrawWarriorInspector(g, wi, true);
            ImGui::PopID();
        }
        ImGui::End();
        if (!open) g_App.poppedInspectors.erase(g_App.poppedInspectors.begin() + (long)i);
        else       ++i;
    }
}
