// =============================================================================
//  Listing generator -- machine code to editable source.
//
//  Turns a warrior's bytes into assembly the editor can show and the assembler
//  can turn straight back into the same bytes:
//
//    * instructions that re-assemble exactly are written normally;
//    * ones that do not (several encodings spell the same instruction) become
//      `db 0x8B, 0xFE ;$1 MOV DI, SI` -- the db is authoritative, the ;$N
//      comment records which alternative it is so Alt can cycle them;
//    * jump / call / loop targets inside the code become labels: `start:` at
//      offset 0, `end:` one past the last byte, `loop1:` for loop targets,
//      `prcd1:` for call targets, `label1:` for everything else. Targets that
//      land outside the code stay numeric.
//
//  Every line carries its byte offset and length so the editor can highlight
//  the current instruction and colour unsupported ones.
// =============================================================================
#pragma once

#include "engine.h"
#include <string>
#include <vector>

namespace cw {

struct ListLine {
    int  offset      = -1;      // byte offset, or -1 for a label-only line
    int  length      = 0;
    bool isLabel     = false;
    bool unsupported = false;   // decodes, but the engine kills the warrior
    bool invalid     = false;
    bool ambiguous   = false;   // written as db ... ;$N
    bool raw         = false;   // plain db: the bytes a re-frame cut in half
    int  altIndex    = 0;       // which encoding, when ambiguous
    int  altCount    = 1;       // how many encodings exist
    std::string text;           // the source line, without a trailing newline
};

struct Listing {
    std::vector<ListLine> lines;
    std::string text;           // all the lines joined with newlines
    // offset -> index into `lines`, for "which line is IP on"
    int lineAtOffset(int off) const;
};

struct ListOptions {
    bool upper = true;          // mnemonic case
    bool labels = true;         // synthesise start: / end: / loop# / prcd# / label#
    int  org = 0;
    // Force an instruction boundary here: whatever instruction straddles this
    // offset is cut, its leading bytes become a plain db, and decoding picks up
    // again from syncAt. -1 decodes straight through, as usual.
    int  syncAt = -1;
};

Listing makeListing(const u8* code, int len, const ListOptions& opt = ListOptions());

} // namespace cw
