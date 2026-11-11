// =============================================================================
//  Timeline
//
//  The x axis is ROUNDS (steps), never cycles: every clock cycle belonging to a
//  step shares that step's column. Survivors get one lane each, ordered top to
//  bottom by the order they take their turn, so a survivor reads as a single
//  continuous horizontal line all the way across.
//
//  Zoomed out, each lane is a bar that stops where the survivor died. Zoomed in,
//  each step's column is divided among that step's cycles inside each lane, and
//  once there is room the instruction that ran is written into the block.
//
//  Included by main.cpp (needs AppState, the palette helpers and WarriorColor).
// =============================================================================

// Cycle range [first, last) belonging to a round.
static void RoundCycleRange(const cw::Replay& R, int r, cw::u64& first, cw::u64& last)
{
    const int n = (int)R.roundStart.size();
    first = (r >= 0 && r < n) ? R.roundStart[(size_t)r] : 0;
    last  = (r + 1 < n) ? R.roundStart[(size_t)(r + 1)] : R.cycleCount();
}

static void DrawTimeline(cw::GuiSession& g, float height)
{
    AppState& S = g_App;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 o = ImGui::GetCursorScreenPos();
    const float  w = ImGui::GetContentRegionAvail().x;

    dl->AddRectFilled(o, ImVec2(o.x + w, o.y + height), HexU(Col::Bg1));
    dl->AddLine(o, ImVec2(o.x + w, o.y), HexU(Col::Line));

    const cw::Replay& R = g.replay();
    const double totalR = (double)R.roundStart.size();
    if (!g.ready() || totalR <= 0.0)
    {
        dl->AddText(ImVec2(o.x + 12 * g_Scale, o.y + (height - ImGui::GetTextLineHeight()) * 0.5f),
                    HexU(Col::TextDim, 0.7f), "timeline appears once a war has been precomputed");
        ImGui::Dummy(ImVec2(w, height));
        return;
    }

    if (S.tlHi <= S.tlLo) { S.tlLo = 0.0; S.tlHi = totalR; }

    const float padX   = 10.0f * g_Scale;
    const float lineH  = ImGui::GetTextLineHeight();
    const float labelY = o.y + height - lineH - 3.0f * g_Scale;
    const float rangeH = 11.0f * g_Scale;
    const float rangeY = labelY - rangeH - 4.0f * g_Scale;
    const float trackY = o.y + 9.0f * g_Scale;
    const float trackH = ImMax(10.0f, rangeY - trackY - 7.0f * g_Scale);
    const float x0 = o.x + padX, x1 = o.x + w - padX;
    const float tw = ImMax(1.0f, x1 - x0);

    // The track owns everything above the range bar; the range bar is its own
    // item below it, so the two never fight over the same drag.
    ImGui::SetCursorScreenPos(o);
    BtnState bar = BareButton("##timeline", ImVec2(w, rangeY - o.y));
    ImGuiIO& io = ImGui::GetIO();

    // wheel zooms about the cursor
    if (bar.hovered && io.MouseWheel != 0.0f)
    {
        const double f  = (double)ImClamp((io.MousePos.x - x0) / tw, 0.0f, 1.0f);
        const double at = S.tlLo + f * (S.tlHi - S.tlLo);
        double span = (S.tlHi - S.tlLo) * pow(0.82, (double)io.MouseWheel);
        span = ImClamp(span, 1.0, totalR);
        S.tlLo = at - f * span;
        S.tlHi = S.tlLo + span;
        if (S.tlLo < 0.0)   { S.tlLo = 0.0;   S.tlHi = span; }
        if (S.tlHi > totalR){ S.tlHi = totalR; S.tlLo = ImMax(0.0, totalR - span); }
    }

    // middle / right drag pans, left drag scrubs
    const bool panning = bar.hovered &&
        (ImGui::IsMouseDragging(ImGuiMouseButton_Middle) || ImGui::IsMouseDragging(ImGuiMouseButton_Right));
    if (panning)
    {
        const double perPx = (S.tlHi - S.tlLo) / (double)tw;
        double shift = -(double)io.MouseDelta.x * perPx;
        if (S.tlLo + shift < 0.0)      shift = -S.tlLo;
        if (S.tlHi + shift > totalR)   shift = totalR - S.tlHi;
        S.tlLo += shift; S.tlHi += shift;
    }
    else if (bar.held)
    {
        const double f = (double)ImClamp((io.MousePos.x - x0) / tw, 0.0f, 1.0f);
        const int r = (int)ImClamp(S.tlLo + f * (S.tlHi - S.tlLo), 0.0, totalR - 1.0);
        cw::u64 first, last;
        RoundCycleRange(R, r, first, last);
        g.setRunning(false);
        g.seekCycle(first);
    }
    if (bar.hovered) ImGui::SetMouseCursor(panning ? ImGuiMouseCursor_ResizeEW : ImGuiMouseCursor_Hand);

    // while playing, keep the playhead in view: once it drifts into the last
    // fifth of the window, slide the window along under it
    if (g.running() && !panning && !bar.held)
    {
        const int cr = R.roundOfCycle(g.cycle());
        const double sp = S.tlHi - S.tlLo;
        if ((double)cr > S.tlHi - sp * 0.2 || (double)cr < S.tlLo)
        {
            double lo2 = (double)cr - sp * 0.5;
            if (lo2 < 0.0) lo2 = 0.0;
            if (lo2 + sp > totalR) lo2 = ImMax(0.0, totalR - sp);
            S.tlLo = lo2;
            S.tlHi = lo2 + sp;
        }
    }

    const double lo = S.tlLo, hi = S.tlHi, span = ImMax(1e-9, hi - lo);
    auto toX = [&](double r) { return x0 + (float)((r - lo) / span) * tw; };
    const double pxPerRound = (double)tw / span;

    dl->AddRectFilled(ImVec2(x0, trackY), ImVec2(x1, trackY + trackH), HexU(Col::Bg2), 3.0f);

    const int nw = ImMax(1, R.numWarriors);

    // Lane layout. Once the lanes are too thin to carry text, hovering one
    // swells it and squeezes the rest, so a crowded war can still be read one
    // survivor at a time. The hit test uses last frame's layout so the lane
    // under the cursor does not flip as the rows move.
    static std::vector<float> s_LaneTop;      // nw + 1 entries, screen y
    static float s_Expand = 0.0f;             // 0..1, eased so it does not snap
    static int   s_ExpandLane = -1;

    const float flatH   = trackH / (float)nw;
    const float wantH   = 15.0f * g_Scale;    // enough for a mnemonic
    const float minLane = 8.0f * g_Scale;     // never squeeze a lane thinner
    const float bigLane = 34.0f * g_Scale;    // what an expanded lane aims for

    // Expanding is only worth it once the steps are wide enough to write an
    // instruction into -- zoomed out there is nothing to reveal, and swelling a
    // lane would only shove the others out of reach.
    const bool canExpand = pxPerRound >= 34.0 * g_Scale && g_App.fontMono != NULL;
    const bool crowded   = canExpand && flatH < wantH && nw > 1;

    int hoverLane = -1;
    if (bar.hovered && io.MousePos.y >= trackY && io.MousePos.y <= trackY + trackH)
    {
        if ((int)s_LaneTop.size() == nw + 1)
        {
            for (int i = 0; i < nw; i++)
                if (io.MousePos.y >= s_LaneTop[(size_t)i] && io.MousePos.y < s_LaneTop[(size_t)(i + 1)])
                { hoverLane = i; break; }
        }
        else
            hoverLane = (int)ImClamp((io.MousePos.y - trackY) / flatH, 0.0f, (float)(nw - 1));
    }

    if (crowded && hoverLane >= 0)
    {
        if (hoverLane != s_ExpandLane) { s_ExpandLane = hoverLane; s_Expand = 0.0f; }
        s_Expand = ImMin(1.0f, s_Expand + io.DeltaTime * 8.0f);
    }
    else
    {
        s_Expand = ImMax(0.0f, s_Expand - io.DeltaTime * 8.0f);
        if (s_Expand <= 0.0f) s_ExpandLane = -1;
    }

    // How much track the lanes really need. When that is more than the strip has,
    // the strip grows rather than the lanes shrinking out of reach; the request
    // is built from the target sizes, not the eased ones, so it does not wobble
    // while a lane is opening.
    {
        const float need = (s_ExpandLane >= 0) ? bigLane + (float)(nw - 1) * minLane
                                               : (float)nw * minLane;
        g_TimelineWantH = (height - trackH) + need;
    }

    std::vector<float> laneTop((size_t)nw + 1);
    std::vector<float> laneHs((size_t)nw);
    {
        const float big = (s_ExpandLane >= 0)
                        ? flatH + (ImMin(trackH * 0.45f, bigLane) - flatH) * s_Expand
                        : flatH;
        const float rest = (nw > 1) ? (trackH - big) / (float)(nw - 1) : trackH;
        float y = trackY;
        for (int i = 0; i < nw; i++)
        {
            laneTop[(size_t)i] = y;
            laneHs[(size_t)i]  = (i == s_ExpandLane) ? big : rest;
            y += laneHs[(size_t)i];
        }
        laneTop[(size_t)nw] = trackY + trackH;
    }
    s_LaneTop = laneTop;


    // round each warrior died on, so the lanes stop where the survivor does
    int deathRound[cw::War::MAX_WARRIORS];
    for (int i = 0; i < nw && i < cw::War::MAX_WARRIORS; i++) deathRound[i] = (int)totalR;
    for (const cw::DeathEvent& d : R.deaths)
        if ((int)d.warrior < nw && (int)d.warrior < cw::War::MAX_WARRIORS)
            deathRound[(int)d.warrior] = ImMin(deathRound[(int)d.warrior], R.roundOfCycle(d.cycle));

    // lane separators keep the rows readable
    for (int i = 1; i < nw; i++)
        dl->AddLine(ImVec2(x0, laneTop[(size_t)i]), ImVec2(x1, laneTop[(size_t)i]),
                    HexU(Col::Bg1, 0.55f), 1.0f);

    if (pxPerRound < 1.5)
    {
        // too dense for individual steps: one bar per survivor, ending at death
        for (int i = 0; i < nw; i++)
        {
            const float lx0 = toX(ImMax(lo, 0.0));
            const float lx1 = toX(ImMin(hi, (double)deathRound[i]));
            if (lx1 > lx0)
                dl->AddRectFilled(ImVec2(lx0, laneTop[(size_t)i] + 1.0f),
                                  ImVec2(lx1, laneTop[(size_t)(i + 1)] - 1.0f),
                                  WarriorColor(i, 0.85f));
        }
    }
    else
    {
        const int r0 = (int)ImMax(0.0, floor(lo));
        const int r1 = (int)ImMin(totalR, ceil(hi) + 1.0);
        // One block per warrior per round -- a round is the only unit the
        // debugger moves in, so nothing is sub-divided inside its column.
        const float textLaneH = (s_ExpandLane >= 0) ? laneHs[(size_t)s_ExpandLane]
                                                    : (nw > 0 ? laneHs[0] : flatH);
        const bool wantMnem = pxPerRound >= 34.0 * g_Scale && g_App.fontMono
                           && textLaneH >= 11.0f * g_Scale;
        const bool wantFull = pxPerRound >= 150.0 * g_Scale;
        const float opSize  = ImClamp((float)ImMin(pxPerRound * 0.16, (double)textLaneH * 0.72),
                                      8.0f * g_Scale, 14.0f * g_Scale);
        if (wantMnem) ImGui::PushFont(g_App.fontMono, opSize);

        for (int r = r0; r < r1; r++)
        {
            cw::u64 first, last;
            RoundCycleRange(R, r, first, last);
            if (last <= first) continue;

            int  cnt[cw::War::MAX_WARRIORS] = {0};
            cw::u64 firstOf[cw::War::MAX_WARRIORS];
            for (int i = 0; i < cw::War::MAX_WARRIORS; i++) firstOf[i] = 0;
            for (cw::u64 c = first; c < last; ++c)
            {
                const int wv = R.cycles[(size_t)c].warrior;
                if (wv < 0 || wv >= cw::War::MAX_WARRIORS) continue;
                if (cnt[wv] == 0) firstOf[wv] = c;
                cnt[wv]++;
            }

            const float bx0 = toX((double)r);
            const float bx1 = toX((double)r + 1.0);
            for (int wv = 0; wv < nw && wv < cw::War::MAX_WARRIORS; wv++)
            {
                if (cnt[wv] == 0) continue;                  // no turn this round
                const float by0 = laneTop[(size_t)wv] + 1.0f;
                const float by1 = laneTop[(size_t)(wv + 1)] - 1.0f;
                dl->AddRectFilled(ImVec2(bx0, by0), ImVec2(bx1 - (bx1 - bx0 > 3.0f ? 1.0f : 0.0f), by1),
                                  WarriorColor(wv, 0.9f));

                if (!wantMnem || (bx1 - bx0) < 26.0f * g_Scale) continue;
                if ((by1 - by0) < 10.0f * g_Scale) continue;      // this lane is squeezed

                const cw::Insn in = cw::disasm(R.opsOf(firstOf[wv]), cw::kOpBytes);
                char txt[80];
                if (cnt[wv] > 1) snprintf(txt, sizeof(txt), "%s +%d",
                                          wantFull ? in.text : in.mnem, cnt[wv] - 1);
                else             snprintf(txt, sizeof(txt), "%s", wantFull ? in.text : in.mnem);
                const ImU32 tc  = in.invalid     ? HexU(Col::Danger, 0.95f)
                                : in.unsupported ? HexU(Col::Warn,   0.95f)
                                                 : HexU(Col::Bg0,    0.92f);
                dl->PushClipRect(ImVec2(bx0 + 1, by0), ImVec2(bx1 - 1, by1), true);
                const ImVec2 ts = ImGui::CalcTextSize(txt);
                dl->AddText(ImVec2(bx0 + 3 * g_Scale, (by0 + by1 - ts.y) * 0.5f), tc, txt);
                dl->PopClipRect();
            }
        }
        if (wantMnem) ImGui::PopFont();
    }

    // Deaths, marked just after the round that carried the fatal opcode: the
    // warrior is dead once that operation has run, not before it.
    for (const cw::DeathEvent& d : R.deaths)
    {
        const double dr = (double)R.roundOfCycle(d.cycle) + 1.0;
        if (dr < lo || dr > hi) continue;
        const float dx = toX(dr);
        dl->AddLine(ImVec2(dx, trackY - 3 * g_Scale), ImVec2(dx, trackY + trackH + 3 * g_Scale),
                    HexU(Col::Danger, 0.95f), 1.6f);
        dl->AddCircleFilled(ImVec2(dx, trackY - 4 * g_Scale), 2.6f * g_Scale, HexU(Col::Danger));
    }

    // breakpoints, once steps are distinguishable
    if (pxPerRound >= 0.02 && g_App.breakOnTimeline)
        for (cw::u64 c = 0; c < R.cycleCount(); ++c)
            if (CycleIsBreak(R, c))
            {
                const double br = (double)R.roundOfCycle(c) + 1.0;
                if (br < lo || br > hi) continue;
                const float bx = toX(br);
                dl->AddTriangleFilled(ImVec2(bx - 3 * g_Scale, trackY + trackH + 7 * g_Scale),
                                      ImVec2(bx + 3 * g_Scale, trackY + trackH + 7 * g_Scale),
                                      ImVec2(bx, trackY + trackH + 1 * g_Scale),
                                      HexU(Col::Warn, 0.95f));
            }

    // Playhead. It is a boundary, not a cell: everything left of it has run,
    // everything right of it has not. Transport only ever lands on a round
    // edge, so it falls exactly between two columns.
    {
        const double pr = (g.cycle() >= R.cycleCount())
                        ? totalR
                        : (double)R.roundOfCycle(g.cycle());
        if (pr >= lo && pr <= hi)
        {
            const float px = toX(pr);
            // the round about to run is washed over, so "next" reads as pending
            const float nx = toX(ImMin(pr + 1.0, hi));
            if (nx > px)
                dl->AddRectFilled(ImVec2(px, trackY), ImVec2(nx, trackY + trackH),
                                  HexU(ContrastInk(), 0.10f));
            dl->AddLine(ImVec2(px, trackY - 5 * g_Scale),
                        ImVec2(px, trackY + trackH + 5 * g_Scale), HexU(ContrastInk(), 0.95f), 1.8f);
        }
    }

    // hover: name only the instruction under the cursor's own lane
    if (bar.hovered && !bar.held && !panning &&
        io.MousePos.y >= trackY && io.MousePos.y <= trackY + trackH)
    {
        const double f  = (double)ImClamp((io.MousePos.x - x0) / tw, 0.0f, 1.0f);
        const double at = lo + f * span;
        const int hr = (int)ImClamp(at, 0.0, totalR - 1.0);
        const int lane = (hoverLane >= 0) ? hoverLane
                       : (int)ImClamp((io.MousePos.y - trackY) / flatH, 0.0f, (float)(nw - 1));

        cw::u64 first, last;
        RoundCycleRange(R, hr, first, last);

        // A round is one turn per warrior, and a turn can be more than one
        // opcode, so the whole turn is listed rather than one opcode picked by
        // where inside the column the cursor happens to be.
        int m = 0;
        for (cw::u64 c = first; c < last; ++c) if (R.cycles[(size_t)c].warrior == lane) m++;

        ImGui::BeginTooltip();
        if (m > 0)
        {
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(WarriorColor(lane, 1.0f)),
                               "%s", R.warriorName[(size_t)lane].c_str());
            bool died = false, brk = false;
            int shown = 0;
            for (cw::u64 c = first; c < last; ++c)
            {
                if (R.cycles[(size_t)c].warrior != lane) continue;
                const cw::CycleRec& rec = R.cycles[(size_t)c];
                died |= (rec.flags & cw::CF_DEATH) != 0;
                brk  |= (rec.flags & cw::CF_BRK) != 0;
                if (shown < 8)
                {
                    const cw::Insn in = cw::disasm(R.opsOf(c), cw::kOpBytes);
                    ImGui::TextColored(Hex(in.invalid ? Col::Danger
                                         : in.unsupported ? Col::Warn : Col::Text),
                                       "%s", in.text);
                }
                shown++;
            }
            if (shown > 8) ImGui::TextColored(Hex(Col::TextDim), "... and %d more", shown - 8);
            if (died) ImGui::TextColored(Hex(Col::Danger), "dies on this round");
            if (brk)  ImGui::TextColored(Hex(Col::Warn),   "breakpoint");
            ImGui::Separator();
            ImGui::TextColored(Hex(Col::TextDim), "round %d", hr);
        }
        else if (lane < nw)
        {
            ImGui::TextColored(Hex(Col::TextDim, 0.6f), "%s  -  no turn",
                               R.warriorName[(size_t)lane].c_str());
            ImGui::Separator();
            ImGui::TextColored(Hex(Col::TextDim), "round %d", hr);
        }
        ImGui::EndTooltip();
    }

    // ---- overview / range bar ------------------------------------------
    // The whole war squeezed to the strip's width; the handle is the viewport,
    // so its left edge is where the view starts and its right edge where it
    // ends. Drag the middle to pan, drag an end to open or close the window.
    {
        static int  s_Grab = 0;          // 0 none, 1 left edge, 2 right edge, 3 body
        static double s_GrabLo = 0.0, s_GrabHi = 0.0;
        static float  s_GrabX = 0.0f;

        ImGui::SetCursorScreenPos(ImVec2(o.x, rangeY - 3 * g_Scale));
        BtnState rb = BareButton("##tlrange", ImVec2(w, rangeH + 6 * g_Scale));

        dl->AddRectFilled(ImVec2(x0, rangeY), ImVec2(x1, rangeY + rangeH), HexU(Col::Bg0), 3.0f);

        // the stretch where more than one group was still alive, as context
        if (R.contestedUntilRound > 0)
        {
            const float cx = x0 + (float)(ImMin((double)R.contestedUntilRound, totalR) / totalR) * tw;
            dl->AddRectFilled(ImVec2(x0, rangeY), ImVec2(cx, rangeY + rangeH), HexU(Col::Bg2), 3.0f);
        }
        for (const cw::DeathEvent& d : R.deaths)
        {
            const float dx = x0 + (float)((double)R.roundOfCycle(d.cycle) / totalR) * tw;
            dl->AddLine(ImVec2(dx, rangeY + 1), ImVec2(dx, rangeY + rangeH - 1), HexU(Col::Danger, 0.5f), 1.0f);
        }
        {
            const float px = x0 + (float)((double)R.roundOfCycle(g.cycle()) / totalR) * tw;
            dl->AddLine(ImVec2(px, rangeY), ImVec2(px, rangeY + rangeH), HexU(ContrastInk(), 0.8f), 1.2f);
        }

        float hx0 = x0 + (float)(lo / totalR) * tw;
        float hx1 = x0 + (float)(hi / totalR) * tw;
        if (hx1 - hx0 < 10.0f * g_Scale) hx1 = hx0 + 10.0f * g_Scale;
        if (hx1 > x1) { hx1 = x1; hx0 = ImMax(x0, x1 - 10.0f * g_Scale); }

        const float edge = 5.0f * g_Scale;
        const bool  onL  = rb.hovered && fabsf(io.MousePos.x - hx0) <= edge;
        const bool  onR  = rb.hovered && fabsf(io.MousePos.x - hx1) <= edge;

        if (rb.hovered) ImGui::SetMouseCursor((onL || onR) ? ImGuiMouseCursor_ResizeEW
                                                           : ImGuiMouseCursor_Hand);
        if (ImGui::IsItemActivated())
        {
            s_GrabLo = lo; s_GrabHi = hi; s_GrabX = io.MousePos.x;
            if (onL)      s_Grab = 1;
            else if (onR) s_Grab = 2;
            else if (io.MousePos.x > hx0 && io.MousePos.x < hx1) s_Grab = 3;
            else
            {
                // clicked the empty track: centre the window where it landed
                const double at2 = ImClamp((double)(io.MousePos.x - x0) / (double)tw, 0.0, 1.0) * totalR;
                const double sp2 = hi - lo;
                S.tlLo = ImClamp(at2 - sp2 * 0.5, 0.0, ImMax(0.0, totalR - sp2));
                S.tlHi = S.tlLo + sp2;
                s_GrabLo = S.tlLo; s_GrabHi = S.tlHi; s_Grab = 3;
            }
        }
        if (!ImGui::IsItemActive()) { if (ImGui::IsItemDeactivated()) s_Grab = 0; }
        else if (s_Grab)
        {
            const double d = (double)(io.MousePos.x - s_GrabX) / (double)tw * totalR;
            if (s_Grab == 1)
            {
                S.tlLo = ImClamp(s_GrabLo + d, 0.0, s_GrabHi - 1.0);
                S.tlHi = s_GrabHi;
            }
            else if (s_Grab == 2)
            {
                S.tlHi = ImClamp(s_GrabHi + d, s_GrabLo + 1.0, totalR);
                S.tlLo = s_GrabLo;
            }
            else
            {
                const double sp2 = s_GrabHi - s_GrabLo;
                S.tlLo = ImClamp(s_GrabLo + d, 0.0, ImMax(0.0, totalR - sp2));
                S.tlHi = S.tlLo + sp2;
            }
        }

        const ImU32 hc = HexU(Col::Accent, (rb.hovered || s_Grab) ? 0.55f : 0.35f);
        dl->AddRectFilled(ImVec2(hx0, rangeY), ImVec2(hx1, rangeY + rangeH), hc, 3.0f);
        dl->AddRect(ImVec2(hx0, rangeY), ImVec2(hx1, rangeY + rangeH), HexU(Col::Accent, 0.95f), 3.0f, 0, 1.4f);
        // grips, so the draggable ends are visible
        for (int e = 0; e < 2; e++)
        {
            const float gxp = e ? hx1 - 2.0f * g_Scale : hx0 + 0.5f * g_Scale;
            dl->AddRectFilled(ImVec2(gxp, rangeY + 2), ImVec2(gxp + 1.5f * g_Scale, rangeY + rangeH - 2),
                              HexU(Col::Accent, 1.0f));
        }
        dl->AddRect(ImVec2(x0, rangeY), ImVec2(x1, rangeY + rangeH), HexU(Col::Line, 0.9f), 3.0f);
    }

    char lbl[192];
    snprintf(lbl, sizeof(lbl), "round %d / %d    view %.0f-%.0f%s",
             g.round(), g.totalRounds(), lo, hi,
             (S.tlLo > 0.0 || S.tlHi < totalR) ? "   (double-click to fit)" : "");
    dl->AddText(ImVec2(x0, labelY), HexU(Col::TextDim, 0.85f), lbl);

    if (bar.hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) { S.tlLo = 0.0; S.tlHi = totalR; }
}
