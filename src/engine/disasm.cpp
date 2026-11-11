// =============================================================================
//  8086 disassembler -- see disasm.h
// =============================================================================
#include "disasm.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace cw {
namespace {

const char* const R8 [8] = { "AL","CL","DL","BL","AH","CH","DH","BH" };
const char* const R16[8] = { "AX","CX","DX","BX","SP","BP","SI","DI" };
const char* const SEG[8] = { "ES","CS","SS","DS","ES","CS","SS","DS" };
const char* const ALU[8] = { "ADD","OR","ADC","SBB","AND","SUB","XOR","CMP" };
const char* const SHF[8] = { "ROL","ROR","RCL","RCR","SHL","SHR","???","SAR" };
const char* const CC [16]= { "JO","JNO","JB","JNB","JZ","JNZ","JBE","JA",
                             "JS","JNS","JP","JNP","JL","JGE","JLE","JG" };
// base expression for each mod != 3 r/m encoding
const char* const RM [8] = { "BX+SI","BX+DI","BP+SI","BP+DI","SI","DI","BP","BX" };

struct Cur {
    const u8* p;
    int avail;
    int at = 0;
    u8 byte() { u8 v = (at < avail) ? p[at] : (u8)0; ++at; return v; }
    u16 word() { u16 lo = byte(); u16 hi = byte(); return (u16)(lo | (hi << 8)); }
};

void hex16(char* out, size_t n, u16 v) { snprintf(out, n, "0x%04X", v); }
void hex8 (char* out, size_t n, u8  v) { snprintf(out, n, "0x%02X", v); }

struct Modrm { u8 mod, reg, rm; char text[32]; };

// Reads the mod/reg/rm byte plus any displacement, and renders the memory form.
Modrm modrm(Cur& c) {
    Modrm m{};
    const u8 b = c.byte();
    m.mod = (u8)((b >> 6) & 3);
    m.reg = (u8)((b >> 3) & 7);
    m.rm  = (u8)(b & 7);
    m.text[0] = 0;

    if (m.mod == 3) return m;                    // register form; caller picks 8/16 bit

    if (m.mod == 0 && m.rm == 6) {               // direct address
        char h[12]; hex16(h, sizeof(h), c.word());
        snprintf(m.text, sizeof(m.text), "[%s]", h);
        return m;
    }
    if (m.mod == 0) { snprintf(m.text, sizeof(m.text), "[%s]", RM[m.rm]); return m; }
    if (m.mod == 1) {
        const int d = (int)(s8)c.byte();
        // a zero disp8 is not the shortest encoding (mod 0 is) -- except for
        // [BP], where mod 1 is the only way to say it
        const char* kw = (d == 0 && m.rm != 6) ? "byte " : "";
        if (d < 0) snprintf(m.text, sizeof(m.text), "[%s-%s0x%02X]", RM[m.rm], kw, (unsigned)(-d));
        else       snprintf(m.text, sizeof(m.text), "[%s+%s0x%02X]", RM[m.rm], kw, (unsigned)d);
        return m;
    }
    const int d = (int)(s16)c.word();
    // a disp16 that would have fitted in a byte is likewise not the default
    const char* kw = (d >= -128 && d <= 127) ? "word " : "";
    if (d < 0) snprintf(m.text, sizeof(m.text), "[%s-%s0x%04X]", RM[m.rm], kw, (unsigned)(-d));
    else       snprintf(m.text, sizeof(m.text), "[%s+%s0x%04X]", RM[m.rm], kw, (unsigned)d);
    return m;
}

const char* rmOp(const Modrm& m, bool wide) {
    return (m.mod == 3) ? (wide ? R16[m.rm] : R8[m.rm]) : m.text;
}

// Same, but memory gets an explicit BYTE / WORD. Used wherever the other
// operand is an immediate or absent, so nothing else implies the width.
const char* rmOpW(const Modrm& m, bool wide, char* buf, size_t n) {
    if (m.mod == 3) return wide ? R16[m.rm] : R8[m.rm];
    snprintf(buf, n, "%s %s", wide ? "WORD" : "BYTE", m.text);
    return buf;
}

void set0(Insn& i, const char* mnem) {
    snprintf(i.mnem, sizeof(i.mnem), "%s", mnem);
    snprintf(i.text, sizeof(i.text), "%s", mnem);
}

void set(Insn& i, const char* mnem, const char* fmt, ...) {
    snprintf(i.mnem, sizeof(i.mnem), "%s", mnem);
    char ops[48];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(ops, sizeof(ops), fmt, ap);
    va_end(ap);
    snprintf(i.text, sizeof(i.text), "%s %s", mnem, ops);
}

} // namespace

Insn disasm(const u8* p, int avail, bool upper, long long atOffset, bool mergeNrg) {
    Insn i;
    Cur c{ p, avail, 0 };
    // set() a branch operand: absolute when we know where we are, else relative
    auto branch = [&](const char* mn, int d) {
        if (atOffset >= 0) set(i, mn, "%lld", (long long)(atOffset + c.at + d));
        else               set(i, mn, "%+d", d);
    };
    const u8 op = c.byte();

    switch (op >> 4) {

    // ---- 0x00..0x3F : ALU forms, segment push/pop, prefixes, BCD ----
    case 0x0: case 0x1: case 0x2: case 0x3: {
        const u8 lo = (u8)(op & 7);
        if (lo <= 5) {
            const char* mn = ALU[(op >> 3) & 7];
            const bool wide = (lo & 1) != 0;
            if (lo <= 3) {
                Modrm m = modrm(c);
                const char* r = wide ? R16[m.reg] : R8[m.reg];
                if (lo <= 1) set(i, mn, "%s, %s", rmOp(m, wide), r);
                else         set(i, mn, "%s, %s", r, rmOp(m, wide));
            } else if (lo == 4) {
                char h[12]; hex8(h, sizeof(h), c.byte());  set(i, mn, "AL, %s", h);
            } else {
                char h[12]; hex16(h, sizeof(h), c.word()); set(i, mn, "AX, %s", h);
            }
            break;
        }
        if (op == 0x0F) { i.invalid = true; set0(i, "(bad)"); break; }
        if (op == 0x26 || op == 0x2E || op == 0x36 || op == 0x3E) {
            i.unsupported = true;                       // segment prefix: engine refuses it
            { char t[8]; snprintf(t, sizeof(t), "%s:", SEG[(op >> 3) & 3]); set0(i, t); }
            break;
        }
        if (op == 0x27 || op == 0x2F || op == 0x37 || op == 0x3F) {
            static const char* const bcd[4] = { "DAA", "DAS", "AAA", "AAS" };
            i.unsupported = true; set0(i, bcd[(op >> 3) & 3]);
            break;
        }
        set(i, (lo == 6) ? "PUSH" : "POP", "%s", SEG[(op >> 3) & 3]);
        break;
    }

    // ---- 0x40..0x5F ----
    case 0x4: set(i, (op < 0x48) ? "INC"  : "DEC", "%s", R16[op & 7]); break;
    case 0x5: set(i, (op < 0x58) ? "PUSH" : "POP", "%s", R16[op & 7]); break;

    case 0x6: i.invalid = true; set0(i, "(bad)"); break;

    // ---- 0x70..0x7F : conditional short jumps ----
    case 0x7: {
        const int d = (int)(s8)c.byte();
        i.control = true;
        branch(CC[op & 15], d);
        break;
    }

    case 0x8: {
        if (op <= 0x83) {                                // grp1 r/m, imm
            Modrm m = modrm(c);
            const bool wide = (op != 0x80 && op != 0x82);
            char h[16], rb[40];
            if (op == 0x81) {
                const u16 v = (u16)c.word();
                // the short 0x83 form could have encoded this, so the long one
                // has to be spelled out -- size keyword before the immediate
                if ((u16)(s16)(s8)v == v) snprintf(h, sizeof(h), "WORD 0x%04X", v);
                else                      snprintf(h, sizeof(h), "0x%04X", v);
            }
            else if (op == 0x83) hex16(h, sizeof(h), (u16)(s16)(s8)c.byte());  // sign-extended
            else                 hex8(h, sizeof(h), c.byte());
            set(i, ALU[m.reg], "%s, %s", rmOpW(m, wide, rb, sizeof(rb)), h);
            break;
        }
        switch (op) {
        case 0x84: case 0x85: {
            Modrm m = modrm(c); const bool w = (op & 1) != 0;
            set(i, "TEST", "%s, %s", rmOp(m, w), w ? R16[m.reg] : R8[m.reg]);
            break; }
        case 0x86: case 0x87: {
            Modrm m = modrm(c); const bool w = (op & 1) != 0;
            set(i, "XCHG", "%s, %s", w ? R16[m.reg] : R8[m.reg], rmOp(m, w));
            break; }
        case 0x88: case 0x89: case 0x8A: case 0x8B: {
            Modrm m = modrm(c); const bool w = (op & 1) != 0;
            const char* r = w ? R16[m.reg] : R8[m.reg];
            if (op <= 0x89) set(i, "MOV", "%s, %s", rmOp(m, w), r);
            else            set(i, "MOV", "%s, %s", r, rmOp(m, w));
            break; }
        case 0x8C: { Modrm m = modrm(c); set(i, "MOV", "%s, %s", rmOp(m, true), SEG[m.reg]); break; }
        case 0x8D: { Modrm m = modrm(c);
            if (m.mod == 3) { i.invalid = true; set0(i, "(bad)"); }
            else set(i, "LEA", "%s, %s", R16[m.reg], m.text);
            break; }
        case 0x8E: { Modrm m = modrm(c); set(i, "MOV", "%s, %s", SEG[m.reg], rmOp(m, true)); break; }
        default:   { Modrm m = modrm(c); set(i, "POP", "%s", rmOp(m, true)); break; }
        }
        break;
    }

    case 0x9: {
        switch (op) {
        case 0x90: set0(i, "NOP"); break;
        case 0x91: case 0x92: case 0x93: case 0x94:
        case 0x95: case 0x96: case 0x97:
            set(i, "XCHG", "%s, AX", R16[op & 7]); break;
        case 0x98: set0(i, "CBW"); break;
        case 0x99: set0(i, "CWD"); break;
        case 0x9A: { const u16 off = c.word(); const u16 sg = c.word();
            i.control = true; set(i, "CALL", "0x%04X:0x%04X", sg, off); break; }
        case 0x9B: {                                     // virtual NRG
            const u8 nx = c.byte();
            if (nx != 0x9B) { i.unsupported = true; set0(i, "WAIT"); break; }
            // the engine takes NRG two bytes at a time; source spells it as four
            if (mergeNrg && avail >= 4 && p[2] == 0x9B && p[3] == 0x9B) { c.byte(); c.byte(); }
            set0(i, "NRG");
            break; }
        case 0x9C: set0(i, "PUSHF"); break;
        case 0x9D: set0(i, "POPF");  break;
        case 0x9E: set0(i, "SAHF");  break;
        default:   set0(i, "LAHF");  break;
        }
        break;
    }

    case 0xA: {
        char h[12];
        switch (op) {
        case 0xA0: hex16(h, sizeof(h), c.word()); set(i, "MOV", "AL, [%s]", h); break;
        case 0xA1: hex16(h, sizeof(h), c.word()); set(i, "MOV", "AX, [%s]", h); break;
        case 0xA2: hex16(h, sizeof(h), c.word()); set(i, "MOV", "[%s], AL", h); break;
        case 0xA3: hex16(h, sizeof(h), c.word()); set(i, "MOV", "[%s], AX", h); break;
        case 0xA4: set0(i, "MOVSB"); break;
        case 0xA5: set0(i, "MOVSW"); break;
        case 0xA6: set0(i, "CMPSB"); break;
        case 0xA7: set0(i, "CMPSW"); break;
        case 0xA8: hex8(h, sizeof(h), c.byte());  set(i, "TEST", "AL, %s", h); break;
        case 0xA9: hex16(h, sizeof(h), c.word()); set(i, "TEST", "AX, %s", h); break;
        case 0xAA: set0(i, "STOSB"); break;
        case 0xAB: set0(i, "STOSW"); break;
        case 0xAC: set0(i, "LODSB"); break;
        case 0xAD: set0(i, "LODSW"); break;
        case 0xAE: set0(i, "SCASB"); break;
        default:   set0(i, "SCASW"); break;
        }
        break;
    }

    case 0xB: {
        char h[12];
        if (op < 0xB8) { hex8(h, sizeof(h), c.byte());  set(i, "MOV", "%s, %s", R8 [op & 7], h); }
        else           { hex16(h, sizeof(h), c.word()); set(i, "MOV", "%s, %s", R16[op & 7], h); }
        break;
    }

    case 0xC: {
        switch (op) {
        case 0xC0: case 0xC1: case 0xC8: case 0xC9:
            i.invalid = true; set0(i, "(bad)"); break;
        case 0xC2: { const u16 n = c.word(); i.control = true; set(i, "RET", "0x%04X", n); break; }
        case 0xC3: i.control = true; set0(i, "RET"); break;
        case 0xC4: case 0xC5: { Modrm m = modrm(c);
            if (m.mod == 3) { i.invalid = true; set0(i, "(bad)"); }
            else set(i, (op == 0xC4) ? "LES" : "LDS", "%s, %s", R16[m.reg], m.text);
            break; }
        case 0xC6: { Modrm m = modrm(c); char h[12], rb[40]; hex8(h, sizeof(h), c.byte());
            set(i, "MOV", "%s, %s", rmOpW(m, false, rb, sizeof(rb)), h); break; }
        case 0xC7: { Modrm m = modrm(c); char h[12], rb[40]; hex16(h, sizeof(h), c.word());
            set(i, "MOV", "%s, %s", rmOpW(m, true, rb, sizeof(rb)), h); break; }
        case 0xCA: { const u16 n = c.word(); i.control = true; set(i, "RETF", "0x%04X", n); break; }
        case 0xCB: i.control = true; set0(i, "RETF"); break;
        case 0xCC: i.control = true; i.unsupported = true; set0(i, "INT3"); break;
        case 0xCD: { const u8 n = c.byte(); i.control = true;
            if (n != 0x86 && n != 0x87) i.unsupported = true;   // only 86h/87h exist here
            set(i, "INT", "0x%02X", n); break; }
        case 0xCE: i.control = true; i.unsupported = true; set0(i, "INTO"); break;
        default:   i.control = true; set0(i, "IRET"); break;
        }
        break;
    }

    case 0xD: {
        if (op <= 0xD3) {
            Modrm m = modrm(c);
            const bool wide = (op & 1) != 0;
            if (m.reg == 6) { i.invalid = true; set0(i, "(bad)"); break; }
            char rb[40];
            if (op <= 0xD1) set(i, SHF[m.reg], "%s, 1",  rmOpW(m, wide, rb, sizeof(rb)));
            else            set(i, SHF[m.reg], "%s, CL", rmOpW(m, wide, rb, sizeof(rb)));
            break;
        }
        switch (op) {
        case 0xD4: case 0xD5: { i.unsupported = true;
            const u8 n = c.byte();
            const char* mn = (op == 0xD4) ? "AAM" : "AAD";
            if (n == 0x0A) set0(i, mn); else set(i, mn, "0x%02X", n);
            break; }
        case 0xD6: i.invalid = true; set0(i, "(bad)"); break;
        case 0xD7: set0(i, "XLATB"); break;
        default:   i.unsupported = true; set(i, "ESC", "0x%02X", op); break;  // 0xD8..0xDF, FPU
        }
        break;
    }

    case 0xE: {
        switch (op) {
        case 0xE0: case 0xE1: case 0xE2: case 0xE3: {
            static const char* const lp[4] = { "LOOPNZ", "LOOPZ", "LOOP", "JCXZ" };
            const int d = (int)(s8)c.byte();
            i.control = true;
            branch(lp[op & 3], d);
            break; }
        case 0xE4: case 0xE5: { i.unsupported = true; char h[12]; hex8(h, sizeof(h), c.byte());
            set(i, "IN", "%s, %s", (op & 1) ? "AX" : "AL", h); break; }
        case 0xE6: case 0xE7: { i.unsupported = true; char h[12]; hex8(h, sizeof(h), c.byte());
            set(i, "OUT", "%s, %s", h, (op & 1) ? "AX" : "AL"); break; }
        case 0xE8: { const int d = (int)(s16)c.word(); i.control = true; branch("CALL", d); break; }
        case 0xE9: { const int d = (int)(s16)c.word(); i.control = true; branch("JMP", d); break; }
        case 0xEA: { const u16 off = c.word(); const u16 sg = c.word();
            i.control = true; set(i, "JMP", "0x%04X:0x%04X", sg, off); break; }
        case 0xEB: { const int d = (int)(s8)c.byte(); i.control = true;
            if (d == 0) set0(i, "BRK");                  // virtual breakpoint: jmp $+2
            else        branch("JMP", d);
            break; }
        case 0xEC: case 0xED: i.unsupported = true;
            set(i, "IN",  "%s, DX", (op & 1) ? "AX" : "AL"); break;
        default:   i.unsupported = true;
            set(i, "OUT", "DX, %s", (op & 1) ? "AX" : "AL"); break;
        }
        break;
    }

    default: {                                            // 0xF0..0xFF
        switch (op) {
        case 0xF0: i.unsupported = true; set0(i, "LOCK"); break;
        case 0xF1: i.invalid = true; set0(i, "(bad)"); break;
        case 0xF2: case 0xF3: {                           // REP prefix + one string op
            const u8 nx = c.byte();
            const char* pre = (op == 0xF2) ? "REPNZ" : "REP";
            const char* sop = nullptr;
            switch (nx) {
            case 0xA4: sop = "MOVSB"; break;  case 0xA5: sop = "MOVSW"; break;
            case 0xA6: sop = "CMPSB"; break;  case 0xA7: sop = "CMPSW"; break;
            case 0xAA: sop = "STOSB"; break;  case 0xAB: sop = "STOSW"; break;
            case 0xAC: sop = "LODSB"; break;  case 0xAD: sop = "LODSW"; break;
            case 0xAE: sop = "SCASB"; break;  case 0xAF: sop = "SCASW"; break;
            default: break;
            }
            i.control = true;                             // rewinds IP while CX != 0
            if (!sop) { i.invalid = true; set(i, pre, "%s", "(bad)"); }
            else      set(i, pre, "%s", sop);
            break; }
        case 0xF4: i.unsupported = true; set0(i, "HLT"); break;
        case 0xF5: set0(i, "CMC"); break;
        case 0xF6: case 0xF7: {
            Modrm m = modrm(c);
            const bool wide = (op & 1) != 0;
            char h[12], rb[40];
            const char* R = rmOpW(m, wide, rb, sizeof(rb));
            switch (m.reg) {
            case 0:
                if (wide) hex16(h, sizeof(h), c.word()); else hex8(h, sizeof(h), c.byte());
                set(i, "TEST", "%s, %s", R, h);
                break;
            case 1: i.invalid = true; set0(i, "(bad)"); break;
            case 2: set(i, "NOT", "%s", R); break;
            case 3: set(i, "NEG", "%s", R); break;
            case 4: set(i, "MUL", "%s", R); break;
            case 5: i.unsupported = true; set(i, "IMUL", "%s", R); break;
            case 6: set(i, "DIV", "%s", R); break;
            default: i.unsupported = true; set(i, "IDIV", "%s", R); break;
            }
            break; }
        case 0xF8: set0(i, "CLC"); break;
        case 0xF9: set0(i, "STC"); break;
        case 0xFA: set0(i, "CLI"); break;
        case 0xFB: set0(i, "STI"); break;
        case 0xFC: set0(i, "CLD"); break;
        case 0xFD: set0(i, "STD"); break;
        case 0xFE: { Modrm m = modrm(c); char rb[40];
            const char* R = rmOpW(m, false, rb, sizeof(rb));
            if (m.reg == 0)      set(i, "INC", "%s", R);
            else if (m.reg == 1) set(i, "DEC", "%s", R);
            else { i.invalid = true; set0(i, "(bad)"); }
            break; }
        default: { Modrm m = modrm(c); char rb[40];        // 0xFF
            const char* R = rmOpW(m, true, rb, sizeof(rb));
            switch (m.reg) {
            case 0: set(i, "INC", "%s", R); break;
            case 1: set(i, "DEC", "%s", R); break;
            case 2: i.control = true; set(i, "CALL", "%s", R); break;
            case 3: i.control = true;
                if (m.mod == 3) { i.invalid = true; set0(i, "(bad)"); }
                else set(i, "CALL", "FAR %s", m.text);
                break;
            case 4: i.control = true; set(i, "JMP", "%s", R); break;
            case 5: i.control = true;
                if (m.mod == 3) { i.invalid = true; set0(i, "(bad)"); }
                else set(i, "JMP", "FAR %s", m.text);
                break;
            case 6: set(i, "PUSH", "%s", R); break;
            default: i.invalid = true; set0(i, "(bad)"); break;
            }
            break; }
        }
        break;
    }
    }

    i.len = c.at < 1 ? 1 : c.at;

    if (!upper) {
        for (char* q = i.mnem; *q; ++q) if (*q >= 'A' && *q <= 'Z') *q = (char)(*q - 'A' + 'a');
        for (char* q = i.text; *q; ++q) if (*q >= 'A' && *q <= 'Z') *q = (char)(*q - 'A' + 'a');
    }
    return i;
}

} // namespace cw
