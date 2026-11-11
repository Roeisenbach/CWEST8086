// =============================================================================
//  8086 assembler -- see asm.h
// =============================================================================
#include "asm.h"

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

        if      (u == "BX") o.bx = true;
        else if (u == "BP") o.bp = true;
        else if (u == "SI") o.si = true;
        else if (u == "DI") o.di = true;
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
        if (sh == 6 || M != SHF[sh]) continue;
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
        if (M != cc.n || p.nOps != 1 || A.kind != OK::Imm) continue;
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
            rel8(0xEB, A.imm);
            const long long d = A.imm - (atOffset + 3);
            Enc e; e.u8v(0xE9); e.u16v((int)d); push(e);
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
                std::vector<std::vector<u8>> v = encodeVariants(bodyWithLabels(r, iter > 0, &unk), at);
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
            std::vector<std::vector<u8>> v = encodeVariants(text, al.offset);
            if (!v.empty()) {
                const size_t pick = (r.altIndex > 0 && (size_t)r.altIndex < v.size())
                                  ? (size_t)r.altIndex : 0;
                res.bytes.insert(res.bytes.end(), v[pick].begin(), v[pick].end());
            } else {
                // Every row is visited exactly once here, so this is the place
                // to complain: a line that encodes to nothing used to be
                // dropped without a word, shifting everything after it.
                res.diags.push_back({ r.line, true,
                                      unk.empty() ? ("cannot assemble: " + r.body)
                                                  : ("unknown symbol: " + unk) });
                res.ok = false;
            }
        }
        al.length = (int)res.bytes.size() + opt.org - al.offset;
        res.lines.push_back(al);
    }
    return res;
}

} // namespace cw
