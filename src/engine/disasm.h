// =============================================================================
//  8086 disassembler -- covers exactly the opcode map the engine implements
//  (tests/ref/java/il/co/codeguru/corewars8086/cpu/Cpu.java), including the virtual
//  opcodes: NRG (9B 9B), BRK (EB 00) and INT 86h / 87h.
//
//  `len` is what the CPU actually consumes, so it can be checked against the
//  engine's IP advance -- see the disasm length check in selftest.cpp.
//  Opcodes the engine refuses (invalid / unimplemented / unsupported) still
//  decode and report their length; they are flagged so the UI can warn on them
//  rather than silently hiding them.
// =============================================================================
#pragma once

#include "engine.h"

namespace cw {

struct Insn {
    int  len         = 1;
    bool invalid     = false;   // InvalidOpcodeException
    bool unsupported = false;   // Unsupported/Unimplemented -- decodes, but kills the warrior
    bool control     = false;   // jmp/call/ret/loop/int/rep: IP does not simply advance by len
    char mnem[10]    = {0};     // "MOV"
    char text[64]    = {0};     // "MOV AX, 0x1234"
};

// Decodes one instruction from `p` (at most `avail` bytes readable).
// `atOffset` >= 0 makes relative branches print their absolute target, which
// is what the assembler and the label pass need; -1 keeps the +/-disp form.
// `mergeNrg` folds a run of four WAITs into one NRG, which is how the spec
// spells it in source. The engine executes NRG two bytes at a time, so the
// timeline and the length check leave it off.
Insn disasm(const u8* p, int avail, bool upper = true, long long atOffset = -1,
            bool mergeNrg = false);

} // namespace cw
