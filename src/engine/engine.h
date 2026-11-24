// =============================================================================
//  CWEST8086 engine core  --  a logically-identical C++ port of
//  il.co.codeguru.corewars8086 (the Java reference in tests/ref/java).
//
//  Scope of this header: the 8086 interpreter, the restricted real-mode memory
//  model, the war logic, and an exact re-implementation of java.util.Random.
//  "Logically identical" means: given the same warrior bytes and the same seed,
//  this produces the same per-turn memory writes, the same deaths, and the same
//  final scores as the Java engine. Bugs in the Java engine are reproduced on
//  purpose (stale OF/AF, SAHF touching only the low byte, shl16/shr16/sar16
//  updating only ZF from the no-carry-overflow path, REP rewinding IP, opcode
//  0x82 aliasing 0x80, ...).
//
//  Nothing here touches ImGui or the GUI; it builds standalone (see selftest.cpp).
// =============================================================================
#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>
#include <array>

namespace cw {

// -----------------------------------------------------------------------------
//  Fixed-width helpers. The Java code leans hard on signed byte/short overflow
//  and on Unsigned.unsignedByte / Unsigned.unsignedShort; mirror both.
// -----------------------------------------------------------------------------
using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using s8  = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;

static inline u16 u8x(s8 v)  { return (u16)((u16)v & 0xFF); }   // Unsigned.unsignedByte
static inline u32 u16x(s16 v) { return (u32)((u16)v); }         // Unsigned.unsignedShort
static inline u32 u16x(s32 v) { return (u32)((u16)(s16)v); }

// -----------------------------------------------------------------------------
//  java.util.Random  --  exact (linear congruential, 48-bit state).
//  Required because Competition advances the seed by +1 per game and the whole
//  point of the replay / split features is bit-for-bit reproducibility.
// -----------------------------------------------------------------------------
class JavaRandom {
public:
    JavaRandom() { setSeed(0); }
    explicit JavaRandom(s64 seed) { setSeed(seed); }

    void setSeed(s64 seed) {
        m_seed = (u64)(seed ^ 0x5DEECE66DLL) & ((1ULL << 48) - 1);
    }

    s32 next(int bits) {
        m_seed = (m_seed * 0x5DEECE66DULL + 0xBULL) & ((1ULL << 48) - 1);
        return (s32)(m_seed >> (48 - bits));
    }

    s32 nextInt() { return next(32); }
    u64 rawSeed() const { return m_seed; }   // for War::stateDigest

    // java.util.Random.nextInt(int bound)  (bound > 0)
    s32 nextInt(s32 bound) {
        s32 r = next(31);
        s32 m = bound - 1;
        if ((bound & m) == 0) {                 // bound is a power of two
            r = (s32)(((s64)bound * (s64)r) >> 31);
        } else {
            for (s32 u = r; u - (r = u % bound) + m < 0; u = next(31)) {}
        }
        return r;
    }

private:
    u64 m_seed = 0;
};

// -----------------------------------------------------------------------------
//  Faults. The Java engine throws typed exceptions out of Cpu.nextOpcode();
//  War.nextRound catches CpuException / MemoryException and kills the warrior.
// -----------------------------------------------------------------------------
enum class Fault : u8 {
    None = 0,
    InvalidOpcode,      // InvalidOpcodeException      (CpuException)
    UnimplementedOpcode,// UnimplementedOpcodeException (CpuException)
    UnsupportedOpcode,  // UnsupportedOpcodeException  (CpuException)  -- HLT, WAIT, IN/OUT, LOCK, FPU
    IntOpcode,          // IntOpcodeException          (CpuException)  -- INT/INT3/INTO other than 86/87
    Division,           // DivisionException           (CpuException)
    Memory              // MemoryException  -- access outside the warrior's allowed regions
};

struct CpuFault {   // thrown value
    Fault kind;
};

const char* faultReason(Fault f);   // "CPU exception" / "memory exception" as the Java listener strings

// -----------------------------------------------------------------------------
//  CpuState  --  port of cpu/CpuState.java
// -----------------------------------------------------------------------------
// Register slots the debugger is allowed to poke, in the order the inspector
// lists them. Shared by the recorder and the GUI so the two cannot drift.
enum PokeReg : int {
    PR_AX, PR_BX, PR_CX, PR_DX, PR_SI, PR_DI, PR_BP, PR_SP,
    PR_DS, PR_ES, PR_SS, PR_CS, PR_IP, PR_FLAGS, PR_NRG, PR_COUNT
};

struct CpuState {
    // 16-bit registers (stored signed to match Java `short` overflow semantics
    // exactly; helpers below cast as needed).
    //
    // Declared in the 8086's own register-index order -- AX CX DX BX SP BP SI DI
    // for the general registers, ES CS SS DS for the segments -- so decoding a
    // ModRM field is an array index rather than an eight-way switch. Nothing
    // reads these by position except reg16()/seg() just below; every other
    // reference is by name and does not care where they sit.
    s16 ax=0, cx=0, dx=0, bx=0, sp=0, bp=0, si=0, di=0;
    s16 es=0, cs=0, ss=0, ds=0;
    s16 ip=0, flags=0;
    s16 energy=0;              // virtual Energy register (speed)
    s8  bomb1=0, bomb2=0;      // virtual bomb counters (INT 86 / INT 87)

    // index 0..7 -> AX CX DX BX SP BP SI DI ;  index 0..3 -> ES CS SS DS
    s16*       reg16s()       { return &ax; }
    const s16* reg16s() const { return &ax; }
    s16*       segs()         { return &es; }
    const s16* segs()   const { return &es; }

    // one of the slots above, for the debugger's edit tool
    s16* slot(int i) {
        switch (i) {
            case PR_AX: return &ax;  case PR_BX: return &bx;
            case PR_CX: return &cx;  case PR_DX: return &dx;
            case PR_SI: return &si;  case PR_DI: return &di;
            case PR_BP: return &bp;  case PR_SP: return &sp;
            case PR_DS: return &ds;  case PR_ES: return &es;
            case PR_SS: return &ss;  case PR_CS: return &cs;
            case PR_IP: return &ip;  case PR_FLAGS: return &flags;
            case PR_NRG: return &energy;
        }
        return nullptr;
    }

    // 8-bit accessors
    s8 al() const { return (s8)ax; }
    s8 bl() const { return (s8)bx; }
    s8 cl() const { return (s8)cx; }
    s8 dl() const { return (s8)dx; }
    s8 ah() const { return (s8)(ax >> 8); }
    s8 bh() const { return (s8)(bx >> 8); }
    s8 ch() const { return (s8)(cx >> 8); }
    s8 dh() const { return (s8)(dx >> 8); }
    void set_al(s8 v){ ax = (s16)((ax & 0xFF00) | u8x(v)); }
    void set_bl(s8 v){ bx = (s16)((bx & 0xFF00) | u8x(v)); }
    void set_cl(s8 v){ cx = (s16)((cx & 0xFF00) | u8x(v)); }
    void set_dl(s8 v){ dx = (s16)((dx & 0xFF00) | u8x(v)); }
    void set_ah(s8 v){ ax = (s16)((ax & 0x00FF) | (u8x(v) << 8)); }
    void set_bh(s8 v){ bx = (s16)((bx & 0x00FF) | (u8x(v) << 8)); }
    void set_ch(s8 v){ cx = (s16)((cx & 0x00FF) | (u8x(v) << 8)); }
    void set_dh(s8 v){ dx = (s16)((dx & 0x00FF) | (u8x(v) << 8)); }

    enum : u16 {
        F_CARRY=0x0001, F_PARITY=0x0004, F_AUX=0x0010, F_ZERO=0x0040,
        F_SIGN=0x0080, F_TRAP=0x0100, F_INT=0x0200, F_DIR=0x0400, F_OVER=0x0800
    };
    bool cf() const { return ((u16)flags & F_CARRY)  == F_CARRY; }
    bool pf() const { return ((u16)flags & F_PARITY) == F_PARITY; }
    bool af() const { return ((u16)flags & F_AUX)    == F_AUX; }
    bool zf() const { return ((u16)flags & F_ZERO)   == F_ZERO; }
    bool sf() const { return ((u16)flags & F_SIGN)   == F_SIGN; }
    bool tf() const { return ((u16)flags & F_TRAP)   == F_TRAP; }
    bool if_() const{ return ((u16)flags & F_INT)    == F_INT; }
    bool df() const { return ((u16)flags & F_DIR)    == F_DIR; }
    bool of() const { return ((u16)flags & F_OVER)   == F_OVER; }
    void set_cf(bool b){ setflag(F_CARRY, b); }
    void set_pf(bool b){ setflag(F_PARITY, b); }
    void set_af(bool b){ setflag(F_AUX, b); }
    void set_zf(bool b){ setflag(F_ZERO, b); }
    void set_sf(bool b){ setflag(F_SIGN, b); }
    void set_tf(bool b){ setflag(F_TRAP, b); }
    void set_if(bool b){ setflag(F_INT, b); }
    void set_df(bool b){ setflag(F_DIR, b); }
    void set_of(bool b){ setflag(F_OVER, b); }
private:
    void setflag(u16 mask, bool b){ flags = (s16)(b ? ((u16)flags | mask) : ((u16)flags & ~mask)); }
};

// -----------------------------------------------------------------------------
//  Real-mode address arithmetic  --  port of memory/RealModeAddress.java
// -----------------------------------------------------------------------------
struct RMAddr {
    s16 segment;
    s16 offset;
    s32 linear;   // (u16(seg)*16 + u16(off)) % 0x100000

    static constexpr s32 PARAGRAPH_SIZE       = 0x10;
    static constexpr s32 PARAGRAPHS_IN_SEGMENT= 0x1000;
    static constexpr s32 NUM_PARAGRAPHS       = 64 * 1024;
    static constexpr s32 MEMORY_SIZE          = NUM_PARAGRAPHS * PARAGRAPH_SIZE; // 0x100000

    RMAddr(s16 seg, s16 off) : segment(seg), offset(off) {
        // Both halves are unsigned 16-bit, so the sum cannot be negative and
        // the wrap is a mask. It used to go through a signed 64-bit modulo,
        // which every single memory reference paid for.
        const u32 full = u16x(seg) * (u32)PARAGRAPH_SIZE + u16x(off);
        linear = (s32)(full & (u32)(MEMORY_SIZE - 1));
    }
    explicit RMAddr(s32 lin) {
        lin %= MEMORY_SIZE;
        if (lin < 0) lin += MEMORY_SIZE;          // Java % on non-negative here, but be safe
        s32 seg = (s32)u16x(lin / PARAGRAPH_SIZE);
        s32 off = (s32)u16x(lin - seg * PARAGRAPH_SIZE);
        segment = (s16)seg;
        offset  = (s16)off;
        linear  = lin;
    }
};

// -----------------------------------------------------------------------------
//  Memory  --  the physical core plus per-warrior access restriction.
//  Ports memory/RealModeMemoryImpl.java + RestrictedAccessRealModeMemory.java
//  + AbstractRealModeMemory (word = two byte ops with in-segment offset wrap).
// -----------------------------------------------------------------------------
struct Region { s32 lo, hi; bool has(s32 a) const { return a >= lo && a <= hi; } };

// Write-notification hook. The old byte is handed over so a recorder can build
// a reversible xor delta without re-reading memory.
struct MemoryListener {
    virtual ~MemoryListener() = default;
    virtual void onWrite(s32 linear, u8 oldVal, u8 newVal) = 0;
};

class Core {
public:
    Core() { m_data.assign(RMAddr::MEMORY_SIZE, 0); }

    u8*       data()       { return m_data.data(); }
    const u8* data() const { return m_data.data(); }

    void setListener(MemoryListener* l) { m_listener = l; }

    // raw, unrestricted (used by War to build the arena / by the debugger)
    s8   rawByte(const RMAddr& a) const { return (s8)m_data[a.linear]; }
    void rawWriteByte(const RMAddr& a, s8 v, bool notify) {
        const u8 old = m_data[a.linear];
        m_data[a.linear] = (u8)v;
        if (notify && m_listener) m_listener->onWrite(a.linear, old, (u8)v);
    }

private:
    std::vector<u8> m_data;
    MemoryListener* m_listener = nullptr;
    friend class RestrictedMemory;
};

class RestrictedMemory {
public:
    // A warrior is given exactly three regions -- its stack, the arena, and its
    // group's shared block -- so the count is fixed and the three range checks
    // unroll into branchless straight-line code on the path of every single
    // memory reference. They are disjoint, so the order they are tested in
    // cannot change an answer; init() puts the widest (the arena) first because
    // that is where nearly every access lands.
    //
    // A region is held as base + span so "inside" is one unsigned compare:
    // (u32)(addr - lo) <= span. Everything here is in the header on purpose --
    // these are the innermost operations of the interpreter and they have to
    // inline into it.
    static constexpr int MAX_REGIONS = 3;

    RestrictedMemory() = default;
    void init(Core* core,
              const Region* rd, int nrd, const Region* wr, int nwr,
              const Region* ex, int nex);

    s8 readByte(const RMAddr& a) const {
        const u32 lin = (u32)a.linear;
        if (!allowed(m_rd, lin)) throw CpuFault{Fault::Memory};
        return (s8)m_data[lin];
    }
    void writeByte(const RMAddr& a, s8 v) {
        const u32 lin = (u32)a.linear;
        if (!allowed(m_wr, lin)) throw CpuFault{Fault::Memory};
        store(lin, (u8)v);
    }
    s8 readExecuteByte(const RMAddr& a) const {
        const u32 lin = (u32)a.linear;
        if ((u32)(lin - m_exLo) > m_exSpan) throw CpuFault{Fault::Memory};
        return (s8)m_data[lin];
    }

    // A word is two byte accesses whose offsets wrap inside the segment, which
    // is what the Java base class does. When the pair neither wraps nor
    // straddles a region -- the overwhelmingly common case -- it is one check
    // and one 16-bit load instead.
    s16 readWord(const RMAddr& a) const {
        const u32 lin = (u32)a.linear;
        if ((u16)a.offset != 0xFFFFu && allowedPair(m_rd, lin)) return load16(lin);
        const s8 low  = readByte(a);
        const s8 high = readByte(RMAddr(a.segment, (s16)(a.offset + 1)));
        return (s16)((u8x(high) << 8) | u8x(low));
    }
    s16 readExecuteWord(const RMAddr& a) const {
        const u32 lin = (u32)a.linear;
        if ((u16)a.offset != 0xFFFFu && (u32)(lin - m_exLo) < m_exSpan) return load16(lin);
        const s8 low  = readExecuteByte(a);
        const s8 high = readExecuteByte(RMAddr(a.segment, (s16)(a.offset + 1)));
        return (s16)((u8x(high) << 8) | u8x(low));
    }
    void writeWord(const RMAddr& a, s16 v) {
        const u32 lin = (u32)a.linear;
        // A recorder wants the two bytes as two events, so it keeps the slow path.
        if (!m_listener && (u16)a.offset != 0xFFFFu && allowedPair(m_wr, lin)) {
            const u16 w = (u16)v;
            std::memcpy(m_data + lin, &w, 2);
            return;
        }
        writeByte(a, (s8)v);
        writeByte(RMAddr(a.segment, (s16)(a.offset + 1)), (s8)(v >> 8));
    }

private:
    struct Spans { u32 lo[MAX_REGIONS]; u32 span[MAX_REGIONS]; };

    static bool allowed(const Spans& s, u32 a) {
        // bitwise-or, not ||: three independent compares beat two branches
        return (((u32)(a - s.lo[0]) <= s.span[0]) |
                ((u32)(a - s.lo[1]) <= s.span[1]) |
                ((u32)(a - s.lo[2]) <= s.span[2])) != 0;
    }
    // both `a` and `a+1` inside one and the same region. Strictly-less, so a
    // zero span (an unused slot) can never match.
    static bool allowedPair(const Spans& s, u32 a) {
        return (((u32)(a - s.lo[0]) < s.span[0]) |
                ((u32)(a - s.lo[1]) < s.span[1]) |
                ((u32)(a - s.lo[2]) < s.span[2])) != 0;
    }
    s16 load16(u32 lin) const { u16 w; std::memcpy(&w, m_data + lin, 2); return (s16)w; }
    void store(u32 lin, u8 v) {
        if (m_listener) {
            const u8 old = m_data[lin];
            m_data[lin] = v;
            m_listener->onWrite((s32)lin, old, v);
        } else {
            m_data[lin] = v;
        }
    }

    // __restrict says what is true by construction: the core buffer is not the
    // CpuState, so a store into memory cannot invalidate a register the
    // interpreter is holding. Without it every flag update has to be spilled
    // and reloaded around each memory access.
    u8* __restrict  m_data     = nullptr;
    MemoryListener* m_listener = nullptr;
    Spans  m_rd{}, m_wr{};
    u32    m_exLo = 1, m_exSpan = 0;      // never matches until init()
};

// -----------------------------------------------------------------------------
//  Smart check (CWEST's own, never part of a real war)
//
//  `int 0x89` in source assembles to INT 87h, so a survivor that uses it plays
//  exactly like one that wrote INT 87h. Only the smart check builds a second
//  copy with CD 89 at those places and installs an override on the thread
//  that plays it: INT 89h is then INT 87h searching for the override's AX:DX
//  instead of the registers'. With no override installed -- every ordinary
//  war, Debug, the tournament -- INT 89h faults like any other unknown
//  interrupt, exactly as the reference engine does.
// -----------------------------------------------------------------------------
struct SmartCheckOverride {
    s16 ax = 0, dx = 0;
    // The probe: INT 89h finds nothing (it only uses up the bomb, as a search
    // that misses does) and keeps what it could have searched -- the 64 KB of
    // the ES segment -- one entry per call that had a bomb left. An entry is
    // empty when part of the segment could not be read (a real search there
    // would fault part-way, which a miss does not). Every candidate that
    // matches nowhere in any entry plays exactly the probe's war.
    bool probe = false;
    std::vector<std::vector<u8>>* seen = nullptr;
};
// Per thread; nullptr (the default) switches INT 89h off again.
void setSmartCheckOverride(const SmartCheckOverride* o);

// -----------------------------------------------------------------------------
//  Cpu  --  port of cpu/Cpu.java (+ OpcodeFetcher, IndirectAddressingDecoder,
//  RegisterIndexingDecoder folded in).
// -----------------------------------------------------------------------------
class Cpu {
public:
    Cpu() = default;

    // Executes exactly one instruction. Throws CpuFault on any error.
    void nextOpcode();

    // The registers and the access windows live here, by value. They used to be
    // held by the Warrior and reached through pointers, which put a dependent
    // load in front of every single register reference.
    CpuState&        state()       { return m_state; }
    const CpuState&  state() const { return m_state; }
    RestrictedMemory& mem()        { return m_mem; }

private:
    CpuState         m_state;
    RestrictedMemory m_mem;

    // --- fetcher (CS:IP) ---
    s8  fByte();
    s16 fWord();

    // --- register-index decode ---
    s8  reg8(u8 i) const;   void setReg8(u8 i, s8 v);
    s16 reg16(u8 i) const;  void setReg16(u8 i, s16 v);
    s16 seg(u8 i) const;    void setSeg(u8 i, s16 v);

    // --- indirect-addressing decode (ModRM) ---
    u8   m_regIndex = 0;
    u8   m_memIndex = 0;
    bool m_memIsReg = false;      // mode 3
    RMAddr m_memAddr{(s16)0,(s16)0};
    void modrm();                 // reads the mode byte, resolves the mem operand
    bool hasMemAddr() const { return !m_memIsReg; }
    s8   getMem8() const;   void setMem8(s8 v);
    s16  getMem16() const;  void setMem16(s16 v);
    s8   getReg8() const  { return reg8(m_regIndex); }
    s16  getReg16() const { return reg16(m_regIndex); }
    s16  getSeg() const   { return seg(m_regIndex); }
    void setRegOp8(s8 v)  { setReg8(m_regIndex, v); }
    void setRegOp16(s16 v){ setReg16(m_regIndex, v); }
    void setSegOp(s16 v)  { setSeg(m_regIndex, v); }

    // --- opcode group dispatch ---
    void op0X(s8), op1X(s8), op2X(s8), op3X(s8), op4X(s8), op5X(s8), op6X(s8), op7X(s8);
    void op8X(s8), op9X(s8), opAX(s8), opBX(s8), opCX(s8), opDX(s8), opEX(s8), opFX(s8);

    // --- stack ---
    void push(s16 v);
    s16  pop();

    // --- flag updates ---
    void updateFlags8(s16 v);
    void updateFlags16(s32 v);
    void updateFlagsNCO8(s8 v);
    void updateFlagsNCO16(s16 v);
    static bool parity(s8 v);

    // --- ALU (each updates flags exactly as the Java helpers do) ---
    s8  add8(s8,s8);  s16 add16(s16,s16);
    s8  adc8(s8,s8);  s16 adc16(s16,s16);
    s8  sub8(s8,s8);  s16 sub16(s16,s16);
    s8  sbb8(s8,s8);  s16 sbb16(s16,s16);
    s8  or8 (s8,s8);  s16 or16 (s16,s16);
    s8  and8(s8,s8);  s16 and16(s16,s16);
    s8  xor8(s8,s8);  s16 xor16(s16,s16);
    s8  inc8(s8);     s16 inc16(s16);
    s8  dec8(s8);     s16 dec16(s16);

    // --- control transfer ---
    void callNear(s16 off);
    void callFar(s16 seg_, s16 off);

    // --- string ops ---
    void movsb(); void movsw();
    void cmpsb(); void cmpsw();
    void stosb(); void stosw(); void stosdw();
    void lodsb(); void lodsw();
    void scasb(); void scasw();

    // --- shifts / rotates (operate on the current ModRM mem/reg operand) ---
    void rol8(int),ror8(int),rcl8(int),rcr8(int),shl8(int),shr8(int),sar8(int);
    void rol16(int),ror16(int),rcl16(int),rcr16(int),shl16(int),shr16(int),sar16(int);

    // --- virtual interrupts ---
    void int86();   // 64x stosdw if a bomb is left
    void int87();   // find/replace a 4-byte pattern if a bomb is left
    void int89();   // the smart check's INT 87h (see SmartCheckOverride)
    void smartBomb(s16 ax, s16 dx);
};

// -----------------------------------------------------------------------------
//  Warrior / group model  --  ports of war/Warrior.java, WarriorGroup.java,
//  WarriorData.java, WarriorType.java
// -----------------------------------------------------------------------------
enum class WarriorType : u8 { SURVIVOR, SURVIVOR_1, SURVIVOR_2, ZOMBIE, ZOMBIE_H };
inline bool isZombie(WarriorType t){ return t == WarriorType::ZOMBIE || t == WarriorType::ZOMBIE_H; }

struct WarriorData {
    std::string      name;
    std::vector<u8>  code;      // already clamped to MAX_WARRIOR_SIZE (512) by the loader
    WarriorType      type = WarriorType::SURVIVOR;
    // Zombies only: cycles per turn for this one warrior. 0 keeps the war-wide
    // default, which is what the reference implementation gives every zombie.
    int              speed = 0;
    // Caller's own tag, carried through to the scores. A tournament sets it to
    // the warrior's row in the standings table so a finished war can be folded
    // in without looking anything up by name.
    int              slot  = -1;
};

struct WarriorGroup {
    std::string               name;
    std::vector<WarriorData>   warriors;
    // The front end's own identity for the group (its folder and name). The
    // engine never reads it; a replay carries it so the debugger can tell two
    // groups apart by more than their names.
    std::string               key;
};

class Warrior {
public:
    // Warriors live inside the War, by value, and hold pointers into themselves
    // (the Cpu's state and memory), so they are neither copied nor moved: the
    // war builds them once and re-inits them for each new game.
    Warrior() : m_loadAddr((s16)0, (s16)0) {}
    Warrior(const Warrior&) = delete;
    Warrior& operator=(const Warrior&) = delete;

    void init(std::string name, std::string groupName, int codeSize, Core* core,
              RMAddr loadAddr, RMAddr initialStack, RMAddr groupShared,
              s16 groupSharedSize, WarriorType type);

    bool  alive() const { return m_alive; }
    void  kill()        { m_alive = false; }
    bool  zombie() const{ return isZombie(m_type); }
    // 0 means "use the war's zombie speed"
    int   speed() const { return m_speed; }
    void  setSpeed(int v){ m_speed = v; }
    // Index of this warrior's group, so counting distinct live groups does not
    // have to compare names.
    int   groupId() const { return m_groupId; }
    void  setGroupId(int v){ m_groupId = v; }
    int   slot() const   { return m_slot; }
    void  setSlot(int v) { m_slot = v; }
    WarriorType type() const { return m_type; }
    const std::string& name() const { return m_name; }
    const std::string& groupName() const { return m_groupName; }
    s16   loadOffset() const { return m_loadAddr.offset; }
    int   codeSize() const { return m_codeSize; }
    s16   energy() const { return m_cpu.state().energy; }
    void  setEnergy(s16 v){ m_cpu.state().energy = v; }
    CpuState&       cpu()       { return m_cpu.state(); }
    const CpuState& cpu() const { return m_cpu.state(); }

    void nextOpcode() { m_cpu.nextOpcode(); }

private:
    // Hot first: the round loop touches the CPU (registers plus access
    // windows) and the liveness flags of every warrior still standing, and
    // nothing else.
    Cpu               m_cpu;
    bool              m_alive = true;
    WarriorType       m_type = WarriorType::SURVIVOR;
    int               m_speed = 0;
    int               m_groupId = 0;
    int               m_slot = -1;
    int               m_codeSize = 0;
    RMAddr            m_loadAddr;
    std::string       m_name, m_groupName;
};

// -----------------------------------------------------------------------------
//  War  --  port of war/War.java
// -----------------------------------------------------------------------------
struct WarListener {
    virtual ~WarListener() = default;
    virtual void onWarriorBirth(const std::string& /*name*/) {}
    virtual void onWarriorDeath(const std::string& /*name*/, const char* /*reason*/) {}
};

// Optional per-opcode hook used by the replay recorder. One "cycle" is one
// Cpu::nextOpcode() call; a warrior's turn is 1..N cycles (energy bonus opcode
// or zombie speed). Attaching an observer does not change any game logic.
struct CycleObserver {
    virtual ~CycleObserver() = default;
    virtual void onCycleBegin(int warrior) = 0;
    virtual void onCycleEnd(int warrior) = 0;          // commits the cycle
    virtual void onTurnEnd(int warrior) = 0;           // flags the last cycle
    virtual void onDeath(int warrior, Fault reason) = 0;
};

class War {
public:
    static constexpr s16 ARENA_SEGMENT = 0x1000;
    static constexpr s32 ARENA_SIZE    = RMAddr::PARAGRAPHS_IN_SEGMENT * RMAddr::PARAGRAPH_SIZE; // 0x10000
    // The reference stops at 20 (Warrior[20]), which caps a war at nine groups of
    // two plus a zombie or two. Nothing else in the engine depends on it, so this
    // one holds more: up to the point where the arena itself runs out of room
    // (MIN_GAP between warriors), and a war of 20 or fewer plays exactly as before.
    static constexpr int MAX_WARRIORS  = 64;
    static constexpr int REFERENCE_MAX_WARRIORS = 20;   // what the Java and GPU engines hold
    static constexpr int MAX_ROUND     = 200000;   // Competition.MAX_ROUND
    static constexpr s16 STACK_SIZE    = 2 * 1024; // per-warrior private stack
    static constexpr s16 GROUP_SHARED_MEMORY_SIZE = 1024;   // per-group shared block

    War(MemoryListener* memListener, WarListener* warListener, int zombieSpeed);
    ~War();
    War(const War&) = delete;
    War& operator=(const War&) = delete;

    void setSeed(s64 seed) { m_rand.setSeed(seed); }

    // Debugger support: change one byte of core, or reach a warrior's registers,
    // between opcodes. The write goes through the notify path so an attached
    // recorder sees it as an ordinary delta.
    void pokeByte(s32 linear, u8 v) {
        m_core.rawWriteByte(RMAddr(linear), (s8)v, true);
        // a poke is the one write that can land outside the allocated
        // region, so reset() has to know how far it went
        if (linear + 1 > m_dirtyEnd) m_dirtyEnd = linear + 1;
    }
    Warrior* warriorMut(int i) {
        return (i >= 0 && i < m_numWarriors) ? &m_store[(size_t)i] : nullptr;
    }
    void setObserver(CycleObserver* o) { m_obs = o; }
    void loadWarriorGroups(const std::vector<const WarriorGroup*>& groups);

    // High-water mark of core memory actually in use (arena + stacks + shared).
    // Everything from RMAddr(ARENA_SEGMENT,0).linear up to here is worth saving.
    s32  usedMemoryEnd() const { return m_nextFreeAddress; }

    // Put the war back to the state a freshly built one is in, keeping the
    // megabyte of core allocated. A tournament runs thousands of wars, and
    // building one was dominated by that allocation and its page faults.
    void reset();

    void nextRound(int round);

    // The whole war, from `round` to its end or to `maxRounds`, without leaving
    // the engine in between. A tournament runs millions of rounds whose only
    // product is the final standings, and the round-at-a-time interface makes
    // the caller pay for an isOver() and a call boundary on each one. Returns
    // the round it stopped at. Identical to looping over nextRound().
    int  runToEnd(int maxRounds, int round = 0);

    bool isOver() const {
        return m_numWarriorsAlive <= 1
            || m_numSurvivorsAlive == 1
            || (m_numSurvivorsAlive == 2 && countAliveSurvivorGroups() == 1);
    }

    int  currentWarrior() const { return m_current; }
    int  numWarriors() const    { return m_numWarriors; }
    int  numRemainingSurvivors() const { return m_numSurvivorsAlive; }

    Warrior&       warrior(int i)       { return m_store[(size_t)i]; }
    const Warrior& warrior(int i) const { return m_store[(size_t)i]; }

    Core&       core()       { return m_core; }
    const Core& core() const { return m_core; }

    // scoring: each still-alive survivor gets 1/numSurvivorsAlive
    void collectScores(std::vector<std::pair<std::string,float>>& out) const;

    // Same shares, addressed by the caller's own WarriorData::slot instead of
    // by name -- no strings, no lookup table, nothing allocated per war.
    // Slots below zero are skipped. Returns the number of scorers.
    int  collectScoreSlots(int* slots, float& share) const;

    // A 64-bit hash of everything the rest of the war depends on: the core in
    // use, every warrior's registers and whether it lives, and the random
    // generator. Two wars between the same warriors with the same digest at
    // the same round play out the same from there (the smart check uses it to
    // stop a candidate's war the moment it is the probe's war again).
    u64  stateDigest() const;

private:
    void loadWarriorGroup(const WarriorGroup& g);
    void initCore();
    RMAddr allocateCore(s16 size);
    s16  getLoadOffset(int warriorSize);
    bool shouldRunExtra(Warrior& w);
    int  calcSpeed(int energy) const;
    // The mask is kept up to date as warriors die, so the count that isOver()
    // asks for on every round of a two-survivor war is a popcount and not a
    // walk over the whole roster.
    long countAliveSurvivorGroups() const { return (long)__builtin_popcountll(m_aliveGroupMask); }
    void rebuildAliveGroups();
    void killWarrior(int idx, Fault reason);
    void compactLive();


    static constexpr s8  ARENA_BYTE = (s8)0xCC;
    static constexpr int MAX_LOADING_TRIES = 100;
    static constexpr int MIN_GAP = 1024;
    static constexpr int MAX_SPEED = 16;
    static constexpr int DECELERATION_ROUNDS = 5;

    Core                m_core;
    JavaRandom          m_rand;
    WarListener*        m_warListener;
    CycleObserver*      m_obs = nullptr;
    int                 m_zombieSpeed;

    // group name -> index, resolved once at load time. Names are compared here
    // and nowhere else, so two groups sharing a name still count as one, which
    // is what the set of names used to do.
    std::vector<std::string> m_groupNames;

    // Warriors by value: building one is a memset, not an allocation, and the
    // round loop walks a contiguous block instead of chasing twenty pointers.
    std::array<Warrior, MAX_WARRIORS> m_store{};

    // Indices of the warriors still standing, in load order. The round loop
    // reads this, so a war that is down to two live warriors out of twenty
    // stops paying for the other eighteen on every one of its rounds.
    std::array<u8, MAX_WARRIORS> m_live{};
    int m_numLive = 0;
    bool m_liveDirty = false;

    int m_numWarriors = 0;
    int m_numWarriorsAlive = 0;
    int m_numSurvivorsAlive = 0;
    u64 m_aliveGroupMask = 0;    // one bit per group id; a war holds at most 64 groups
    s32 m_nextFreeAddress = 0;
    s32 m_dirtyEnd = 0;          // high-water mark of anything written
    int m_current = 0;
};

} // namespace cw
