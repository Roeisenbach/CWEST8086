// =============================================================================
//  Listing generator -- see listing.h
// =============================================================================
#include "listing.h"
#include "asm.h"
#include "disasm.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>

namespace cw {
namespace {

// One decoded instruction, before labels are applied.
struct Step {
    int  off = 0;
    int  len = 1;
    Insn in;
    bool ambiguous = false;
    bool raw = false;
    int  altIndex = 0;
    int  altCount = 1;
};

// How a branch target is reached, worst (most specific) wins.
enum TgtKind { TK_LABEL = 0, TK_LOOP = 1, TK_PRCD = 2 };

bool isLoopMnem(const char* m) {
    return !strcmp(m, "LOOP") || !strcmp(m, "LOOPZ") || !strcmp(m, "LOOPE")
        || !strcmp(m, "LOOPNZ") || !strcmp(m, "LOOPNE") || !strcmp(m, "JCXZ");
}

// Only these actually take a code address. RET / INT / IRET are "control" too,
// but their operand is a pop count or an interrupt number -- turning those into
// labels was silently producing unassemblable source.
bool isBranchMnem(const char* m) {
    if (m[0] == 'J') return true;                      // Jcc, JMP, JCXZ
    return !strcmp(m, "CALL") || !strncmp(m, "LOOP", 4);
}

// The trailing operand of a branch, if it is a bare number we produced.
bool branchTarget(const Insn& in, long long& target) {
    if (!in.control || !isBranchMnem(in.mnem)) return false;
    const char* sp = strchr(in.text, ' ');
    if (!sp) return false;
    const char* p = sp + 1;
    if (strchr(p, ':') || strchr(p, '[')) return false;    // far ptr / indirect
    char* end = nullptr;
    const long long v = strtoll(p, &end, 0);
    if (end == p || (end && *end != 0)) return false;
    target = v;
    return true;
}

std::string lower(std::string s) {
    for (char& c : s) c = (char)tolower((unsigned char)c);
    return s;
}

} // namespace

int Listing::lineAtOffset(int off) const {
    for (size_t i = 0; i < lines.size(); i++)
        if (!lines[i].isLabel && lines[i].offset <= off &&
            off < lines[i].offset + lines[i].length)
            return (int)i;
    return -1;
}

Listing makeListing(const u8* code, int len, const ListOptions& opt) {
    Listing out;
    if (!code || len <= 0) return out;

    // ---- decode linearly, deciding ambiguity by asking the assembler --------
    std::vector<Step> steps;
    for (int off = 0; off < len; ) {
        u8 win[8] = {0};
        for (int k = 0; k < 6; k++) win[k] = (off + k < len) ? code[off + k] : (u8)0;

        Step st;
        st.off = off;
        st.in  = disasm(win, 6, true, opt.org + off, /*mergeNrg=*/true);
        st.len = std::min(st.in.len, len - off);

        // Re-framing from a byte in the middle of an instruction: the bytes in
        // front of it stop being an instruction at all, so they are emitted as
        // data and decoding restarts on the boundary the user asked for.
        if (opt.syncAt > off && opt.syncAt < off + st.len) {
            st.len = opt.syncAt - off;
            st.raw = true;
            steps.push_back(st);
            off += st.len;
            continue;
        }

        if (!st.in.invalid) {
            const std::vector<u8> orig(win, win + st.len);
            std::vector<std::vector<u8>> v = encodeVariants(st.in.text, opt.org + off);
            st.altCount = (int)std::max<size_t>(1, v.size());
            if (v.empty() || v[0] != orig) {
                st.ambiguous = true;
                st.altIndex = -1;
                for (size_t k = 0; k < v.size(); k++) if (v[k] == orig) { st.altIndex = (int)k; break; }
            }
        }
        steps.push_back(st);
        off += st.len;
    }

    // The instruction a byte offset falls inside, so a branch into the middle of
    // one can still be written against a label.
    auto anchorOf = [&](int rel) -> int {
        if (rel == len) return len;                     // one past the end: "end:"
        for (const Step& st : steps)
            if (rel >= st.off && rel < st.off + st.len) return st.off;
        return -1;
    };

    // ---- work out which offsets deserve a label ----------------------------
    std::map<int, int> kindAt;                 // offset -> TgtKind
    if (opt.labels) {
        for (const Step& s : steps) {
            long long t;
            if (s.ambiguous || s.raw || !branchTarget(s.in, t)) continue;
            const int rel = (int)(t - opt.org);
            if (rel < 0 || rel > len) continue;                 // outside the code
            if (rel == s.off) continue;         // branches to itself: written "$"
            const int anchor = anchorOf(rel);
            if (anchor < 0) continue;
            const int k = !strcmp(s.in.mnem, "CALL") ? TK_PRCD
                        : isLoopMnem(s.in.mnem)      ? TK_LOOP
                                                     : TK_LABEL;
            auto it = kindAt.find(anchor);
            if (it == kindAt.end() || k > it->second) kindAt[anchor] = k;
        }
    }

    // name them, numbering each kind in address order
    std::map<int, std::string> nameAt;
    {
        int nLoop = 0, nPrcd = 0, nLabel = 0;
        for (auto& kv : kindAt) {
            const int off = kv.first;
            std::string nm;
            if (off == 0)            nm = "start";
            else if (off == len)     nm = "end";
            else {
                char buf[24];
                if (kv.second == TK_PRCD)      snprintf(buf, sizeof(buf), "prcd%d",  ++nPrcd);
                else if (kv.second == TK_LOOP) snprintf(buf, sizeof(buf), "loop%d",  ++nLoop);
                else                            snprintf(buf, sizeof(buf), "label%d", ++nLabel);
                nm = buf;
            }
            nameAt[off] = nm;
        }
    }

    // ---- emit ---------------------------------------------------------------
    auto caseFix = [&](std::string s) { return opt.upper ? s : lower(s); };

    for (const Step& s : steps) {
        auto lab = nameAt.find(s.off);
        if (lab != nameAt.end()) {
            ListLine L;
            L.isLabel = true;
            L.offset = s.off;
            L.text = lab->second + ":";
            out.lines.push_back(L);
        }

        ListLine L;
        L.offset = s.off;
        L.length = s.len;
        L.unsupported = s.in.unsupported;
        L.invalid = s.in.invalid;
        L.ambiguous = s.ambiguous;
        L.raw = s.raw;
        L.altIndex = s.altIndex;
        L.altCount = s.altCount;

        if (s.raw) {
            // just data -- there is no instruction here to name
            std::string t = std::string("    ") + (opt.upper ? "DB " : "db ");
            char b[8];
            for (int k = 0; k < s.len; k++) {
                snprintf(b, sizeof(b), "0x%02X", code[s.off + k]);
                if (k) t += ", ";
                t += b;
            }
            L.unsupported = false;
            L.invalid = false;
            L.text = t;
        } else if (s.ambiguous || s.in.invalid) {
            // db is authoritative; the ;$N comment says what it means and which
            // of the encodings it is
            std::string t = std::string("    ") + (opt.upper ? "DB " : "db ");
            char b[8];
            for (int k = 0; k < s.len; k++) {
                snprintf(b, sizeof(b), "0x%02X", code[s.off + k]);
                if (k) t += ", ";
                t += b;
            }
            if (!s.in.invalid && s.altIndex >= 0) {
                char n[16];
                snprintf(n, sizeof(n), " ;$%d ", s.altIndex);
                t += n;
                t += caseFix(s.in.text);
            } else if (!s.in.invalid) {
                // nothing we can write re-creates these bytes, so this is only a
                // hint: a plain comment, not a ";$" the editor could cycle
                t += "  ; ";
                t += caseFix(s.in.text);
            }
            L.text = t;
        } else {
            std::string body = s.in.text;
            long long tgt;
            if (opt.labels && branchTarget(s.in, tgt)) {
                const int rel = (int)(tgt - opt.org);
                const char* sp = strchr(s.in.text, ' ');
                const std::string mn(s.in.text, (size_t)(sp - s.in.text));
                if (rel == s.off) {
                    // an instruction that jumps to itself is "$", not a label
                    // wrapped round one line
                    body = mn + " $";
                } else {
                    const int anchor = anchorOf(rel);
                    auto it = (anchor >= 0) ? nameAt.find(anchor) : nameAt.end();
                    if (it != nameAt.end()) {
                        body = mn + " " + it->second;
                        // landing inside an instruction: name the instruction
                        // and count on from it
                        if (rel != anchor) body += "+" + std::to_string(rel - anchor);
                    }
                }
            }
            L.text = "    " + caseFix(body);
        }
        out.lines.push_back(L);
    }

    // a label sitting one past the last byte
    auto endLab = nameAt.find(len);
    if (endLab != nameAt.end()) {
        ListLine L;
        L.isLabel = true;
        L.offset = len;
        L.text = endLab->second + ":";
        out.lines.push_back(L);
    }

    for (size_t i = 0; i < out.lines.size(); i++) {
        if (i) out.text += "\n";
        out.text += out.lines[i].text;
    }
    return out;
}

} // namespace cw
