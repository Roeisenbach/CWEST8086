// =============================================================================
//  cw::GuiSession implementation -- silent precompute + playback
// =============================================================================
#include "session.h"

#include <algorithm>
#include <cmath>

namespace cw {

void GuiSession::cancelPrecompute() {
    if (!m_thread) return;
    m_prog.cancel.store(true);
    if (m_thread->joinable()) m_thread->join();
    m_thread.reset();
    m_prog.cancel.store(false);
}

void GuiSession::clear() {
    cancelPrecompute();
    m_player.unbind();
    m_replay.clear();
    m_pending.clear();
    m_groups.clear();
    m_running = false;
    m_accum = 0.0f;
    m_status = "no war loaded";
}

void GuiSession::arm(std::vector<WarriorGroup> groups, s64 seed, int zombieSpeed,
                     int maxRounds, bool autoPlay, std::vector<Poke> pokes) {
    cancelPrecompute();
    m_autoPlay = autoPlay;
    m_player.unbind();
    m_replay.clear();
    m_pending.clear();
    m_running = false;
    m_accum = 0.0f;

    m_groups = std::move(groups);
    m_maxRounds = maxRounds;
    m_pokes = std::move(pokes);
    m_prog.rounds.store(0);
    m_prog.done.store(false);
    m_prog.cancel.store(false);
    m_recordOk = false;
    m_status = "precomputing...";

    RecordOptions opt;
    opt.seed = seed;
    opt.zombieSpeed = zombieSpeed;
    opt.pokes = m_pokes;
    opt.maxRounds = maxRounds;
    opt.breakPatterns.push_back({ 0xEB, 0x00 });   // BRK == jmp $+2
    opt.runUntilAllSurvivorsDead = true;           // debugger, not scoring

    m_thread.reset(new std::thread([this, opt]() {
        m_recordOk = recordWar(m_groups, opt, m_pending, &m_prog);
        m_prog.done.store(true);
    }));
}

void GuiSession::poll() {
    if (!m_thread) return;
    if (!m_prog.done.load()) return;

    if (m_thread->joinable()) m_thread->join();
    m_thread.reset();

    if (!m_recordOk || m_pending.snaps.empty()) {
        m_status = m_pending.note.empty() ? "precompute failed" : m_pending.note;
        m_pending.clear();
        return;
    }

    m_replay = std::move(m_pending);
    m_pending.clear();
    m_player.bind(&m_replay);
    // A re-record made for a debugger edit comes back to where it was left,
    // one round past the change, rather than to the top of the war.
    if (m_seekRound > 0 && m_seekRound < (int)m_replay.roundStart.size())
        m_player.seek(m_replay.roundStart[(size_t)m_seekRound]);
    else
        m_player.seek(0);
    m_seekRound = -1;
    ++m_generation;
    if (m_autoPlay) { m_running = true; m_accum = 0.0f; }

    char buf[192];
    snprintf(buf, sizeof(buf), "%d warriors  %d rounds  %llu cycles  %.1f MB",
             m_replay.numWarriors, m_replay.totalRounds,
             (unsigned long long)m_replay.cycleCount(),
             (double)m_replay.bytes() / (1024.0 * 1024.0));
    m_status = buf;
}

float GuiSession::progress() const {
    if (!m_thread) return ready() ? 1.0f : 0.0f;
    const int done = m_prog.rounds.load();
    return m_maxRounds > 0 ? std::min(1.0f, (float)done / (float)m_maxRounds) : 0.0f;
}

void GuiSession::seekFraction(float f) {
    if (!ready()) return;
    const u64 n = m_replay.cycleCount();
    f = std::min(1.0f, std::max(0.0f, f));
    m_player.seek((u64)(f * (double)n));
}

int GuiSession::survivorsAlive() const {
    if (!ready()) return 0;
    int n = 0;
    for (int i = 0; i < m_replay.numWarriors; ++i)
        if (!warriorZombie(i) && m_player.alive(i)) ++n;
    return n;
}

void GuiSession::tick(float dt, float roundsPerSec) {
    if (!ready() || !m_running) return;

    const int dir = reverse ? -1 : 1;
    m_accum += dt * roundsPerSec;
    int guard = 0;
    while (m_accum >= 1.0f && m_running && guard < 400000) {
        m_accum -= 1.0f;
        ++guard;

        if (dir > 0 && m_player.cycle() >= m_replay.cycleCount()) { m_running = false; break; }
        if (dir < 0 && m_player.cycle() == 0)                     { m_running = false; break; }

        const u64 before = m_player.cycle();
        m_player.stepRound(dir);

        // Playback only ever lands on a round boundary, so an event is looked
        // for across the whole round just crossed rather than on one opcode.
        if (stopOnEvents) {
            const u64 a = before < m_player.cycle() ? before : m_player.cycle();
            const u64 b = before < m_player.cycle() ? m_player.cycle() : before;
            for (u64 c = a; c < b; ++c)
                if (m_replay.cycles[(size_t)c].flags & (CF_DEATH | CF_BRK)) { m_running = false; break; }
            if (!m_running) break;
        }
    }
    if (!m_running) m_accum = 0.0f;
}

} // namespace cw
