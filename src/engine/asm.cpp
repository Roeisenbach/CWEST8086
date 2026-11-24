// =============================================================================
//  8086 assembler -- see asm.h
// =============================================================================
#include <unordered_map>
#include "asm.h"
#include "disasm.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <map>

namespace cw {
namespace {

// ---------------------------------------------------------------------------
//  Tables (mirroring disasm.cpp)
// ---------------------------------------------------------------------------
const char* const R8 [8] = { "AL","CL","DL","BL","AH","CH","DH","BH" };
const char* const R16[8] = { "AX","CX","DX","BX","SP","BP","SI","DI" };
const char* const SEG4[4] = { "ES","CS","SS","DS" };
const char* const ALU[8] = { "ADD","OR","ADC","SBB","AND","SUB","XOR","CMP" };
const char* const SHF[8] = { "ROL","ROR","RCL","RCR","SHL","SHR","","SAR" };
// condition codes in opcode order 0x70..0x7F, with their common aliases
struct CcName { const char* n; int code; };
const CcName CC[] = {
    {"JO",0},{"JNO",1},{"JB",2},{"JC",2},{"JNAE",2},{"JNB",3},{"JNC",3},{"JAE",3},
    {"JZ",4},{"JE",4},{"JNZ",5},{"JNE",5},{"JBE",6},{"JNA",6},{"JA",7},{"JNBE",7},
    {"JS",8},{"JNS",9},{"JP",10},{"JPE",10},{"JNP",11},{"JPO",11},
    {"JL",12},{"JNGE",12},{"JGE",13},{"JNL",13},{"JLE",14},{"JNG",14},{"JG",15},{"JNLE",15}
};

std::string upper(std::string s) {
    for (char& c : s) c = (char)toupper((unsigned char)c);
    return s;
}
std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && isspace((unsigned char)s[a])) ++a;
    while (b > a && isspace((unsigned char)s[b - 1])) --b;
    return s.substr(a, b - a);
}

int regIndex(const char* const* tbl, int n, const std::string& s) {
    for (int i = 0; i < n; i++) if (s == tbl[i]) return i;
    return -1;
}

// 0x1234 / 1234h / 1010b / 123 / -5 / +7
bool parseNumber(const std::string& raw, long long& out) {
    std::string s = trim(raw);
    if (s.empty()) return false;
    int sign = 1;
    size_t i = 0;
    if (s[0] == '-') { sign = -1; i = 1; }
    else if (s[0] == '+') i = 1;
    std::string t = upper(s.substr(i));
    if (t.empty()) return false;

    int base = 10;
    if (t.size() > 2 && t[0] == '0' && t[1] == 'X') { base = 16; t = t.substr(2); }
    else if (t.size() > 1 && t.back() == 'H')       { base = 16; t.pop_back(); }
    else if (t.size() > 1 && t.back() == 'B' &&
             t.find_first_not_of("01") == t.size() - 1) { base = 2; t.pop_back(); }
    if (t.empty()) return false;

    long long v = 0;
    for (char c : t) {
        int d;
        if (c >= '0' && c <= '9')      d = c - '0';
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else return false;
        if (d >= base) return false;
        v = v * base + d;
    }
    out = sign * v;
    return true;
}

// ---------------------------------------------------------------------------
//  Operands
// ---------------------------------------------------------------------------
enum class OK { None, Reg8, Reg16, Seg, Imm, Mem, Label };

struct Operand {
    OK   kind = OK::None;
    int  reg = -1;
    long long imm = 0;
    // memory
    bool bx = false, bp = false, si = false, di = false;
    long long disp = 0;
    bool hasDisp = false;
    int  sizeHint = 0;          // operand width: 0 unset, 1 byte, 2 word
    int  dispHint = 0;          // width forced on the displacement itself
    bool isFar = false;         // CALL / JMP through a far pointer
    int  jumpHint = 0;          // JMP SHORT x = 1, JMP NEAR x = 2
    int  farSeg = -1;           // segment half of a SEG:OFF immediate
    std::string label;          // unresolved symbol (branch target or disp)
    bool labelIsDisp = false;
};

struct Parsed {
    std::string mnem;
    Operand a, b;
    int  nOps = 0;
    bool ok = false;
    std::string err;
};

// split on commas at depth 0
// Fold "<number> +/- <number>" into one value, so a branch written against a
// label plus an offset ("loop1+2", which becomes "37+2" once the label is
// substituted) reaches the encoders as a single number.
std::string foldSums(const std::string& in) {
    std::string s = in;
    for (int pass = 0; pass < 8; ++pass) {
        bool changed = false;
        for (size_t i = 0; i < s.size(); ) {
            if (!isdigit((unsigned char)s[i])) { ++i; continue; }
            size_t a = i;
            while (i < s.size() && (isalnum((unsigned char)s[i]) || s[i] == 'x' || s[i] == 'X')) ++i;
            const size_t aEnd = i;
            size_t j = i;
            while (j < s.size() && (s[j] == ' ' || s[j] == '\t')) ++j;
            if (j >= s.size() || (s[j] != '+' && s[j] != '-')) continue;
            const int sign = (s[j] == '-') ? -1 : 1;
            ++j;
            while (j < s.size() && (s[j] == ' ' || s[j] == '\t')) ++j;
            if (j >= s.size() || !isdigit((unsigned char)s[j])) continue;
            size_t b = j;
            while (j < s.size() && (isalnum((unsigned char)s[j]) || s[j] == 'x' || s[j] == 'X')) ++j;

            long long lv, rv;
            if (!parseNumber(s.substr(a, aEnd - a), lv)) continue;
            if (!parseNumber(s.substr(b, j - b), rv)) continue;
            char buf[24];
            snprintf(buf, sizeof(buf), "%lld", lv + sign * rv);
            s = s.substr(0, a) + buf + s.substr(j);
            i = a;
            changed = true;
        }
        if (!changed) break;
    }
    return s;
}

std::vector<std::string> splitOps(const std::string& s) {
    std::vector<std::string> out;
    int depth = 0;
    std::string cur;
    for (char c : s) {
        if (c == '[') depth++;
        if (c == ']') depth--;
        if (c == ',' && depth == 0) { out.push_back(trim(cur)); cur.clear(); }
        else cur.push_back(c);
    }
    if (!trim(cur).empty()) out.push_back(trim(cur));
    return out;
}

bool parseMem(const std::string& inner, Operand& o, std::string& err) {
    // BX + SI + byte 0x12   /  0x1234  /  BP - 4
    std::string s = inner;
    for (char& c : s) if (c == '-') c = '-';       // keep as-is; handled per term
    // tokenise into +/- separated terms, keeping the sign
    std::vector<std::pair<int, std::string>> terms;
    std::string cur;
    int sign = 1;
    for (size_t i = 0; i < s.size(); i++) {
        const char c = s[i];
        if (c == '+' || c == '-') {
            if (!trim(cur).empty()) { terms.push_back({ sign, trim(cur) }); cur.clear(); }
            sign = (c == '-') ? -1 : 1;
        } else cur.push_back(c);
    }
    if (!trim(cur).empty()) terms.push_back({ sign, trim(cur) });

    for (auto& t : terms) {
        std::string u = upper(t.second);
        // an optional size keyword may sit right before the displacement
        if (u.rfind("BYTE ", 0) == 0) { o.dispHint = 1; u = trim(u.substr(5)); }
        else if (u.rfind("WORD ", 0) == 0) { o.dispHint = 2; u = trim(u.substr(5)); }

        const bool baseIdx = (u == "BX" || u == "BP" || u == "SI" || u == "DI");
        if (baseIdx && t.first < 0) {
            err = "a register can't be subtracted in an address: [BX-SI] isn't one";
            return false;
        }
        if      (u == "BX") o.bx = true;
        else if (u == "BP") o.bp = true;
        else if (u == "SI") o.si = true;
        else if (u == "DI") o.di = true;
        else if (regIndex(R8, 8, u) >= 0 || regIndex(R16, 8, u) >= 0 || regIndex(SEG4, 4, u) >= 0) {
            // [AX], [CX+2]: taken for a label before, which quietly made [0]
            err = u + " can't address memory on the 8086 - only BX, BP, SI and DI can: "
                      "[BX], [BP+SI], [DI+4], [1234h]";
            return false;
        }
        else {
            long long v;
            if (parseNumber(u, v)) { o.disp += t.first * v; o.hasDisp = true; }
            else if (!u.empty() && (isalpha((unsigned char)u[0]) || u[0] == '_')) {
                o.label = u; o.labelIsDisp = true; o.hasDisp = true;
            } else { err = "bad memory term: " + t.second; return false; }
        }
    }
    o.kind = OK::Mem;
    return true;
}

// A leading keyword such as BYTE / WORD / PTR / FAR, whether it is followed by
// whitespace or runs straight into the bracket ("word[bx+3]", "far[bx+si]").
bool eatKeyword(std::string& s, std::string& u, const char* kw) {
    const size_t n = strlen(kw);
    if (u.size() <= n || u.compare(0, n, kw) != 0) return false;
    const char c = u[n];
    if (c != ' ' && c != '\t' && c != '[') return false;
    s = trim(s.substr(n));
    u = upper(s);
    return true;
}

bool parseOperand(const std::string& raw, Operand& o, std::string& err) {
    std::string s = trim(raw);
    if (s.empty()) return false;
    std::string u = upper(s);

    if      (eatKeyword(s, u, "BYTE")) o.sizeHint = 1;
    else if (eatKeyword(s, u, "WORD")) o.sizeHint = 2;
    eatKeyword(s, u, "PTR");
    if      (eatKeyword(s, u, "SHORT")) o.jumpHint = 1;
    else if (eatKeyword(s, u, "NEAR"))  o.jumpHint = 2;
    const bool far = eatKeyword(s, u, "FAR");

    if (!s.empty() && s.front() == '[') {
        const size_t close = s.rfind(']');
        if (close == std::string::npos) { err = "missing ]"; return false; }
        if (!parseMem(s.substr(1, close - 1), o, err)) return false;
        o.isFar = far;
        return true;
    }
    int r;
    if ((r = regIndex(R8, 8, u)) >= 0)  { o.kind = OK::Reg8;  o.reg = r; return true; }
    if ((r = regIndex(R16, 8, u)) >= 0) { o.kind = OK::Reg16; o.reg = r; return true; }
    if ((r = regIndex(SEG4, 4, u)) >= 0){ o.kind = OK::Seg;   o.reg = r; return true; }

    const size_t colon = s.find(':');
    if (colon != std::string::npos) {
        long long sg, of;
        if (parseNumber(s.substr(0, colon), sg) && parseNumber(s.substr(colon + 1), of)) {
            o.kind = OK::Imm; o.imm = of; o.farSeg = (int)sg; o.isFar = true;
            return true;
        }
    }
    long long v;
    if (parseNumber(s, v)) { o.kind = OK::Imm; o.imm = v; return true; }
    if (isalpha((unsigned char)s[0]) || s[0] == '_' || s[0] == '.') {
        o.kind = OK::Label; o.label = u; return true;
    }
    err = "cannot parse operand: " + s;
    return false;
}

// Where the mnemonic stops. "mov[bx], ax" is as legal as "mov [bx], ax", so a
// bracket ends it just like whitespace does.
size_t mnemEnd(const std::string& s) { return s.find_first_of(" \t["); }

Parsed parseInsn(const std::string& text) {
    Parsed p;
    std::string s = trim(text);
    if (s.empty()) return p;
    size_t sp = mnemEnd(s);
    p.mnem = upper(sp == std::string::npos ? s : s.substr(0, sp));
    const std::string rest = (sp == std::string::npos) ? "" : trim(s.substr(sp));
    std::vector<std::string> ops = splitOps(rest);
    if (ops.size() > 2) { p.err = "too many operands"; return p; }
    if (ops.size() >= 1 && !parseOperand(ops[0], p.a, p.err)) return p;
    if (ops.size() >= 2 && !parseOperand(ops[1], p.b, p.err)) return p;
    p.nOps = (int)ops.size();
    p.ok = true;
    return p;
}

// ---------------------------------------------------------------------------
//  Encoding helpers
// ---------------------------------------------------------------------------
struct Enc {
    std::vector<u8> b;
    void u8v(int v)  { b.push_back((u8)(v & 0xFF)); }
    void u16v(int v) { b.push_back((u8)(v & 0xFF)); b.push_back((u8)((v >> 8) & 0xFF)); }
};

bool memRm(const Operand& m, int& rm, int& modOut, int& dispBytes) {
    const bool direct = !m.bx && !m.bp && !m.si && !m.di;
    if (direct) { rm = 6; modOut = 0; dispBytes = 2; return true; }
    if      (m.bx && m.si && !m.bp && !m.di) rm = 0;
    else if (m.bx && m.di && !m.bp && !m.si) rm = 1;
    else if (m.bp && m.si && !m.bx && !m.di) rm = 2;
    else if (m.bp && m.di && !m.bx && !m.si) rm = 3;
    else if (m.si && !m.bx && !m.bp && !m.di) rm = 4;
    else if (m.di && !m.bx && !m.bp && !m.si) rm = 5;
    else if (m.bp && !m.bx && !m.si && !m.di) rm = 6;
    else if (m.bx && !m.bp && !m.si && !m.di) rm = 7;
    else return false;

    const long long d = m.disp;
    if (d == 0 && rm != 6)                 { modOut = 0; dispBytes = 0; }
    else if (d >= -128 && d <= 127)        { modOut = 1; dispBytes = 1; }
    else                                   { modOut = 2; dispBytes = 2; }
    // a size keyword written inside the brackets forces the displacement width,
    // including forcing a zero displacement to actually be emitted
    if (m.dispHint == 1) { modOut = 1; dispBytes = 1; }
    if (m.dispHint == 2) { modOut = 2; dispBytes = 2; }
    return true;
}

// emit modrm (+displacement) for a memory or register r/m operand
bool emitRm(Enc& e, const Operand& rmOp, int reg) {
    if (rmOp.kind == OK::Reg8 || rmOp.kind == OK::Reg16) {
        e.u8v(0xC0 | ((reg & 7) << 3) | (rmOp.reg & 7));
        return true;
    }
    if (rmOp.kind != OK::Mem) return false;
    int rm = 0, mod = 0, db = 0;
    if (!memRm(rmOp, rm, mod, db)) return false;
    e.u8v((mod << 6) | ((reg & 7) << 3) | rm);
    if (db == 1) e.u8v((int)rmOp.disp);
    else if (db == 2) e.u16v((int)rmOp.disp);
    return true;
}

bool isIdent(const std::string& s) {
    if (s.empty()) return false;
    const unsigned char c0 = (unsigned char)s[0];
    if (!isalpha(c0) && c0 != '_' && c0 != '.') return false;
    for (char ch : s) {
        const unsigned char c = (unsigned char)ch;
        if (!isalnum(c) && c != '_' && c != '.') return false;
    }
    return true;
}

// Replace every %define'd identifier in `s` with its value. Purely textual,
// so a value may be a number, a register, a label, a whole memory operand or
// even a complete instruction. Re-run until nothing changes so a define may
// refer to another one; the pass cap stops a self-referencing define.
std::string expandDefines(std::string s, const std::map<std::string, std::string>& defs) {
    if (defs.empty()) return s;
    for (int pass = 0; pass < 8; ++pass) {
        std::string out;
        bool changed = false;
        size_t i = 0;
        while (i < s.size()) {
            const unsigned char c0 = (unsigned char)s[i];
            size_t j = i;
            if (isdigit(c0)) {
                // a number such as 0FFh is one token, not a digit + identifier
                while (j < s.size() && (isalnum((unsigned char)s[j]) || s[j] == '_')) ++j;
                out += s.substr(i, j - i);
            } else if (isalpha(c0) || c0 == '_' || c0 == '.') {
                while (j < s.size() && (isalnum((unsigned char)s[j]) || s[j] == '_' || s[j] == '.')) ++j;
                const std::string tok = s.substr(i, j - i);
                auto it = defs.find(upper(tok));
                if (it != defs.end()) { out += it->second; changed = true; }
                else out += tok;
            } else {
                out.push_back(s[i]);
                j = i + 1;
            }
            i = j;
        }
        if (!changed) break;
        s = out;
    }
    return s;
}

bool isAcc(const Operand& o, bool wide) {
    return wide ? (o.kind == OK::Reg16 && o.reg == 0) : (o.kind == OK::Reg8 && o.reg == 0);
}
bool isRm(const Operand& o) {
    return o.kind == OK::Mem || o.kind == OK::Reg8 || o.kind == OK::Reg16;
}
// operand width: 1 = byte, 2 = word, 0 = unknown (memory with no hint)
int widthOf(const Operand& o) {
    if (o.kind == OK::Reg8) return 1;
    if (o.kind == OK::Reg16 || o.kind == OK::Seg) return 2;
    if (o.kind == OK::Mem) return o.sizeHint;
    return 0;
}

} // namespace

// ---------------------------------------------------------------------------
//  encodeVariants -- canonical first, then the legal alternatives
// ---------------------------------------------------------------------------
std::vector<std::vector<u8>> encodeVariants(const std::string& insnText, int atOffset) {
    std::vector<std::vector<u8>> out;
    Parsed p = parseInsn(insnText);
    if (!p.ok) return out;
    const std::string& M = p.mnem;
    const Operand& A = p.a;
    const Operand& B = p.b;

    auto push = [&](const Enc& e) {
        if (e.b.empty()) return;
        for (const auto& prev : out) if (prev == e.b) return;
        out.push_back(e.b);
    };

    // Two operands of different widths ("mov al, bx") have no encoding: the
    // width bit would silently reinterpret one of them as the other size.
    if (p.nOps == 2 && A.kind != OK::Seg && B.kind != OK::Seg) {
        const int wa = widthOf(A), wb = widthOf(B);
        if (wa && wb && wa != wb && !(M == "IN" || M == "OUT") &&
            !(B.kind == OK::Reg8 && B.reg == 1 && (M == "ROL" || M == "ROR" || M == "RCL" || M == "RCR" ||
                                                   M == "SHL" || M == "SAL" || M == "SHR" || M == "SAR")))
            return out;
    }

    // ---- virtual opcodes -------------------------------------------------
    if (M == "BRK") { Enc e; e.u8v(0xEB); e.u8v(0x00); push(e); return out; }
    if (M == "NRG") { Enc e; for (int i = 0; i < 4; i++) e.u8v(0x9B); push(e);
                      Enc e2; e2.u8v(0x9B); e2.u8v(0x9B); push(e2); return out; }
    if (M == "WAIT2") { Enc e; e.u8v(0x9B); e.u8v(0x9B); push(e); return out; }

    // ---- segment override prefix (engine refuses it, but it must round-trip) --
    if (p.nOps == 0 && M.size() == 3 && M[2] == ':') {
        static const struct { const char* n; int op; } kSeg[] = {
            {"ES:",0x26},{"CS:",0x2E},{"SS:",0x36},{"DS:",0x3E}
        };
        for (const auto& sg : kSeg)
            if (M == sg.n) { Enc e; e.u8v(sg.op); push(e); return out; }
    }

    // ---- no-operand forms -------------------------------------------------
    struct Simple { const char* m; int op; };
    static const Simple kSimple[] = {
        {"NOP",0x90},{"CBW",0x98},{"CWD",0x99},{"WAIT",0x9B},{"PUSHF",0x9C},{"POPF",0x9D},
        {"SAHF",0x9E},{"LAHF",0x9F},{"MOVSB",0xA4},{"MOVSW",0xA5},{"CMPSB",0xA6},{"CMPSW",0xA7},
        {"STOSB",0xAA},{"STOSW",0xAB},{"LODSB",0xAC},{"LODSW",0xAD},{"SCASB",0xAE},{"SCASW",0xAF},
        {"RET",0xC3},{"RETN",0xC3},{"RETF",0xCB},{"INT3",0xCC},{"INTO",0xCE},{"IRET",0xCF},
        {"XLAT",0xD7},{"XLATB",0xD7},{"HLT",0xF4},{"CMC",0xF5},{"CLC",0xF8},{"STC",0xF9},
        {"CLI",0xFA},{"STI",0xFB},{"CLD",0xFC},{"STD",0xFD},{"LOCK",0xF0},
        {"DAA",0x27},{"DAS",0x2F},{"AAA",0x37},{"AAS",0x3F}
    };
    if (p.nOps == 0) {
        for (const Simple& s : kSimple)
            if (M == s.m) { Enc e; e.u8v(s.op); push(e); return out; }
        if (M == "AAM") { Enc e; e.u8v(0xD4); e.u8v(0x0A); push(e); return out; }
        if (M == "AAD") { Enc e; e.u8v(0xD5); e.u8v(0x0A); push(e); return out; }
    }
    if (p.nOps == 1 && M == "ESC" && A.kind == OK::Imm) {
        Enc e; e.u8v((int)A.imm); push(e); return out;
    }
    if (p.nOps == 1 && (M == "AAM" || M == "AAD") && A.kind == OK::Imm) {
        Enc e; e.u8v(M == "AAM" ? 0xD4 : 0xD5); e.u8v((int)A.imm); push(e); return out;
    }
    if (p.nOps == 0) {
    }
    if (p.nOps == 1 && (M == "RET" || M == "RETN") && A.kind == OK::Imm) {
        Enc e; e.u8v(0xC2); e.u16v((int)A.imm); push(e); return out;
    }
    if (p.nOps == 1 && M == "RETF" && A.kind == OK::Imm) {
        Enc e; e.u8v(0xCA); e.u16v((int)A.imm); push(e); return out;
    }
    if (p.nOps == 1 && M == "INT" && A.kind == OK::Imm) {
        Enc e; e.u8v(0xCD); e.u8v((int)A.imm); push(e); return out;
    }

    // ---- string-op prefixes ----------------------------------------------
    if ((M == "REP" || M == "REPE" || M == "REPZ" || M == "REPNE" || M == "REPNZ") && p.nOps == 1
        && A.kind == OK::Label) {
        static const struct { const char* m; int op; } sops[] = {
            {"MOVSB",0xA4},{"MOVSW",0xA5},{"CMPSB",0xA6},{"CMPSW",0xA7},
            {"STOSB",0xAA},{"STOSW",0xAB},{"LODSB",0xAC},{"LODSW",0xAD},
            {"SCASB",0xAE},{"SCASW",0xAF}
        };
        const int pre = (M == "REPNE" || M == "REPNZ") ? 0xF2 : 0xF3;
        for (const auto& s : sops)
            if (A.label == s.m) { Enc e; e.u8v(pre); e.u8v(s.op); push(e); return out; }
    }

    // ---- ALU group --------------------------------------------------------
    for (int aluOp = 0; aluOp < 8; aluOp++) {
        if (M != ALU[aluOp]) continue;
        if (p.nOps != 2) return out;
        const int w = widthOf(A) ? widthOf(A) : widthOf(B);
        const bool wide = (w == 2);

        // reg <- r/m and r/m <- reg
        if (isRm(A) && (B.kind == OK::Reg8 || B.kind == OK::Reg16)) {
            Enc e; e.u8v((aluOp << 3) | (wide ? 1 : 0));
            if (emitRm(e, A, B.reg)) push(e);
        }
        if ((A.kind == OK::Reg8 || A.kind == OK::Reg16) && isRm(B)) {
            Enc e; e.u8v((aluOp << 3) | (wide ? 3 : 2));
            if (emitRm(e, B, A.reg)) push(e);
        }
        if (B.kind == OK::Imm) {
            // accumulator short form
            if (isAcc(A, wide)) {
                Enc e; e.u8v((aluOp << 3) | (wide ? 5 : 4));
                if (wide) e.u16v((int)B.imm); else e.u8v((int)B.imm);
                push(e);
            }
            // sign-extended byte immediate (word operands only)
            // An explicit WORD before the immediate forces the long spelling.
            const s16 v16 = (s16)B.imm;
            if (wide && (s16)(s8)v16 == v16 && B.sizeHint != 2) {
                Enc e; e.u8v(0x83);
                if (emitRm(e, A, aluOp)) { e.u8v((int)(s8)v16); push(e); }
            }
            { Enc e; e.u8v(wide ? 0x81 : 0x80);
              if (emitRm(e, A, aluOp)) {
                  if (wide) e.u16v((int)B.imm); else e.u8v((int)B.imm);
                  push(e);
              } }
            if (!wide) {   // 0x82 is an undocumented alias of 0x80
                Enc e; e.u8v(0x82);
                if (emitRm(e, A, aluOp)) { e.u8v((int)B.imm); push(e); }
            }
        }
        return out;
    }

    // ---- MOV --------------------------------------------------------------
    if (M == "MOV" && p.nOps == 2) {
        const int w = widthOf(A) ? widthOf(A) : widthOf(B);
        const bool wide = (w == 2);
        if (A.kind == OK::Seg && isRm(B)) { Enc e; e.u8v(0x8E); if (emitRm(e, B, A.reg)) push(e);
            Enc e2; e2.u8v(0x8E); if (emitRm(e2, B, A.reg + 4)) push(e2); return out; }
        if (isRm(A) && B.kind == OK::Seg) { Enc e; e.u8v(0x8C); if (emitRm(e, A, B.reg)) push(e);
            Enc e2; e2.u8v(0x8C); if (emitRm(e2, A, B.reg + 4)) push(e2); return out; }

        // accumulator <-> direct address
        if (isAcc(A, wide) && B.kind == OK::Mem && !B.bx && !B.bp && !B.si && !B.di && B.label.empty()) {
            Enc e; e.u8v(wide ? 0xA1 : 0xA0); e.u16v((int)B.disp); push(e);
        }
        if (A.kind == OK::Mem && !A.bx && !A.bp && !A.si && !A.di && A.label.empty() && isAcc(B, wide)) {
            Enc e; e.u8v(wide ? 0xA3 : 0xA2); e.u16v((int)A.disp); push(e);
        }
        if (isRm(A) && (B.kind == OK::Reg8 || B.kind == OK::Reg16)) {
            Enc e; e.u8v(wide ? 0x89 : 0x88); if (emitRm(e, A, B.reg)) push(e);
        }
        if ((A.kind == OK::Reg8 || A.kind == OK::Reg16) && isRm(B)) {
            Enc e; e.u8v(wide ? 0x8B : 0x8A); if (emitRm(e, B, A.reg)) push(e);
        }
        if (B.kind == OK::Imm) {
            if (A.kind == OK::Reg8)  { Enc e; e.u8v(0xB0 + A.reg); e.u8v((int)B.imm); push(e); }
            if (A.kind == OK::Reg16) { Enc e; e.u8v(0xB8 + A.reg); e.u16v((int)B.imm); push(e); }
            // C6/C7 ignore the reg field, so all eight spellings are legal
            for (int r = 0; r < 8; r++) {
                Enc e; e.u8v(wide ? 0xC7 : 0xC6);
                if (!emitRm(e, A, r)) break;
                if (wide) e.u16v((int)B.imm); else e.u8v((int)B.imm);
                push(e);
            }
        }
        return out;
    }

    // ---- one-operand r/m groups ------------------------------------------
    struct Grp { const char* m; int op8, op16, reg; };
    static const Grp kFE[] = { {"INC",0xFE,0xFF,0}, {"DEC",0xFE,0xFF,1} };
    for (const Grp& gp : kFE) {
        if (M != gp.m || p.nOps != 1) continue;
        if (A.kind == OK::Reg16) { Enc e; e.u8v((gp.reg == 0 ? 0x40 : 0x48) + A.reg); push(e); }
        const bool wide = widthOf(A) == 2;
        Enc e; e.u8v(wide ? gp.op16 : gp.op8);
        if (emitRm(e, A, gp.reg)) push(e);
        return out;
    }
    if ((M == "PUSH" || M == "POP") && p.nOps == 1) {
        const bool isPush = (M == "PUSH");
        if (A.kind == OK::Reg16) { Enc e; e.u8v((isPush ? 0x50 : 0x58) + A.reg); push(e); }
        if (A.kind == OK::Seg) {
            static const int pushSeg[4] = { 0x06, 0x0E, 0x16, 0x1E };
            static const int popSeg [4] = { 0x07, -1,   0x17, 0x1F };
            const int op = isPush ? pushSeg[A.reg] : popSeg[A.reg];
            if (op >= 0) { Enc e; e.u8v(op); push(e); }
        }
        if (A.kind == OK::Mem) {
            if (isPush) { Enc e; e.u8v(0xFF); if (emitRm(e, A, 6)) push(e); }
            else for (int r = 0; r < 8; r++) {   // 8F ignores the reg field
                Enc e; e.u8v(0x8F); if (!emitRm(e, A, r)) break; push(e);
            }
        }
        return out;
    }
    struct G3 { const char* m; int reg; };
    static const G3 kF6[] = { {"TEST",0},{"NOT",2},{"NEG",3},{"MUL",4},{"IMUL",5},{"DIV",6},{"IDIV",7} };
    for (const G3& gp : kF6) {
        if (M != gp.m) continue;
        const bool wide = (widthOf(A) ? widthOf(A) : widthOf(B)) == 2;
        if (gp.reg == 0) {                       // TEST has its own forms
            if (p.nOps != 2) return out;
            if (B.kind == OK::Imm) {
                if (isAcc(A, wide)) { Enc e; e.u8v(wide ? 0xA9 : 0xA8);
                    if (wide) e.u16v((int)B.imm); else e.u8v((int)B.imm); push(e); }
                Enc e; e.u8v(wide ? 0xF7 : 0xF6);
                if (emitRm(e, A, 0)) { if (wide) e.u16v((int)B.imm); else e.u8v((int)B.imm); push(e); }
                return out;
            }
            if (isRm(A) && (B.kind == OK::Reg8 || B.kind == OK::Reg16)) {
                Enc e; e.u8v(wide ? 0x85 : 0x84); if (emitRm(e, A, B.reg)) push(e);
            }
            if ((A.kind == OK::Reg8 || A.kind == OK::Reg16) && isRm(B)) {
                Enc e; e.u8v(wide ? 0x85 : 0x84); if (emitRm(e, B, A.reg)) push(e);
            }
            return out;
        }
        if (p.nOps != 1) return out;
        Enc e; e.u8v(wide ? 0xF7 : 0xF6);
        if (emitRm(e, A, gp.reg)) push(e);
        return out;
    }

    // ---- XCHG -------------------------------------------------------------
    if (M == "XCHG" && p.nOps == 2) {
        const bool wide = (widthOf(A) ? widthOf(A) : widthOf(B)) == 2;
        if (A.kind == OK::Reg16 && isAcc(B, true)) { Enc e; e.u8v(0x90 + A.reg); push(e); }
        if (B.kind == OK::Reg16 && isAcc(A, true)) { Enc e; e.u8v(0x90 + B.reg); push(e); }
        if ((A.kind == OK::Reg8 || A.kind == OK::Reg16) && isRm(B)) {
            Enc e; e.u8v(wide ? 0x87 : 0x86); if (emitRm(e, B, A.reg)) push(e);
        }
        if (isRm(A) && (B.kind == OK::Reg8 || B.kind == OK::Reg16)) {
            Enc e; e.u8v(wide ? 0x87 : 0x86); if (emitRm(e, A, B.reg)) push(e);
        }
        return out;
    }

    // ---- LEA / LES / LDS --------------------------------------------------
    if ((M == "LEA" || M == "LES" || M == "LDS") && p.nOps == 2 &&
        A.kind == OK::Reg16 && B.kind == OK::Mem) {
        Enc e; e.u8v(M == "LEA" ? 0x8D : (M == "LES" ? 0xC4 : 0xC5));
        if (emitRm(e, B, A.reg)) push(e);
        return out;
    }

    // ---- shifts -----------------------------------------------------------
    for (int sh = 0; sh < 8; sh++) {
        // SAL is SHL by another name
        if (sh == 6 || (M != SHF[sh] && !(sh == 4 && M == "SAL"))) continue;
        if (p.nOps != 2) return out;
        const bool wide = widthOf(A) == 2;
        const bool byCl = (B.kind == OK::Reg8 && B.reg == 1);
        const bool byOne = (B.kind == OK::Imm && B.imm == 1);
        if (!byCl && !byOne) return out;
        Enc e; e.u8v((byCl ? 0xD2 : 0xD0) + (wide ? 1 : 0));
        if (emitRm(e, A, sh)) push(e);
        return out;
    }

    // ---- branches ---------------------------------------------------------
    auto rel8 = [&](int op, long long target) {
        const long long d = target - (atOffset + 2);
        if (d < -128 || d > 127) return;
        Enc e; e.u8v(op); e.u8v((int)d); push(e);
    };
    for (const CcName& cc : CC) {
        if (M != cc.n || p.nOps != 1 || A.kind != OK::Imm || A.jumpHint == 2) continue;
        rel8(0x70 + cc.code, A.imm);
        return out;
    }
    static const struct { const char* m; int op; } kLoop[] = {
        {"LOOPNZ",0xE0},{"LOOPNE",0xE0},{"LOOPZ",0xE1},{"LOOPE",0xE1},{"LOOP",0xE2},{"JCXZ",0xE3}
    };
    for (const auto& l : kLoop) {
        if (M != l.m || p.nOps != 1 || A.kind != OK::Imm) continue;
        rel8(l.op, A.imm);
        return out;
    }
    if (M == "JMP" && p.nOps == 1) {
        if (A.kind == OK::Imm && A.farSeg >= 0) {
            Enc e; e.u8v(0xEA); e.u16v((int)A.imm); e.u16v(A.farSeg); push(e); return out;
        }
        if (A.kind == OK::Imm) {
            // SHORT keeps only the 2-byte form, NEAR only the 3-byte one
            if (A.jumpHint != 2) rel8(0xEB, A.imm);
            if (A.jumpHint != 1) {
                const long long d = A.imm - (atOffset + 3);
                Enc e; e.u8v(0xE9); e.u16v((int)d); push(e);
            }
            return out;
        }
        if (A.kind == OK::Mem)   { Enc e; e.u8v(0xFF); if (emitRm(e, A, A.isFar ? 5 : 4)) push(e); return out; }
        if (A.kind == OK::Reg16) { Enc e; e.u8v(0xFF); if (emitRm(e, A, 4)) push(e); return out; }
    }
    if (M == "CALL" && p.nOps == 1) {
        if (A.kind == OK::Imm && A.farSeg >= 0) {
            Enc e; e.u8v(0x9A); e.u16v((int)A.imm); e.u16v(A.farSeg); push(e); return out;
        }
        if (A.kind == OK::Imm) {
            const long long d = A.imm - (atOffset + 3);
            Enc e; e.u8v(0xE8); e.u16v((int)d); push(e);
            return out;
        }
        if (isRm(A)) { Enc e; e.u8v(0xFF); if (emitRm(e, A, A.isFar ? 3 : 2)) push(e); return out; }
    }

    // ---- IN / OUT ---------------------------------------------------------
    if (M == "IN" && p.nOps == 2) {
        const bool wide = (A.kind == OK::Reg16);
        if (B.kind == OK::Imm) { Enc e; e.u8v(wide ? 0xE5 : 0xE4); e.u8v((int)B.imm); push(e); }
        else                   { Enc e; e.u8v(wide ? 0xED : 0xEC); push(e); }
        return out;
    }
    if (M == "OUT" && p.nOps == 2) {
        const bool wide = (B.kind == OK::Reg16);
        if (A.kind == OK::Imm) { Enc e; e.u8v(wide ? 0xE7 : 0xE6); e.u8v((int)A.imm); push(e); }
        else                   { Enc e; e.u8v(wide ? 0xEF : 0xEE); push(e); }
        return out;
    }

    return out;
}

// ---------------------------------------------------------------------------
//  Diagnostics -- why a line has no encoding, in terms of the rule it broke and
//  the form the instruction does take; and the warnings for lines that do
//  assemble but will not behave the way they read.
// ---------------------------------------------------------------------------
namespace {

bool inList(const std::string& m, const char* const* list) {
    for (; *list; ++list) if (m == *list) return true;
    return false;
}

const char* const kAluNames[]   = { "ADD","OR","ADC","SBB","AND","SUB","XOR","CMP", nullptr };
const char* const kShiftNames[] = { "ROL","ROR","RCL","RCR","SHL","SAL","SHR","SAR", nullptr };
const char* const kOneOpNames[] = { "INC","DEC","NOT","NEG","MUL","IMUL","DIV","IDIV", nullptr };
const char* const kLoopNames[]  = { "LOOP","LOOPZ","LOOPE","LOOPNZ","LOOPNE","JCXZ", nullptr };
const char* const kStringOps[]  = { "MOVSB","MOVSW","CMPSB","CMPSW","STOSB","STOSW","LODSB","LODSW",
                                    "SCASB","SCASW", nullptr };
const char* const kNoOperand[]  = { "NOP","CBW","CWD","WAIT","PUSHF","POPF","SAHF","LAHF","MOVSB","MOVSW",
                                    "CMPSB","CMPSW","STOSB","STOSW","LODSB","LODSW","SCASB","SCASW",
                                    "INT3","INTO","IRET","XLAT","XLATB","HLT","CMC","CLC","STC","CLI",
                                    "STI","CLD","STD","LOCK","DAA","DAS","AAA","AAS","BRK","NRG", nullptr };
const char* const kOther[]      = { "MOV","TEST","XCHG","PUSH","POP","LEA","LES","LDS","JMP","CALL",
                                    "RET","RETN","RETF","INT","IN","OUT","ESC","AAM","AAD","REP","REPE",
                                    "REPZ","REPNE","REPNZ", nullptr };

bool isJcc(const std::string& m) {
    for (const CcName& c : CC) if (m == c.n) return true;
    return false;
}

bool knownMnemonic(const std::string& m) {
    return inList(m, kAluNames) || inList(m, kShiftNames) || inList(m, kOneOpNames) ||
           inList(m, kLoopNames) || inList(m, kNoOperand) || inList(m, kOther) || isJcc(m);
}

// The forms an instruction takes, the way a reference card puts them.
std::string usageOf(const std::string& m) {
    if (m == "MOV")  return "MOV reg/mem, reg/mem/imm (not mem, mem) | MOV seg, reg16/mem | MOV reg16/mem, seg";
    if (inList(m, kAluNames)) return m + " reg/mem, reg/mem/imm (not mem, mem)";
    if (m == "TEST") return "TEST reg/mem, reg/imm";
    if (m == "XCHG") return "XCHG reg, reg/mem";
    if (inList(m, kOneOpNames)) return m + " reg/mem (memory needs BYTE or WORD)";
    if (m == "PUSH") return "PUSH reg16 / seg / WORD [mem]";
    if (m == "POP")  return "POP reg16 / seg (not CS) / WORD [mem]";
    if (inList(m, kShiftNames)) return m + " reg/mem, 1 | " + m + " reg/mem, CL";
    if (isJcc(m) || inList(m, kLoopNames)) return m + " label (short: -128..+127 bytes away)";
    if (m == "JMP")  return "JMP label | JMP reg16 / [mem] | JMP FAR [mem] | JMP seg:off";
    if (m == "CALL") return "CALL label | CALL reg16 / [mem] | CALL FAR [mem] | CALL seg:off";
    if (m == "LEA" || m == "LES" || m == "LDS") return m + " reg16, [mem]";
    if (m == "INT")  return "INT n (0..255; CodeGuru's are INT 86h and INT 87h)";
    if (m == "RET" || m == "RETN" || m == "RETF") return m + " | " + m + " n";
    if (m == "IN")   return "IN AL/AX, imm8 | IN AL/AX, DX";
    if (m == "OUT")  return "OUT imm8, AL/AX | OUT DX, AL/AX";
    if (m.rfind("REP", 0) == 0) return m + " MOVSB / STOSW / CMPSB / ... (one string instruction)";
    if (inList(m, kNoOperand)) return m + " (no operands)";
    return m;
}

std::string regName(const Operand& o) {
    if (o.kind == OK::Reg8)  return R8[o.reg & 7];
    if (o.kind == OK::Reg16) return R16[o.reg & 7];
    if (o.kind == OK::Seg)   return SEG4[o.reg & 3];
    return "";
}

int editDistance(const std::string& a, const std::string& b) {
    std::vector<int> prev(b.size() + 1), cur(b.size() + 1);
    for (size_t j = 0; j <= b.size(); ++j) prev[j] = (int)j;
    for (size_t i = 1; i <= a.size(); ++i) {
        cur[0] = (int)i;
        for (size_t j = 1; j <= b.size(); ++j)
            cur[j] = std::min({ prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1) });
        prev.swap(cur);
    }
    int d = prev[b.size()];
    // two letters swapped ("MVO") is one slip, not two
    if (a.size() == b.size()) {
        int diff = 0; size_t at = 0;
        for (size_t i = 0; i < a.size(); ++i) if (a[i] != b[i]) { if (!diff) at = i; ++diff; }
        if (diff == 2 && at + 1 < a.size() && a[at] == b[at + 1] && a[at + 1] == b[at]) d = 1;
    }
    return d;
}

std::string closestMnemonic(const std::string& m) {
    std::string best;
    int bestD = 3;
    auto consider = [&](const char* const* list) {
        for (; *list; ++list) { const int d = editDistance(m, *list); if (d < bestD) { bestD = d; best = *list; } }
    };
    consider(kAluNames); consider(kShiftNames); consider(kOneOpNames); consider(kLoopNames);
    consider(kNoOperand); consider(kOther);
    for (const CcName& c : CC) { const int d = editDistance(m, c.n); if (d < bestD) { bestD = d; best = c.n; } }
    return (m.size() >= 2) ? best : std::string();
}

// The CPU an instruction (or register) first appeared on, when it is not the 8086.
const char* laterCpu(const std::string& m) {
    static const char* const k186[] = { "PUSHA","POPA","ENTER","LEAVE","BOUND","INS","INSB","INSW",
                                        "OUTS","OUTSB","OUTSW", nullptr };
    static const char* const k286[] = { "ARPL","CLTS","LAR","LSL","LGDT","LIDT","LLDT","LMSW","LTR","SGDT",
                                        "SIDT","SLDT","SMSW","STR","VERR","VERW", nullptr };
    static const char* const k386[] = { "MOVZX","MOVSX","BSF","BSR","BT","BTC","BTR","BTS","SHLD","SHRD",
                                        "CWDE","CDQ","PUSHAD","POPAD","PUSHFD","POPFD","IRETD","JECXZ",
                                        "LOOPD","LFS","LGS","LSS","MOVSD","CMPSD","STOSD","LODSD","SCASD",
                                        "INSD","OUTSD", nullptr };
    static const char* const k486[] = { "BSWAP","XADD","CMPXCHG","INVD","WBINVD","INVLPG", nullptr };
    static const char* const kP5[]  = { "CPUID","RDTSC","RDMSR","WRMSR","CMPXCHG8B","SYSENTER","SYSEXIT",
                                        "SYSCALL","SYSRET","UD2","RDPMC", nullptr };
    if (inList(m, k186)) return "the 80186";
    if (inList(m, k286)) return "the 80286";
    if (inList(m, k386) || (m.size() > 3 && m.compare(0, 3, "SET") == 0)) return "the 386";
    if (inList(m, k486)) return "the 486";
    if (inList(m, kP5) || (m.size() > 4 && m.compare(0, 4, "CMOV") == 0)) return "the Pentium";
    return nullptr;
}

// Code written for something other than this 8086: 32-bit registers, later
// CPUs' instructions, the FPU, another assembler's directives. Checked before
// symbols are looked up, so "mov eax, 1" says why rather than "unknown: eax".
std::string foreignReason(const std::string& text) {
    const std::string t = trim(text);
    const std::string U = upper(t);
    const size_t sp = mnemEnd(t);
    const std::string M = upper(sp == std::string::npos ? t : t.substr(0, sp));
    {
        static const char* const k32[] = { "EAX","EBX","ECX","EDX","ESI","EDI","ESP","EBP", nullptr };
        std::string tok;
        for (size_t i = (sp == std::string::npos ? t.size() : sp); i <= U.size(); ++i) {
            const char c = i < U.size() ? U[i] : ' ';
            if (isalnum((unsigned char)c)) { tok.push_back(c); continue; }
            if (inList(tok, k32))
                return tok + " is a 32-bit register (386 and later) - the 8086 has only " + tok.substr(1);
            if (tok == "FS" || tok == "GS")
                return tok + " arrived with the 386 - the 8086's segment registers are ES, CS, SS and DS";
            tok.clear();
        }
    }
    if (const char* cpu = laterCpu(M)) {
        std::string hint;
        if (M == "PUSHA" || M == "POPA") hint = " - push or pop the registers one by one";
        if (M == "MOVZX") hint = " - MOV the byte into the low half and zero the high half (MOV AH, 0)";
        if (M == "LEAVE") hint = " - MOV SP, BP / POP BP";
        return M + " is an instruction of " + cpu + ", not the 8086 - CodeGuru runs 8086 code only" + hint;
    }
    if (M.size() > 1 && M[0] == 'F' && (M == "FLD" || M == "FST" || M == "FSTP" || M == "FADD" ||
        M == "FSUB" || M == "FMUL" || M == "FDIV" || M == "FILD" || M == "FIST" || M == "FINIT" ||
        M == "FWAIT" || M == "FXCH" || M == "FCOM"))
        return M + " is an x87 floating-point instruction - CodeGuru's 8086 has no FPU";
    static const char* const kDirectives[] = { "ORG","SEGMENT","ENDS","ASSUME","PROC","ENDP","END","TIMES",
                                               "RESB","RESW","DD","DQ","DT","ALIGN","BITS","USE16","SECTION",
                                               "GLOBAL","EXTERN","INCLUDE","MACRO","ENDM","OFFSET", nullptr };
    if (inList(M, kDirectives))
        return M + " is an assembler directive this assembler doesn't have - it knows DB, DW, EQU and %define"
               + std::string(M == "ORG" ? " (code is placed wherever the arena puts it)" : "");
    if (M.rfind("REP", 0) == 0 && sp != std::string::npos) {
        const std::string op = upper(trim(t.substr(sp)));
        if (knownMnemonic(op) && !inList(op, kStringOps))
            return M + " repeats a string instruction only (MOVSB, STOSW, CMPSB, ...) - not " + op;
    }
    if (!knownMnemonic(M)) {
        const std::string near = closestMnemonic(M);
        return "unknown instruction: " + M + (near.empty() ? std::string() : " - did you mean " + near + "?");
    }
    return std::string();
}

std::string explainFailure(const std::string& text, int at) {
    const std::string t = trim(text);
    const size_t sp = mnemEnd(t);
    const std::string M = upper(sp == std::string::npos ? t : t.substr(0, sp));
    {
        const std::string why = foreignReason(t);
        if (!why.empty()) return why;
    }

    Parsed p = parseInsn(t);
    if (!p.ok) {
        if (p.err == "too many operands")
            return "too many operands - an 8086 instruction takes at most two: " + usageOf(M);
        return p.err.empty() ? ("can't read the operands of " + M) : p.err;
    }

    const Operand& A = p.a;
    const Operand& B = p.b;
    const std::string use = usageOf(M);
    auto badAddress = [](const Operand& o) {
        if (o.kind != OK::Mem) return false;
        int rm, mod, db;
        return !memRm(o, rm, mod, db);
    };
    if (badAddress(A) || badAddress(B))
        return "not an 8086 address - it can add one of BX / BP, one of SI / DI and a number: "
               "[BX+SI+4], [BP-2], [DI], [1234h] (no [BX+BP], no [SI+DI])";

    // ---- no operands ----------------------------------------------------
    if (inList(M, kNoOperand) && p.nOps > 0) {
        if (inList(M, kStringOps))
            return M + " takes no operands - it always works on DS:SI and ES:DI (and AL/AX)";
        return M + " takes no operands";
    }

    // ---- two-operand data instructions -----------------------------------
    const bool isData = (M == "MOV" || M == "TEST" || M == "XCHG" || inList(M, kAluNames));
    if (isData) {
        if (p.nOps != 2) return M + " takes two operands: " + use;
        if (A.kind == OK::Imm)
            return "the first operand is where " + M + " puts its result, so it can't be a number: " + use;
        if (A.kind == OK::Mem && B.kind == OK::Mem)
            return "memory to memory isn't possible on the 8086 - go through a register "
                   "(MOV AX, [SI] / MOV [DI], AX)";
        const int wa = widthOf(A), wb = widthOf(B);
        if (A.kind != OK::Seg && B.kind != OK::Seg && wa && wb && wa != wb)
            return "operand sizes differ: " + (regName(A).empty() ? std::string("the memory operand") : regName(A)) +
                   " is " + (wa == 1 ? "8" : "16") + "-bit, " +
                   (regName(B).empty() ? std::string("the memory operand") : regName(B)) +
                   " is " + (wb == 1 ? "8" : "16") + "-bit";
        if (A.kind == OK::Seg || B.kind == OK::Seg) {
            if (M != "MOV") return M + " can't use a segment register - only MOV, PUSH and POP can";
            if (A.kind == OK::Seg && B.kind == OK::Seg)
                return "no MOV from one segment register to another - go through a general register "
                       "(MOV AX, " + regName(B) + " / MOV " + regName(A) + ", AX)";
            if (A.kind == OK::Seg && B.kind == OK::Imm)
                return "no MOV of a number into a segment register - MOV AX, " + trim(t.substr(t.rfind(',') + 1)) +
                       " / MOV " + regName(A) + ", AX";
            if ((A.kind == OK::Seg && B.kind == OK::Reg8) || (B.kind == OK::Seg && A.kind == OK::Reg8))
                return "segment registers are 16-bit - use a 16-bit register";
        }
        if (M == "XCHG" && (B.kind == OK::Imm))
            return "XCHG swaps two places, so neither can be a number: " + use;
        if (M == "XCHG" && A.kind == OK::Mem && B.kind == OK::Mem)
            return "XCHG can't swap two memory operands: " + use;
        if (M == "TEST" && B.kind == OK::Mem && A.kind == OK::Mem)
            return "TEST can't compare two memory operands: " + use;
        if (B.kind == OK::Imm && A.kind == OK::Mem && A.sizeHint == 0)
            return "the size is unknown - write BYTE [...] or WORD [...] so " + M +
                   " knows whether it works on a byte or a word";
        if (A.kind == OK::Label || B.kind == OK::Label)
            return "'" + (A.kind == OK::Label ? A.label : B.label) +
                   "' isn't a register, memory or a number here: " + use;
        return "these operands don't fit " + M + ": " + use;
    }

    // ---- one-operand groups -----------------------------------------------
    if (inList(M, kOneOpNames)) {
        if (M == "IMUL" && p.nOps >= 2)
            return "IMUL with more than one operand is the 80186 - on the 8086 it is IMUL reg/mem "
                   "(AX = AL * operand, or DX:AX = AX * operand)";
        if (p.nOps != 1) return M + " takes one operand: " + use;
        if (A.kind == OK::Imm)
            return (M == "MUL" || M == "IMUL" || M == "DIV" || M == "IDIV")
                ? M + " works on AL/AX with a register or memory, not a number - MOV it into a register first"
                : M + " needs a register or memory, not a number";
        if (A.kind == OK::Seg) return M + " can't use a segment register";
        if (A.kind == OK::Mem && A.sizeHint == 0)
            return "the size is unknown - write " + M + " BYTE [...] or " + M + " WORD [...]";
        return use;
    }

    // ---- PUSH / POP ---------------------------------------------------------
    if (M == "PUSH" || M == "POP") {
        if (p.nOps != 1) return M + " takes one operand - one " + M + " per line: " + use;
        if (A.kind == OK::Imm)
            return M == "PUSH" ? "PUSH of a number is the 80186 - on the 8086: MOV AX, n / PUSH AX"
                               : "POP needs somewhere to put the word, not a number: " + use;
        if (A.kind == OK::Reg8) return M + " moves 16-bit words - " + regName(A) + " is 8-bit; use " +
                                       std::string(R16[A.reg & 3]);
        if (M == "POP" && A.kind == OK::Seg && A.reg == 1)
            return "POP CS isn't an 8086 instruction - CS changes with a far JMP, CALL or RETF";
        if (A.kind == OK::Mem && A.sizeHint == 1) return M + " moves words, not bytes: " + use;
        return use;
    }

    // ---- shifts ---------------------------------------------------------------
    if (inList(M, kShiftNames)) {
        if (p.nOps == 1) return M + " needs a count: " + M + " " + trim(t.substr(sp)) + ", 1  or  " + M + " " +
                                trim(t.substr(sp)) + ", CL";
        if (p.nOps != 2) return use;
        if (A.kind == OK::Imm) return "the first operand is what gets shifted, so it can't be a number: " + use;
        if (B.kind == OK::Imm && B.imm != 1)
            return "shifting by " + std::to_string(B.imm) + " in one go is the 80186 - on the 8086 shift by 1, "
                   "or put the count in CL: MOV CL, " + std::to_string(B.imm) + " / " + M + " " +
                   trim(t.substr(sp, t.find(',') - sp)) + ", CL";
        if (B.kind != OK::Imm && !(B.kind == OK::Reg8 && B.reg == 1))
            return "the count must be 1 or CL: " + use;
        if (A.kind == OK::Mem && A.sizeHint == 0)
            return "the size is unknown - write " + M + " BYTE [...] or " + M + " WORD [...]";
        if (A.kind == OK::Seg) return M + " can't shift a segment register";
        return use;
    }

    // ---- branches -------------------------------------------------------------
    if (isJcc(M) || inList(M, kLoopNames)) {
        if (p.nOps != 1) return M + " takes one target: " + use;
        if (A.kind == OK::Imm && A.jumpHint == 2)
            return "a NEAR conditional jump is the 386 - on the 8086 conditional jumps are short only";
        if (A.kind == OK::Imm) {
            const long long d = A.imm - (at + 2);
            if (d < -128 || d > 127) {
                std::string inv;
                if (isJcc(M)) {
                    // the opposite condition jumps over a JMP that can reach
                    for (const CcName& c : CC) if (c.n == M) {
                        for (const CcName& o : CC) if (o.code == (c.code ^ 1)) { inv = o.n; break; }
                        break;
                    }
                    return M + " reaches only -128..+127 bytes; the target is " + std::to_string(d) +
                           " away. Jump over a JMP instead: " + inv + " skip / JMP target / skip:";
                }
                return M + " reaches only -128..+127 bytes; the target is " + std::to_string(d) +
                       " away. Use DEC CX / JNZ over a JMP, or move the loop closer";
            }
        }
        if (A.kind != OK::Imm) return M + " goes to a label or an address, not through a register or memory: " + use;
        return use;
    }
    if (M == "JMP" || M == "CALL") {
        if (p.nOps != 1) return M + " takes one target: " + use;
        if (A.kind == OK::Reg8) return M + " through a register needs a 16-bit one: " + use;
        if (A.kind == OK::Seg) return M + " can't go through a segment register: " + use;
        if (M == "JMP" && A.kind == OK::Imm && A.jumpHint == 1) {
            const long long d = A.imm - (at + 2);
            return "JMP SHORT reaches only -128..+127 bytes; the target is " + std::to_string(d) +
                   " away - drop SHORT";
        }
        return use;
    }
    if (M == "LEA" || M == "LES" || M == "LDS") {
        if (p.nOps != 2) return M + " takes two operands: " + use;
        if (A.kind != OK::Reg16) return M + " loads a 16-bit general register: " + use;
        if (B.kind != OK::Mem)
            return "the second operand of " + M + " must be memory in brackets - " + M + " " + regName(A) +
                   ", [label] or " + M + " " + regName(A) + ", [BX+4]";
        return use;
    }
    if (M == "INT") {
        if (p.nOps != 1 || A.kind != OK::Imm) return use;
        if (A.imm < 0 || A.imm > 255) return "an interrupt number is 0..255: " + use;
        return use;
    }
    if (M.rfind("REP", 0) == 0) {
        if (p.nOps == 0) return M + " needs its string instruction on the same line: " + use;
        return M + " repeats a string instruction only: " + use;
    }
    if (M == "IN" || M == "OUT") return "operands don't fit: " + use;
    return "can't assemble " + t + " - " + use;
}

// A line that assembles but will not do what it reads like: an immediate or a
// displacement cut down to fit, or an opcode CodeGuru's engine refuses.
std::string lineWarning(const std::string& text, const std::vector<u8>& bytes) {
    Parsed p = parseInsn(text);
    if (p.ok && p.nOps == 2 && p.b.kind == OK::Imm && p.b.farSeg < 0) {
        const int w = widthOf(p.a);
        const long long v = p.b.imm;
        char cut[16];
        if (w == 1 && (v < -128 || v > 255)) {
            snprintf(cut, sizeof(cut), "0x%02X", (unsigned)(v & 0xFF));
            return std::to_string(v) + " doesn't fit in 8 bits - it is cut to " + cut;
        }
        if (w == 2 && (v < -32768 || v > 65535)) {
            snprintf(cut, sizeof(cut), "0x%04X", (unsigned)(v & 0xFFFF));
            return std::to_string(v) + " doesn't fit in 16 bits - it is cut to " + cut;
        }
    }
    if (p.ok) {
        for (const Operand* o : { &p.a, &p.b })
            if (o->kind == OK::Mem && (o->disp < -32768 || o->disp > 65535))
                return "the address offset " + std::to_string(o->disp) + " doesn't fit in 16 bits";
        if (p.mnem == "INT" && p.a.kind == OK::Imm && (p.a.imm < 0 || p.a.imm > 255))
            return "an interrupt number is 0..255";
        if (p.mnem == "INT" && p.a.kind == OK::Imm && p.a.imm != 0x86 && p.a.imm != 0x87 && p.a.imm != 0x89)
            return "CodeGuru has no DOS or BIOS: only INT 86h and INT 87h exist (and int 0x89, which is INT 87h for the smart check) - a warrior that runs this dies";
        // memory with no size, and nothing else on the line to give it one
        const Operand* mem = (p.a.kind == OK::Mem) ? &p.a : (p.b.kind == OK::Mem ? &p.b : nullptr);
        const std::string& m = p.mnem;
        // a shift's CL is only its count, so it says nothing about the size
        const bool shift = inList(m, kShiftNames);
        const bool sized = (p.a.kind == OK::Reg8 || p.a.kind == OK::Reg16 || p.a.kind == OK::Seg ||
                            (!shift && (p.b.kind == OK::Reg8 || p.b.kind == OK::Reg16 || p.b.kind == OK::Seg)));
        const bool sizeMatters = m == "MOV" || m == "TEST" || inList(m, kAluNames) ||
                                 inList(m, kOneOpNames) || inList(m, kShiftNames);
        if (mem && mem->sizeHint == 0 && !sized && sizeMatters)
            return "the size isn't given, so this works on a BYTE - write " + m +
                   " BYTE [...] or " + m + " WORD [...] to say which";
    }
    if (!bytes.empty()) {
        u8 win[8] = { 0 };
        for (size_t k = 0; k < bytes.size() && k < 8; ++k) win[k] = bytes[k];
        const Insn in = disasm(win, (int)std::min<size_t>(bytes.size(), 8));
        if (in.invalid)
            return "the engine treats this as an invalid opcode - a warrior that reaches it dies";
        if (in.unsupported)
            return "legal 8086, but CodeGuru's engine doesn't support " + std::string(in.mnem[0] ? in.mnem : "it") +
                   " - a warrior that reaches it dies";
    }
    return std::string();
}

} // namespace

// ---------------------------------------------------------------------------
//  Constant expressions -- the value of an EQU, and of a db / dw item
//
//  Numbers in any of the usual spellings, symbols (labels and EQU names), `$`
//  for the line's own address, + - * / % with the usual precedence, unary
//  minus and brackets. A symbol that is not known yet is reported through
//  `unknown` so the caller can try again once more of the file is resolved.
// ---------------------------------------------------------------------------
namespace {
struct ExprEval {
    const std::string& s;
    const std::map<std::string, int>& syms;
    long long here;
    size_t i = 0;
    bool bad = false;
    std::string unknown;

    ExprEval(const std::string& text, const std::map<std::string, int>& m, long long at)
        : s(text), syms(m), here(at) {}

    void ws() { while (i < s.size() && isspace((unsigned char)s[i])) ++i; }

    long long primary() {
        ws();
        if (i >= s.size()) { bad = true; return 0; }
        const unsigned char c = (unsigned char)s[i];
        if (c == '(') {
            ++i;
            const long long v = sum();
            ws();
            if (i < s.size() && s[i] == ')') ++i; else bad = true;
            return v;
        }
        if (c == '$') { ++i; return here; }
        if (isdigit(c)) {
            size_t j = i;
            while (j < s.size() && isalnum((unsigned char)s[j])) ++j;
            long long v;
            if (!parseNumber(s.substr(i, j - i), v)) bad = true;
            i = j;
            return bad ? 0 : v;
        }
        if (isalpha(c) || c == '_' || c == '.') {
            size_t j = i;
            while (j < s.size() && (isalnum((unsigned char)s[j]) || s[j] == '_' || s[j] == '.')) ++j;
            const std::string tok = s.substr(i, j - i);
            i = j;
            auto it = syms.find(upper(tok));
            if (it != syms.end()) return it->second;
            if (unknown.empty()) unknown = tok;
            return 0;
        }
        bad = true;
        return 0;
    }
    long long unary() {
        ws();
        if (i < s.size() && s[i] == '-') { ++i; return -unary(); }
        if (i < s.size() && s[i] == '+') { ++i; return unary(); }
        return primary();
    }
    long long product() {
        long long v = unary();
        for (;;) {
            ws();
            if (i >= s.size() || (s[i] != '*' && s[i] != '/' && s[i] != '%')) return v;
            const char op = s[i++];
            const long long r = unary();
            if (op == '*') v *= r;
            else if (r == 0) { bad = true; return 0; }       // dividing by zero
            else v = (op == '/') ? v / r : v % r;
        }
    }
    long long sum() {
        long long v = product();
        for (;;) {
            ws();
            if (i >= s.size() || (s[i] != '+' && s[i] != '-')) return v;
            const char op = s[i++];
            const long long r = product();
            v = (op == '+') ? v + r : v - r;
        }
    }
    // The whole text as one expression; false if it is not one.
    bool run(long long& out) {
        out = sum();
        ws();
        if (i != s.size()) bad = true;
        return !bad && unknown.empty();
    }
};

// Whether an EQU's value is an alias for text -- a register, a memory operand,
// a size keyword -- rather than a number. Those are substituted the way
// %define is; everything else is a constant.
bool equIsText(const std::string& v) {
    if (v.find_first_of("[],:'\"") != std::string::npos) return true;
    size_t i = 0;
    while (i < v.size()) {
        const unsigned char c = (unsigned char)v[i];
        if (isdigit(c)) { while (i < v.size() && isalnum((unsigned char)v[i])) ++i; continue; }
        if (!isalpha(c) && c != '_' && c != '.') { ++i; continue; }
        size_t j = i;
        while (j < v.size() && (isalnum((unsigned char)v[j]) || v[j] == '_' || v[j] == '.')) ++j;
        const std::string u = upper(v.substr(i, j - i));
        if (regIndex(R8, 8, u) >= 0 || regIndex(R16, 8, u) >= 0 || regIndex(SEG4, 4, u) >= 0 ||
            u == "BYTE" || u == "WORD" || u == "PTR" || u == "FAR" || u == "SHORT" || u == "NEAR")
            return true;
        // an instruction ("x equ nop"): a known mnemonic encodes to something
        if (i == 0 && !encodeVariants(v, 0).empty()) return true;
        i = j;
    }
    return false;
}
} // namespace

// ---------------------------------------------------------------------------
//  assemble
// ---------------------------------------------------------------------------
AsmResult assemble(const std::string& src, const AsmOptions& opt) {
    AsmResult res;
    res.ok = true;

    // ---- split into lines, peel labels / comments -------------------------
    struct Row {
        int line = 0;
        std::string label;
        std::string body;         // instruction text, may be empty
        std::vector<std::string> dbItems;   // db / dw: one expression per item
        bool dbWide = false;      // dw
        bool isDb = false;
        int  altIndex = -1;       // ;$N  -> pick encoding N of `body`
        int  offset = 0, length = 0;
        // NAME EQU expr: a constant symbol. It takes no room; its value is
        // worked out with the labels, so it may use them, other EQUs and $.
        std::string equName, equExpr;
        bool equDone = false;     // resolved in the current sizing pass
    };
    std::vector<Row> rows;

    // %define NAME value  (%def for short). NASM-style single-line macros:
    // textual, forward-only (a line only sees the defines above it), and
    // case-insensitive like everything else here.
    std::map<std::string, std::string> defines;

    {
        size_t pos = 0;
        int lineNo = 0;
        while (pos <= src.size()) {
            size_t nl = src.find('\n', pos);
            std::string line = src.substr(pos, (nl == std::string::npos ? src.size() : nl) - pos);
            pos = (nl == std::string::npos) ? src.size() + 1 : nl + 1;

            Row r;
            r.line = lineNo++;

            // a ";$" comment names the instruction a db line really encodes
            const size_t alt = line.find(";$");
            std::string altText;
            if (alt != std::string::npos) {
                std::string a = trim(line.substr(alt + 2));
                r.altIndex = 0;
                size_t k = 0;
                while (k < a.size() && isdigit((unsigned char)a[k])) k++;
                if (k > 0) { r.altIndex = atoi(a.substr(0, k).c_str()); a = trim(a.substr(k)); }
                if (r.altIndex < 0) r.altIndex = 0;
                altText = a;
                line = line.substr(0, alt);
            } else {
                const size_t c = line.find(';');
                if (c != std::string::npos) line = line.substr(0, c);
            }

            std::string t = trim(line);

            if (!t.empty() && t[0] == '%') {
                const size_t sp = t.find_first_of(" \t");
                const std::string dir = upper(t.substr(0, sp));
                if (dir == "%DEFINE" || dir == "%DEF") {
                    const std::string rest = (sp == std::string::npos) ? "" : trim(t.substr(sp));
                    const size_t ne = rest.find_first_of(" \t");
                    const std::string name = rest.substr(0, ne);
                    if (!isIdent(name)) {
                        res.diags.push_back({ r.line, true, dir == "%DEF"
                            ? "%def needs a name: %def NAME value"
                            : "%define needs a name: %define NAME value" });
                        res.ok = false;
                    } else {
                        defines[upper(name)] = (ne == std::string::npos) ? "" : trim(rest.substr(ne));
                    }
                } else {
                    res.diags.push_back({ r.line, true, "unknown directive: " + t.substr(0, sp) });
                    res.ok = false;
                }
                rows.push_back(r);
                if (nl == std::string::npos) break;
                continue;
            }

            // NAME EQU value  (also "NAME: EQU value"). A number or an
            // expression becomes a constant symbol; a register, a memory
            // operand or an instruction is an alias, substituted like %define.
            {
                size_t j = 0;
                while (j < t.size() && (isalnum((unsigned char)t[j]) || t[j] == '_' || t[j] == '.')) ++j;
                size_t k = j;
                while (k < t.size() && (t[k] == ' ' || t[k] == '\t')) ++k;
                if (k < t.size() && t[k] == ':') { ++k; while (k < t.size() && (t[k] == ' ' || t[k] == '\t')) ++k; }
                size_t e = k;
                while (e < t.size() && isalpha((unsigned char)t[e])) ++e;
                const bool equ = j > 0 && upper(t.substr(k, e - k)) == "EQU" &&
                                 (e == t.size() || t[e] == ' ' || t[e] == '\t');
                if (equ) {
                    const std::string name = upper(t.substr(0, j));
                    const std::string value = trim(expandDefines(trim(t.substr(e)), defines));
                    if (!isIdent(name) || regIndex(R8, 8, name) >= 0 || regIndex(R16, 8, name) >= 0 ||
                        regIndex(SEG4, 4, name) >= 0) {
                        res.diags.push_back({ r.line, true, "EQU needs a name of its own: NAME equ value" });
                        res.ok = false;
                    } else if (value.empty()) {
                        res.diags.push_back({ r.line, true, "EQU needs a value: " + name + " equ value" });
                        res.ok = false;
                    } else if (equIsText(value)) {
                        defines[name] = value;          // an alias, from here on
                    } else {
                        r.equName = name;
                        r.equExpr = value;
                    }
                    rows.push_back(r);
                    if (nl == std::string::npos) break;
                    continue;
                }
            }

            // label:  -- but ES: / CS: / SS: / DS: are segment-override prefixes
            const size_t colon = t.find(':');
            if (colon != std::string::npos) {
                std::string lab = trim(t.substr(0, colon));
                const std::string labU = upper(lab);
                const bool isSegPrefix = trim(t.substr(colon + 1)).empty() &&
                                         regIndex(SEG4, 4, labU) >= 0;
                if (!lab.empty() && !isSegPrefix &&
                    lab.find_first_of(" \t[]") == std::string::npos) {
                    r.label = labU;
                    t = trim(t.substr(colon + 1));
                }
            }
            r.body = t;
            if (!altText.empty()) {
                // The ";$" note IS the instruction: editing it re-derives the db
                // bytes in front of it, rather than the other way round.
                r.body = altText;
            }
            r.body = trim(expandDefines(r.body, defines));
            rows.push_back(r);
            if (nl == std::string::npos) break;
        }
    }

    // ---- db / dw: the size is fixed by the item count -----------------------
    // The values are worked out at emit time, so an item may be a number, a
    // label, an EQU name or an expression of them.
    for (Row& r : rows) {
        if (r.body.empty()) continue;
        const std::string m = upper(r.body.substr(0, r.body.find_first_of(" \t")));
        if (m != "DB" && m != "DW") continue;
        r.dbItems = splitOps(trim(r.body.substr(2)));
        r.dbWide = (m == "DW");
        r.isDb = true;
        r.length = (int)r.dbItems.size() * (r.dbWide ? 2 : 1);
    }

    // a name defined twice is an error, whichever way it was defined
    {
        std::map<std::string, int> seen;
        for (const Row& r : rows) {
            const std::string& n = !r.equName.empty() ? r.equName : r.label;
            if (n.empty()) continue;
            auto it = seen.find(n);
            if (it != seen.end()) {
                res.diags.push_back({ r.line, true, n + " is already defined on line " +
                                                    std::to_string(it->second + 1) });
                res.ok = false;
            } else seen[n] = r.line;
        }
    }

    // ---- sizing: iterate to a fixed point ---------------------------------
    // A branch is short or near depending on how far its target turns out to
    // be, and that distance depends on every branch size before it. Start with
    // the shortest guess and re-run until nothing moves.
    std::map<std::string, int> labels;

    // Words that look like identifiers but are part of the syntax, so they are
    // never label references.
    auto isReserved = [](const std::string& u) {
        if (regIndex(R8, 8, u) >= 0 || regIndex(R16, 8, u) >= 0 || regIndex(SEG4, 4, u) >= 0)
            return true;
        return u == "BYTE" || u == "WORD" || u == "PTR" || u == "FAR" ||
               u == "SHORT" || u == "NEAR";
    };

    // Substitute every declared label wherever it appears in the operands: a
    // branch target, a plain immediate ("add ax, zombie") or a displacement
    // inside brackets ("[bx + thecopy]"). Branch encodings turn the absolute
    // address back into a displacement themselves, so one rule covers both.
    auto bodyWithLabels = [&](const Row& r, bool haveLabels, std::string* unknown) {
        const std::string& text = r.body;
        const size_t sp = mnemEnd(text);
        if (sp == std::string::npos) return text;

        std::string out = text.substr(0, sp);
        const std::string ops = text.substr(sp);
        size_t i = 0;
        while (i < ops.size()) {
            const unsigned char c0 = (unsigned char)ops[i];
            // "$" is this instruction's own address, as every assembler spells it
            if (c0 == '$') {
                char buf[24];
                snprintf(buf, sizeof(buf), "%d", r.offset);
                out += buf;
                ++i;
                continue;
            }
            if (isdigit(c0)) {
                // a number is one token however it is spelled: "100h" must not
                // read as 100 followed by a symbol called h
                size_t j = i;
                while (j < ops.size() && (isalnum((unsigned char)ops[j]) || ops[j] == '_')) ++j;
                out += ops.substr(i, j - i);
                i = j;
                continue;
            }
            if (!isalpha(c0) && c0 != '_' && c0 != '.') { out.push_back(ops[i++]); continue; }

            size_t j = i;
            while (j < ops.size()) {
                const unsigned char c = (unsigned char)ops[j];
                if (!isalnum(c) && c != '_' && c != '.') break;
                ++j;
            }
            const std::string tok = ops.substr(i, j - i);
            const std::string U = upper(tok);
            long long dummy;
            char buf[32];

            if (isReserved(U) || parseNumber(tok, dummy)) {
                out += tok;                       // syntax, or a number like 0FFh
            } else {
                auto it = labels.find(U);
                if (it != labels.end())      { snprintf(buf, sizeof(buf), "%d", it->second); out += buf; }
                else if (!haveLabels)        { snprintf(buf, sizeof(buf), "%d", r.offset); out += buf; }
                else { out += tok; if (unknown && unknown->empty()) *unknown = tok; }
            }
            i = j;
        }
        out = foldSums(out);

        // A whole operand that is a constant expression foldSums cannot do --
        // "mov ax, SIZE*2", "push (END-START)/2" -- is worked out here. Only
        // those: an operand that already assembles keeps its exact spelling.
        const size_t msp = mnemEnd(out);
        if (msp == std::string::npos) return out;
        std::vector<std::string> ops2 = splitOps(out.substr(msp));
        bool folded = false;
        static const std::map<std::string, int> kNoSyms;
        for (std::string& op : ops2) {
            if (op.find_first_of("*/%()") == std::string::npos) continue;
            if (op.find_first_of("[]:") != std::string::npos) continue;
            ExprEval ev(op, kNoSyms, r.offset);
            long long v;
            if (!ev.run(v)) continue;
            op = std::to_string(v);
            folded = true;
        }
        if (!folded) return out;
        std::string rebuilt = out.substr(0, msp) + " ";
        for (size_t k = 0; k < ops2.size(); ++k) rebuilt += (k ? ", " : "") + ops2[k];
        return rebuilt;
    };

    // EQU values, from the labels as they stand and each other. Repeated until
    // nothing more resolves, so an EQU may use one defined further down. One
    // still unresolved stands in as 0 for sizing; emit reports it.
    auto resolveEqus = [&]() {
        for (Row& r : rows) r.equDone = false;
        for (int pass = 0; pass < 64; ++pass) {
            bool progress = false;
            for (Row& r : rows) {
                if (r.equName.empty() || r.equDone) continue;
                ExprEval ev(r.equExpr, labels, r.offset);
                long long v;
                if (ev.run(v)) { labels[r.equName] = (int)v; r.equDone = true; progress = true; }
            }
            if (!progress) break;
        }
        for (Row& r : rows)
            if (!r.equName.empty() && !r.equDone && !labels.count(r.equName)) labels[r.equName] = 0;
    };

    // encodeVariants is the expensive part, and the sizing passes and the
    // emit ask it the same (text, address) questions over and over: answer
    // each once per assemble() call.
    std::unordered_map<std::string, std::vector<std::vector<u8>>> encMemo;
    auto encodeMemo = [&](const std::string& text, int at) -> const std::vector<std::vector<u8>>& {
        std::string key = text;
        key.push_back((char)1);   // never in source text
        key += std::to_string(at);
        auto it = encMemo.find(key);
        if (it != encMemo.end()) return it->second;
        return encMemo.emplace(std::move(key), encodeVariants(text, at)).first->second;
    };

    for (int iter = 0; iter < 8; ++iter) {
        bool changed = false;
        labels.clear();
        int at = opt.org;
        for (Row& r : rows) {
            if (r.offset != at) { r.offset = at; changed = true; }
            if (!r.label.empty()) labels[r.label] = at;
            at += r.length;
        }
        resolveEqus();
        at = opt.org;
        for (Row& r : rows) {
            r.offset = at;
            if (!r.isDb && !r.body.empty()) {
                std::string unk;
                const std::vector<std::vector<u8>>& v = encodeMemo(bodyWithLabels(r, iter > 0, &unk), at);
                // size the encoding we are actually going to emit: a ";$N" line
                // may pick a longer or shorter one than the canonical form, and
                // sizing the wrong one shifts every label after it
                const size_t pk = (r.altIndex > 0 && (size_t)r.altIndex < v.size())
                                ? (size_t)r.altIndex : 0;
                const int len = v.empty() ? 0 : (int)v[pk].size();
                if (len != r.length) { r.length = len; changed = true; }
            }
            at += r.length;
        }
        if (!changed) break;
    }

    // ---- pass 2: emit -----------------------------------------------------
    // the final labels, and EQUs from them
    {
        labels.clear();
        int at = opt.org;
        for (Row& r : rows) {
            r.offset = at;
            if (!r.label.empty()) labels[r.label] = at;
            at += r.length;
        }
        resolveEqus();
    }
    res.bytes.clear();
    for (Row& r : rows) {
        AsmLine al;
        al.line = r.line;
        al.offset = (int)res.bytes.size() + opt.org;
        if (!r.equName.empty()) {
            if (!r.equDone) {
                ExprEval ev(r.equExpr, labels, r.offset);
                long long v;
                // it evaluates now only because the names it waits on are
                // standing in as 0: they wait on it in turn
                const bool cycle = ev.run(v);
                res.diags.push_back({ r.line, true, !ev.unknown.empty()
                    ? ("unknown symbol in EQU: " + ev.unknown)
                    : cycle ? ("EQU depends on itself: " + r.equName + " equ " + r.equExpr)
                            : ("bad EQU value: " + r.equExpr) });
                res.ok = false;
            }
        } else if (r.isDb) {
            for (const std::string& item : r.dbItems) {
                ExprEval ev(item, labels, al.offset);
                long long v = 0;
                if (!ev.run(v)) {
                    res.diags.push_back({ r.line, true, !ev.unknown.empty()
                        ? ("unknown symbol: " + ev.unknown)
                        : ("bad " + std::string(r.dbWide ? "DW" : "DB") + " value: " + item) });
                    res.ok = false;
                    v = 0;
                }
                res.bytes.push_back((u8)(v & 0xFF));
                if (r.dbWide) res.bytes.push_back((u8)((v >> 8) & 0xFF));
            }
        } else if (!r.body.empty()) {
            std::string unk;
            const std::string text = bodyWithLabels(r, true, &unk);
            const std::vector<std::vector<u8>>& v = encodeMemo(text, al.offset);
            if (!v.empty()) {
                const size_t pick = (r.altIndex > 0 && (size_t)r.altIndex < v.size())
                                  ? (size_t)r.altIndex : 0;
                if (r.altIndex >= (int)v.size()) {
                    // a ";$N" that names an encoding the instruction does not have
                    res.diags.push_back({ r.line, true, v.size() == 1
                        ? ("this instruction has only one encoding - ;$" + std::to_string(r.altIndex) +
                           " can't pick another (write it without the db and ;$)")
                        : ("this instruction has " + std::to_string(v.size()) + " encodings, ;$0 to ;$" +
                           std::to_string(v.size() - 1) + " - ;$" + std::to_string(r.altIndex) + " isn't one") });
                    res.ok = false;
                }
                std::vector<u8> enc = v[pick];
                // `int 0x89`, CWEST's smart-check bomb: a real war only knows
                // INT 87h, so that is what it assembles to; the place is kept
                // for the smart check, which plays a copy with CD 89 there.
                if (enc.size() == 2 && enc[0] == 0xCD && enc[1] == 0x89) {
                    res.smartSites.push_back((int)res.bytes.size() + opt.org);
                    enc[1] = 0x87;
                }
                res.bytes.insert(res.bytes.end(), enc.begin(), enc.end());
                const std::string w = lineWarning(text, enc);
                if (!w.empty()) { res.diags.push_back({ r.line, false, w }); res.hasWarning = true; }
            } else {
                // Every row is visited exactly once here, so this is the place
                // to complain: a line that encodes to nothing used to be
                // dropped without a word, shifting everything after it. Say
                // which rule it broke rather than only that it failed.
                // a word that looks like a missing label is often code for some
                // other CPU ("mov eax, 1", "fld st0") -- say that instead
                const std::string foreign = unk.empty() ? std::string() : foreignReason(r.body);
                res.diags.push_back({ r.line, true,
                                      unk.empty() ? explainFailure(text, al.offset)
                                                  : !foreign.empty() ? foreign
                                                  : ("unknown symbol: " + unk + " - not a label, an EQU or a register") });
                res.ok = false;
            }
        }
        al.length = (int)res.bytes.size() + opt.org - al.offset;
        res.lines.push_back(al);
    }
    for (const Row& r : rows)
        if (!r.equName.empty()) {
            AsmSymbol s;
            s.name = r.equName;
            s.line = r.line;
            s.resolved = r.equDone;
            auto it = labels.find(r.equName);
            if (it != labels.end()) s.value = it->second;
            res.equs.push_back(s);
        }
    return res;
}

} // namespace cw
