// Full-file listing round trip: bytes -> source -> bytes, for every warrior.
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
    if (dirs.empty()) { dirs.push_back("tests/data/nightly-2014/survivors"); dirs.push_back("CGX-engine/zombies"); }

    int files = 0, ok = 0, bad = 0;
    long long dbLines = 0, srcLines = 0, labels = 0;
    std::vector<std::string> failures;

    for (const std::string& d : dirs) {
        if (!fs::is_directory(d)) continue;
        for (const auto& e : fs::directory_iterator(d)) {
            if (!e.is_regular_file()) continue;
            if (e.path().filename().string().find('.') != std::string::npos) continue;
            std::ifstream f(e.path(), std::ios::binary);
            std::vector<u8> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            if (b.empty()) continue;
            if (b.size() > 512) b.resize(512);
            files++;

            ListOptions lo;
            Listing L = makeListing(b.data(), (int)b.size(), lo);
            for (const ListLine& ln : L.lines) {
                srcLines++;
                if (ln.isLabel) labels++;
                if (ln.ambiguous || ln.invalid) dbLines++;
            }
            AsmResult r = assemble(L.text);
            if (r.bytes == b) ok++;
            else {
                bad++;
                if (failures.size() < 6) {
                    char buf[220];
                    size_t k = 0;
                    while (k < b.size() && k < r.bytes.size() && b[k] == r.bytes[k]) k++;
                    snprintf(buf, sizeof(buf), "%-30s in %zu out %zu  first diff at %zu",
                             e.path().filename().string().c_str(), b.size(), r.bytes.size(), k);
                    failures.push_back(buf);
                }
            }
        }
    }
    std::printf("files              : %d\n", files);
    std::printf("  byte-exact       : %d\n", ok);
    std::printf("  mismatched       : %d\n", bad);
    std::printf("source lines       : %lld  (%lld labels, %lld db)\n", srcLines, labels, dbLines);
    std::printf("db lines per file  : %.1f\n", files ? (double)dbLines / files : 0.0);
    for (const std::string& s : failures) std::printf("  FAIL %s\n", s.c_str());
    return bad ? 1 : 0;
}
