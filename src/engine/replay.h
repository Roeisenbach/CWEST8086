// =============================================================================
//  Deterministic replay.
//
//  A war is played out ONCE, silently, the moment you press Play. Everything
//  that happened is stored as:
//
//    * one CycleRec per clock cycle (= one Cpu::nextOpcode call), holding the
//      acting warrior, flags, and slices into the delta / register streams;
//    * memory deltas as (linear address, 1..4 bytes of XOR). XOR is its own
//      inverse, so stepping forward is "apply", and the state after any cycle
//      is reachable from any snapshot by applying the deltas in between;
//    * register deltas as a 16-bit mask over the CpuState's 16 u16 slots plus
//      the XOR values for the changed ones;
//    * a full snapshot (memory + ownership map + every warrior's CpuState)
//      every `snapInterval` cycles, so seeking anywhere is bounded work.
//
//  Nothing here re-runs the CPU: playback is pure state reconstruction, so
//  scrubbing backwards is exact and the same war can be replayed forever.
// =============================================================================
#pragma once

#include "engine.h"
#include <atomic>
#include <string>
#include <vector>

namespace cw {

// Everything below this address is untouched by the game, so snapshots start here.
static constexpr s32 SNAP_LO = 0x10000;          // == RMAddr(War::ARENA_SEGMENT, 0).linear

// ---------------------------------------------------------------------------
//  CpuState viewed as 16 u16 slots, so register changes compress to a bitmask
// ---------------------------------------------------------------------------
enum RegSlot : int {
    RS_AX = 0, RS_BX, RS_CX, RS_DX, RS_DS, RS_ES, RS_SI, RS_DI,
    RS_SS, RS_BP, RS_SP, RS_CS, RS_IP, RS_FLAGS, RS_ENERGY, RS_BOMBS,
    RS_COUNT = 16
};
void cpuToSlots(const CpuState& s, u16 out[RS_COUNT]);
void slotsToCpu(const u16 in[RS_COUNT], CpuState& s);
const char* regSlotName(int slot);

// ---------------------------------------------------------------------------
//  Storage
// ---------------------------------------------------------------------------
struct MemDelta {
    u32 packed;      // bits 0..19 linear address, bits 20..21 length-1
    u8  x[4];        // xor values
    u32 addr() const { return packed & 0xFFFFFu; }
    u32 len()  const { return ((packed >> 20) & 3u) + 1u; }
};

enum CycleFlag : u8 {
    CF_TURN_END = 1 << 0,   // last cycle of this warrior's turn
    CF_DEATH    = 1 << 1,   // the warrior died on this cycle
    CF_BRK      = 1 << 2    // a breakpoint pattern sat at CS:IP
};

struct CycleRec {
    u32 deltaFirst;
    u32 regFirst;
    u16 deltaCount;
    u16 regMask;
    u8  warrior;
    u8  flags;
    u8  _pad[2];
};

struct DeathEvent { u64 cycle; u16 warrior; u8 reason; u8 _pad; };

// Bytes captured at CS:IP for every cycle, so the timeline / code view can name
// the instruction that ran without rewinding the whole core to that moment.
static constexpr int kOpBytes = 6;   // longest instruction the engine can execute

struct Snapshot {
    u64 cycle = 0;                 // state as it is *before* this cycle runs
    int round = 0;
    std::vector<u8>       mem;     // [SNAP_LO, memHi)
    std::vector<u8>       owner;   // War::ARENA_SIZE entries, 0 = free else warrior+1
    std::vector<CpuState> cpu;
    std::vector<u8>       alive;
};

struct Replay {
    std::vector<CycleRec>   cycles;
    std::vector<MemDelta>   deltas;
    std::vector<u16>        regs;
    std::vector<u8>         opBytes;       // kOpBytes per cycle
    std::vector<Snapshot>   snaps;
    std::vector<DeathEvent> deaths;
    std::vector<u64>        roundStart;    // cycle index each round begins at

    s32 memHi        = SNAP_LO;
    int numWarriors  = 0;
    int snapInterval = 256;
    int totalRounds  = 0;
    s64 seed         = 0;
    int zombieSpeed  = 2;
    bool complete    = false;
    // Last round on which more than one group still had a living survivor. The
    // debugger opens the timeline on [0, this] because everything after it is a
    // lone survivor running out the clock; scrolling past it is still allowed.
    int  contestedUntilRound = 0;
    std::string note;

    // per-warrior identity, resolved once at load time
    std::vector<std::string> warriorName;
    std::vector<int>         warriorGroup;    // index into groupName, -1 for zombies
    std::vector<u8>          warriorSlot;     // 0 or 1 within the group
    std::vector<u8>          warriorZombie;
    // the segments the warrior was born with: its private stack and its group's
    // shared block. Warriors routinely repoint SS/ES, so these are kept so the
    // debugger can show the real blocks rather than wherever the registers went.
    std::vector<u16>         homeSS;
    std::vector<u16>         homeES;
    std::vector<std::string> groupName;
    std::vector<std::string> groupKey;        // WarriorGroup::key, per group

    u64    cycleCount() const { return cycles.size(); }
    // the bytes the given cycle executed (kOpBytes of them), or nullptr
    const u8* opsOf(u64 c) const {
        return (c < cycles.size()) ? &opBytes[(size_t)c * kOpBytes] : nullptr;
    }
    size_t bytes() const;
    void   clear();

    // cycle index -> round number
    int roundOfCycle(u64 c) const;
};

// ---------------------------------------------------------------------------
//  Recording
// ---------------------------------------------------------------------------
// A change made from the debugger's edit tool, replayed into the war at the
// cycle it was made so that everything after it follows from the edit.
struct Poke {
    u64  cycle   = 0;
    bool isReg   = false;
    s32  linear  = 0;      // memory: absolute core address
    u8   value   = 0;
    int  warrior = 0;      // register: which warrior
    int  reg     = 0;      // PokeReg slot
    u16  regValue = 0;
};

struct RecordOptions {
    s64  seed = 0;
    int  zombieSpeed = 2;
    int  maxRounds = War::MAX_ROUND;
    int  baseSnapInterval = 256;
    int  maxSnapshots = 512;             // thinned by 2x whenever exceeded
    // The engine calls a war over as soon as one group is left standing, which
    // is what scoring needs. The debugger instead wants to watch the last
    // survivor play on, so it keeps going until every survivor is dead.
    bool runUntilAllSurvivorsDead = false;
    std::vector<std::vector<u8>> breakPatterns;   // matched at CS:IP, default { EB 00 }
    std::vector<Poke> pokes;                      // debugger edits, by cycle
};

struct RecordProgress {
    std::atomic<int>  rounds{0};
    std::atomic<bool> done{false};
    std::atomic<bool> cancel{false};
};

// Plays the whole war and fills `out`. Returns false only if the warriors could
// not be placed. Safe to call on a worker thread.
bool recordWar(const std::vector<WarriorGroup>& groups,
               const RecordOptions& opt, Replay& out, RecordProgress* prog);

// ---------------------------------------------------------------------------
//  Playback
// ---------------------------------------------------------------------------
class Player {
public:
    void bind(const Replay* r);
    void unbind() { m_rep = nullptr; }
    bool bound() const { return m_rep != nullptr; }
    const Replay* replay() const { return m_rep; }

    // Move to the state *before* cycle `c` (clamped to [0, cycleCount]).
    void seek(u64 c);

    u64  cycle() const { return m_cycle; }
    int  round() const { return m_round; }

    // one clock cycle / one full round, either direction
    void stepCycle(int dir);
    void stepRound(int dir);
    // next death or breakpoint in the given direction; returns false if none
    bool seekEvent(int dir);

    // ---- readback ----
    const u8* arena() const { return m_mem.empty() ? nullptr : m_mem.data(); }   // ARENA at index 0
    const u8* memory() const { return m_mem.empty() ? nullptr : m_mem.data(); }
    s32       memBase() const { return SNAP_LO; }
    u8   readByte(s32 linear) const;
    int  ownerAt(int arenaOff) const {
        return (arenaOff >= 0 && arenaOff < War::ARENA_SIZE) ? (int)m_owner[(size_t)arenaOff] - 1 : -1;
    }
    const CpuState& cpu(int w) const { return m_cpu[(size_t)w]; }
    bool  alive(int w) const { return m_alive[(size_t)w] != 0; }
    int   numWarriors() const { return m_rep ? m_rep->numWarriors : 0; }
    // arena offset of a warrior's CS:IP, or -1 when it is outside the arena
    int   ipOffset(int w) const;
    // the warrior that is about to act at the current cycle, or -1 at the end
    int   actingWarrior() const;

private:
    void restoreSnapshot(size_t idx);
    void applyForward(u64 upTo);          // apply cycles [m_cycle, upTo)
    size_t snapshotFor(u64 c) const;      // last snapshot with cycle <= c

    const Replay* m_rep = nullptr;
    std::vector<u8>       m_mem;
    std::vector<u8>       m_owner;
    std::vector<CpuState> m_cpu;
    std::vector<u8>       m_alive;
    u64 m_cycle = 0;
    int m_round = 0;
};

} // namespace cw
