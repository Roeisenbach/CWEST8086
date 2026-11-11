// Re-framing a listing from an arbitrary byte must still assemble back to the
// original machine code: the cut instruction becomes a db, everything after it
// is decoded afresh, and the bytes have to come out identical.
#include "listing.h"
#include "asm.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
namespace fs = std::filesystem;
using namespace cw;

int main(int argc, char** argv) {
    std::vector<std::string> dirs;
    for (int i = 1; i < argc; i++) dirs.push_back(argv[i]);
    if (dirs.empty()) dirs.push_back("tests/data/nightly-2014/survivors");

    int files = 0, cuts = 0, bad = 0;
    for (const std::string& d : dirs) {
        if (!fs::exists(d)) continue;
        for (const auto& ent : fs::directory_iterator(d)) {
            if (!ent.is_regular_file()) continue;
            if (ent.path().has_extension()) continue;
            std::ifstream f(ent.path(), std::ios::binary);
            std::vector<u8> code((std::istreambuf_iterator<char>(f)),
                                  std::istreambuf_iterator<char>());
            if (code.empty()) continue;
            if (code.size() > 512) code.resize(512);
            files++;

            // cut at every byte in the file
            for (int at = 1; at < (int)code.size(); at++) {
                ListOptions lo;
                lo.syncAt = at;
                Listing L = makeListing(code.data(), (int)code.size(), lo);
                AsmResult r = assemble(L.text);
                cuts++;
                std::vector<u8> back(r.bytes.begin(), r.bytes.end());
                if (!r.ok || back != code) {
                    if (bad < 6)
                        std::printf("  MISMATCH %s cut at %04X: %s, %d bytes vs %d\n",
                                    ent.path().filename().string().c_str(), (unsigned)at,
                                    r.ok ? "assembled" : "FAILED",
                                    (int)back.size(), (int)code.size());
                    bad++;
                }
            }
        }
    }
    std::printf("files %d   cuts tested %d   mismatched %d\n", files, cuts, bad);
    return bad ? 1 : 0;
}
