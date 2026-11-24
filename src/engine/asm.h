// =============================================================================
//  8086 assembler -- the inverse of src/engine/disasm.cpp, over exactly the same
//  opcode set (including the virtual BRK / NRG / INT 86h,87h).
//
//  Two jobs:
//   * assemble()      turns source text into bytes, reporting per-line offsets
//                     so the editor can line up its assembled preview, plus
//                     errors and "unsupported opcode" warnings.
//   * encodeVariants() lists every legal encoding of one instruction, canonical
//                     first. The disassembler uses it to decide when a byte
//                     sequence is ambiguous: if the canonical encoding of the
//                     decoded text is not the original bytes, the line has to be
//                     written as `db ... ;$N <text>` so it survives a round trip.
// =============================================================================
#pragma once

#include "engine.h"
#include <string>
#include <vector>

namespace cw {

struct AsmDiag {
    int         line = 0;       // 0-based source line
    bool        error = false;  // false => warning (assembles, but kills a warrior)
    std::string text;
};

struct AsmLine {
    int line   = 0;             // 0-based source line
    int offset = 0;             // byte offset of what this line emitted
    int length = 0;             // bytes emitted (0 for labels / blank / comment)
};

// A numeric EQU and what it came to (names are upper case).
struct AsmSymbol {
    std::string name;
    int         line = 0;       // 0-based source line of the EQU
    long long   value = 0;
    bool        resolved = false;
};

struct AsmResult {
    std::vector<u8>      bytes;
    std::vector<AsmLine> lines;
    std::vector<AsmDiag> diags;
    std::vector<AsmSymbol> equs;
    // Where `int 0x89` was written: the offset of its CD. Those assemble to
    // INT 87h (CD 87); the smart check plays a copy with CD 89 there instead.
    std::vector<int> smartSites;
    bool ok = false;
    bool hasWarning = false;
};

struct AsmOptions {
    int  org = 0;               // address the first byte is assembled at
};

AsmResult assemble(const std::string& src, const AsmOptions& opt = AsmOptions());

// Every legal encoding of a single instruction, canonical (what assemble()
// picks) first. Empty if the text does not parse. `atOffset` matters for
// relative branches.
std::vector<std::vector<u8>> encodeVariants(const std::string& insnText, int atOffset = 0);

} // namespace cw
