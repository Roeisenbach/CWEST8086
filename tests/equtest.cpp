// EQU: each case assembles to exactly the bytes of the same program written
// out by hand, or fails with the expected message.
#include "asm.h"
#include <cstdio>
#include <string>
#include <vector>
using namespace cw;

static std::string hex(const std::vector<u8>& b) {
    std::string s; char t[4];
    for (u8 c : b) { snprintf(t, sizeof(t), "%02X ", c); s += t; }
    return s;
}
static std::string diags(const AsmResult& r) {
    std::string s;
    for (const AsmDiag& d : r.diags) s += "[line " + std::to_string(d.line + 1) + "] " + d.text + "  ";
    return s;
}

int main() {
    struct Same { const char* name; const char* withEqu; const char* plain; };
    const Same same[] = {
        { "number",            "size equ 5\nmov ax, size",                      "mov ax, 5" },
        { "upper case",        "SIZE EQU 0x10\nmov ax, SIZE",                   "mov ax, 16" },
        { "colon form",        "x: equ 7\nmov cl, x",                            "mov cl, 7" },
        { "hex value",         "big equ 0FFFFh\nmov dx, big",                   "mov dx, 0xFFFF" },
        { "expression",        "size equ 5\nstep equ size*2+1\nmov ax, step",   "mov ax, 11" },
        { "brackets",          "a equ (3+4)*2\nmov ax, a",                      "mov ax, 14" },
        { "operand expr",      "size equ 5\nmov ax, size*2",                    "mov ax, 10" },
        { "operand sum",       "size equ 5\nmov ax, size+2",                    "mov ax, 7" },
        { "used before def",   "mov cx, len\nstart: nop\nnop\nend:\nlen equ end - start",
                               "mov cx, 2\nnop\nnop" },
        { "equ uses later equ","mov ax, b\nb equ a+1\na equ 4",                 "mov ax, 5" },
        { "branch target",     "start: nop\nnop\ntarget equ start + 1\njmp target",
                               "nop\nnop\njmp 1" },
        { "dollar",            "nop\nnop\nnop\nhere equ $\njmp here",           "nop\nnop\nnop\njmp 3" },
        { "in memory operand", "off equ 4\nmov ax, [bx+off]",                   "mov ax, [bx+4]" },
        { "alias register",    "p equ bx\nmov ax, p",                           "mov ax, bx" },
        { "alias memory",      "slot equ [bx+si+4]\nmov al, slot",             "mov al, [bx+si+4]" },
        { "db / dw with equ",  "size equ 5\ndb size, size*2\ndw size\nend:\ndw end",
                               "db 5, 10\ndw 5\ndw 4" },
        { "db label",          "start: nop\ndb start+1",                        "nop\ndb 1" },
        { "comment after",     "size equ 3 ; three\nmov ax, size",              "mov ax, 3" },
        { "define inside",     "%define N 6\nsize equ N*2\nmov ax, size",       "mov ax, 12" },
        { "negative",          "m equ -1\nmov ax, m",                           "mov ax, -1" },
        { "plain still works", "start: mov ax, 0\nadd ax, 0FFh\njmp start",     "mov ax, 0\nadd ax, 0FFh\njmp 0" },
    };
    struct Bad { const char* name; const char* src; const char* want; };
    const Bad bad[] = {
        { "unknown symbol",    "x equ nope+1\nmov ax, x",                       "unknown symbol in EQU: nope" },
        { "register name",     "ax equ 5",                                      "EQU needs a name of its own" },
        { "no value",          "x equ",                                         "EQU needs a value" },
        { "defined twice",     "x equ 1\nx equ 2",                              "already defined" },
        { "label and equ",     "x: nop\nx equ 2",                               "already defined" },
        { "divide by zero",    "x equ 4/0",                                     "bad EQU value" },
        { "circular",          "a equ b\nb equ a\nmov ax, a",                   "depends on itself" },
    };

    int fails = 0;
    for (const Same& c : same) {
        const AsmResult a = assemble(c.withEqu), b = assemble(c.plain);
        const bool ok = a.ok && b.ok && a.bytes == b.bytes;
        if (!ok) fails++;
        printf("%-4s %-20s %s%s\n", ok ? "ok" : "FAIL", c.name, hex(a.bytes).c_str(),
               ok ? "" : ("  want " + hex(b.bytes) + " " + diags(a) + diags(b)).c_str());
    }
    for (const Bad& c : bad) {
        const AsmResult a = assemble(c.src);
        const bool ok = !a.ok && diags(a).find(c.want) != std::string::npos;
        if (!ok) fails++;
        printf("%-4s %-20s %s\n", ok ? "ok" : "FAIL", c.name, diags(a).c_str());
    }
    printf("\n%d failure(s)\n", fails);
    return fails ? 1 : 0;
}
