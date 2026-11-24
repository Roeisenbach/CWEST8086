// =============================================================================
//  Editor plumbing for the assembler
//
//  A binary survivor is opened as generated source (see src/engine/listing.h), not a
//  hex dump: labels, `db ... ;$N` for ambiguous encodings, forced size keywords.
//
//  The buffer is re-assembled on every keystroke, which drives
//    * the assembled bytes shown in a gutter to the LEFT of the code, each
//      instruction on its own source line;
//    * hovering a byte, which disassembles from exactly that byte -- and when
//      the byte sits inside an instruction, shows the leading bytes as `db` the
//      way a real re-read would;
//    * yellow markers on lines the engine would refuse.
//  Alt on a line cycles that instruction through its alternative encodings.
//
//  Included by main.cpp after SurvivorAt.
// =============================================================================

// Rebuild `bin` and the diagnostics from whatever is currently in `code`.
static void ReassembleSurvivor(Survivor& sv)
{
    cw::AsmResult r = cw::assemble(sv.code);
    sv.bin.assign(r.bytes.begin(), r.bytes.end());
    sv.asmLines = r.lines;
    sv.asmDiags = r.diags;
    sv.asmEqus = r.equs;
    sv.asmOk = r.ok;
    sv.smartSites = r.smartSites;
}

// Set by the byte gutter when a double-click asks for the listing to be re-cut
// from that byte; consumed by the editor panel once the gutter is done reading
// the survivor it was handed.
static int g_ReframeAt = -1;

// One entry per re-read, so Ctrl+Z / Ctrl+Y can walk back out of them.
static std::vector<std::string> g_ReframeUndo, g_ReframeRedo;

// Turn machine code into editable source for the given survivor. syncAt forces
// an instruction boundary, re-reading everything from there to the end.
static void ListSurvivor(Survivor& sv, const std::vector<unsigned char>& code, int syncAt = -1)
{
    cw::ListOptions lo;
    lo.upper = g_App.asmUpperCase;
    lo.syncAt = syncAt;
    cw::Listing L = cw::makeListing(code.data(), (int)code.size(), lo);
    sv.code = L.text;
    sv.bin  = code;
    sv.listing = L;
    ReassembleSurvivor(sv);
}

// Assembler errors and warnings, handed to the editor. The assembler itself
// flags an opcode the engine refuses, from the bytes the line assembles to
// now. (These used to be matched up from the listing made when the survivor
// was first read, by byte offset -- once the code was edited, those offsets
// pointed at other lines and perfectly good instructions got flagged.)
static void PushEditorMarkers(const Survivor& sv)
{
    TextEditor::ErrorMarkers marks;
    for (const cw::AsmDiag& d : sv.asmDiags)
        marks[d.line + 1] = (d.error ? "error: " : "warning: ") + d.text;
    g_App.editor.SetErrorMarkers(marks);
}

// Where the `db` of a "db ... ;$N" line starts: after the indentation and any
// "label:", before `mark`. npos when the line is not one of those.
static size_t FindDbToken(const std::string& L, size_t mark)
{
    size_t i = 0;
    while (i < mark && (L[i] == ' ' || L[i] == '\t')) i++;
    for (int pass = 0; pass < 2; pass++)
    {
        if (i + 2 < mark && toupper((unsigned char)L[i]) == 'D' && toupper((unsigned char)L[i + 1]) == 'B' &&
            (L[i + 2] == ' ' || L[i + 2] == '\t'))
            return i;
        // a label in front: skip "name:" and the space after it, once
        const size_t c = L.find(':', i);
        if (pass == 1 || c == std::string::npos || c >= mark) break;
        i = c + 1;
        while (i < mark && (L[i] == ' ' || L[i] == '\t')) i++;
    }
    return std::string::npos;
}

// Rewrite the `db` bytes in front of every ";$" note so they match what the
// note now assembles to. The note is the instruction; the bytes are its
// rendering, so editing the note updates them. Done on demand (recompile / Alt)
// rather than per keystroke, so it never fights the cursor while typing.
static bool NormalizeAltLines(Survivor& sv)
{
    std::vector<std::string> lines;
    size_t pos = 0;
    while (pos <= sv.code.size())
    {
        const size_t nl = sv.code.find('\n', pos);
        lines.push_back(sv.code.substr(pos, (nl == std::string::npos ? sv.code.size() : nl) - pos));
        if (nl == std::string::npos) break;
        pos = nl + 1;
    }

    bool changed = false;
    for (const cw::AsmLine& al : sv.asmLines)
    {
        if (al.line < 0 || al.line >= (int)lines.size() || al.length <= 0) continue;
        std::string& L = lines[(size_t)al.line];
        const size_t mark = L.find(";$");
        if (mark == std::string::npos) continue;

        // keep everything in front of the db -- indentation, a label -- as written
        const size_t dbAt = FindDbToken(L, mark);
        if (dbAt == std::string::npos) continue;
        std::string rebuilt = L.substr(0, dbAt) + (g_App.asmUpperCase ? "DB " : "db ");
        char b[8];
        for (int k = 0; k < al.length; k++)
        {
            const int off = al.offset + k;
            if (off >= (int)sv.bin.size()) break;
            snprintf(b, sizeof(b), "0x%02X", sv.bin[(size_t)off]);
            if (k) rebuilt += ", ";
            rebuilt += b;
        }
        rebuilt += " ";
        rebuilt += L.substr(mark);
        if (rebuilt != L) { L = rebuilt; changed = true; }
    }
    if (!changed) return false;

    std::string joined;
    for (size_t i = 0; i < lines.size(); i++) { if (i) joined += "\n"; joined += lines[i]; }
    sv.code = joined;
    return true;
}

// The live form of the above, run after every edit. In `db ... ;$N text` the
// text is the instruction and the db is only its spelling in bytes, so the db
// is rewritten in the editor the moment the text (or N) assembles to something
// else -- in place, outside the undo history (the editor shifts its undo
// records to match), so typing and undo are never disturbed. A line that does
// not assemble right now is left as it is until it does.
// Returns true when any line was rewritten.
static bool SyncAltLinesLive(Survivor& sv, TextEditor& ed)
{
    if (sv.code.find(";$") == std::string::npos) return false;
    std::vector<std::string> lines;
    {
        size_t pos = 0;
        while (pos <= sv.code.size())
        {
            const size_t nl = sv.code.find('\n', pos);
            lines.push_back(sv.code.substr(pos, (nl == std::string::npos ? sv.code.size() : nl) - pos));
            if (nl == std::string::npos) break;
            pos = nl + 1;
        }
    }
    std::set<int> bad;
    for (const cw::AsmDiag& d : sv.asmDiags) if (d.error) bad.insert(d.line);

    bool changed = false;
    for (const cw::AsmLine& al : sv.asmLines)
    {
        if (al.line < 0 || al.line >= (int)lines.size() || al.length <= 0 || bad.count(al.line)) continue;
        if (al.offset + al.length > (int)sv.bin.size()) continue;
        const std::string& L = lines[(size_t)al.line];
        const size_t mark = L.find(";$");
        if (mark == std::string::npos) continue;

        // only a line that really reads "[label:] db ... ;$" is touched
        const size_t from = FindDbToken(L, mark);
        if (from == std::string::npos) continue;

        const bool up = L[from] == 'D';   // keep the case it was written in
        std::string want = up ? "DB " : "db ";
        char b[8];
        for (int k = 0; k < al.length; k++)
        {
            snprintf(b, sizeof(b), "0x%02X", sv.bin[(size_t)(al.offset + k)]);
            if (k) want += ", ";
            want += b;
        }
        want += " ";
        if (L.compare(from, mark - from, want) == 0) continue;
        ed.ReplaceDerivedText(al.line, (int)from, (int)mark, want);
        changed = true;
    }
    return changed;
}

// Alt on the cursor line steps that line to its next encoding, by rewriting the
// `;$N` index (or dropping it, when the next one is the canonical form).
static void CycleAlternativeOnCursorLine()
{
    AppState& S = g_App;
    Survivor* sv = SurvivorAt(S.open);
    if (!sv || !sv->present) return;

    const int want = S.editor.GetCursorPosition().mLine;

    std::vector<std::string> lines;
    size_t pos = 0;
    while (pos <= sv->code.size())
    {
        const size_t nl = sv->code.find('\n', pos);
        lines.push_back(sv->code.substr(pos, (nl == std::string::npos ? sv->code.size() : nl) - pos));
        if (nl == std::string::npos) break;
        pos = nl + 1;
    }
    if (want < 0 || want >= (int)lines.size()) return;

    std::string& L = lines[(size_t)want];
    const size_t mark = L.find(";$");

    int atOffset = 0;
    for (const cw::AsmLine& al : sv->asmLines)
        if (al.line == want) { atOffset = al.offset; break; }

    // What stands before the instruction -- the indentation and any label --
    // is kept exactly as it was written; Alt only changes the instruction.
    std::string lead;
    {
        size_t ws = 0;
        while (ws < L.size() && (L[ws] == ' ' || L[ws] == '\t')) ws++;
        lead = L.substr(0, ws);
        const std::string body = L.substr(ws, (mark == std::string::npos ? L.size() : mark) - ws);
        const size_t c = body.find(':');
        const size_t semi = body.find(';');
        if (c != std::string::npos && body.find('[') == std::string::npos && (semi == std::string::npos || c < semi))
        {
            size_t e = c + 1;
            while (e < body.size() && (body[e] == ' ' || body[e] == '\t')) e++;
            // "ES:" alone is a segment prefix, not a label
            if (e < body.size()) lead += body.substr(0, e);
        }
    }

    int curIndex = 0;
    std::string insnText;
    if (mark != std::string::npos)
    {
        std::string a = L.substr(mark + 2);
        size_t k = 0;
        while (k < a.size() && isdigit((unsigned char)a[k])) k++;
        curIndex = (k > 0) ? atoi(a.substr(0, k).c_str()) : 0;
        insnText = a.substr(k);
    }
    else
    {
        insnText = L;
        const size_t c = insnText.find(';');
        if (c != std::string::npos) insnText = insnText.substr(0, c);
    }
    while (!insnText.empty() && isspace((unsigned char)insnText.front())) insnText.erase(insnText.begin());
    const size_t colon = insnText.find(':');
    if (colon != std::string::npos && insnText.find('[') == std::string::npos)
        insnText = insnText.substr(colon + 1);
    while (!insnText.empty() && isspace((unsigned char)insnText.front())) insnText.erase(insnText.begin());
    if (insnText.empty()) return;

    std::vector<std::vector<cw::u8>> v = cw::encodeVariants(insnText, atOffset);
    if (v.size() < 2) { S.status = "only one encoding for this instruction"; return; }

    const int next = (curIndex + 1) % (int)v.size();
    std::string out = lead;
    if (next == 0) out += insnText;
    else
    {
        char b[8];
        out += g_App.asmUpperCase ? "DB " : "db ";
        for (size_t k = 0; k < v[(size_t)next].size(); k++)
        {
            snprintf(b, sizeof(b), "0x%02X", v[(size_t)next][k]);
            if (k) out += ", ";
            out += b;
        }
        char n[16];
        snprintf(n, sizeof(n), " ;$%d ", next);
        out += n;
        out += insnText;
    }
    L = out;

    std::string joined;
    for (size_t i = 0; i < lines.size(); i++) { if (i) joined += "\n"; joined += lines[i]; }
    sv->code = joined;
    sv->dirty = true;
    NoteEdit();
    S.editor.SetText(sv->code);
    S.editor.SetCursorPosition(TextEditor::Coordinates(want, 0));
    ReassembleSurvivor(*sv);
    if (NormalizeAltLines(*sv))
    {
        ReassembleSurvivor(*sv);
        S.editor.SetText(sv->code);
        S.editor.SetCursorPosition(TextEditor::Coordinates(want, 0));
    }

    char msg[96];
    snprintf(msg, sizeof(msg), "encoding %d of %d", next + 1, (int)v.size());
    S.status = msg;
}

// What you get if you start decoding at `off` rather than at the instruction
// that actually contains it: the bytes you skipped past can only be read as db.
static void ByteHoverTooltip(const Survivor& sv, int off)
{
    // find the instruction that contains this byte
    int owner = -1, ownerLen = 0;
    for (const cw::AsmLine& al : sv.asmLines)
        if (al.length > 0 && off >= al.offset && off < al.offset + al.length)
        { owner = al.offset; ownerLen = al.length; break; }

    ImGui::BeginTooltip();
    ImGui::TextColored(Hex(Col::TextDim), "offset %04X   byte %02X",
                       (unsigned)off, (unsigned)sv.bin[(size_t)off]);

    auto decodeAt = [&](int at) {
        cw::u8 win[8] = {0};
        for (int k = 0; k < 6; k++)
            win[k] = (at + k < (int)sv.bin.size()) ? sv.bin[(size_t)(at + k)] : (cw::u8)0;
        return cw::disasm(win, 6, g_App.asmUpperCase, at);
    };

    if (owner >= 0 && owner != off)
    {
        // mid-instruction: show the real instruction, then the re-read
        const cw::Insn whole = decodeAt(owner);
        ImGui::TextColored(Hex(Col::TextDim, 0.8f), "inside %04X  %s", (unsigned)owner, whole.text);
        ImGui::Separator();
        ImGui::TextColored(Hex(Col::Warn), "reading from %04X instead:", (unsigned)off);
        // the leading bytes of the original instruction are no longer an
        // instruction at all -- they are just data
        std::string db = g_App.asmUpperCase ? "DB " : "db ";
        char b[8];
        for (int k = owner; k < off; k++)
        {
            snprintf(b, sizeof(b), "0x%02X", sv.bin[(size_t)k]);
            if (k > owner) db += ", ";
            db += b;
        }
        ImGui::TextColored(Hex(Col::TextDim), "  %s", db.c_str());
        const cw::Insn in = decodeAt(off);
        ImGui::TextColored(Hex(in.invalid ? Col::Danger : in.unsupported ? Col::Warn : Col::Text),
                           "  %s", in.text);
        (void)ownerLen;
    }
    else
    {
        const cw::Insn in = decodeAt(off);
        ImGui::TextColored(Hex(in.invalid ? Col::Danger : in.unsupported ? Col::Warn : Col::Text),
                           "%s", in.text);
    }
    ImGui::EndTooltip();
}

// The assembled bytes, one source line per row, drawn to the left of the code.
// Returns the width it used.
static float DrawByteGutter(const Survivor& sv, int totalLines, float lineH, float height)
{
    AppState& S = g_App;
    const float w = ImGui::CalcTextSize("0000  00 00 00 00 00 00 ").x;
    const ImVec2 base = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddRectFilled(base, ImVec2(base.x + w, base.y + height), HexU(Col::Bg0));
    dl->AddLine(ImVec2(base.x + w - 1, base.y), ImVec2(base.x + w - 1, base.y + height),
                HexU(Col::Line));

    const int curLine = S.editor.GetCursorPosition().mLine;

    for (const cw::AsmLine& al : sv.asmLines)
    {
        if (al.line < 0 || al.line >= totalLines) continue;
        const float y = base.y + al.line * lineH;
        if (y + lineH < base.y || y > base.y + height) continue;   // off screen

        if (al.line == curLine)
            dl->AddRectFilled(ImVec2(base.x, y), ImVec2(base.x + w - 2, y + lineH),
                              HexU(Col::Accent, 0.14f));
        if (al.length <= 0) continue;

        char a[12];
        snprintf(a, sizeof(a), "%04X", (unsigned)al.offset);
        dl->AddText(ImVec2(base.x + 4 * g_Scale, y), HexU(Col::TextDim, 0.55f), a);

        float x = base.x + 4 * g_Scale + ImGui::CalcTextSize("0000  ").x;
        for (int k = 0; k < al.length && k < 6; k++)
        {
            const int off = al.offset + k;
            if (off >= (int)sv.bin.size()) break;
            char b[4];
            snprintf(b, sizeof(b), "%02X", sv.bin[(size_t)off]);
            const float bw = ImGui::CalcTextSize("00 ").x;
            const bool hot = ImGui::IsWindowHovered() &&
                ImGui::IsMouseHoveringRect(ImVec2(x, y), ImVec2(x + bw - 2, y + lineH));
            if (hot)
                dl->AddRectFilled(ImVec2(x - 1, y), ImVec2(x + bw - 3, y + lineH),
                                  HexU(Col::Accent2, 0.30f), 2.0f);
            dl->AddText(ImVec2(x, y), HexU(al.line == curLine ? Col::Text : Col::TextDim,
                                           al.line == curLine ? 1.0f : 0.8f), b);
            if (hot)
            {
                ByteHoverTooltip(sv, off);
                if (S.dblClickDisasm && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    g_ReframeAt = off;
            }
            x += bw;
        }
        if (al.length > 6)
            dl->AddText(ImVec2(x, y), HexU(Col::TextDim, 0.5f), "+");
    }

    ImGui::Dummy(ImVec2(w, height));
    return w;
}
