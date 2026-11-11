// =============================================================================
//  Assembler / disassembler round-trip harness.
//
//  For every real warrior binary: decode it instruction by instruction, ask the
//  assembler to encode the decoded text again, and compare. Bytes that do NOT
//  come back identical are exactly the ambiguous encodings the spec wants
//  written as `db ... ;$N <text>`, so this also tells us how common they are and
//  whether an alternative index exists for each one.
//
//  build:
//    g++ -O2 -std=c++17 tests/rttest.cpp src/engine/disasm.cpp src/engine/asm.cpp -o build/tests/rttest.exe
// =============================================================================
#include "asm.h"
#include "disasm.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace cw;

static constexpr int kWin = 6;   // longest instruction the engine can execute

int main(int argc, char** argv) {
    std::vector<std::string> dirs;
    for (int i = 1; i < argc; i++) dirs.push_back(argv[i]);
    if (dirs.empty()) { dirs.push_back("tests/data/nightly-2014/survivors"); dirs.push_back("CGX-engine/zombies"); }

    long long files = 0, insns = 0, exact = 0, ambiguous = 0, noEncoding = 0;
    long long unsupported = 0, invalid = 0;
    std::map<std::string, long long> ambByMnem;
    std::map<int, long long> altIndexHist;
    std::vector<std::string> examples;

    for (const std::string& d : dirs) {
        if (!fs::is_directory(d)) continue;
        for (const auto& ent : fs::directory_iterator(d)) {
            if (!ent.is_regular_file()) continue;
            if (ent.path().filename().string().find('.') != std::string::npos) continue;
            std::ifstream f(ent.path(), std::ios::binary);
            std::vector<u8> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            if (b.empty()) continue;
            if (b.size() > 512) b.resize(512);
            files++;

            int off = 0;
            while (off < (int)b.size()) {
                u8 win[kWin] = {0};
                for (int k = 0; k < kWin; k++)
                    win[k] = (off + k < (int)b.size()) ? b[(size_t)(off + k)] : (u8)0;
                const Insn in = disasm(win, kWin, true, off, true);
                insns++;
                if (in.unsupported) unsupported++;
                if (in.invalid) { invalid++; off += in.len; continue; }

                std::vector<std::vector<u8>> v = encodeVariants(in.text, off);
                const std::vector<u8> orig(win, win + in.len);

                if (v.empty()) {
                    noEncoding++;
                    if (examples.size() < 12)
                        examples.push_back("no encoding: " + std::string(in.text));
                    off += in.len;
                    continue;
                }
                if (v[0] == orig) { exact++; }
                else {
                    ambiguous++;
                    ambByMnem[in.mnem]++;
                    int idx = -1;
                    for (size_t k = 0; k < v.size(); k++) if (v[k] == orig) { idx = (int)k; break; }
                    altIndexHist[idx]++;
                    if (idx < 0 && examples.size() < 12) {
                        char buf[160];
                        int n = snprintf(buf, sizeof(buf), "unmatched: %-24s orig", in.text);
                        for (int k = 0; k < in.len; k++)
                            n += snprintf(buf + n, sizeof(buf) - (size_t)n, " %02X", orig[(size_t)k]);
                        n += snprintf(buf + n, sizeof(buf) - (size_t)n, "  canon");
                        for (u8 x : v[0]) n += snprintf(buf + n, sizeof(buf) - (size_t)n, " %02X", x);
                        examples.push_back(buf);
                    }
                }
                off += in.len;
            }
        }
    }

    std::printf("files            : %lld\n", files);
    std::printf("instructions     : %lld\n", insns);
    std::printf("  exact round-trip: %lld  (%.2f%%)\n", exact, 100.0 * (double)exact / (double)insns);
    std::printf("  ambiguous       : %lld  (%.2f%%)\n", ambiguous, 100.0 * (double)ambiguous / (double)insns);
    std::printf("  no encoding     : %lld\n", noEncoding);
    std::printf("  invalid opcode  : %lld\n", invalid);
    std::printf("  unsupported     : %lld\n", unsupported);

    std::printf("\nambiguous by mnemonic:\n");
    std::vector<std::pair<std::string, long long>> byM(ambByMnem.begin(), ambByMnem.end());
    std::sort(byM.begin(), byM.end(), [](auto& a, auto& b) { return a.second > b.second; });
    for (size_t i = 0; i < byM.size() && i < 14; i++)
        std::printf("  %-8s %lld\n", byM[i].first.c_str(), byM[i].second);

    std::printf("\nwhich alternative the original was (-1 = not produced at all):\n");
    for (auto& kv : altIndexHist) std::printf("  index %-3d %lld\n", kv.first, kv.second);

    if (!examples.empty()) {
        std::printf("\nexamples needing attention:\n");
        for (const std::string& e : examples) std::printf("  %s\n", e.c_str());
    }
    return (noEncoding || altIndexHist.count(-1)) ? 1 : 0;
}
