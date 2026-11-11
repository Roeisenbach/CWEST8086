// Assemble the files named on the command line and report every diagnostic,
// then dump the bytes. Used to chase down source the reference engine accepts
// but this assembler does not.
#include "asm.h"
#include "disasm.h"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <vector>

int main(int argc, char** argv)
{
    int bad = 0;
    for (int a = 1; a < argc; a++) {
        std::ifstream f(argv[a]);
        if (!f) { std::printf("cannot open %s\n", argv[a]); bad++; continue; }
        std::stringstream ss; ss << f.rdbuf();
        const std::string src = ss.str();

        cw::AsmResult r = cw::assemble(src);
        std::printf("=== %s : %s, %d bytes, %d diag(s)\n",
                    argv[a], r.ok ? "OK" : "FAILED", (int)r.bytes.size(), (int)r.diags.size());

        // line text, for readable diagnostics
        std::vector<std::string> lines;
        { std::stringstream ls(src); std::string l; while (std::getline(ls, l)) lines.push_back(l); }

        for (const cw::AsmDiag& d : r.diags) {
            std::printf("  %-5s line %3d: %-42s | %s\n",
                        d.error ? "ERROR" : "warn", d.line + 1, d.text.c_str(),
                        (d.line >= 0 && d.line < (int)lines.size()) ? lines[(size_t)d.line].c_str() : "");
            if (d.error) bad++;
        }
        if (!r.ok) continue;

        std::printf("  bytes:");
        for (size_t i = 0; i < r.bytes.size(); i++) std::printf(" %02X", r.bytes[i]);
        std::printf("\n");

        // drop the machine code beside the source so it can be fed to a war
        std::string out = argv[a];
        const size_t dot = out.find_last_of('.');
        if (dot != std::string::npos) out = out.substr(0, dot);
        std::ofstream ob(out.c_str(), std::ios::binary);
        if (!r.bytes.empty()) ob.write((const char*)r.bytes.data(), (std::streamsize)r.bytes.size());
    }
    return bad ? 1 : 0;
}
