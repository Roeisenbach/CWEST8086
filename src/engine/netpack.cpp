#include "netpack.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <unordered_map>

namespace cw {

// ---- helpers ---------------------------------------------------------------

// Names carry spaces and whatever else the file system allowed, so they are
// the last field on their line and run to the end of it; only the newline and
// the carriage return are unsafe, and those never survive a file name anyway.
static std::string cleanName(const std::string& s) {
    std::string out;
    for (char c : s) if (c != '\n' && c != '\r') out += c;
    return out;
}

static const char* typeWord(WarriorType t) {
    switch (t) {
        case WarriorType::SURVIVOR:   return "s";
        case WarriorType::SURVIVOR_1: return "s1";
        case WarriorType::SURVIVOR_2: return "s2";
        case WarriorType::ZOMBIE:     return "z";
        case WarriorType::ZOMBIE_H:   return "zh";
    }
    return "s";
}

static bool typeFromWord(const std::string& w, WarriorType& t) {
    if (w == "s")  { t = WarriorType::SURVIVOR;   return true; }
    if (w == "s1") { t = WarriorType::SURVIVOR_1; return true; }
    if (w == "s2") { t = WarriorType::SURVIVOR_2; return true; }
    if (w == "z")  { t = WarriorType::ZOMBIE;     return true; }
    if (w == "zh") { t = WarriorType::ZOMBIE_H;   return true; }
    return false;
}

static void hexOut(std::string& s, const std::vector<u8>& bytes) {
    static const char* digits = "0123456789abcdef";
    s.reserve(s.size() + bytes.size() * 2);
    for (u8 b : bytes) { s += digits[b >> 4]; s += digits[b & 15]; }
}

static bool hexIn(const std::string& s, std::vector<u8>& out) {
    if (s.size() & 1) return false;
    out.clear();
    out.reserve(s.size() / 2);
    for (size_t i = 0; i < s.size(); i += 2) {
        int v = 0;
        for (int j = 0; j < 2; ++j) {
            const char c = s[i + j];
            int d;
            if (c >= '0' && c <= '9') d = c - '0';
            else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
            else return false;
            v = (v << 4) | d;
        }
        out.push_back((u8)v);
    }
    return true;
}

// Split off the first `n` space-separated words; the remainder (which may hold
// spaces) is returned in `rest`.
static bool words(const std::string& line, int n, std::vector<std::string>& w, std::string& rest) {
    w.clear();
    size_t i = 0;
    for (int k = 0; k < n; ++k) {
        while (i < line.size() && line[i] == ' ') ++i;
        if (i >= line.size()) return false;
        size_t j = line.find(' ', i);
        if (j == std::string::npos) j = line.size();
        w.push_back(line.substr(i, j - i));
        i = j;
    }
    while (i < line.size() && line[i] == ' ') ++i;
    rest = line.substr(i);
    return true;
}

static void eachLine(const std::string& text, const std::function<void(const std::string&)>& fn) {
    size_t i = 0;
    while (i < text.size()) {
        size_t j = text.find('\n', i);
        if (j == std::string::npos) j = text.size();
        std::string line = text.substr(i, j - i);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) fn(line);
        i = j + 1;
    }
}

// ---- field pack ------------------------------------------------------------

std::string writeFieldPack(const FieldPack& f) {
    std::string s;
    s += "cwest-field 1\n";
    if (!f.title.empty()) s += "title " + cleanName(f.title) + "\n";
    char buf[160];
    std::snprintf(buf, sizeof buf, "seed %lld\npair %lld\ncombo %d\nrounds %d\nzspeed %d\nstride %d\nphase %d\ngames %d\nrun %d\n",
                  (long long)f.seed, (long long)f.pairingSeed, f.comboSize, f.maxRounds,
                  f.zombieSpeed, f.stride, f.phase, f.games, f.runId);
    s += buf;
    for (const WarriorGroup& g : f.groups) {
        s += "group " + cleanName(g.name) + "\n";
        for (const WarriorData& w : g.warriors) {
            // w <type> <speed> <hex> <name...>
            s += "w ";
            s += typeWord(w.type);
            std::snprintf(buf, sizeof buf, " %d ", w.speed);
            s += buf;
            if (w.code.empty()) s += "-"; else hexOut(s, w.code);
            s += " " + cleanName(w.name) + "\n";
        }
    }
    s += "end\n";
    return s;
}

bool parseFieldPack(const std::string& text, FieldPack& out, std::string& why) {
    FieldPack f;
    bool header = false, ended = false, bad = false;
    std::vector<std::string> w;
    std::string rest;
    eachLine(text, [&](const std::string& line) {
        if (bad || ended) return;
        if (!header) {
            if (line.rfind("cwest-field ", 0) != 0) { why = "not a field pack"; bad = true; }
            header = true;
            return;
        }
        if (!words(line, 1, w, rest)) return;
        const std::string key = w[0];
        if (key == "end")        { ended = true; return; }
        if (key == "title")      { f.title = rest; return; }
        if (key == "seed")       { f.seed = std::strtoll(rest.c_str(), nullptr, 10); return; }
        if (key == "pair")       { f.pairingSeed = std::strtoll(rest.c_str(), nullptr, 10); return; }
        if (key == "combo")      { f.comboSize = std::atoi(rest.c_str()); return; }
        if (key == "rounds")     { f.maxRounds = std::atoi(rest.c_str()); return; }
        if (key == "zspeed")     { f.zombieSpeed = std::atoi(rest.c_str()); return; }
        if (key == "stride")     { f.stride = std::atoi(rest.c_str()); return; }
        if (key == "phase")      { f.phase = std::atoi(rest.c_str()); return; }
        if (key == "games")      { f.games = std::atoi(rest.c_str()); return; }
        if (key == "run")        { f.runId = std::atoi(rest.c_str()); return; }
        if (key == "group")      { f.groups.push_back(WarriorGroup()); f.groups.back().name = rest; return; }
        if (key == "w") {
            if (f.groups.empty()) { why = "warrior before any group"; bad = true; return; }
            if (!words(line, 4, w, rest)) { why = "short warrior line"; bad = true; return; }
            WarriorData d;
            if (!typeFromWord(w[1], d.type)) { why = "unknown warrior type " + w[1]; bad = true; return; }
            d.speed = std::atoi(w[2].c_str());
            if (w[3] != "-" && !hexIn(w[3], d.code)) { why = "bad code bytes for " + rest; bad = true; return; }
            d.name = rest;
            f.groups.back().warriors.push_back(d);
            return;
        }
        // unknown keys are skipped so a newer app can add lines an older one ignores
    });
    if (bad) return false;
    if (!header)         { why = "empty"; return false; }
    if (!ended)          { why = "truncated field pack"; return false; }
    if (f.groups.empty()){ why = "field pack has no groups"; return false; }
    if (f.stride < 1)    f.stride = 1;
    if (f.phase < 0 || f.phase >= f.stride) { why = "phase outside stride"; return false; }
    out = f;
    return true;
}

// ---- result pack -----------------------------------------------------------

std::string writeResultPack(const CompeteResult& r, const std::string& sender, int runId) {
    std::string s;
    s += "cwest-result 1\n";
    s += "from " + cleanName(sender) + "\n";
    char buf[160];
    std::snprintf(buf, sizeof buf, "run %d\ngames %d %d %.3f\n", runId, r.gamesRun, r.gamesFailed, r.seconds);
    s += buf;
    for (size_t i = 0; i < r.warriorName.size(); ++i) {
        // %.17g so the sum the receiver forms is the one the sender had
        std::snprintf(buf, sizeof buf, "r %.17g %d ", r.warriorScore[i], r.warriorSurvived[i]);
        s += buf;
        s += cleanName(r.warriorName[i]) + "\n";
    }
    s += "end\n";
    return s;
}

bool parseResultPack(const std::string& text, ResultPack& out, std::string& why) {
    ResultPack p;
    bool header = false, ended = false, bad = false;
    std::vector<std::string> w;
    std::string rest;
    eachLine(text, [&](const std::string& line) {
        if (bad || ended) return;
        if (!header) {
            if (line.rfind("cwest-result ", 0) != 0) { why = "not a result pack"; bad = true; }
            header = true;
            return;
        }
        if (!words(line, 1, w, rest)) return;
        const std::string key = w[0];
        if (key == "end")  { ended = true; return; }
        if (key == "from") { p.sender = rest; return; }
        if (key == "run")  { p.runId = std::atoi(rest.c_str()); return; }
        if (key == "games") {
            if (!words(line, 3, w, rest)) { why = "short games line"; bad = true; return; }
            p.gamesRun = std::atoi(w[1].c_str());
            p.gamesFailed = std::atoi(w[2].c_str());
            p.seconds = std::atof(rest.c_str());
            return;
        }
        if (key == "r") {
            if (!words(line, 3, w, rest)) { why = "short result line"; bad = true; return; }
            p.score.push_back(std::atof(w[1].c_str()));
            p.survived.push_back(std::atoi(w[2].c_str()));
            p.name.push_back(rest);
            return;
        }
    });
    if (bad) return false;
    if (!header) { why = "empty"; return false; }
    if (!ended)  { why = "truncated result pack"; return false; }
    out = p;
    return true;
}

int addResultPack(CompeteResult& into, const ResultPack& p) {
    std::unordered_map<std::string, int> row;
    for (size_t i = 0; i < into.warriorName.size(); ++i) row[into.warriorName[i]] = (int)i;
    int unknown = 0;
    for (size_t i = 0; i < p.name.size(); ++i) {
        auto it = row.find(p.name[i]);
        if (it == row.end()) { ++unknown; continue; }
        const int w = it->second;
        into.warriorScore[w]    += p.score[i];
        into.warriorSurvived[w] += p.survived[i];
        const int g = into.warriorGroup[w];
        into.groupScore[g]      += p.score[i];
        into.groupSurvived[g]   += p.survived[i];
    }
    into.gamesRun    += p.gamesRun;
    into.gamesFailed += p.gamesFailed;
    return unknown;
}

} // namespace cw
