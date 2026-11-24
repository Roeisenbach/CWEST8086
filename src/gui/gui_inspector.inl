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
static float g_RegCellW = 0.0f;   // set by the inspector: one width for the whole grid

static void RegCell(const char* name, cw::u16 v, unsigned tint = Col::Text,
                    int wi = -1, int regSlot = -1)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float h = ImGui::GetTextLineHeight() + 6 * g_Scale;
    // an edit that is staged but not yet played out shows as what it will be
    unsigned staged = 0;
    if (regSlot >= 0 && PendingRegValue(wi, regSlot, staged))
    {
        v = (cw::u16)staged;
        tint = Col::Warn;
    }
    char buf[8];
    snprintf(buf, sizeof(buf), "%04X", v);
    const ImVec2 ns = ImGui::CalcTextSize(name);
    const ImVec2 ts = ImGui::CalcTextSize(buf);
    const float w = ImMax(ImMax(g_RegCellW, 64.0f * g_Scale), ns.x + ts.x + 20.0f * g_Scale);

    const ImVec2 p = ImGui::GetCursorScreenPos();
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), HexU(Col::Bg2), 5 * g_Scale);
    dl->AddText(ImVec2(p.x + 7 * g_Scale, p.y + 3 * g_Scale), HexU(Col::TextDim, 0.85f), name);
    if (g_App.fontMono) ImGui::PushFont(g_App.fontMono, ImGui::GetFontSize());
    const ImVec2 ms = ImGui::CalcTextSize(buf);
    dl->AddText(ImVec2(p.x + w - ms.x - 7 * g_Scale, p.y + (h - ms.y) * 0.5f), HexU(tint), buf);
    if (g_App.fontMono) ImGui::PopFont();
    ImGui::Dummy(ImVec2(w, h));
    if (g_App.editMode && regSlot >= 0 && ImGui::IsItemHovered())
    {
        dl->AddRect(p, ImVec2(p.x + w, p.y + h), HexU(Col::Accent2, 0.8f), 4 * g_Scale, 0, 1.4f);
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) RequestRegEdit(wi, regSlot, name, v);
    }
    // being edited: the field takes the value's place in the cell (EditField
    // draws it once the whole grid is laid out)
    if (regSlot >= 0 && EditIsReg(wi, regSlot))
        EditPlace(ImVec2(p.x + w - ms.x - 10 * g_Scale, p.y + 1), ImVec2(p.x + w - 3 * g_Scale, p.y + h - 1));
}

// FLAGS, one lit chip per bit. With the edit tool on, double-clicking a chip
// flips that bit: the whole word is staged as one register edit, exactly as if
// it had been typed into the FLAGS cell above.
static const float kFlagChipMin = 17.0f;
static void FlagBits(cw::u16 flags, int wi)
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
    const int   nb = IM_ARRAYSIZE(bits);
    // the chips share whatever is left of the line, within reason
    const float w = ImClamp((ImGui::GetContentRegionAvail().x - (nb - 1) * 3 * g_Scale) / nb,
                            kFlagChipMin * g_Scale, 30.0f * g_Scale);
    for (int i = 0; i < nb; i++)
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
        if (!ImGui::IsItemHovered()) continue;
        if (g_App.editMode && wi >= 0)
        {
            dl->AddRect(p, ImVec2(p.x + w, p.y + h), HexU(Col::Accent2, 0.8f), 4 * g_Scale, 0, 1.4f);
            ImGui::SetTooltip("%s = %d\nDouble-click to %s it", bits[i].full, on ? 1 : 0,
                              on ? "clear" : "set");
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                StageRegEdit(wi, cw::PR_FLAGS, "FLAGS", (unsigned)(flags ^ bits[i].m));
        }
        else ImGui::SetTooltip("%s = %d", bits[i].full, on ? 1 : 0);
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
// Code view -- a window onto the whole 0000..FFFF stream, and it has no ends.
//
// The address space is a ring: scrolling down past FFFF carries straight on
// into 0000 (and up past 0000 into FFFF), with a rule drawn where the two
// meet. Scrolling glides a pixel at a time instead of jumping a row. The
// scrollbar is the ring unrolled, so when the view straddles the seam its
// thumb is split, part at the bottom of the track and part at the top.
//
// The frame (which segment the view reads through, and where it is scrolled
// to) is the flexible part. The IP row always marks the real CS:IP: when the
// view is on another segment, it is marked wherever that address falls in it.
// ---------------------------------------------------------------------------
struct CodeViewState {
    cw::u64 seenCycle = ~(cw::u64)0;
    double  pos = 0.0;            // byte address at the top edge, fractional, wraps at 64K
    double  target = 0.0;         // where pos is gliding to
    int     seenSeg = -1;
    bool    primed = false;
    float   rawEase = 0.0f;
    bool    thumbDrag = false;
};
static std::map<int, CodeViewState> g_CodeViews;

static double WrapAddr(double a)
{
    a = fmod(a, 65536.0);
    return a < 0.0 ? a + 65536.0 : a;
}
// the shortest way round the ring from `from` to `to`
static double AddrDelta(double from, double to)
{
    double d = fmod(to - from, 65536.0);
    if (d >= 32768.0) d -= 65536.0;
    if (d < -32768.0) d += 65536.0;
    return d;
}

static void CodeView(cw::GuiSession& g, const cw::CpuState& stIn, int wi, float height)
{
    const cw::Player& pl = g.player();
    // The frame: the arena segment, or CS when the view follows it.
    cw::CpuState st = stIn;
    const cw::s16 viewSeg = g_App.followCS ? stIn.cs : (cw::s16)cw::War::ARENA_SEGMENT;
    st.cs = viewSeg;
    // The IP row, by the real CS:IP, wherever that lands in the frame.
    const int ipOff = OffsetInSegment(cw::RMAddr(stIn.cs, stIn.ip).linear, viewSeg);
    const bool ipIn = ipOff >= 0;

    CodeViewState& V = g_CodeViews[wi];
    const ImGuiIO& io = ImGui::GetIO();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, Hex(Col::Bg0));
    ImGui::BeginChild("##code", ImVec2(0, height), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    if (g_App.fontMono) ImGui::PushFont(g_App.fontMono, g_App.fontMonoSize * g_Scale);
    ImDrawList* dl = ImGui::GetWindowDrawList();

    const float rowH = ImGui::GetTextLineHeightWithSpacing();
    const ImVec2 wpos = ImGui::GetWindowPos();
    const ImVec2 wsz  = ImGui::GetWindowSize();
    const int  rows   = ImMax(3, (int)(wsz.y / rowH) + 2);   // one spare at each edge for the glide
    const float sbW   = ImGui::GetStyle().ScrollbarSize;
    const bool hovered = ImGui::IsWindowHovered();

    // ---- where the frame is ----------------------------------------------
    if (!V.primed || V.seenSeg != (int)(cw::u16)viewSeg)
    {
        // first look, or the frame moved to another segment: start with IP a
        // couple of rows down
        V.primed = true;
        V.seenSeg = (int)(cw::u16)viewSeg;
        V.pos = V.target = ipIn ? WalkInsn(pl, st.cs, ipOff, -2) : 0;
    }
    else if (V.seenCycle != g.cycle() && ipIn)
    {
        // Chase IP only once it leaves the comfortable band (off the top, or
        // within three rows of the bottom); a short way glides, a long way jumps.
        const int top = SyncBoundary(pl, st.cs, (int)floor(V.target));
        int at = top, found = -1;
        const int visible = ImMax(3, (int)(wsz.y / rowH));
        for (int r = 0; r < visible; r++)
        {
            if ((cw::u16)at == (cw::u16)ipOff) { found = r; break; }
            at = NextInsn(pl, st.cs, at);
        }
        int want = -1;
        if (found < 0)                      want = WalkInsn(pl, st.cs, ipOff, -2);
        else if (found >= visible - 3)      want = WalkInsn(pl, st.cs, top, found - (visible - 3));
        if (want >= 0)
        {
            V.target = want;
            if (fabs(AddrDelta(V.pos, V.target)) > visible * 6.0) V.pos = V.target;
        }
    }
    V.seenCycle = g.cycle();

    // the wheel: three instructions a notch, then a glide there
    if (hovered && io.MouseWheel != 0.0f)
    {
        int steps = (int)lround(-io.MouseWheel * 3.0f);
        if (steps == 0) steps = io.MouseWheel > 0 ? -1 : 1;
        V.target = WalkInsn(pl, st.cs, SyncBoundary(pl, st.cs, (int)floor(V.target)), steps);
        // keep pos on the same side of the seam as target so the glide goes the short way
    }

    // ---- the scrollbar: the ring unrolled ---------------------------------
    const float trackX0 = wpos.x + wsz.x - sbW, trackY0 = wpos.y, trackH = wsz.y;
    // how much of the ring is on screen, to size the thumb
    double visibleBytes = 0.0;
    {
        int at = SyncBoundary(pl, st.cs, (int)floor(V.pos));
        for (int r = 0; r < (int)(wsz.y / rowH); r++)
        {
            const int nx = NextInsn(pl, st.cs, at);
            visibleBytes += (double)(cw::u16)(nx - at);
            at = nx;
        }
    }
    const float thumbH = ImClamp((float)(trackH * visibleBytes / 65536.0), 18.0f * g_Scale, trackH);
    const float thumbY = (float)(V.pos / 65536.0) * trackH;
    {
        const ImVec2 tmn(trackX0, trackY0), tmx(trackX0 + sbW, trackY0 + trackH);
        const bool overTrack = hovered && ImGui::IsMouseHoveringRect(tmn, tmx);
        const float my = io.MousePos.y - trackY0;
        // on the thumb, counting the half that wrapped round to the top
        const bool onThumb = overTrack &&
            ((my >= thumbY && my < thumbY + thumbH) || (thumbY + thumbH > trackH && my < thumbY + thumbH - trackH));
        if (overTrack && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            if (onThumb) V.thumbDrag = true;
            else
            {
                // a click on the track centres the thumb there
                V.pos = V.target = WrapAddr((double)(my - thumbH * 0.5f) / trackH * 65536.0);
            }
        }
        if (V.thumbDrag)
        {
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) V.thumbDrag = false;
            else if (io.MouseDelta.y != 0.0f)
                V.pos = V.target = WrapAddr(V.pos + (double)io.MouseDelta.y / trackH * 65536.0);
        }
    }

    // ---- the glide --------------------------------------------------------
    {
        const double d = AddrDelta(V.pos, V.target);
        const double k = 1.0 - exp(-io.DeltaTime * 16.0);
        V.pos = (fabs(d) < 0.02) ? V.target : WrapAddr(V.pos + d * k);
    }

    // ---- the rows ---------------------------------------------------------
    // The top edge sits `into` bytes into instruction b0, so the first row is
    // drawn that fraction of a row above the edge: scrolling moves pixels,
    // not whole rows.
    const int base = (int)floor(V.pos) & 0xFFFF;
    const int s = SyncBoundary(pl, st.cs, base);
    const int b0 = (s == base) ? s : PrevInsn(pl, st.cs, s);
    const int len0 = ImMax(1, (int)(cw::u16)(NextInsn(pl, st.cs, b0) - b0));
    const double into = ImClamp(AddrDelta(b0, V.pos), 0.0, (double)len0);
    const float y0 = wpos.y - (float)(into / len0) * rowH + 2.0f;

    // The raw bytes ride on the right. Three wide is enough to read the opcode
    // without eating the disassembly; hovering opens it to a full instruction.
    const float byte1 = ImGui::CalcTextSize("00 ").x;
    const float rawFull = byte1 * 6.0f + 6.0f * g_Scale;
    const float rawNarrow = byte1 * 3.0f + 6.0f * g_Scale;
    const ImVec2 mouse = io.MousePos;
    const bool overRaw = hovered && mouse.x >= wpos.x + wsz.x - rawFull - sbW && mouse.x < wpos.x + wsz.x - sbW;
    float& rawEase = V.rawEase;
    rawEase = ImClamp(rawEase + (overRaw ? 1.0f : -1.0f) * io.DeltaTime * 7.0f, 0.0f, 1.0f);
    const float rawW = rawNarrow + (rawFull - rawNarrow) * rawEase;
    const float rawRight = wpos.x + wsz.x - sbW - 2.0f * g_Scale;
    const float codeRight = rawRight - rawW;

    dl->PushClipRect(wpos, ImVec2(wpos.x + wsz.x - sbW, wpos.y + wsz.y), true);
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

    // the picked pointer pairs that land in this frame (IP has its own row)
    const std::vector<PtrHit> hitsAll = PointerHits(stIn, false);
    std::vector<int> hitOff;
    std::vector<const char*> hitName;
    for (const PtrHit& h : hitsAll)
    {
        const int o = OffsetInSegment(h.lin, viewSeg);
        if (o >= 0) { hitOff.push_back(o); hitName.push_back(h.name); }
    }

    int at = b0;
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
        const int len = in.len > 0 ? in.len : 1;
        const bool here = ipIn && ((cw::u16)at == (cw::u16)ipOff);
        const float y = y0 + row * rowH;

        if (here)
            dl->AddRectFilled(ImVec2(wpos.x, y - 1), ImVec2(wpos.x + wsz.x - sbW, y + rowH - 2),
                              HexU(Col::Accent, 0.20f), 3 * g_Scale);

        char addr[8];
        snprintf(addr, sizeof(addr), "%04X", (unsigned)(cw::u16)at);
        dl->AddText(ImVec2(wpos.x + 4 * g_Scale, y),
                    HexU(here ? Col::Accent : Col::TextDim, here ? 1.0f : 0.6f), addr);

        const unsigned tc = in.invalid ? Col::Danger
                          : in.unsupported ? Col::Warn
                          : (here ? Col::Text : Col::TextDim);
        const float textX = wpos.x + 4 * g_Scale + addrW;
        dl->PushClipRect(ImVec2(wpos.x, y), ImVec2(codeRight - 5 * g_Scale, y + rowH), true);
        dl->AddText(ImVec2(textX, y), HexU(tc, here ? 1.0f : 0.85f), in.text);
        dl->PopClipRect();

        const int shown = ImMin(len, (int)(rawW / byte1));
        char raw[32];
        raw[0] = 0;
        for (int k = 0; k < shown && k < 6; k++)
            snprintf(raw + k * 3, sizeof(raw) - (size_t)(k * 3), "%02X ", b[k]);
        float rawX = rawRight;
        if (raw[0])
        {
            const ImVec2 rs = ImGui::CalcTextSize(raw);
            rawX = rawRight - rs.x;
            dl->AddText(ImVec2(rawX, y), HexU(here ? Col::Text : Col::TextDim, here ? 0.95f : 0.6f), raw);
            if (shown < len)
                dl->AddText(ImVec2(rawX - ImGui::CalcTextSize("+").x, y), HexU(Col::TextDim, 0.5f), "+");
        }

        // pointers into this instruction: a ring on the byte (or the line, when
        // that byte is not on show), the name over the address
        int tagK = 0;
        for (size_t i = 0; i < hitOff.size(); i++)
        {
            const int rel = (int)(cw::u16)(hitOff[i] - at);
            if (rel >= len) continue;
            if (rel < shown)
            {
                const float bx = rawX + rel * byte1;
                PointerRing(dl, ImVec2(bx - 2 * g_Scale, y - 1), ImVec2(bx + byte1 - 3 * g_Scale, y + rowH - 3),
                            3 * g_Scale, 1.6f * g_Scale);
            }
            else
                PointerRing(dl, ImVec2(textX - 3 * g_Scale, y - 1), ImVec2(codeRight - 6 * g_Scale, y + rowH - 3),
                            3 * g_Scale, 1.4f * g_Scale);
            PointerListTag("code", hitName[i], ImVec2(wpos.x + 4 * g_Scale, y), rowH - 2, textX, tagK++);
        }

        const int next = (int)(cw::u16)(at + len);
        // the seam of the ring: FFFF ends above this rule, 0000 starts below it
        if (at + len > 0xFFFF)
        {
            const float ly = y + rowH - 2.5f;
            dl->AddLine(ImVec2(wpos.x, ly), ImVec2(wpos.x + wsz.x - sbW, ly), HexU(Col::Warn, 0.75f), 1.5f * g_Scale);
            ImGui::PushFont(NULL, ImGui::GetFontSize() * 0.75f);
            const char* seam = "FFFF | 0000";
            const ImVec2 ss = ImGui::CalcTextSize(seam);
            const ImVec2 sp(codeRight - 10 * g_Scale - ss.x, ly - ss.y * 0.5f);
            dl->AddRectFilled(ImVec2(sp.x - 4 * g_Scale, sp.y), ImVec2(sp.x + ss.x + 4 * g_Scale, sp.y + ss.y),
                              HexU(Col::Bg0), 3 * g_Scale);
            dl->AddText(sp, HexU(Col::Warn), seam);
            ImGui::PopFont();
        }
        at = next;
    }
    dl->PopClipRect();

    // ---- the scrollbar, drawn last so it sits over the rows ----------------
    {
        const ImVec2 tmn(trackX0, trackY0), tmx(trackX0 + sbW, trackY0 + trackH);
        dl->AddRectFilled(tmn, tmx, HexU(Col::Bg1));
        const bool hot = V.thumbDrag || (hovered && ImGui::IsMouseHoveringRect(tmn, tmx));
        const ImU32 tc = HexU(V.thumbDrag ? Col::Accent : Col::TextDim, hot ? 0.75f : 0.45f);
        const float pad = 2.0f * g_Scale, r = (sbW - pad * 2) * 0.5f;
        const float x0 = trackX0 + pad, x1 = trackX0 + sbW - pad;
        if (thumbY + thumbH <= trackH)
            dl->AddRectFilled(ImVec2(x0, trackY0 + thumbY), ImVec2(x1, trackY0 + thumbY + thumbH), tc, r);
        else
        {
            // across the seam: the bottom piece runs off the end of the track and
            // carries on at the top, both cut square where they meet the ends
            dl->AddRectFilled(ImVec2(x0, trackY0 + thumbY), ImVec2(x1, trackY0 + trackH), tc, r,
                              ImDrawFlags_RoundCornersTop);
            dl->AddRectFilled(ImVec2(x0, trackY0), ImVec2(x1, trackY0 + thumbY + thumbH - trackH), tc, r,
                              ImDrawFlags_RoundCornersBottom);
        }
    }

    // keep the child's content size honest without a real scroll range
    ImGui::Dummy(ImVec2(1.0f, 1.0f));

    if (g_App.fontMono) ImGui::PopFont();
    ImGui::EndChild();
    ImGui::PopStyleColor();
}


// ---------------------------------------------------------------------------
// Two bytes per row, the whole block. The stack and the shared page are shown
// this way, stacked in a narrow column on the arena-facing edge.
// ---------------------------------------------------------------------------
// The side padding inside the stack / shared lists. The column is sized from
// it, so the two have to agree.
static float MemColPad() { return 6.0f * g_Scale; }

static void ByteColumn(const char* id, cw::GuiSession& g, cw::s16 seg, int byteCount,
                       const std::vector<PtrHit>& hits, float height,
                       bool startAtEnd = false)
{
    // where each pointer lands in this block (-1: not here)
    std::vector<int> at(hits.size(), -1);
    for (size_t i = 0; i < hits.size(); i++)
    {
        const int o = OffsetInSegment(hits[i].lin, seg);
        at[i] = (o >= 0 && o < byteCount) ? o : -1;
    }
    const cw::Player& pl = g.player();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, Hex(Col::Bg0));
    // tight padding: the column is sized to its rows (see MemColPad)
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(MemColPad(), ImGui::GetStyle().WindowPadding.y));
    ImGui::BeginChild(id, ImVec2(0, height), ImGuiChildFlags_Borders);
    ImGui::PopStyleVar();
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
            const ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::TextColored(Hex(Col::TextDim, 0.6f), "%04X", (unsigned)off);
            const ImVec2 idxMn = ImGui::GetItemRectMin();
            float contentX = 0.0f;
            for (int k = 0; k < 2 && off + k < byteCount; k++)
            {
                ImGui::SameLine(0, 6 * g_Scale);
                const unsigned bv = (unsigned)MemAt(pl, seg, off + k);
                ImGui::TextColored(Hex(Col::Text, 0.9f), "%02X", bv);
                const ImVec2 bmn = ImGui::GetItemRectMin(), bmx = ImGui::GetItemRectMax();
                if (k == 0) contentX = bmn.x;
                // a ring round every byte a pointer lands on, as in the arena
                for (size_t i = 0; i < hits.size(); i++)
                    if (at[i] == off + k)
                    {
                        PointerRing(dl, ImVec2(bmn.x - 3 * g_Scale, bmn.y - 1), ImVec2(bmx.x + 3 * g_Scale, bmx.y + 1),
                                    3 * g_Scale, 1.6f * g_Scale);
                        break;
                    }
                const cw::s32 lin = cw::RMAddr(seg, (cw::s16)(off + k)).linear;
                if (g_App.editMode && ImGui::IsItemHovered())
                {
                    dl->AddRect(bmn, bmx, HexU(Col::Accent2, 0.8f), 2.0f);
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        RequestByteEdit(lin, bv, id + 2);
                }
                if (EditIsByte(lin)) EditPlace(bmn, bmx);
            }
            // the names go over the row's index (a click moves one over the bytes)
            int k = 0;
            for (size_t i = 0; i < hits.size(); i++)
                if (at[i] == off || at[i] == off + 1)
                    PointerListTag(id, hits[i].name, idxMn, rowH, contentX, k++);
            (void)p;
        }
    }
    clip.End();
    EditField();       // a byte being edited, if it is one of these

    if (g_App.fontMono) ImGui::PopFont();
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ---------------------------------------------------------------------------
// Constants -- a survivor's EQUs, tunable while debugging
//
// Every `NAME equ value` in the survivor's source whose name does not start
// with "_" (the underscore marks one as private) is listed in the panel. A new
// value is written back into the source line, the survivor re-assembled, and
// the war played out again from the same seed, landing on the same round.
// ---------------------------------------------------------------------------
struct EquEntry
{
    std::string name;         // as written
    int         line = 0;     // 0-based source line
    size_t      valFrom = 0;  // the value's byte range in the whole source
    size_t      valTo = 0;
    int         colFrom = 0;  // the same range as editor columns (tabs expanded)
    int         colTo = 0;
    std::string value;
};

static std::vector<EquEntry> FindTunableEqus(const std::string& code)
{
    std::vector<EquEntry> out;
    size_t pos = 0;
    int lineNo = 0;
    auto col = [](const std::string& L, size_t upto) {
        int c = 0;
        for (size_t k = 0; k < upto && k < L.size(); k++)
            c = (L[k] == '\t') ? (c / 4) * 4 + 4 : c + 1;
        return c;
    };
    while (pos <= code.size())
    {
        const size_t nl = code.find('\n', pos);
        const size_t end = (nl == std::string::npos) ? code.size() : nl;
        const std::string L = code.substr(pos, end - pos);
        size_t i = 0;
        while (i < L.size() && (L[i] == ' ' || L[i] == '\t')) i++;
        const size_t n0 = i;
        while (i < L.size() && (isalnum((unsigned char)L[i]) || L[i] == '_' || L[i] == '.')) i++;
        const size_t n1 = i;
        while (i < L.size() && (L[i] == ' ' || L[i] == '\t')) i++;
        if (i < L.size() && L[i] == ':') { i++; while (i < L.size() && (L[i] == ' ' || L[i] == '\t')) i++; }
        const bool isEqu = n1 > n0 && i + 3 <= L.size() && toupper((unsigned char)L[i]) == 'E' &&
                           toupper((unsigned char)L[i + 1]) == 'Q' && toupper((unsigned char)L[i + 2]) == 'U' &&
                           (i + 3 == L.size() || L[i + 3] == ' ' || L[i + 3] == '\t');
        if (isEqu && L[n0] != '_' && !isdigit((unsigned char)L[n0]))
        {
            size_t v0 = i + 3;
            while (v0 < L.size() && (L[v0] == ' ' || L[v0] == '\t')) v0++;
            size_t v1 = L.find(';', v0);
            if (v1 == std::string::npos) v1 = L.size();
            while (v1 > v0 && (L[v1 - 1] == ' ' || L[v1 - 1] == '\t')) v1--;
            EquEntry e;
            e.name = L.substr(n0, n1 - n0);
            e.line = lineNo;
            e.valFrom = pos + v0;
            e.valTo = pos + v1;
            e.colFrom = col(L, v0);
            e.colTo = col(L, v1);
            e.value = L.substr(v0, v1 - v0);
            out.push_back(e);
        }
        if (nl == std::string::npos) break;
        pos = nl + 1;
        lineNo++;
    }
    return out;
}

// The workspace survivor a warrior of the recorded war was built from.
static Ref RefOfWarrior(cw::GuiSession& g, int wi)
{
    AppState& S = g_App;
    if (wi >= 0 && wi < g.numWarriors() && !g.warriorZombie(wi))
    {
        const int p = PlayerOfKey(g.groupKey(g.warriorGroup(wi)));
        const int s = g.warriorSlot(wi);
        if (p >= 0 && s >= 0 && s < kSlotCount && S.players[(size_t)p].slots[s].present)
        { Ref r; r.player = p; r.slot = s; return r; }
    }
    for (int z = 0; z < (int)S.zombies.size(); z++)
        if (wi < g.numWarriors() && g.replay().warriorName[(size_t)wi] == S.zombies[z].name)
        { Ref r; r.player = kZombiePlayer; r.slot = z; return r; }
    return Ref();
}

static void SetEquValue(const Ref& ref, const EquEntry& e, const std::string& value)
{
    AppState& S = g_App;
    Survivor* sv = SurvivorAt(ref);
    if (!sv || value == e.value) return;
    if (ref == S.open)
    {
        // through the editor, so it is an ordinary edit there: Ctrl+Z takes it back
        FlushEditorToModel();
        S.editor.SetSelection(TextEditor::Coordinates(e.line, e.colFrom), TextEditor::Coordinates(e.line, e.colTo));
        if (e.colTo > e.colFrom) S.editor.Delete();
        S.editor.SetCursorPosition(TextEditor::Coordinates(e.line, e.colFrom));
        S.editor.InsertText(value);
        FlushEditorToModel();
        PushEditorMarkers(*sv);
    }
    else
    {
        sv->code.replace(e.valFrom, e.valTo - e.valFrom, value);
        sv->dirty = true;
        ReassembleSurvivor(*sv);
        NoteEdit();
    }
    if (!sv->asmOk)
    {
        std::string why;
        for (const cw::AsmDiag& d : sv->asmDiags) if (d.error) { why = d.text; break; }
        S.status = e.name + " = " + value + " does not assemble: " + why;
        return;
    }
    // the same war again, with the new constant, landing where it was
    S.game.seekRoundWhenReady(S.game.round());
    GameArm(/*keepSeed=*/true);
    S.status = e.name + " = " + value + "  -  replaying the war with it";
}

static void DrawConstants(cw::GuiSession& g, int wi)
{
    const Ref ref = RefOfWarrior(g, wi);
    Survivor* sv = SurvivorAt(ref);
    if (!sv || !sv->present) return;
    const std::vector<EquEntry> equs = FindTunableEqus(sv->code);
    if (equs.empty()) return;

    // what each one came to, by upper-case name
    auto resolved = [&](const std::string& n, long long& v) {
        std::string u = n;
        for (char& c : u) c = (char)toupper((unsigned char)c);
        for (const cw::AsmSymbol& s : sv->asmEqus)
            if (s.name == u && s.resolved) { v = s.value; return true; }
        return false;
    };

    ImGui::Dummy(ImVec2(0, 3 * g_Scale));
    ImGui::PushFont(NULL, ImGui::GetFontSize() * 0.82f);
    ImGui::TextColored(Hex(Col::TextDim, 0.85f), "CONSTANTS");
    ImGui::SameLine(0, 8 * g_Scale);
    ImGui::TextColored(Hex(Col::TextDim, 0.6f), "Enter replays the war with the new value");
    ImGui::PopFont();

    float nameW = 0.0f;
    for (const EquEntry& e : equs) nameW = ImMax(nameW, ImGui::CalcTextSize(e.name.c_str()).x);
    nameW += 14 * g_Scale;
    const float rowH = ImGui::GetFrameHeightWithSpacing();
    const int   shown = ImMin((int)equs.size(), 6);
    ImGui::BeginChild("##consts", ImVec2(0, rowH * shown + 2 * g_Scale), 0,
                      equs.size() > 6 ? 0 : ImGuiWindowFlags_NoScrollbar);
    static std::map<std::string, std::array<char, 96>> bufs;   // what is being typed, per constant
    for (const EquEntry& e : equs)
    {
        ImGui::PushID(e.name.c_str());
        const std::string key = sv->name + "|" + e.name;
        std::array<char, 96>& buf = bufs[key];
        const ImGuiID fid = ImGui::GetID("##v");
        if (ImGui::GetActiveID() != fid)
            snprintf(buf.data(), buf.size(), "%s", e.value.c_str());

        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(Hex(Col::Text), "%s", e.name.c_str());
        ImGui::SameLine(nameW);

        long long v = 0;
        const bool have = resolved(e.name, v);
        char rv[48] = "";
        long long lit = 0;
        // the worked-out value, unless the text already is that number
        if (have && !(sscanf(e.value.c_str(), "%lld", &lit) == 1 && lit == v &&
                      e.value.find_first_not_of("0123456789-") == std::string::npos))
            snprintf(rv, sizeof(rv), "= %lld  %04Xh", v, (unsigned)(v & 0xFFFF));
        const float rvW = rv[0] ? ImGui::CalcTextSize(rv).x + 10 * g_Scale : 0.0f;

        ImGui::SetNextItemWidth(ImMax(60.0f * g_Scale, ImGui::GetContentRegionAvail().x - rvW));
        const bool enter = ImGui::InputText("##v", buf.data(), buf.size(),
                                            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
        const bool commit = enter || ImGui::IsItemDeactivatedAfterEdit();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s equ %s   (line %d)\nA number or an expression of labels and other EQUs.\n"
                              "Enter writes it into the source and plays the war again\nfrom the same seed, "
                              "back at this round.", e.name.c_str(), e.value.c_str(), e.line + 1);
        if (rv[0])
        {
            ImGui::SameLine(0, 10 * g_Scale);
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(Hex(Col::TextDim), "%s", rv);
        }
        if (commit)
        {
            std::string nv = buf.data();
            while (!nv.empty() && isspace((unsigned char)nv.back())) nv.pop_back();
            while (!nv.empty() && isspace((unsigned char)nv.front())) nv.erase(nv.begin());
            if (!nv.empty() && nv != e.value) { SetEquValue(ref, e, nv); ImGui::PopID(); break; }
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
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
    // Exactly wide enough for a row -- "ADDR BB BB" and the rings round the
    // bytes -- plus the list's own (tight) padding, border and scrollbar. The
    // rows decide it, not the headings: a heading in the interface font was
    // wider than the rows and left a strip of nothing beside every byte; now a
    // heading that would not fit is drawn smaller instead. (Pointer names sit
    // over the address, so no room is kept beside the bytes for them either.)
    // Measured with the mono font pushed: sizing it in the interface font made
    // the rows clip.
    const ImGuiStyle& style = ImGui::GetStyle();
    if (g_App.fontMono) ImGui::PushFont(g_App.fontMono, g_App.fontMonoSize * g_Scale);
    const float rowW = ImGui::CalcTextSize("0000").x + ImGui::CalcTextSize("00").x * 2.0f + 12.0f * g_Scale +
                       3.0f * g_Scale;                                   // the last ring's overhang
    if (g_App.fontMono) ImGui::PopFont();
    const float colW = rowW + MemColPad() * 2.0f + style.ScrollbarSize + 2.0f;
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
        // the column is as wide as its bytes; a heading wider than that shrinks
        const float room = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().FramePadding.x * 2.0f;
        const float need = ImGui::CalcTextSize(label).x;
        const bool shrink = need > room && room > 1.0f;
        if (shrink) ImGui::PushFont(NULL, ImMax(ImGui::GetFontSize() * room / need, ImGui::GetFontSize() * 0.6f));
        ImGui::SmallButton(label);
        if (shrink) ImGui::PopFont();
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

    // The pointer pairs picked in Options (the same ones the arena rings), each
    // shown wherever its address falls -- the stack, the shared page or both.
    const std::vector<PtrHit> hits = PointerHits(st, true);

    auto drawColumns = [&](float w, float h) {
        (void)w;
        // Split what is actually left after the two headings and the gaps
        // between the four items, so neither list can spill past the panel.
        const float spacing = ImGui::GetStyle().ItemSpacing.y;
        const float half = ImMax(40.0f,
                                 (h - ImGui::GetTextLineHeight() * 2.0f - spacing * 3.0f) * 0.5f);

        char lab[40];
        snprintf(lab, sizeof(lab), "STACK %04X%s", (unsigned)(cw::u16)stackSeg,
                 g_App.followSS ? " SS" : "");
        followToggle(lab, g_App.followSS, "SS");
        ByteColumn("##stack", g, stackSeg, cw::War::STACK_SIZE, hits, half, true);

        snprintf(lab, sizeof(lab), "SHARED %04X%s", (unsigned)(cw::u16)sharedSeg,
                 g_App.followES ? " ES" : "");
        followToggle(lab, g_App.followES, "ES");
        ByteColumn("##shared", g, sharedSeg, cw::War::GROUP_SHARED_MEMORY_SIZE, hits, half);
    };

    // ---- the main column ---------------------------------------------------
    if (wide && !sideRight) { ImGui::BeginChild("##memcol", ImVec2(colW, whole.y), 0,
                                              ImGuiWindowFlags_NoScrollbar); drawColumns(colW, whole.y); ImGui::EndChild(); ImGui::SameLine(); }

    ImGui::BeginChild("##maincol", ImVec2(wide ? mainW : 0.0f, whole.y));
    {
        // ---- header card: who, whether they are standing, and two actions
        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const float w = ImGui::GetContentRegionAvail().x;
            const float h = (kBarH + 10.0f) * g_Scale;
            const ImVec2 p = ImGui::GetCursorScreenPos();
            const ImU32 tint = WarriorColor(wi, 1.0f);
            dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), HexU(Col::Bg2), 8 * g_Scale);
            // the survivor's colour as a bar down the card's left edge
            dl->AddRectFilled(p, ImVec2(p.x + 4 * g_Scale, p.y + h), tint, 8 * g_Scale,
                              ImDrawFlags_RoundCornersLeft);

            // actions on the right, as one train
            const float bs = kBarH * g_Scale;
            const int nBtn = popped ? 2 : 3;
            float bx = p.x + w - 5 * g_Scale - bs * nBtn;
            const float by = p.y + (h - bs) * 0.5f;
            ImGui::SetCursorScreenPos(ImVec2(bx, by));
            if (ToolButton("##findip", 11, "Find IP - zoom the arena onto this survivor's next instruction, once",
                           false, true, TrainCorners(0, nBtn), 0))
                FocusArenaOnIp(wi);
            ImGui::SameLine(0, 0);
            {
                const bool following = (g_FollowIp == wi);
                if (ToolButton("##followip", 13,
                               following ? "Following IP - the arena keeps this survivor's next instruction in the "
                                           "middle as it plays\nClick, or drag the arena, to stop"
                                         : "Follow IP - keep the arena centred on this survivor's next instruction "
                                           "while it plays",
                               following, true, TrainCorners(1, nBtn), 1))
                {
                    if (following) { g_FollowIp = -1; g_App.status = "Stopped following IP"; }
                    else           { g_FollowIp = wi; FocusArenaOnIp(wi); g_App.status = "Following " + g.replay().warriorName[(size_t)wi] + "'s IP"; }
                }
            }
            if (!popped)
            {
                ImGui::SameLine(0, 0);
                if (ToolButton("##popout", 12, "Pop out - this survivor in a window of its own",
                               false, true, TrainCorners(2, nBtn), 2))
                {
                    bool have = false;
                    for (int x : g_App.poppedInspectors) if (x == wi) have = true;
                    if (!have) g_App.poppedInspectors.push_back(wi);
                }
            }

            // status pill, then the name in whatever room is left
            const char* stTxt = alive ? "ALIVE" : "DEAD";
            ImGui::PushFont(NULL, ImGui::GetFontSize() * 0.82f);
            const ImVec2 ss = ImGui::CalcTextSize(stTxt);
            const float pillW = ss.x + 14 * g_Scale, pillH = ss.y + 6 * g_Scale;
            const ImVec2 pmn(bx - 8 * g_Scale - pillW, p.y + (h - pillH) * 0.5f);
            const unsigned sc = alive ? Col::Ok : Col::Danger;
            dl->AddRectFilled(pmn, ImVec2(pmn.x + pillW, pmn.y + pillH), HexU(sc, 0.16f), pillH * 0.5f);
            dl->AddText(ImVec2(pmn.x + 7 * g_Scale, pmn.y + 3 * g_Scale), HexU(sc), stTxt);
            ImGui::PopFont();

            const char* nm = g.replay().warriorName[(size_t)wi].c_str();
            const ImVec2 ns = ImGui::CalcTextSize(nm);
            dl->PushClipRect(p, ImVec2(pmn.x - 6 * g_Scale, p.y + h), true);
            dl->AddText(ImVec2(p.x + 14 * g_Scale, p.y + (h - ns.y) * 0.5f), HexU(Col::Text), nm);
            dl->PopClipRect();

            ImGui::SetCursorScreenPos(p);
            ImGui::Dummy(ImVec2(w, h));
        }

        // a small caption over each block of the panel
        auto caption = [](const char* t, const char* right = NULL, unsigned rightCol = Col::TextDim) {
            ImGui::Dummy(ImVec2(0, 3 * g_Scale));
            ImGui::PushFont(NULL, ImGui::GetFontSize() * 0.82f);
            ImGui::TextColored(Hex(Col::TextDim, 0.85f), "%s", t);
            if (right)
            {
                ImGui::SameLine(0, 8 * g_Scale);
                ImGui::TextColored(Hex(rightCol), "%s", right);
            }
            ImGui::PopFont();
        };

        // The registers on an even grid: four to a row, each cell the same
        // width, so the columns line up however wide the panel is.
        const float gap = 4 * g_Scale;
        g_RegCellW = (ImGui::GetContentRegionAvail().x - gap * 3.0f) / 4.0f;
        caption("REGISTERS");
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
        RegCell("NRG", (cw::u16)st.energy, Col::Warn, wi, cw::PR_NRG); ImGui::SameLine(0, gap);
        {
            // the two bombs, as one more cell on the grid (twice as wide)
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const float h = ImGui::GetTextLineHeight() + 6 * g_Scale;
            const float w = ImMax(ImGui::GetContentRegionAvail().x, 64.0f * g_Scale);
            const ImVec2 p = ImGui::GetCursorScreenPos();
            dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), HexU(Col::Bg2, 0.6f), 5 * g_Scale);
            dl->AddText(ImVec2(p.x + 7 * g_Scale, p.y + 3 * g_Scale), HexU(Col::TextDim, 0.85f), "BOMBS");
            char bb[24];
            snprintf(bb, sizeof(bb), "%d / %d", (int)(cw::u8)st.bomb1, (int)(cw::u8)st.bomb2);
            const ImVec2 bs = ImGui::CalcTextSize(bb);
            dl->AddText(ImVec2(p.x + w - bs.x - 7 * g_Scale, p.y + 3 * g_Scale), HexU(Col::Text), bb);
            ImGui::Dummy(ImVec2(w, h));
        }

        // FLAGS is a register like the rest: the cell takes a whole new word,
        // the chips beside it flip one bit each. Both show a staged edit as it
        // will be, so several flags can be set before the war is recomputed.
        unsigned flagsNow = (cw::u16)st.flags;
        const bool flagsStaged = PendingRegValue(wi, cw::PR_FLAGS, flagsNow);
        caption("FLAGS", flagsStaged ? "staged" : (g_App.editMode ? "double-click a flag to flip it" : NULL),
                flagsStaged ? Col::Warn : Col::TextDim);
        const float lineW = ImGui::GetContentRegionAvail().x;
        RegCell("FLAGS", (cw::u16)st.flags, Col::Text, wi, cw::PR_FLAGS);
        // the chips ride beside the cell when they fit there, under it if not
        if (lineW - g_RegCellW - gap >= (9 * kFlagChipMin + 8 * 3) * g_Scale) ImGui::SameLine(0, gap);
        FlagBits((cw::u16)flagsNow, wi);
        g_RegCellW = 0.0f;

        DrawConstants(g, wi);

        ImGui::Dummy(ImVec2(0, 3 * g_Scale));
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
    EditField();       // a register being edited, over its cell
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
