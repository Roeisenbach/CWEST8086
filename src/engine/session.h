// =============================================================================
//  cw::GuiSession  --  the bridge between the ImGui front-end and the engine.
//
//  Pressing Play does NOT run the war live any more. It hands the warriors to
//  a background thread which plays the whole fight out silently and records it
//  (see replay.h); once that finishes, the GUI plays back the recording. That
//  makes the same war replayable forever, steppable in both directions, and
//  scrubbable from a timeline, with no risk of the "ran once, now what" state.
// =============================================================================
#pragma once

#include "engine.h"
#include "replay.h"

#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace cw {

class GuiSession {
public:
    ~GuiSession() { cancelPrecompute(); }

    // ----- arming -----
    // Copies the groups, then kicks off the silent precompute on a worker.
    void arm(std::vector<WarriorGroup> groups, s64 seed, int zombieSpeed,
             int maxRounds = War::MAX_ROUND, bool autoPlay = true,
             std::vector<Poke> pokes = std::vector<Poke>());

    // Round to jump to as soon as the next recording lands; -1 for the start.
    void seekRoundWhenReady(int r) { m_seekRound = r; }
    void poll();          // call once per frame: adopts a finished recording
    void clear();
    void cancelPrecompute();

    bool  precomputing() const { return m_thread != nullptr; }
    float progress() const;                       // 0..1 (rounds done / cap)
    int   progressRounds() const { return m_prog.rounds.load(); }
    bool  ready() const { return m_player.bound(); }
    // bumped every time a fresh recording is adopted, so the UI can re-fit views
    u64   generation() const { return m_generation; }
    int   contestedUntilRound() const { return m_replay.contestedUntilRound; }
    const std::string& status() const { return m_status; }

    // ----- transport -----
    bool running() const { return m_running; }
    void setRunning(bool on) { m_running = on && ready(); if (!on) m_accum = 0.0f; }
    void togglePlay() { setRunning(!m_running); }
    void stepCycle(int dir) { if (ready()) { m_running = false; m_player.stepCycle(dir); } }
    void stepRound(int dir) { if (ready()) { m_running = false; m_player.stepRound(dir); } }
    void continueToEvent(int dir) { if (ready()) { m_running = false; m_player.seekEvent(dir); } }
    void seekCycle(u64 c) { if (ready()) m_player.seek(c); }
    void seekFraction(float f);
    void rewind() { if (ready()) { m_running = false; m_player.seek(0); } }
    // Playback advances whole rounds: one round is one full pass in which every
    // living warrior takes its turn, and it is the only unit the UI moves in.
    void tick(float dt, float roundsPerSec);

    // stop when a death / breakpoint is reached while running
    bool stopOnEvents = true;
    // flips the direction of the run / continue controls
    bool reverse = false;

    // ----- readback -----
    const Player& player() const { return m_player; }
    const Replay& replay() const { return m_replay; }

    u64  cycle() const  { return m_player.cycle(); }
    u64  cycleCount() const { return m_replay.cycleCount(); }
    int  round() const  { return m_player.round(); }
    int  totalRounds() const { return m_replay.totalRounds; }
    int  numWarriors() const { return m_replay.numWarriors; }
    bool atEnd() const  { return ready() && m_player.cycle() >= m_replay.cycleCount(); }

    const u8* arena() const { return m_player.memory(); }
    int  ownerAt(int off) const { return m_player.ownerAt(off); }
    int  warriorIpOffset(int w) const { return m_player.ipOffset(w); }
    bool warriorAlive(int w) const { return m_player.bound() && m_player.alive(w); }
    int  warriorGroup(int w) const {
        return (w >= 0 && w < (int)m_replay.warriorGroup.size()) ? m_replay.warriorGroup[(size_t)w] : -1;
    }
    int  warriorSlot(int w) const {
        return (w >= 0 && w < (int)m_replay.warriorSlot.size()) ? m_replay.warriorSlot[(size_t)w] : 0;
    }
    bool warriorZombie(int w) const {
        return w >= 0 && w < (int)m_replay.warriorZombie.size() && m_replay.warriorZombie[(size_t)w];
    }
    const std::string& warriorName(int w) const { return m_replay.warriorName[(size_t)w]; }
    u16  homeSS(int w) const {
        return (w >= 0 && w < (int)m_replay.homeSS.size()) ? m_replay.homeSS[(size_t)w] : 0;
    }
    u16  homeES(int w) const {
        return (w >= 0 && w < (int)m_replay.homeES.size()) ? m_replay.homeES[(size_t)w] : 0;
    }
    // resolve a workspace (group name, slot) to the warrior index in the replay
    int findWarrior(const std::string& group, int slot) const {
        for (int i = 0; i < m_replay.numWarriors; ++i) {
            const int gi = warriorGroup(i);
            if (gi < 0 || gi >= (int)m_replay.groupName.size()) continue;
            if (m_replay.groupName[(size_t)gi] == group && warriorSlot(i) == slot) return i;
        }
        return -1;
    }
    int  groupCount() const { return (int)m_replay.groupName.size(); }
    const std::string& groupName(int gi) const { return m_replay.groupName[(size_t)gi]; }

    int survivorsAlive() const;
    size_t replayBytes() const { return m_replay.bytes(); }

private:
    std::vector<WarriorGroup> m_groups;      // owned; the worker reads these
    Replay        m_replay;
    Replay        m_pending;                 // filled by the worker
    Player        m_player;
    RecordProgress m_prog;
    std::unique_ptr<std::thread> m_thread;
    int    m_maxRounds = War::MAX_ROUND;
    int    m_seekRound = -1;
    std::vector<Poke> m_pokes;
    bool   m_recordOk = false;

    bool   m_running = false;
    bool   m_autoPlay = false;      // start playing as soon as the recording lands
    u64    m_generation = 0;
    float  m_accum = 0.0f;
    std::string m_status = "no war loaded";
};

} // namespace cw
