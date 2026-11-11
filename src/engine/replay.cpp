// =============================================================================
//  Deterministic replay -- recorder and player. See replay.h.
// =============================================================================
#include "replay.h"

#include <algorithm>
#include <cstring>

namespace cw {

// ---------------------------------------------------------------------------
//  CpuState <-> 16 u16 slots
// ---------------------------------------------------------------------------
void cpuToSlots(const CpuState& s, u16 o[RS_COUNT]) {
    o[RS_AX]=(u16)s.ax; o[RS_BX]=(u16)s.bx; o[RS_CX]=(u16)s.cx; o[RS_DX]=(u16)s.dx;
    o[RS_DS]=(u16)s.ds; o[RS_ES]=(u16)s.es; o[RS_SI]=(u16)s.si; o[RS_DI]=(u16)s.di;
    o[RS_SS]=(u16)s.ss; o[RS_BP]=(u16)s.bp; o[RS_SP]=(u16)s.sp; o[RS_CS]=(u16)s.cs;
    o[RS_IP]=(u16)s.ip; o[RS_FLAGS]=(u16)s.flags; o[RS_ENERGY]=(u16)s.energy;
    o[RS_BOMBS]=(u16)((u8)s.bomb1 | ((u16)(u8)s.bomb2 << 8));
}
void slotsToCpu(const u16 i[RS_COUNT], CpuState& s) {
    s.ax=(s16)i[RS_AX]; s.bx=(s16)i[RS_BX]; s.cx=(s16)i[RS_CX]; s.dx=(s16)i[RS_DX];
    s.ds=(s16)i[RS_DS]; s.es=(s16)i[RS_ES]; s.si=(s16)i[RS_SI]; s.di=(s16)i[RS_DI];
    s.ss=(s16)i[RS_SS]; s.bp=(s16)i[RS_BP]; s.sp=(s16)i[RS_SP]; s.cs=(s16)i[RS_CS];
    s.ip=(s16)i[RS_IP]; s.flags=(s16)i[RS_FLAGS]; s.energy=(s16)i[RS_ENERGY];
    s.bomb1=(s8)(i[RS_BOMBS] & 0xFF); s.bomb2=(s8)(i[RS_BOMBS] >> 8);
}
const char* regSlotName(int slot) {
    static const char* n[RS_COUNT] = { "AX","BX","CX","DX","DS","ES","SI","DI",
                                       "SS","BP","SP","CS","IP","FLAGS","NRG","BOMBS" };
    return (slot >= 0 && slot < RS_COUNT) ? n[slot] : "?";
}

// ---------------------------------------------------------------------------
//  Replay
// ---------------------------------------------------------------------------
void Replay::clear() {
    cycles.clear(); deltas.clear(); regs.clear(); opBytes.clear(); snaps.clear();
    deaths.clear(); roundStart.clear();
    memHi = SNAP_LO; numWarriors = 0; totalRounds = 0; complete = false;
    contestedUntilRound = 0; note.clear();
    warriorName.clear(); warriorGroup.clear(); warriorSlot.clear();
    warriorZombie.clear(); homeSS.clear(); homeES.clear(); groupName.clear();
}

size_t Replay::bytes() const {
    size_t n = cycles.size() * sizeof(CycleRec)
             + deltas.size() * sizeof(MemDelta)
             + regs.size()   * sizeof(u16)
             + opBytes.size()
             + deaths.size() * sizeof(DeathEvent)
             + roundStart.size() * sizeof(u64);
    for (const Snapshot& s : snaps)
        n += s.mem.size() + s.owner.size() + s.alive.size() + s.cpu.size() * sizeof(CpuState);
    return n;
}

int Replay::roundOfCycle(u64 c) const {
    if (roundStart.empty()) return 0;
    // last round whose start cycle is <= c
    auto it = std::upper_bound(roundStart.begin(), roundStart.end(), c);
    return (int)(it - roundStart.begin()) - (it == roundStart.begin() ? 0 : 1);
}

// ---------------------------------------------------------------------------
//  Recorder
// ---------------------------------------------------------------------------
namespace {

struct Recorder : MemoryListener, WarListener, CycleObserver {
    Replay*  rep = nullptr;
    War*     war = nullptr;
    const RecordOptions* opt = nullptr;

    // scratch for the cycle currently being recorded
    bool  inCycle = false;
    int   curWarrior = 0;
    bool  curBrk = false;
    u16   before[RS_COUNT]{};
    u8    curOp[kOpBytes]{};      // bytes at CS:IP for the cycle being recorded
    std::vector<std::pair<u32, u8>> pending;   // (linear addr, xor)

    // live ownership map, mirrored into snapshots
    std::vector<u8> owner;

    std::vector<char> pokeDone;   // one flag per debugger edit, so each lands once
    u64  cycleCounter = 0;
    int  curRound = 0;
    int  snapEvery = 256;
    u64  nextSnapAt = 0;

    void begin(int numWarriors) {
        owner.assign((size_t)War::ARENA_SIZE, 0);
        snapEvery = opt->baseSnapInterval;
        nextSnapAt = 0;
        (void)numWarriors;
    }

    // ---- MemoryListener ----
    void onWrite(s32 linear, u8 oldVal, u8 newVal) override {
        if (linear >= 0 && linear < War::ARENA_SIZE + SNAP_LO && linear >= SNAP_LO)
            owner[(size_t)(linear - SNAP_LO)] = (u8)(war->currentWarrior() + 1);
        if (inCycle) pending.emplace_back((u32)linear, (u8)(oldVal ^ newVal));
    }

    // ---- CycleObserver ----
    void onCycleBegin(int w) override {
        inCycle = true;
        curWarrior = w;
        pending.clear();
        cpuToSlots(war->warrior(w).cpu(), before);

        // Debugger edits land here, after the pre-cycle registers are captured
        // and the delta list is cleared, so a poked byte is recorded as an
        // ordinary write and a poked register shows up in this cycle's diff.
        //
        // Only the acting warrior's registers are diffed into a CycleRec, so a
        // register edit waits for the cycle its own warrior runs. Applying it
        // earlier would change the war without the recording ever showing it.
        if (pokeDone.size() != opt->pokes.size()) pokeDone.assign(opt->pokes.size(), 0);
        for (size_t pi = 0; pi < opt->pokes.size(); ++pi) {
            const Poke& p = opt->pokes[pi];
            if (pokeDone[pi]) continue;
            if (p.isReg) {
                if (cycleCounter < p.cycle || w != p.warrior) continue;
                Warrior* tw = war->warriorMut(p.warrior);
                if (tw) if (s16* slot = tw->cpu().slot(p.reg)) *slot = (s16)p.regValue;
            } else {
                if (p.cycle != cycleCounter) continue;
                war->pokeByte(p.linear, p.value);
            }
            pokeDone[pi] = 1;
        }

        curBrk = matchesBreakpoint(w);

        // snapshot the instruction bytes exactly the way the fetcher sees them
        // (offset wraps inside the segment)
        const CpuState& st = war->warrior(w).cpu();
        const u8* core = war->core().data();
        for (int k = 0; k < kOpBytes; ++k)
            curOp[k] = core[RMAddr(st.cs, (s16)(st.ip + k)).linear];
    }

    void onCycleEnd(int w) override {
        if (!inCycle) return;
        inCycle = false;

        CycleRec rec{};
        rec.warrior = (u8)w;
        rec.flags   = curBrk ? CF_BRK : 0;

        // --- memory deltas, coalesced into runs of up to 4 consecutive bytes ---
        rec.deltaFirst = (u32)rep->deltas.size();
        size_t i = 0;
        while (i < pending.size()) {
            MemDelta d{};
            const u32 a0 = pending[i].first;
            u32 len = 1;
            d.x[0] = pending[i].second;
            while (len < 4 && i + len < pending.size() && pending[i + len].first == a0 + len) {
                d.x[len] = pending[i + len].second;
                ++len;
            }
            d.packed = (a0 & 0xFFFFFu) | ((len - 1u) << 20);
            rep->deltas.push_back(d);
            i += len;
        }
        rec.deltaCount = (u16)(rep->deltas.size() - rec.deltaFirst);

        // --- register deltas ---
        u16 after[RS_COUNT];
        cpuToSlots(war->warrior(w).cpu(), after);
        rec.regFirst = (u32)rep->regs.size();
        u16 mask = 0;
        for (int s = 0; s < RS_COUNT; ++s)
            if (after[s] != before[s]) {
                mask |= (u16)(1u << s);
                rep->regs.push_back((u16)(after[s] ^ before[s]));
            }
        rec.regMask = mask;

        rep->cycles.push_back(rec);
        rep->opBytes.insert(rep->opBytes.end(), curOp, curOp + kOpBytes);
        ++cycleCounter;
    }

    void onTurnEnd(int) override {
        if (!rep->cycles.empty()) rep->cycles.back().flags |= CF_TURN_END;
    }

    void onDeath(int w, Fault reason) override {
        if (!rep->cycles.empty()) rep->cycles.back().flags |= CF_DEATH;
        DeathEvent ev{};
        ev.cycle = rep->cycles.empty() ? cycleCounter : rep->cycles.size() - 1;
        ev.warrior = (u16)w;
        ev.reason = (u8)reason;
        rep->deaths.push_back(ev);
    }

    // A breakpoint fires when the bytes at CS:IP start with one of the patterns
    // (default BRK == "EB 00", i.e. jmp $+2).
    bool matchesBreakpoint(int w) const {
        if (opt->breakPatterns.empty()) return false;
        const CpuState& st = war->warrior(w).cpu();
        const u8* core = war->core().data();
        for (const std::vector<u8>& pat : opt->breakPatterns) {
            if (pat.empty()) continue;
            bool ok = true;
            for (size_t k = 0; k < pat.size(); ++k) {
                RMAddr a(st.cs, (s16)(st.ip + (int)k));
                if (core[a.linear] != pat[k]) { ok = false; break; }
            }
            if (ok) return true;
        }
        return false;
    }

    // ---- snapshots ----
    void takeSnapshot() {
        Snapshot s;
        s.cycle = rep->cycles.size();
        s.round = curRound;
        const u8* core = war->core().data();
        s.mem.assign(core + SNAP_LO, core + rep->memHi);
        s.owner = owner;
        s.cpu.resize((size_t)war->numWarriors());
        s.alive.resize((size_t)war->numWarriors());
        for (int i = 0; i < war->numWarriors(); ++i) {
            s.cpu[(size_t)i]   = war->warrior(i).cpu();
            s.alive[(size_t)i] = war->warrior(i).alive() ? 1 : 0;
        }
        rep->snaps.push_back(std::move(s));

        // keep the snapshot count bounded: drop every other one and double the
        // interval, which keeps coverage uniform and seeking cheap
        if ((int)rep->snaps.size() > opt->maxSnapshots) {
            std::vector<Snapshot> kept;
            kept.reserve(rep->snaps.size() / 2 + 1);
            for (size_t i = 0; i < rep->snaps.size(); i += 2)
                kept.push_back(std::move(rep->snaps[i]));
            rep->snaps.swap(kept);
            snapEvery *= 2;
        }
        rep->snapInterval = snapEvery;
        nextSnapAt = rep->cycles.size() + (u64)snapEvery;
    }
};

} // namespace

bool recordWar(const std::vector<WarriorGroup>& groups,
               const RecordOptions& opt, Replay& out, RecordProgress* prog) {
    out.clear();
    out.seed = opt.seed;
    out.zombieSpeed = opt.zombieSpeed;

    Recorder rec;
    rec.rep = &out;
    rec.opt = &opt;

    War war(&rec, &rec, opt.zombieSpeed);
    rec.war = &war;
    rec.begin(0);

    std::vector<const WarriorGroup*> ptrs;
    ptrs.reserve(groups.size());
    for (const WarriorGroup& g : groups) ptrs.push_back(&g);

    war.setSeed(opt.seed);
    try {
        war.loadWarriorGroups(ptrs);          // observer not attached yet: load
    } catch (const CpuFault&) {                // writes paint ownership only
        out.note = "could not place all warriors";
        return false;
    }

    out.memHi       = war.usedMemoryEnd();
    out.numWarriors = war.numWarriors();
    out.snapInterval = opt.baseSnapInterval;

    // identity tables
    for (const WarriorGroup& g : groups) out.groupName.push_back(g.name);
    for (int i = 0; i < war.numWarriors(); ++i) {
        const Warrior& w = war.warrior(i);
        out.warriorName.push_back(w.name());
        out.warriorZombie.push_back(w.zombie() ? 1 : 0);
        out.homeSS.push_back((u16)w.cpu().ss);      // still the initial state here
        out.homeES.push_back((u16)w.cpu().es);
        if (w.zombie()) {
            out.warriorGroup.push_back(-1);
            out.warriorSlot.push_back(0);
        } else {
            int gi = -1;
            for (int g = 0; g < (int)out.groupName.size(); ++g)
                if (out.groupName[(size_t)g] == w.groupName()) { gi = g; break; }
            out.warriorGroup.push_back(gi);
            out.warriorSlot.push_back(w.type() == WarriorType::SURVIVOR_2 ? 1 : 0);
        }
    }

    war.setObserver(&rec);

    int round = 0;
    rec.takeSnapshot();                        // snapshot 0 == the starting core
    for (; round < opt.maxRounds; ++round) {
        if (prog && prog->cancel.load()) break;
        if (opt.runUntilAllSurvivorsDead) {
            // debugger: play on until the last survivor is gone
            if (war.numRemainingSurvivors() <= 0) break;
        } else if (war.isOver()) {
            break;
        }

        rec.curRound = round;
        out.roundStart.push_back(rec.rep->cycles.size());

        war.nextRound(round);

        if (rec.rep->cycles.size() >= rec.nextSnapAt) {
            rec.curRound = round + 1;
            rec.takeSnapshot();
        }
        if (prog) prog->rounds.store(round + 1);
    }

    out.totalRounds = round;
    out.complete = true;

    // Work out how long the war was actually contested: replay the deaths in
    // order and note when the number of groups still holding a live survivor
    // falls to one.
    {
        std::vector<int> aliveInGroup((size_t)out.groupName.size() + 1, 0);
        int groupsAlive = 0;
        for (int i = 0; i < out.numWarriors; ++i) {
            if (out.warriorZombie[(size_t)i]) continue;
            const int gi = out.warriorGroup[(size_t)i];
            if (gi < 0) continue;
            if (aliveInGroup[(size_t)gi]++ == 0) ++groupsAlive;
        }
        out.contestedUntilRound = out.totalRounds;
        if (groupsAlive > 1) {
            for (const DeathEvent& d : out.deaths) {
                const int wi = (int)d.warrior;
                if (wi >= out.numWarriors || out.warriorZombie[(size_t)wi]) continue;
                const int gi = out.warriorGroup[(size_t)wi];
                if (gi < 0) continue;
                if (--aliveInGroup[(size_t)gi] == 0 && --groupsAlive <= 1) {
                    out.contestedUntilRound = out.roundOfCycle(d.cycle);
                    break;
                }
            }
        } else {
            out.contestedUntilRound = 0;      // never contested: show everything
        }
    }
    if (prog) prog->done.store(true);
    return true;
}

// ---------------------------------------------------------------------------
//  Player
// ---------------------------------------------------------------------------
void Player::bind(const Replay* r) {
    m_rep = r;
    if (!r || r->snaps.empty()) { m_mem.clear(); m_owner.clear(); m_cpu.clear(); m_alive.clear(); return; }
    m_cycle = 0; m_round = 0;
    restoreSnapshot(0);
}

size_t Player::snapshotFor(u64 c) const {
    // last snapshot whose cycle <= c
    size_t lo = 0, hi = m_rep->snaps.size();
    while (lo + 1 < hi) {
        size_t mid = (lo + hi) / 2;
        if (m_rep->snaps[mid].cycle <= c) lo = mid; else hi = mid;
    }
    return lo;
}

void Player::restoreSnapshot(size_t idx) {
    const Snapshot& s = m_rep->snaps[idx];
    m_mem   = s.mem;
    m_owner = s.owner;
    m_cpu   = s.cpu;
    m_alive = s.alive;
    m_cycle = s.cycle;
    m_round = s.round;
}

void Player::applyForward(u64 upTo) {
    const Replay& R = *m_rep;
    const s32 base = SNAP_LO;
    while (m_cycle < upTo) {
        const CycleRec& c = R.cycles[(size_t)m_cycle];

        // memory + ownership
        const u8 own = (u8)(c.warrior + 1);
        for (u32 k = 0; k < c.deltaCount; ++k) {
            const MemDelta& d = R.deltas[c.deltaFirst + k];
            const u32 a = d.addr(), n = d.len();
            for (u32 b = 0; b < n; ++b) {
                const s32 lin = (s32)(a + b);
                const size_t off = (size_t)(lin - base);
                if (off < m_mem.size()) m_mem[off] ^= d.x[b];
                if (lin >= SNAP_LO && lin < SNAP_LO + War::ARENA_SIZE)
                    m_owner[(size_t)(lin - SNAP_LO)] = own;
            }
        }

        // registers
        if (c.regMask) {
            u16 slots[RS_COUNT];
            cpuToSlots(m_cpu[(size_t)c.warrior], slots);
            u32 at = c.regFirst;
            for (int s = 0; s < RS_COUNT; ++s)
                if (c.regMask & (1u << s)) slots[s] ^= R.regs[at++];
            slotsToCpu(slots, m_cpu[(size_t)c.warrior]);
        }

        if (c.flags & CF_DEATH) m_alive[(size_t)c.warrior] = 0;
        ++m_cycle;
    }
    m_round = R.roundOfCycle(m_cycle);
}

void Player::seek(u64 c) {
    if (!m_rep || m_rep->snaps.empty()) return;
    const u64 total = m_rep->cycleCount();
    if (c > total) c = total;

    // going backwards (or jumping far) restarts from the nearest snapshot;
    // going forwards from where we are is just more deltas
    const size_t si = snapshotFor(c);
    if (c < m_cycle || m_rep->snaps[si].cycle > m_cycle)
        restoreSnapshot(si);
    applyForward(c);
}

void Player::stepCycle(int dir) {
    if (!m_rep) return;
    if (dir > 0) seek(m_cycle + 1);
    else if (m_cycle > 0) seek(m_cycle - 1);
}

void Player::stepRound(int dir) {
    if (!m_rep || m_rep->roundStart.empty()) return;
    const std::vector<u64>& rs = m_rep->roundStart;
    const int r = m_rep->roundOfCycle(m_cycle);

    if (dir > 0) {
        if (r + 1 < (int)rs.size()) seek(rs[(size_t)(r + 1)]);
        else                        seek(m_rep->cycleCount());
        return;
    }
    // backwards: rewind to the start of this round first, then to the one before
    if (m_cycle > rs[(size_t)r]) seek(rs[(size_t)r]);
    else if (r > 0)              seek(rs[(size_t)(r - 1)]);
    else                         seek(0);
}

bool Player::seekEvent(int dir) {
    if (!m_rep) return false;
    const Replay& R = *m_rep;
    const u64 total = R.cycleCount();
    if (dir > 0) {
        for (u64 c = m_cycle + 1; c < total; ++c)
            if (R.cycles[(size_t)c].flags & (CF_DEATH | CF_BRK)) { seek(c); return true; }
        seek(total);
        return false;
    }
    for (u64 c = m_cycle; c-- > 0; )
        if (R.cycles[(size_t)c].flags & (CF_DEATH | CF_BRK)) { seek(c); return true; }
    seek(0);
    return false;
}

u8 Player::readByte(s32 linear) const {
    const size_t off = (size_t)(linear - SNAP_LO);
    return off < m_mem.size() ? m_mem[off] : (u8)0;
}

int Player::ipOffset(int w) const {
    if (!m_rep || w < 0 || w >= (int)m_cpu.size() || !m_alive[(size_t)w]) return -1;
    const CpuState& s = m_cpu[(size_t)w];
    const int off = RMAddr(s.cs, s.ip).linear - SNAP_LO;
    return (off >= 0 && off < War::ARENA_SIZE) ? off : -1;
}

int Player::actingWarrior() const {
    if (!m_rep || m_cycle >= m_rep->cycleCount()) return -1;
    return m_rep->cycles[(size_t)m_cycle].warrior;
}

} // namespace cw
