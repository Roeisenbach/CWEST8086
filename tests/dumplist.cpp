// Print the listing for one warrior binary, so the generated source can be eyeballed.
#include "listing.h"
#include <cstdio>
#include <fstream>
#include <vector>
using namespace cw;

int main(int argc, char** argv) {
    if (argc < 2) { std::printf("usage: dumplist <binary>\n"); return 2; }
    std::ifstream f(argv[1], std::ios::binary);
    std::vector<u8> code((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (code.empty()) { std::printf("empty\n"); return 2; }
    if (code.size() > 512) code.resize(512);
    ListOptions lo;
    Listing L = makeListing(code.data(), (int)code.size(), lo);
    std::printf("%s\n", L.text.c_str());
    return 0;
}
