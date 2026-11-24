// =============================================================================
//  cw::Core / cw::RestrictedMemory / cw::Warrior / cw::War
//  Faithful port of il.co.codeguru.corewars8086.memory.* and .war.{War,Warrior}.
// =============================================================================
#include "engine.h"
#include <cmath>
#include <cstring>

namespace cw {

const char* faultReason(Fault f) {
    return (f == Fault::Memory) ? "memory exception" : "CPU exception";
}

// -----------------------------------------------------------------------------
//  RestrictedMemory  --  AbstractRealModeMemory + RestrictedAccessRealModeMemory
//  Word access is two byte accesses with the high byte's offset wrapping inside
//  the segment, exactly as the Java base class does.
// -----------------------------------------------------------------------------
// The accessors themselves are in the header so they inline into the
// interpreter; only the one-off setup lives here. The regions are handed over
// as inclusive [lo,hi] pairs and stored as base+span, sorted widest-first: they
// are disjoint, so the test order is free to be chosen for speed.
void RestrictedMemory::init(Core* core,
                            const Region* rd, int nrd, const Region* wr, int nwr,
                            const Region* ex, int nex) {
    m_data     = core->data();
    m_listener = core->m_listener;

    auto fill = [](Spans& s, const Region* r, int n) {
        Region tmp[MAX_REGIONS];
        for (int i = 0; i < MAX_REGIONS; ++i) tmp[i] = (i < n) ? r[i] : Region{1, 0};
        for (int i = 0; i < MAX_REGIONS; ++i)          // three elements: a plain
            for (int j = i + 1; j < MAX_REGIONS; ++j)  // selection sort is fine
                if (tmp[j].hi - tmp[j].lo > tmp[i].hi - tmp[i].lo) std::swap(tmp[i], tmp[j]);
        for (int i = 0; i < MAX_REGIONS; ++i) {
            const bool empty = tmp[i].hi < tmp[i].lo;
            s.lo[i]   = empty ? 0xFFFFFFFFu : (u32)tmp[i].lo;
            s.span[i] = empty ? 0u : (u32)(tmp[i].hi - tmp[i].lo);
        }
    };
    fill(m_rd, rd, nrd);
    fill(m_wr, wr, nwr);

    // execution is allowed in exactly one region -- the arena
    if (nex > 0) { m_exLo = (u32)ex[0].lo; m_exSpan = (u32)(ex[0].hi - ex[0].lo); }
    else         { m_exLo = 1; m_exSpan = 0; }
}

// -----------------------------------------------------------------------------
//  Warrior  --  war/Warrior.java
// -----------------------------------------------------------------------------
void Warrior::init(std::string name, std::string groupName, int codeSize, Core* core,
                   RMAddr loadAddr, RMAddr initialStack, RMAddr groupShared,
                   s16 groupSharedSize, WarriorType type)
{
    m_name = std::move(name);
    m_groupName = std::move(groupName);
    m_codeSize = codeSize;
    m_loadAddr = loadAddr;
    m_type = type;
    m_alive = true;
    m_speed = 0;
    m_groupId = 0;
    m_slot = -1;

    // --- initial CPU state (Warrior.initializeCpuState) ---
    CpuState& st = m_cpu.state();
    st.ax = loadAddr.offset;
    st.bx = st.cx = st.dx = 0;
    st.ds = loadAddr.segment;
    st.es = groupShared.segment;
    st.si = st.di = 0;
    st.ss = initialStack.segment;
    st.bp = 0;
    st.sp = initialStack.offset;
    st.cs = loadAddr.segment;
    st.ip = loadAddr.offset;
    st.flags  = 0;
    st.energy = 0;
    st.bomb1  = 2;
    st.bomb2  = 1;

    // --- access regions ---
    RMAddr lowestStack (initialStack.segment, (s16)0);
    RMAddr lowestCore  (loadAddr.segment,     (s16)0);
    RMAddr highestCore (loadAddr.segment,     (s16)-1);              // offset 0xFFFF
    RMAddr highestShared(groupShared.segment, (s16)(groupSharedSize - 1));

    const Region rw[3] = {
        { lowestStack.linear,  initialStack.linear },
        { lowestCore.linear,   highestCore.linear  },
        { groupShared.linear,  highestShared.linear }
    };
    const Region ex[1] = { { lowestCore.linear, highestCore.linear } };

    m_cpu.mem().init(core, rw, 3, rw, 3, ex, 1);
}

// -----------------------------------------------------------------------------
//  War  --  war/War.java
// -----------------------------------------------------------------------------
// The arena segment is one contiguous run of core -- RMAddr(ARENA_SEGMENT, off)
// is 0x10000 + off for every off, with no wrap -- so filling it is a memset
// rather than 65536 calls each building an address.
void War::initCore() {
    std::memset(m_core.data() + RMAddr(ARENA_SEGMENT, (s16)0).linear,
                (unsigned char)ARENA_BYTE, (size_t)ARENA_SIZE);

    m_nextFreeAddress = RMAddr::PARAGRAPH_SIZE *
        (ARENA_SEGMENT + RMAddr::PARAGRAPHS_IN_SEGMENT);   // 0x20000
}

War::War(MemoryListener* memListener, WarListener* warListener, int zombieSpeed)
    : m_warListener(warListener), m_zombieSpeed(zombieSpeed)
{
    initCore();                       // no notification: the listener is not hooked yet
    m_core.setListener(memListener);  // from here on, loading and play are observed
}

void War::reset() {
    m_numWarriors = 0;
    m_numWarriorsAlive = 0;
    m_numSurvivorsAlive = 0;
    m_aliveGroupMask = 0;
    m_numLive = 0;
    m_liveDirty = false;
    m_current = 0;
    m_groupNames.clear();

    // Stacks and shared blocks start zeroed, exactly as a fresh Java byte[]
    // would. Only what the last war actually reached needs clearing: warrior
    // writes are confined to the allocated region by RestrictedMemory, so the
    // rest of the megabyte has been zero since the core was built. The first
    // 64 KB is below the arena and nothing can ever reach it, so clearing
    // starts where the stacks do; initCore() then repaints the arena itself.
    const s32 stacksLo = RMAddr(ARENA_SEGMENT, (s16)0).linear + ARENA_SIZE;   // 0x20000
    s32 clearTo = m_nextFreeAddress > m_dirtyEnd ? m_nextFreeAddress : m_dirtyEnd;
    if (clearTo > (s32)RMAddr::MEMORY_SIZE) clearTo = (s32)RMAddr::MEMORY_SIZE;
    if (m_dirtyEnd && m_dirtyEnd < stacksLo) {
        // a debugger poke can land anywhere; then it really is the whole range
        std::memset(m_core.data(), 0, (size_t)clearTo);
    } else if (clearTo > stacksLo) {
        std::memset(m_core.data() + stacksLo, 0, (size_t)(clearTo - stacksLo));
    }
    m_dirtyEnd = 0;
    initCore();
}

War::~War() = default;

RMAddr War::allocateCore(s16 size) {
    RMAddr a(m_nextFreeAddress);
    m_nextFreeAddress += (u16)size;      // size is always a paragraph multiple
    return a;
}

void War::loadWarriorGroups(const std::vector<const WarriorGroup*>& groups) {
    m_current = 0;
    // The draw removes the picked group and keeps the rest in order, which is
    // what decides who is loaded next -- so the shift stays, it is only the
    // per-war heap vector that goes.
    // 64 is far past the point where loading throws for want of warrior slots,
    // so the cap can never be what decides a game.
    static constexpr int MAX_GROUPS = 64;
    const WarriorGroup* left[MAX_GROUPS];
    int n = (int)groups.size();
    if (n > MAX_GROUPS) n = MAX_GROUPS;
    for (int i = 0; i < n; ++i) left[i] = groups[(size_t)i];
    while (n > 0) {
        const int r = m_rand.nextInt(n);
        loadWarriorGroup(*left[r]);
        for (int i = r; i < n - 1; ++i) left[i] = left[i + 1];
        --n;
    }
    rebuildAliveGroups();
    compactLive();
}

void War::loadWarriorGroup(const WarriorGroup& g) {
    RMAddr groupShared = allocateCore(GROUP_SHARED_MEMORY_SIZE);

    int gid = -1;
    for (size_t i = 0; i < m_groupNames.size(); ++i)
        if (m_groupNames[i] == g.name) { gid = (int)i; break; }
    if (gid < 0) { gid = (int)m_groupNames.size(); m_groupNames.push_back(g.name); }

    for (const WarriorData& wd : g.warriors) {
        int codeLen = (int)wd.code.size();
        s16 loadOffset = getLoadOffset(codeLen);

        RMAddr loadAddr(ARENA_SEGMENT, loadOffset);
        RMAddr stackMem = allocateCore(STACK_SIZE);
        RMAddr initialStack(stackMem.segment, STACK_SIZE);

        // Java stores into a fixed Warrior[MAX_WARRIORS]; going past it throws
        // ArrayIndexOutOfBoundsException out of the whole competition. Match the
        // "loud failure", but as a catchable fault rather than UB.
        if (m_numWarriors >= MAX_WARRIORS) throw CpuFault{Fault::Memory};

        Warrior* w = &m_store[(size_t)m_numWarriors];
        w->init(wd.name, g.name, codeLen, &m_core,
                loadAddr, initialStack, groupShared,
                GROUP_SHARED_MEMORY_SIZE, wd.type);
        w->setSpeed(wd.speed);
        w->setGroupId(gid);
        w->setSlot(wd.slot);
        m_live[(size_t)m_numWarriors] = (u8)m_numWarriors;
        ++m_numWarriors;
        ++m_numLive;

        for (int off = 0; off < codeLen; ++off)
            m_core.rawWriteByte(RMAddr(ARENA_SEGMENT, (s16)(loadOffset + off)),
                                (s8)wd.code[(size_t)off], /*notify=*/true);

        ++m_numWarriorsAlive;
        ++m_current;
        if (!w->zombie()) ++m_numSurvivorsAlive;

        if (m_warListener) m_warListener->onWarriorBirth(wd.name);
    }
}

s16 War::getLoadOffset(int warriorSize) {
    int loadAddress = 0;
    bool found = false;
    int tries = 0;
    while (!found && tries < MAX_LOADING_TRIES) {
        ++tries;
        loadAddress = m_rand.nextInt(ARENA_SIZE);
        found = true;
        if (loadAddress < MIN_GAP) found = false;
        if (loadAddress + warriorSize > ARENA_SIZE - MIN_GAP) found = false;
        for (int i = 0; i < m_numWarriors; ++i) {
            int otherLoad = (int)u16x(m_store[(size_t)i].loadOffset());
            int otherSize = m_store[(size_t)i].codeSize();
            int otherStart = otherLoad - MIN_GAP;
            int otherEnd   = otherLoad + otherSize + MIN_GAP;
            if (loadAddress + warriorSize >= otherStart && loadAddress < otherEnd)
                found = false;
        }
    }
    if (!found) throw CpuFault{Fault::Memory};   // "no room" -- Java throws a bare Exception
    return (s16)loadAddress;
}

// Every living warrior asks for its speed on every round, and it was two libm
// calls and a division each time. The curve is monotone in energy and tops out
// at MAX_SPEED, so all it really holds is sixteen thresholds -- and those are
// derived from the very same expression, once, so the numbers are identical to
// what the reference produces.
//
// Namespace scope, not a function-local static: a local would put a
// thread-safe-initialisation check on the path of every round of every war on
// every worker thread.
static constexpr int SPEED_MAX = 16;             // == War::MAX_SPEED

struct SpeedCurve {
    // step[s] is the lowest energy whose speed is s, for s in 1..SPEED_MAX
    s32 step[SPEED_MAX + 2];
    SpeedCurve() {
        for (int s = 0; s <= SPEED_MAX + 1; ++s) step[s] = 0x10000;
        step[0] = 0;
        for (int e = 1; e < 65536; ++e) {
            const int sp = (int)std::min<long>(
                SPEED_MAX, 1 + (long)(std::log((double)e) / std::log(2.0)));
            if (sp >= 0 && sp <= SPEED_MAX && e < step[sp]) step[sp] = e;
        }
    }
};
static const SpeedCurve g_speedCurve;

int War::calcSpeed(int energy) const {
    static_assert(SPEED_MAX == MAX_SPEED, "speed curve built for the wrong ceiling");
    const s32 e = energy & 0xFFFF;
    if (e == 0) return 0;
    // 1 + floor(log2(e)), which is what the reference's expression comes to;
    // the two corrections below are what make that a guarantee rather than an
    // assumption about how the compiler rounds log(e)/log(2).
    int s = 32 - __builtin_clz((u32)e);
    if (s > MAX_SPEED) s = MAX_SPEED;
    while (s > 0        && e <  g_speedCurve.step[s])     --s;
    while (s < MAX_SPEED && e >= g_speedCurve.step[s + 1]) ++s;
    return s;
}

bool War::shouldRunExtra(Warrior& w) {
    int e = (int)u16x(w.energy());
    int speed = calcSpeed(e);
    return m_rand.nextInt(MAX_SPEED) < speed;
}

// Group ids are resolved at load time, so "how many distinct groups still have
// a survivor standing" is a bitmask. Deaths are rare -- at most one per warrior
// per war -- so the mask is rebuilt whenever one happens and merely read on
// every round.
void War::rebuildAliveGroups() {
    u64 mask = 0;
    for (int i = 0; i < m_numWarriors; ++i) {
        const Warrior& w = m_store[(size_t)i];
        if (w.alive() && !w.zombie()) mask |= 1ULL << (u32)(w.groupId() & 63);
    }
    m_aliveGroupMask = mask;
}

void War::killWarrior(int idx, Fault reason) {
    Warrior& w = m_store[(size_t)idx];
    if (m_warListener) m_warListener->onWarriorDeath(w.name(), faultReason(reason));
    w.kill();
    --m_numWarriorsAlive;
    if (!w.zombie()) { --m_numSurvivorsAlive; rebuildAliveGroups(); }
    m_liveDirty = true;
}

// Drops the dead out of the live list, keeping load order. Called once at the
// end of a round that had a death rather than inside the loop, so a warrior
// dying cannot disturb the walk that is in progress.
void War::compactLive() {
    if (!m_liveDirty) return;
    int n = 0;
    for (int k = 0; k < m_numLive; ++k) {
        const u8 i = m_live[(size_t)k];
        if (m_store[(size_t)i].alive()) m_live[(size_t)n++] = i;
    }
    m_numLive = n;
    m_liveDirty = false;
}

void War::nextRound(int round) {
    // The observer calls below are pure notifications: they never touch war
    // state and never consume RNG, so behaviour is identical with or without a
    // recorder attached.
    const bool decelerate = (round % DECELERATION_ROUNDS) == 0;

    for (int k = 0; k < m_numLive; ++k) {
        const int i = (int)m_live[(size_t)k];
        Warrior& w = m_store[(size_t)i];
        m_current = i;

        try {
            if (m_obs) m_obs->onCycleBegin(i);
            w.nextOpcode();
            if (m_obs) m_obs->onCycleEnd(i);

            if (w.zombie()) {
                // Each zombie may carry its own speed; falling back to the
                // war-wide one reproduces the reference behaviour exactly.
                const int base = (w.speed() > 0) ? w.speed() : m_zombieSpeed;
                int rem = (base * ((w.type() == WarriorType::ZOMBIE_H) ? 2 : 1)) - 1;
                for (int j = 0; j < rem; ++j) {
                    if (m_obs) m_obs->onCycleBegin(i);
                    w.nextOpcode();
                    if (m_obs) m_obs->onCycleEnd(i);
                }
            } else {
                if (decelerate) {
                    const int e = (int)u16x(w.energy());
                    if (e > 0) w.setEnergy((s16)(e - 1));
                }
                if (shouldRunExtra(w)) {
                    if (m_obs) m_obs->onCycleBegin(i);
                    w.nextOpcode();
                    if (m_obs) m_obs->onCycleEnd(i);
                }
            }
        } catch (const CpuFault& f) {
            // the faulting opcode may already have written memory: close its
            // cycle so those writes are recorded before the death is logged
            if (m_obs) m_obs->onCycleEnd(i);
            killWarrior(i, f.kind);
            if (m_obs) m_obs->onDeath(i, f.kind);
        }
        if (m_obs) m_obs->onTurnEnd(i);
    }
    compactLive();
}

int War::runToEnd(int maxRounds, int round) {
    for (; round < maxRounds; ++round) {
        if (isOver()) break;
        nextRound(round);
    }
    return round;
}

void War::collectScores(std::vector<std::pair<std::string,float>>& out) const {
    float score = 1.0f / (float)m_numSurvivorsAlive;
    for (int i = 0; i < m_numWarriors; ++i) {
        const Warrior& w = m_store[(size_t)i];
        if (w.alive() && !w.zombie())
            out.emplace_back(w.name(), score);
    }
}

u64 War::stateDigest() const {
    u64 h = 0x9E3779B97F4A7C15ULL;
    auto mix = [&h](u64 v) {
        h ^= v + 0x9E3779B97F4A7C15ULL + (h << 6) + (h >> 2);
        h *= 0xBF58476D1CE4E5B9ULL;
        h ^= h >> 31;
    };
    mix(m_rand.rawSeed());
    mix((u64)(u32)m_numWarriors);
    const s32 lo = RMAddr(ARENA_SEGMENT, (s16)0).linear;
    const s32 hi = m_nextFreeAddress > lo ? m_nextFreeAddress : lo + ARENA_SIZE;
    const u8* d = m_core.data();
    s32 a = lo;
    for (; a + 8 <= hi; a += 8) { u64 v; std::memcpy(&v, d + a, 8); mix(v); }
    for (; a < hi; ++a) mix(d[a]);
    for (int i = 0; i < m_numWarriors; ++i) {
        const Warrior& w = m_store[(size_t)i];
        const CpuState& c = w.cpu();
        const s16 r[] = { c.ax, c.cx, c.dx, c.bx, c.sp, c.bp, c.si, c.di,
                          c.es, c.cs, c.ss, c.ds, c.ip, c.flags, c.energy };
        for (s16 v : r) mix((u64)(u16)v);
        mix((u64)(u8)c.bomb1 | ((u64)(u8)c.bomb2 << 8) | ((u64)(w.alive() ? 1 : 0) << 16));
    }
    return h;
}

int War::collectScoreSlots(int* slots, float& share) const {
    share = 1.0f / (float)m_numSurvivorsAlive;
    int n = 0;
    for (int i = 0; i < m_numWarriors; ++i) {
        const Warrior& w = m_store[(size_t)i];
        if (w.alive() && !w.zombie() && w.slot() >= 0) slots[n++] = w.slot();
    }
    return n;
}

} // namespace cw
