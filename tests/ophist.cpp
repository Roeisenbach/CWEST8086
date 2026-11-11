// Opcode histogram over a real tournament -- which instructions the engine
// actually spends its life on. Reads the byte at CS:IP at the start of every
// cycle, which is the opcode about to run.
//
//   ophist <survivorDir> <games> <combo>
#include "engine.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
namespace fs = std::filesystem;
using namespace cw;

static std::vector<u8> readFile(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::vector<u8> v((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (v.size() > 512) v.resize(512);
    return v;
}

static std::vector<WarriorGroup> loadField(const std::string& dir) {
    std::vector<fs::path> files;
    if (fs::exists(dir))
        for (const auto& e : fs::directory_iterator(dir))
            if (e.is_regular_file() && !e.path().has_extension()) files.push_back(e.path());
    std::sort(files.begin(), files.end());
    std::vector<WarriorGroup> out;
    WarriorGroup cur; bool open = false;
    for (const fs::path& p : files) {
        std::string n = p.filename().string();
        std::vector<u8> code = readFile(p);
        if (code.empty()) continue;
        if (n.back() == '1') {
            cur = WarriorGroup(); cur.name = n.substr(0, n.size() - 1);
            cur.warriors.push_back({n, code, WarriorType::SURVIVOR_1, 0, -1});
            open = true;
        } else if (n.back() == '2' && open) {
            cur.warriors.push_back({n, code, WarriorType::SURVIVOR_2, 0, -1});
            out.push_back(cur); open = false;
        } else {
            WarriorGroup g; g.name = n;
            g.warriors.push_back({n, code, WarriorType::SURVIVOR, 0, -1});
            out.push_back(g); open = false;
        }
    }
    return out;
}

static const char* NAME[256] = {0};
static void names() {
    for (int i = 0; i < 256; i++) NAME[i] = "";
    NAME[0x88] = "mov rm8,r8";   NAME[0x89] = "mov rm16,r16";
    NAME[0x8A] = "mov r8,rm8";   NAME[0x8B] = "mov r16,rm16";
    NAME[0xEB] = "jmp short";    NAME[0xE9] = "jmp near";
    NAME[0xE8] = "call near";    NAME[0xC3] = "ret";
    NAME[0xAA] = "stosb";        NAME[0xAB] = "stosw";
    NAME[0xF3] = "rep";          NAME[0xF2] = "repne";
    NAME[0xCD] = "int";          NAME[0xE2] = "loop";
    NAME[0x80] = "grp1 rm8,i8";  NAME[0x81] = "grp1 rm16,i16";
    NAME[0x83] = "grp1 rm16,i8"; NAME[0xF6] = "grp3 rm8";
    NAME[0xF7] = "grp3 rm16";    NAME[0xFE] = "inc/dec rm8";
    NAME[0xFF] = "grp5 rm16";    NAME[0xC6] = "mov rm8,i8";
    NAME[0xC7] = "mov rm16,i16"; NAME[0x8D] = "lea";
    NAME[0x86] = "xchg r8";      NAME[0x87] = "xchg r16";
    NAME[0xD0] = "shift rm8,1";  NAME[0xD1] = "shift rm16,1";
    NAME[0xD2] = "shift rm8,cl"; NAME[0xD3] = "shift rm16,cl";
    NAME[0x9B] = "wait/NRG";     NAME[0x90] = "nop";
    for (int i = 0x40; i <= 0x47; i++) NAME[i] = "inc r16";
    for (int i = 0x48; i <= 0x4F; i++) NAME[i] = "dec r16";
    for (int i = 0x50; i <= 0x57; i++) NAME[i] = "push r16";
    for (int i = 0x58; i <= 0x5F; i++) NAME[i] = "pop r16";
    for (int i = 0xB0; i <= 0xB7; i++) NAME[i] = "mov r8,i8";
    for (int i = 0xB8; i <= 0xBF; i++) NAME[i] = "mov r16,i16";
    for (int i = 0x70; i <= 0x7F; i++) NAME[i] = "jcc short";
}

struct Hist : CycleObserver {
    War* war = nullptr;
    unsigned long long n[256] = {0};
    unsigned long long total = 0;
    void onCycleBegin(int w) override {
        const Warrior& wr = war->warrior(w);
        const RMAddr a(wr.cpu().cs, wr.cpu().ip);
        n[war->core().data()[a.linear]]++;
        total++;
    }
    void onCycleEnd(int) override {}
    void onTurnEnd(int) override {}
    void onDeath(int, Fault) override {}
};

int main(int argc, char** argv) {
    const std::string dir = (argc > 1) ? argv[1] : "CGX-engine/survivors";
    const int games       = (argc > 2) ? std::atoi(argv[2]) : 60;
    const int combo       = (argc > 3) ? std::atoi(argv[3]) : 4;
    names();

    std::vector<WarriorGroup> field = loadField(dir);
    if ((int)field.size() < combo) { std::printf("need groups\n"); return 2; }

    Hist h;
    War war(nullptr, nullptr, 2);
    h.war = &war;

    for (int k = 0; k < games; ++k) {
        JavaRandom pick((s64)((u64)(s64)k * 6364136223846793005ULL + 1442695040888963407ULL));
        std::vector<int> idx((size_t)field.size());
        for (size_t i = 0; i < idx.size(); ++i) idx[i] = (int)i;
        for (int i = 0; i < combo; ++i) std::swap(idx[(size_t)i], idx[(size_t)(i + pick.nextInt((s32)(idx.size() - (size_t)i)))]);
        std::vector<const WarriorGroup*> ptrs;
        for (int i = 0; i < combo; ++i) ptrs.push_back(&field[(size_t)idx[(size_t)i]]);

        war.reset();
        war.setSeed(k);
        war.setObserver(&h);
        try { war.loadWarriorGroups(ptrs); } catch (const CpuFault&) { continue; }
        war.runToEnd(War::MAX_ROUND);
    }

    std::vector<int> order(256);
    for (int i = 0; i < 256; i++) order[(size_t)i] = i;
    std::sort(order.begin(), order.end(), [&](int a, int b) { return h.n[a] > h.n[b]; });
    std::printf("%llu opcodes over %d wars\n", h.total, games);
    double cum = 0;
    for (int r = 0; r < 28; r++) {
        const int op = order[(size_t)r];
        if (!h.n[op]) break;
        const double pc = 100.0 * (double)h.n[op] / (double)h.total;
        cum += pc;
        std::printf("  %02X  %-14s %10llu  %5.2f%%   cum %5.1f%%\n",
                    op, NAME[op], h.n[op], pc, cum);
    }
    return 0;
}
