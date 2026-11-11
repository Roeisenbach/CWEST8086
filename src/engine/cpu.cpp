// =============================================================================
//  cw::Cpu  --  faithful port of il.co.codeguru.corewars8086.cpu.Cpu
//  (with OpcodeFetcher, IndirectAddressingDecoder and RegisterIndexingDecoder
//  folded in). Behaviour, including the engine's own bugs, matches the Java
//  reference in tests/ref/java/il/co/codeguru/corewars8086/cpu/Cpu.java line-for-line.
// =============================================================================
#include "engine.h"

namespace cw {

// -----------------------------------------------------------------------------
//  Opcode fetch (CS:IP)  --  OpcodeFetcher.java
// -----------------------------------------------------------------------------
s8 Cpu::fByte() {
    RMAddr a(m_state.cs, m_state.ip);
    m_state.ip = (s16)(m_state.ip + 1);
    return m_mem.readExecuteByte(a);
}
s16 Cpu::fWord() {
    RMAddr a(m_state.cs, m_state.ip);
    m_state.ip = (s16)(m_state.ip + 2);
    return m_mem.readExecuteWord(a);
}

// -----------------------------------------------------------------------------
//  Register-index decode  --  RegisterIndexingDecoder.java
// -----------------------------------------------------------------------------
// CpuState declares the registers in index order, so these are array accesses.
// They used to be eight-way switches, and one of them is on the path of very
// nearly every instruction the engine executes.
static_assert(sizeof(CpuState) >= 8 * sizeof(s16), "register file too small");

s8 Cpu::reg8(u8 i) const {
    // 0..3 are the low bytes of AX CX DX BX, 4..7 their high bytes
    const s16 v = m_state.reg16s()[i & 3];
    return (i & 4) ? (s8)((u16)v >> 8) : (s8)v;
}
void Cpu::setReg8(u8 i, s8 v) {
    s16& r = m_state.reg16s()[i & 3];
    r = (i & 4) ? (s16)(((u16)r & 0x00FF) | (u8x(v) << 8))
                : (s16)(((u16)r & 0xFF00) | u8x(v));
}
s16 Cpu::reg16(u8 i) const        { return m_state.reg16s()[i & 7]; }
void Cpu::setReg16(u8 i, s16 v)   { m_state.reg16s()[i & 7] = v; }
s16 Cpu::seg(u8 i) const          { return m_state.segs()[i & 3]; }
void Cpu::setSeg(u8 i, s16 v)     { m_state.segs()[i & 3] = v; }

// -----------------------------------------------------------------------------
//  Indirect-addressing decode  --  IndirectAddressingDecoder.java
// -----------------------------------------------------------------------------
void Cpu::modrm() {
    u8 modeByte = (u8)fByte();
    u8 mode   = (u8)((modeByte >> 6) & 0x03);
    m_regIndex = (u8)((modeByte >> 3) & 0x07);
    m_memIndex = (u8)(modeByte & 0x07);
    m_memIsReg = false;

    // mode 3 -- a register operand -- is the commonest form by a wide margin,
    // and it needs none of the base/index registers below
    if (mode == 3) { m_memIsReg = true; return; }

    const s16 BX = m_state.bx, SI = m_state.si, DI = m_state.di, BP = m_state.bp;
    const s16 DS = m_state.ds, SS = m_state.ss;

    if (mode == 0) {
        switch (m_memIndex) {
            case 0: m_memAddr = RMAddr(DS, (s16)(BX + SI)); break;
            case 1: m_memAddr = RMAddr(DS, (s16)(BX + DI)); break;
            case 2: m_memAddr = RMAddr(SS, (s16)(BP + SI)); break;
            case 3: m_memAddr = RMAddr(SS, (s16)(BP + DI)); break;
            case 4: m_memAddr = RMAddr(DS, SI); break;
            case 5: m_memAddr = RMAddr(DS, DI); break;
            case 6: m_memAddr = RMAddr(DS, fWord()); break;
            case 7: m_memAddr = RMAddr(DS, BX); break;
        }
    } else if (mode == 1) {
        s8 d = 0;
        switch (m_memIndex) {
            case 0: d = fByte(); m_memAddr = RMAddr(DS, (s16)(BX + SI + d)); break;
            case 1: d = fByte(); m_memAddr = RMAddr(DS, (s16)(BX + DI + d)); break;
            case 2: d = fByte(); m_memAddr = RMAddr(SS, (s16)(BP + SI + d)); break;
            case 3: d = fByte(); m_memAddr = RMAddr(SS, (s16)(BP + DI + d)); break;
            case 4: d = fByte(); m_memAddr = RMAddr(DS, (s16)(SI + d)); break;
            case 5: d = fByte(); m_memAddr = RMAddr(DS, (s16)(DI + d)); break;
            case 6: d = fByte(); m_memAddr = RMAddr(SS, (s16)(BP + d)); break;
            case 7: d = fByte(); m_memAddr = RMAddr(DS, (s16)(BX + d)); break;
        }
    } else if (mode == 2) {
        s16 d = 0;
        switch (m_memIndex) {
            case 0: d = fWord(); m_memAddr = RMAddr(DS, (s16)(BX + SI + d)); break;
            case 1: d = fWord(); m_memAddr = RMAddr(DS, (s16)(BX + DI + d)); break;
            case 2: d = fWord(); m_memAddr = RMAddr(SS, (s16)(BP + SI + d)); break;
            case 3: d = fWord(); m_memAddr = RMAddr(SS, (s16)(BP + DI + d)); break;
            case 4: d = fWord(); m_memAddr = RMAddr(DS, (s16)(SI + d)); break;
            case 5: d = fWord(); m_memAddr = RMAddr(DS, (s16)(DI + d)); break;
            case 6: d = fWord(); m_memAddr = RMAddr(SS, (s16)(BP + d)); break;
            case 7: d = fWord(); m_memAddr = RMAddr(DS, (s16)(BX + d)); break;
        }
    }   // mode 3 returned above
}

s8  Cpu::getMem8()  const { return m_memIsReg ? reg8(m_memIndex)  : m_mem.readByte(m_memAddr); }
s16 Cpu::getMem16() const { return m_memIsReg ? reg16(m_memIndex) : m_mem.readWord(m_memAddr); }
void Cpu::setMem8(s8 v)   { if (m_memIsReg) setReg8(m_memIndex, v);  else m_mem.writeByte(m_memAddr, v); }
void Cpu::setMem16(s16 v) { if (m_memIsReg) setReg16(m_memIndex, v); else m_mem.writeWord(m_memAddr, v); }

// -----------------------------------------------------------------------------
//  Stack
// -----------------------------------------------------------------------------
void Cpu::push(s16 v) {
    m_state.sp = (s16)(m_state.sp - 2);
    RMAddr sp(m_state.ss, m_state.sp);
    m_mem.writeWord(sp, v);
}
s16 Cpu::pop() {
    RMAddr sp(m_state.ss, m_state.sp);
    s16 v = m_mem.readWord(sp);
    m_state.sp = (s16)(m_state.sp + 2);
    return v;
}

// -----------------------------------------------------------------------------
//  Flag updates
// -----------------------------------------------------------------------------
// The 256-entry parity table packed into four words: 32 bytes of rodata that
// stay in L1 next to everything else, instead of a 256-byte array.
static const u64 PARITY_BITS[4] = {
    0x9669699669969669ULL, 0x6996966996696996ULL,
    0x6996966996696996ULL, 0x9669699669969669ULL
};
bool Cpu::parity(s8 v) {
    const unsigned b = (unsigned)u8x(v) & 0xFFu;
    return ((PARITY_BITS[b >> 6] >> (b & 63)) & 1ULL) != 0;
}

// One read-modify-write of `flags` instead of one per flag. The bits are
// disjoint, so folding them costs nothing in behaviour -- and it takes the
// interpreter's hottest store off the critical path of every ALU operation.
void Cpu::updateFlagsNCO8(s8 v) {
    u16 f = (u16)((u16)m_state.flags & (u16)~(u16)(CpuState::F_PARITY | CpuState::F_SIGN | CpuState::F_ZERO));
    if (parity(v))     f |= CpuState::F_PARITY;
    if (v & 0x80)      f |= CpuState::F_SIGN;
    if (v == 0)        f |= CpuState::F_ZERO;
    m_state.flags = (s16)f;
}
void Cpu::updateFlagsNCO16(s16 v) {
    u16 f = (u16)((u16)m_state.flags & (u16)~(u16)(CpuState::F_PARITY | CpuState::F_SIGN | CpuState::F_ZERO));
    if (parity((s8)v))          f |= CpuState::F_PARITY;
    if ((u16)v & 0x8000)        f |= CpuState::F_SIGN;
    if (v == 0)                 f |= CpuState::F_ZERO;
    m_state.flags = (s16)f;
}
void Cpu::updateFlags8(s16 v) {
    const s8 b = (s8)v;
    u16 f = (u16)((u16)m_state.flags & (u16)~(u16)(CpuState::F_CARRY | CpuState::F_PARITY |
                                                 CpuState::F_SIGN  | CpuState::F_ZERO));
    if ((u16)v & 0xFF00) f |= CpuState::F_CARRY;
    if (parity(b))       f |= CpuState::F_PARITY;
    if (b & 0x80)        f |= CpuState::F_SIGN;
    if (b == 0)          f |= CpuState::F_ZERO;
    m_state.flags = (s16)f;
}
void Cpu::updateFlags16(s32 v) {
    const s16 w = (s16)v;
    u16 f = (u16)((u16)m_state.flags & (u16)~(u16)(CpuState::F_CARRY | CpuState::F_PARITY |
                                                 CpuState::F_SIGN  | CpuState::F_ZERO));
    if ((u32)v & 0xFFFF0000u) f |= CpuState::F_CARRY;
    if (parity((s8)w))        f |= CpuState::F_PARITY;
    if ((u16)w & 0x8000)      f |= CpuState::F_SIGN;
    if (w == 0)               f |= CpuState::F_ZERO;
    m_state.flags = (s16)f;
}

// -----------------------------------------------------------------------------
//  ALU
// -----------------------------------------------------------------------------
s8  Cpu::add8(s8 a, s8 b)  { s16 r = (s16)(u8x(a) + u8x(b));           updateFlags8(r);  return (s8)r; }
s16 Cpu::add16(s16 a,s16 b){ s32 r = (s32)(u16x(a) + u16x(b));         updateFlags16(r); return (s16)r; }
s8  Cpu::adc8(s8 a, s8 b)  { s16 r = (s16)(u8x(a) + u8x(b)); if (m_state.cf()) ++r; updateFlags8(r);  return (s8)r; }
s16 Cpu::adc16(s16 a,s16 b){ s32 r = (s32)(u16x(a) + u16x(b)); if (m_state.cf()) ++r; updateFlags16(r); return (s16)r; }
s8  Cpu::sub8(s8 a, s8 b)  { s16 r = (s16)(u8x(a) - u8x(b));           updateFlags8(r);  return (s8)r; }
s16 Cpu::sub16(s16 a,s16 b){ s32 r = (s32)((s32)u16x(a) - (s32)u16x(b)); updateFlags16(r); return (s16)r; }
s8  Cpu::sbb8(s8 a, s8 b)  { s16 r = (s16)(u8x(a) - u8x(b)); if (m_state.cf()) --r; updateFlags8(r);  return (s8)r; }
s16 Cpu::sbb16(s16 a,s16 b){ s32 r = (s32)((s32)u16x(a) - (s32)u16x(b)); if (m_state.cf()) --r; updateFlags16(r); return (s16)r; }
s8  Cpu::or8 (s8 a, s8 b)  { s16 r = (s16)(u8x(a) | u8x(b));           updateFlags8(r);  return (s8)r; }
s16 Cpu::or16 (s16 a,s16 b){ s32 r = (s32)(u16x(a) | u16x(b));         updateFlags16(r); return (s16)r; }
s8  Cpu::and8(s8 a, s8 b)  { s16 r = (s16)(u8x(a) & u8x(b));           updateFlags8(r);  return (s8)r; }
s16 Cpu::and16(s16 a,s16 b){ s32 r = (s32)(u16x(a) & u16x(b));         updateFlags16(r); return (s16)r; }
s8  Cpu::xor8(s8 a, s8 b)  { s16 r = (s16)(u8x(a) ^ u8x(b));           updateFlags8(r);  return (s8)r; }
s16 Cpu::xor16(s16 a,s16 b){ s32 r = (s32)(u16x(a) ^ u16x(b));         updateFlags16(r); return (s16)r; }

s8 Cpu::inc8(s8 v)  { bool c = m_state.cf(); s8  r = add8(v, (s8)1);  m_state.set_cf(c); return r; }
s16 Cpu::inc16(s16 v){ bool c = m_state.cf(); s16 r = add16(v,(s16)1); m_state.set_cf(c); return r; }
s8 Cpu::dec8(s8 v)  { bool c = m_state.cf(); s8  r = sub8(v, (s8)1);  m_state.set_cf(c); return r; }
s16 Cpu::dec16(s16 v){ bool c = m_state.cf(); s16 r = sub16(v,(s16)1); m_state.set_cf(c); return r; }

// -----------------------------------------------------------------------------
//  Control transfer
// -----------------------------------------------------------------------------
void Cpu::callNear(s16 off) { push(m_state.ip); m_state.ip = off; }
void Cpu::callFar(s16 s_, s16 off) { push(m_state.cs); m_state.cs = s_; callNear(off); }

// -----------------------------------------------------------------------------
//  String ops
// -----------------------------------------------------------------------------
void Cpu::movsb() {
    RMAddr src(m_state.ds, m_state.si), dst(m_state.es, m_state.di);
    m_mem.writeByte(dst, m_mem.readByte(src));
    s8 d = m_state.df() ? (s8)-1 : (s8)1;
    m_state.si = (s16)(m_state.si + d); m_state.di = (s16)(m_state.di + d);
}
void Cpu::movsw() {
    RMAddr src(m_state.ds, m_state.si), dst(m_state.es, m_state.di);
    m_mem.writeWord(dst, m_mem.readWord(src));
    s8 d = m_state.df() ? (s8)-2 : (s8)2;
    m_state.si = (s16)(m_state.si + d); m_state.di = (s16)(m_state.di + d);
}
void Cpu::cmpsb() {
    RMAddr a1(m_state.ds, m_state.si), a2(m_state.es, m_state.di);
    sub8(m_mem.readByte(a1), m_mem.readByte(a2));
    s8 d = m_state.df() ? (s8)-1 : (s8)1;
    m_state.si = (s16)(m_state.si + d); m_state.di = (s16)(m_state.di + d);
}
void Cpu::cmpsw() {
    RMAddr a1(m_state.ds, m_state.si), a2(m_state.es, m_state.di);
    sub16(m_mem.readWord(a1), m_mem.readWord(a2));
    s8 d = m_state.df() ? (s8)-2 : (s8)2;
    m_state.si = (s16)(m_state.si + d); m_state.di = (s16)(m_state.di + d);
}
void Cpu::stosb() {
    RMAddr a(m_state.es, m_state.di);
    m_mem.writeByte(a, m_state.al());
    s8 d = m_state.df() ? (s8)-1 : (s8)1;
    m_state.di = (s16)(m_state.di + d);
}
void Cpu::stosw() {
    RMAddr a(m_state.es, m_state.di);
    m_mem.writeWord(a, m_state.ax);
    s8 d = m_state.df() ? (s8)-2 : (s8)2;
    m_state.di = (s16)(m_state.di + d);
}
void Cpu::stosdw() {
    RMAddr a1(m_state.es, m_state.di);
    m_mem.writeWord(a1, m_state.ax);
    RMAddr a2(m_state.es, (s16)(m_state.di + 2));
    m_mem.writeWord(a2, m_state.dx);
    s8 d = m_state.df() ? (s8)-4 : (s8)4;
    m_state.di = (s16)(m_state.di + d);
}
void Cpu::lodsb() {
    RMAddr a(m_state.ds, m_state.si);
    m_state.set_al(m_mem.readByte(a));
    s8 d = m_state.df() ? (s8)-1 : (s8)1;
    m_state.si = (s16)(m_state.si + d);
}
void Cpu::lodsw() {
    RMAddr a(m_state.ds, m_state.si);
    m_state.ax = m_mem.readWord(a);
    s8 d = m_state.df() ? (s8)-2 : (s8)2;
    m_state.si = (s16)(m_state.si + d);
}
void Cpu::scasb() {
    RMAddr a(m_state.es, m_state.di);
    sub8(m_state.al(), m_mem.readByte(a));
    s8 d = m_state.df() ? (s8)-1 : (s8)1;
    m_state.di = (s16)(m_state.di + d);
}
void Cpu::scasw() {
    RMAddr a(m_state.es, m_state.di);
    sub16(m_state.ax, m_mem.readWord(a));
    s8 d = m_state.df() ? (s8)-2 : (s8)2;
    m_state.di = (s16)(m_state.di + d);
}

// -----------------------------------------------------------------------------
//  Shifts / rotates  --  each re-reads the operand every iteration, exactly
//  like the Java loop. Flag side effects match the reference (8-bit forms and
//  ROL/ROR/RCL 16-bit update the no-carry-overflow flags; RCR16 and the SHL/
//  SHR/SAR 16-bit forms deliberately do not).
// -----------------------------------------------------------------------------
void Cpu::rol8(int c){ c &= 0x1F; for(int i=0;i<c;i++){ s8 v=getMem8(); s8 m1=(s8)((v>>7)&1), m2=(s8)((v>>6)&1);
    v=(s8)((v<<1)|m1); setMem8(v); m_state.set_cf(m1!=0); m_state.set_of(m1!=m2); } }
void Cpu::ror8(int c){ c &= 0x1F; for(int i=0;i<c;i++){ s8 v=getMem8(); s8 lsb=(s8)(v&1);
    v=(s8)(((v&0xFF)>>1)|(lsb<<7)); setMem8(v); s8 m1=(s8)((v>>7)&1), m2=(s8)((v>>6)&1);
    m_state.set_cf(lsb!=0); m_state.set_of(m1!=m2); } }
void Cpu::rcl8(int c){ c &= 0x1F; for(int i=0;i<c;i++){ s8 v=getMem8(); s8 m1=(s8)((v>>7)&1), m2=(s8)((v>>6)&1);
    s8 cf=(s8)(m_state.cf()?1:0); v=(s8)((v<<1)|cf); setMem8(v); m_state.set_cf(m1!=0); m_state.set_of(m1!=m2); } }
void Cpu::rcr8(int c){ c &= 0x1F; for(int i=0;i<c;i++){ s8 v=getMem8(); s8 lsb=(s8)(v&1); s8 cf=(s8)(m_state.cf()?1:0);
    v=(s8)(((v&0xFF)>>1)|(cf<<7)); setMem8(v); s8 m1=(s8)((v>>7)&1), m2=(s8)((v>>6)&1);
    m_state.set_cf(lsb!=0); m_state.set_of(m1!=m2); } }
void Cpu::shl8(int c){ c &= 0x1F; for(int i=0;i<c;i++){ s8 v=getMem8(); s8 m1=(s8)((v>>7)&1), m2=(s8)((v>>6)&1);
    v=(s8)(v<<1); setMem8(v); m_state.set_cf(m1!=0); m_state.set_of(m1!=m2); updateFlagsNCO8(v); } }
void Cpu::shr8(int c){ c &= 0x1F; for(int i=0;i<c;i++){ s8 v=getMem8(); s8 lsb=(s8)(v&1);
    v=(s8)((v&0xFF)>>1); setMem8(v); s8 m1=(s8)((v>>7)&1), m2=(s8)((v>>6)&1);
    m_state.set_cf(lsb!=0); m_state.set_of(m1!=m2); updateFlagsNCO8(v); } }
void Cpu::sar8(int c){ c &= 0x1F; for(int i=0;i<c;i++){ s8 v=getMem8(); s8 lsb=(s8)(v&1);
    v=(s8)(v>>1); setMem8(v); s8 m1=(s8)((v>>7)&1), m2=(s8)((v>>6)&1);
    m_state.set_cf(lsb!=0); m_state.set_of(m1!=m2); updateFlagsNCO8(v); } }

void Cpu::rol16(int c){ c &= 0x1F; for(int i=0;i<c;i++){ s16 v=getMem16(); s8 m1=(s8)((v>>15)&1), m2=(s8)((v>>14)&1);
    v=(s16)((v<<1)|m1); setMem16(v); m_state.set_cf(m1!=0); m_state.set_of(m1!=m2); updateFlagsNCO16(v); } }
void Cpu::ror16(int c){ c &= 0x1F; for(int i=0;i<c;i++){ s16 v=getMem16(); s8 lsb=(s8)(v&1);
    v=(s16)(((u16)v>>1)|(lsb<<15)); setMem16(v); s8 m1=(s8)((v>>15)&1), m2=(s8)((v>>14)&1);
    m_state.set_cf(lsb!=0); m_state.set_of(m1!=m2); updateFlagsNCO16(v); } }
void Cpu::rcl16(int c){ c &= 0x1F; for(int i=0;i<c;i++){ s16 v=getMem16(); s8 m1=(s8)((v>>15)&1), m2=(s8)((v>>14)&1);
    s8 cf=(s8)(m_state.cf()?1:0); v=(s16)((v<<1)|cf); setMem16(v); m_state.set_cf(m1!=0); m_state.set_of(m1!=m2); updateFlagsNCO16(v); } }
void Cpu::rcr16(int c){ c &= 0x1F; for(int i=0;i<c;i++){ s16 v=getMem16(); s8 lsb=(s8)(v&1); s8 cf=(s8)(m_state.cf()?1:0);
    v=(s16)(((u16)v>>1)|(cf<<15)); setMem16(v); s8 m1=(s8)((v>>15)&1), m2=(s8)((v>>14)&1);
    m_state.set_cf(lsb!=0); m_state.set_of(m1!=m2); } }
void Cpu::shl16(int c){ c &= 0x1F; for(int i=0;i<c;i++){ s16 v=getMem16(); s8 m1=(s8)((v>>15)&1), m2=(s8)((v>>14)&1);
    v=(s16)(v<<1); setMem16(v); m_state.set_cf(m1!=0); m_state.set_of(m1!=m2); m_state.set_zf(v==0); } }
void Cpu::shr16(int c){ c &= 0x1F; for(int i=0;i<c;i++){ s16 v=getMem16(); s8 lsb=(s8)(v&1);
    v=(s16)((u16)v>>1); setMem16(v); s8 m1=(s8)((v>>15)&1), m2=(s8)((v>>14)&1);
    m_state.set_cf(lsb!=0); m_state.set_of(m1!=m2); m_state.set_zf(v==0); } }
void Cpu::sar16(int c){ c &= 0x1F; for(int i=0;i<c;i++){ s16 v=getMem16(); s8 lsb=(s8)(v&1);
    v=(s16)(v>>1); setMem16(v); s8 m1=(s8)((v>>15)&1), m2=(s8)((v>>14)&1);
    m_state.set_cf(lsb!=0); m_state.set_of(m1!=m2); m_state.set_zf(v==0); } }

// -----------------------------------------------------------------------------
//  Virtual interrupts
// -----------------------------------------------------------------------------
void Cpu::int86() {
    s8 n = m_state.bomb1;
    if (n != 0) {
        m_state.bomb1 = (s8)(n - 1);
        for (int i = 0; i < 64; ++i) stosdw();
    }
}
void Cpu::int87() {
    s8 n = m_state.bomb2;
    if (n == 0) return;
    m_state.bomb2 = (s8)(n - 1);
    for (int i = 0; i <= 0xFFFF; ++i) {
        int diff = m_state.df() ? -i : i;
        RMAddr a1(m_state.es, (s16)(m_state.di + diff));
        if (m_mem.readWord(a1) == m_state.ax) {
            RMAddr a2(m_state.es, (s16)(m_state.di + diff + 2));
            if (m_mem.readWord(a2) == m_state.dx) {
                m_mem.writeWord(a1, m_state.bx);
                m_mem.writeWord(a2, m_state.cx);
                break;
            }
        }
    }
}

// -----------------------------------------------------------------------------
//  Top-level dispatch
// -----------------------------------------------------------------------------
void Cpu::nextOpcode() {
    s8 opcode = fByte();
    switch ((u8)opcode >> 4) {
        case 0x0: op0X(opcode); break;  case 0x1: op1X(opcode); break;
        case 0x2: op2X(opcode); break;  case 0x3: op3X(opcode); break;
        case 0x4: op4X(opcode); break;  case 0x5: op5X(opcode); break;
        case 0x6: op6X(opcode); break;  case 0x7: op7X(opcode); break;
        case 0x8: op8X(opcode); break;  case 0x9: op9X(opcode); break;
        case 0xA: opAX(opcode); break;  case 0xB: opBX(opcode); break;
        case 0xC: opCX(opcode); break;  case 0xD: opDX(opcode); break;
        case 0xE: opEX(opcode); break;  case 0xF: opFX(opcode); break;
    }
}

[[noreturn]] static void fault(Fault f) { throw CpuFault{f}; }

// --- 0x0X : ADD / OR / PUSH-POP ES,CS ---
void Cpu::op0X(s8 opcode) {
    switch ((u8)opcode) {
        case 0x00: modrm(); setMem8 (add8 (getMem8(),  getReg8()));  break;
        case 0x01: modrm(); setMem16(add16(getMem16(), getReg16())); break;
        case 0x02: modrm(); setRegOp8 (add8 (getReg8(),  getMem8()));  break;
        case 0x03: modrm(); setRegOp16(add16(getReg16(), getMem16())); break;
        case 0x04: m_state.set_al(add8 (m_state.al(), fByte())); break;
        case 0x05: m_state.ax = add16(m_state.ax, fWord()); break;
        case 0x06: push(m_state.es); break;
        case 0x07: m_state.es = pop(); break;
        case 0x08: modrm(); setMem8 (or8 (getMem8(),  getReg8()));  break;
        case 0x09: modrm(); setMem16(or16(getMem16(), getReg16())); break;
        case 0x0A: modrm(); setRegOp8 (or8 (getReg8(),  getMem8()));  break;
        case 0x0B: modrm(); setRegOp16(or16(getReg16(), getMem16())); break;
        case 0x0C: m_state.set_al(or8 (m_state.al(), fByte())); break;
        case 0x0D: m_state.ax = or16(m_state.ax, fWord()); break;
        case 0x0E: push(m_state.cs); break;
        case 0x0F: fault(Fault::InvalidOpcode);
    }
}

// --- 0x1X : ADC / SBB / PUSH-POP SS,DS ---
void Cpu::op1X(s8 opcode) {
    switch ((u8)opcode) {
        case 0x10: modrm(); setMem8 (adc8 (getMem8(),  getReg8()));  break;
        case 0x11: modrm(); setMem16(adc16(getMem16(), getReg16())); break;
        case 0x12: modrm(); setRegOp8 (adc8 (getReg8(),  getMem8()));  break;
        case 0x13: modrm(); setRegOp16(adc16(getReg16(), getMem16())); break;
        case 0x14: m_state.set_al(adc8 (m_state.al(), fByte())); break;
        case 0x15: m_state.ax = adc16(m_state.ax, fWord()); break;
        case 0x16: push(m_state.ss); break;
        case 0x17: m_state.ss = pop(); break;
        case 0x18: modrm(); setMem8 (sbb8 (getMem8(),  getReg8()));  break;
        case 0x19: modrm(); setMem16(sbb16(getMem16(), getReg16())); break;
        case 0x1A: modrm(); setRegOp8 (sbb8 (getReg8(),  getMem8()));  break;
        case 0x1B: modrm(); setRegOp16(sbb16(getReg16(), getMem16())); break;
        case 0x1C: m_state.set_al(sbb8 (m_state.al(), fByte())); break;
        case 0x1D: m_state.ax = sbb16(m_state.ax, fWord()); break;
        case 0x1E: push(m_state.ds); break;
        case 0x1F: m_state.ds = pop(); break;
    }
}

// --- 0x2X : AND / SUB (+ ES:/CS: prefixes and DAA/DAS are unimplemented) ---
void Cpu::op2X(s8 opcode) {
    switch ((u8)opcode) {
        case 0x20: modrm(); setMem8 (and8 (getMem8(),  getReg8()));  break;
        case 0x21: modrm(); setMem16(and16(getMem16(), getReg16())); break;
        case 0x22: modrm(); setRegOp8 (and8 (getReg8(),  getMem8()));  break;
        case 0x23: modrm(); setRegOp16(and16(getReg16(), getMem16())); break;
        case 0x24: m_state.set_al(and8 (m_state.al(), fByte())); break;
        case 0x25: m_state.ax = and16(m_state.ax, fWord()); break;
        case 0x26: case 0x27: fault(Fault::UnimplementedOpcode);
        case 0x28: modrm(); setMem8 (sub8 (getMem8(),  getReg8()));  break;
        case 0x29: modrm(); setMem16(sub16(getMem16(), getReg16())); break;
        case 0x2A: modrm(); setRegOp8 (sub8 (getReg8(),  getMem8()));  break;
        case 0x2B: modrm(); setRegOp16(sub16(getReg16(), getMem16())); break;
        case 0x2C: m_state.set_al(sub8 (m_state.al(), fByte())); break;
        case 0x2D: m_state.ax = sub16(m_state.ax, fWord()); break;
        case 0x2E: case 0x2F: fault(Fault::UnimplementedOpcode);
    }
}

// --- 0x3X : XOR / CMP (+ SS:/DS: prefixes and AAA/AAS are unimplemented) ---
void Cpu::op3X(s8 opcode) {
    switch ((u8)opcode) {
        case 0x30: modrm(); setMem8 (xor8 (getMem8(),  getReg8()));  break;
        case 0x31: modrm(); setMem16(xor16(getMem16(), getReg16())); break;
        case 0x32: modrm(); setRegOp8 (xor8 (getReg8(),  getMem8()));  break;
        case 0x33: modrm(); setRegOp16(xor16(getReg16(), getMem16())); break;
        case 0x34: m_state.set_al(xor8 (m_state.al(), fByte())); break;
        case 0x35: m_state.ax = xor16(m_state.ax, fWord()); break;
        case 0x36: case 0x37: fault(Fault::UnimplementedOpcode);
        case 0x38: modrm(); sub8 (getMem8(),  getReg8());  break;
        case 0x39: modrm(); sub16(getMem16(), getReg16()); break;
        case 0x3A: modrm(); sub8 (getReg8(),  getMem8());  break;
        case 0x3B: modrm(); sub16(getReg16(), getMem16()); break;
        case 0x3C: sub8 (m_state.al(), fByte()); break;
        case 0x3D: sub16(m_state.ax, fWord()); break;
        case 0x3E: case 0x3F: fault(Fault::UnimplementedOpcode);
    }
}

// --- 0x4X : INC/DEC reg16 ---
void Cpu::op4X(s8 opcode) {
    u8 idx = (u8)(opcode & 0x07);
    if ((u8)opcode <= 0x47) setReg16(idx, inc16(reg16(idx)));
    else                    setReg16(idx, dec16(reg16(idx)));
}

// --- 0x5X : PUSH/POP reg16 ---
void Cpu::op5X(s8 opcode) {
    u8 idx = (u8)(opcode & 0x07);
    if ((u8)opcode <= 0x57) push(reg16(idx));
    else                    setReg16(idx, pop());
}

// --- 0x6X : all invalid on the 8086 ---
void Cpu::op6X(s8) { fault(Fault::InvalidOpcode); }

// --- 0x7X : conditional short jumps ---
void Cpu::op7X(s8 opcode) {
    bool br = false;
    switch ((u8)opcode) {
        case 0x70: br =  m_state.of(); break;
        case 0x71: br = !m_state.of(); break;
        case 0x72: br =  m_state.cf(); break;
        case 0x73: br = !m_state.cf(); break;
        case 0x74: br =  m_state.zf(); break;
        case 0x75: br = !m_state.zf(); break;
        case 0x76: br =  m_state.cf() ||  m_state.zf(); break;
        case 0x77: br = !m_state.cf() && !m_state.zf(); break;
        case 0x78: br =  m_state.sf(); break;
        case 0x79: br = !m_state.sf(); break;
        case 0x7A: br =  m_state.pf(); break;
        case 0x7B: br = !m_state.pf(); break;
        case 0x7C: br =  m_state.sf() != m_state.of(); break;
        case 0x7D: br =  m_state.sf() == m_state.of(); break;
        case 0x7E: br =  m_state.zf() || (m_state.sf() != m_state.of()); break;
        case 0x7F: br = !m_state.zf() && (m_state.sf() == m_state.of()); break;
    }
    s8 off = fByte();
    if (br) m_state.ip = (s16)(m_state.ip + off);
}

// --- 0x8X : grp1 imm, TEST, XCHG, MOV, LEA, POP [X] ---
void Cpu::op8X(s8 opcode) {
    switch ((u8)opcode) {
        case 0x80: case 0x82:
            modrm();
            switch (m_regIndex) {
                case 0: setMem8(add8(getMem8(), fByte())); break;
                case 1: setMem8(or8 (getMem8(), fByte())); break;
                case 2: setMem8(adc8(getMem8(), fByte())); break;
                case 3: setMem8(sbb8(getMem8(), fByte())); break;
                case 4: setMem8(and8(getMem8(), fByte())); break;
                case 5: setMem8(sub8(getMem8(), fByte())); break;
                case 6: setMem8(xor8(getMem8(), fByte())); break;
                case 7: sub8(getMem8(), fByte()); break;
            }
            break;
        case 0x81:
            modrm();
            switch (m_regIndex) {
                case 0: setMem16(add16(getMem16(), fWord())); break;
                case 1: setMem16(or16 (getMem16(), fWord())); break;
                case 2: setMem16(adc16(getMem16(), fWord())); break;
                case 3: setMem16(sbb16(getMem16(), fWord())); break;
                case 4: setMem16(and16(getMem16(), fWord())); break;
                case 5: setMem16(sub16(getMem16(), fWord())); break;
                case 6: setMem16(xor16(getMem16(), fWord())); break;
                case 7: sub16(getMem16(), fWord()); break;
            }
            break;
        case 0x83:
            modrm();
            switch (m_regIndex) {
                case 0: setMem16(add16(getMem16(), (s16)fByte())); break;
                case 1: setMem16(or16 (getMem16(), (s16)fByte())); break;
                case 2: setMem16(adc16(getMem16(), (s16)fByte())); break;
                case 3: setMem16(sbb16(getMem16(), (s16)fByte())); break;
                case 4: setMem16(and16(getMem16(), (s16)fByte())); break;
                case 5: setMem16(sub16(getMem16(), (s16)fByte())); break;
                case 6: setMem16(xor16(getMem16(), (s16)fByte())); break;
                case 7: sub16(getMem16(), (s16)fByte()); break;
            }
            break;
        case 0x84: modrm(); and8 (getReg8(),  getMem8());  break;
        case 0x85: modrm(); and16(getReg16(), getMem16()); break;
        case 0x86: { modrm(); s8 t = getReg8();  setRegOp8(getMem8());  setMem8(t);  break; }
        case 0x87: { modrm(); s16 t = getReg16(); setRegOp16(getMem16()); setMem16(t); break; }
        case 0x88: modrm(); setMem8 (getReg8());  break;
        case 0x89: modrm(); setMem16(getReg16()); break;
        case 0x8A: modrm(); setRegOp8 (getMem8());  break;
        case 0x8B: modrm(); setRegOp16(getMem16()); break;
        case 0x8C: modrm(); setMem16(getSeg()); break;
        case 0x8D:
            modrm();
            if (m_memIsReg) fault(Fault::InvalidOpcode);
            setRegOp16(m_memAddr.offset);
            break;
        case 0x8E: modrm(); setSegOp(getMem16()); break;
        case 0x8F: modrm(); setMem16(pop()); break;
    }
}

// --- 0x9X : XCHG AX, CBW/CWD, CALL far, WAIT/NRG, PUSHF/POPF, SAHF/LAHF ---
void Cpu::op9X(s8 opcode) {
    switch ((u8)opcode) {
        case 0x90: case 0x91: case 0x92: case 0x93:
        case 0x94: case 0x95: case 0x96: case 0x97: {
            u8 idx = (u8)(opcode & 0x07);
            s16 t = reg16(idx); setReg16(idx, m_state.ax); m_state.ax = t; break;
        }
        case 0x98: m_state.set_ah(m_state.al() < 0 ? (s8)0xFF : (s8)0); break;
        case 0x99: m_state.dx = (m_state.ax < 0) ? (s16)0xFFFF : (s16)0; break;
        case 0x9A: { s16 nip = fWord(); s16 ncs = fWord(); callFar(ncs, nip); break; }
        case 0x9B: {
            if (fByte() != (s8)0x9B) fault(Fault::UnsupportedOpcode);
            u32 e = u16x(m_state.energy);
            if (e < 0xFFFF) m_state.energy = (s16)(e + 1);
            break;
        }
        case 0x9C: push(m_state.flags); break;
        case 0x9D: m_state.flags = pop(); break;
        case 0x9E: {   // SAHF -- reproduces the Java sign-extension bug
            s16 f = m_state.flags;
            f = (s16)(((u16)f & 0xFF00) | (u32)(s32)m_state.ah());
            m_state.flags = f;
            break;
        }
        case 0x9F: m_state.set_ah((s8)m_state.flags); break;
    }
}

// --- 0xAX : MOV AL/AX <-> [imm16], string ops, TEST AL/AX imm ---
void Cpu::opAX(s8 opcode) {
    switch ((u8)opcode) {
        case 0xA0: { RMAddr a(m_state.ds, fWord()); m_state.set_al(m_mem.readByte(a)); break; }
        case 0xA1: { RMAddr a(m_state.ds, fWord()); m_state.ax = m_mem.readWord(a); break; }
        case 0xA2: { RMAddr a(m_state.ds, fWord()); m_mem.writeByte(a, m_state.al()); break; }
        case 0xA3: { RMAddr a(m_state.ds, fWord()); m_mem.writeWord(a, m_state.ax); break; }
        case 0xA4: movsb(); break;
        case 0xA5: movsw(); break;
        case 0xA6: cmpsb(); break;
        case 0xA7: cmpsw(); break;
        case 0xA8: and8 (m_state.al(), fByte()); break;
        case 0xA9: and16(m_state.ax, fWord()); break;
        case 0xAA: stosb(); break;
        case 0xAB: stosw(); break;
        case 0xAC: lodsb(); break;
        case 0xAD: lodsw(); break;
        case 0xAE: scasb(); break;
        case 0xAF: scasw(); break;
    }
}

// --- 0xBX : MOV reg, imm ---
void Cpu::opBX(s8 opcode) {
    u8 idx = (u8)(opcode & 0x07);
    if ((u8)opcode <= 0xB7) setReg8(idx, fByte());
    else                    setReg16(idx, fWord());
}

// --- 0xCX : RET/RETF, LES/LDS, MOV [X] imm, INT, IRET ---
void Cpu::opCX(s8 opcode) {
    switch ((u8)opcode) {
        case 0xC0: case 0xC1: fault(Fault::InvalidOpcode);
        case 0xC2: { s16 n = fWord(); m_state.ip = pop(); m_state.sp = (s16)(m_state.sp + n); break; }
        case 0xC3: m_state.ip = pop(); break;
        case 0xC4: {
            modrm();
            if (m_memIsReg) fault(Fault::InvalidOpcode);
            RMAddr a1 = m_memAddr;
            RMAddr a2(a1.segment, (s16)(a1.offset + 2));
            setRegOp16(m_mem.readWord(a1));
            m_state.es = m_mem.readWord(a2);
            break;
        }
        case 0xC5: {
            modrm();
            if (m_memIsReg) fault(Fault::InvalidOpcode);
            RMAddr a1 = m_memAddr;
            RMAddr a2(a1.segment, (s16)(a1.offset + 2));
            setRegOp16(m_mem.readWord(a1));
            m_state.ds = m_mem.readWord(a2);
            break;
        }
        case 0xC6: modrm(); setMem8(fByte()); break;
        case 0xC7: modrm(); setMem16(fWord()); break;
        case 0xC8: case 0xC9: fault(Fault::InvalidOpcode);
        case 0xCA: { s16 n = fWord(); m_state.ip = pop(); m_state.cs = pop(); m_state.sp = (s16)(m_state.sp + n); break; }
        case 0xCB: m_state.ip = pop(); m_state.cs = pop(); break;
        case 0xCC: fault(Fault::IntOpcode);
        case 0xCD: {
            s8 id = fByte();
            if (id == (s8)0x86) int86();
            else if (id == (s8)0x87) int87();
            else fault(Fault::IntOpcode);
            break;
        }
        case 0xCE: fault(Fault::IntOpcode);
        case 0xCF: m_state.ip = pop(); m_state.cs = pop(); m_state.flags = pop(); break;
    }
}

// --- 0xDX : shift/rotate groups, AAM/AAD, XLAT, FPU ---
void Cpu::opDX(s8 opcode) {
    switch ((u8)opcode) {
        case 0xD0:
            modrm();
            switch (m_regIndex) {
                case 0: rol8(1); break; case 1: ror8(1); break;
                case 2: rcl8(1); break; case 3: rcr8(1); break;
                case 4: shl8(1); break; case 5: shr8(1); break;
                case 6: fault(Fault::InvalidOpcode); case 7: sar8(1); break;
            }
            break;
        case 0xD1:
            modrm();
            switch (m_regIndex) {
                case 0: rol16(1); break; case 1: ror16(1); break;
                case 2: rcl16(1); break; case 3: rcr16(1); break;
                case 4: shl16(1); break; case 5: shr16(1); break;
                case 6: fault(Fault::InvalidOpcode); case 7: sar16(1); break;
            }
            break;
        case 0xD2:
            modrm();
            switch (m_regIndex) {
                case 0: rol8(m_state.cl()); break; case 1: ror8(m_state.cl()); break;
                case 2: rcl8(m_state.cl()); break; case 3: rcr8(m_state.cl()); break;
                case 4: shl8(m_state.cl()); break; case 5: shr8(m_state.cl()); break;
                case 6: fault(Fault::InvalidOpcode); case 7: sar8(m_state.cl()); break;
            }
            break;
        case 0xD3:
            modrm();
            switch (m_regIndex) {
                case 0: rol16(m_state.cl()); break; case 1: ror16(m_state.cl()); break;
                case 2: rcl16(m_state.cl()); break; case 3: rcr16(m_state.cl()); break;
                case 4: shl16(m_state.cl()); break; case 5: shr16(m_state.cl()); break;
                case 6: fault(Fault::InvalidOpcode); case 7: sar16(m_state.cl()); break;
            }
            break;
        case 0xD4: case 0xD5: fault(Fault::UnimplementedOpcode);
        case 0xD6: fault(Fault::InvalidOpcode);
        case 0xD7: {
            RMAddr a(m_state.ds, (s16)(m_state.bx + u8x(m_state.al())));
            m_state.set_al(m_mem.readByte(a));
            break;
        }
        case 0xD8: case 0xD9: case 0xDA: case 0xDB:
        case 0xDC: case 0xDD: case 0xDE: case 0xDF:
            fault(Fault::UnsupportedOpcode);
    }
}

// --- 0xEX : LOOP/JCXZ, CALL/JMP rel, IN/OUT ---
void Cpu::opEX(s8 opcode) {
    switch ((u8)opcode) {
        case 0xE0: { s8 o = fByte(); s16 cx = (s16)(m_state.cx - 1); m_state.cx = cx;
            if (cx != 0 && !m_state.zf()) { m_state.ip = (s16)(m_state.ip + o); }
            break; }
        case 0xE1: { s8 o = fByte(); s16 cx = (s16)(m_state.cx - 1); m_state.cx = cx;
            if (cx != 0 &&  m_state.zf()) { m_state.ip = (s16)(m_state.ip + o); }
            break; }
        case 0xE2: { s8 o = fByte(); s16 cx = (s16)(m_state.cx - 1); m_state.cx = cx;
            if (cx != 0) { m_state.ip = (s16)(m_state.ip + o); }
            break; }
        case 0xE3: { s8 o = fByte();
            if (m_state.cx == 0) { m_state.ip = (s16)(m_state.ip + o); }
            break; }
        case 0xE4: case 0xE5: case 0xE6: case 0xE7: fault(Fault::UnsupportedOpcode);
        case 0xE8: { s16 o = fWord(); callNear((s16)(m_state.ip + o)); break; }
        case 0xE9: { s16 o = fWord(); m_state.ip = (s16)(m_state.ip + o); break; }
        case 0xEA: { s16 nip = fWord(); s16 ncs = fWord(); m_state.ip = nip; m_state.cs = ncs; break; }
        case 0xEB: { s8 o = fByte(); m_state.ip = (s16)(m_state.ip + o); break; }
        case 0xEC: case 0xED: case 0xEE: case 0xEF: fault(Fault::UnsupportedOpcode);
    }
}

// --- 0xFX : REP/REPNZ, HLT, flag ops, grp3 (F6/F7), INC/DEC/CALL/JMP/PUSH (FE/FF) ---
void Cpu::opFX(s8 opcode) {
    switch ((u8)opcode) {
        case 0xF0: fault(Fault::UnsupportedOpcode);
        case 0xF1: fault(Fault::InvalidOpcode);
        case 0xF2: {
            s8 next = fByte();
            bool done = true;
            if (m_state.cx != 0) { m_state.cx = (s16)(m_state.cx - 1); done = false; }
            switch ((u8)next) {
                case 0xA6: if (!done) { cmpsb(); done =  m_state.zf(); } break;
                case 0xA7: if (!done) { cmpsw(); done =  m_state.zf(); } break;
                case 0xAE: if (!done) { scasb(); done =  m_state.zf(); } break;
                case 0xAF: if (!done) { scasw(); done =  m_state.zf(); } break;
                default: fault(Fault::InvalidOpcode);
            }
            if (!done) m_state.ip = (s16)(m_state.ip - 2);
            break;
        }
        case 0xF3: {
            s8 next = fByte();
            bool done = true;
            if (m_state.cx != 0) { m_state.cx = (s16)(m_state.cx - 1); done = false; }
            switch ((u8)next) {
                case 0xA4: if (!done) movsb(); break;
                case 0xA5: if (!done) movsw(); break;
                case 0xA6: if (!done) { cmpsb(); done = !m_state.zf(); } break;
                case 0xA7: if (!done) { cmpsw(); done = !m_state.zf(); } break;
                case 0xAA: if (!done) stosb(); break;
                case 0xAB: if (!done) stosw(); break;
                case 0xAC: if (!done) lodsb(); break;
                case 0xAD: if (!done) lodsw(); break;
                case 0xAE: if (!done) { scasb(); done = !m_state.zf(); } break;
                case 0xAF: if (!done) { scasw(); done = !m_state.zf(); } break;
                default: fault(Fault::InvalidOpcode);
            }
            if (!done) m_state.ip = (s16)(m_state.ip - 2);
            break;
        }
        case 0xF4: fault(Fault::UnsupportedOpcode);
        case 0xF5: m_state.set_cf(!m_state.cf()); break;
        case 0xF6:
            modrm();
            switch (m_regIndex) {
                case 0: and8(getMem8(), fByte()); break;
                case 1: fault(Fault::InvalidOpcode);
                case 2: setMem8((s8)(getMem8() ^ 0xFF)); break;
                case 3: setMem8(sub8((s8)0, getMem8())); break;
                case 4: {
                    s16 r = (s16)(u8x(m_state.al()) * u8x(getMem8()));
                    m_state.set_ah((s8)(r >> 8));
                    m_state.set_al((s8)r);
                    bool nz = m_state.ah() != 0;
                    m_state.set_of(nz); m_state.set_cf(nz);
                    break;
                }
                case 5: fault(Fault::UnimplementedOpcode);
                case 6: {
                    s32 tmp = (s32)u16x(m_state.ax);
                    s16 divisor = (s16)u8x(getMem8());
                    if (divisor == 0) fault(Fault::Division);
                    s16 q = (s16)(tmp / divisor);
                    if (q > 0xFF) fault(Fault::Division);
                    m_state.set_al((s8)q);
                    m_state.set_ah((s8)(tmp % divisor));
                    break;
                }
                case 7: fault(Fault::UnimplementedOpcode);
            }
            break;
        case 0xF7:
            modrm();
            switch (m_regIndex) {
                case 0: and16(getMem16(), fWord()); break;
                case 1: fault(Fault::InvalidOpcode);
                case 2: setMem16((s16)(getMem16() ^ 0xFFFF)); break;
                case 3: setMem16(sub16((s16)0, getMem16())); break;
                case 4: {
                    s32 r = (s32)(u16x(m_state.ax) * u16x(getMem16()));
                    m_state.dx = (s16)(r >> 16);
                    m_state.ax = (s16)r;
                    bool nz = m_state.dx != 0;
                    m_state.set_of(nz); m_state.set_cf(nz);
                    break;
                }
                case 5: fault(Fault::UnimplementedOpcode);
                case 6: {
                    u64 tmp = (u64)(((u32)u16x(m_state.dx) << 16) | (u32)u16x(m_state.ax));
                    u32 divisor = u16x(getMem16());
                    if (divisor == 0) fault(Fault::Division);
                    u64 q = tmp / divisor;
                    if (q > 0xFFFF) fault(Fault::Division);
                    m_state.ax = (s16)q;
                    m_state.dx = (s16)(tmp % divisor);
                    break;
                }
                case 7: fault(Fault::UnimplementedOpcode);
            }
            break;
        case 0xF8: m_state.set_cf(false); break;
        case 0xF9: m_state.set_cf(true); break;
        case 0xFA: m_state.set_if(false); break;
        case 0xFB: m_state.set_if(true); break;
        case 0xFC: m_state.set_df(false); break;
        case 0xFD: m_state.set_df(true); break;
        case 0xFE:
            modrm();
            switch (m_regIndex) {
                case 0: setMem8(inc8(getMem8())); break;
                case 1: setMem8(dec8(getMem8())); break;
                default: fault(Fault::InvalidOpcode);
            }
            break;
        case 0xFF:
            modrm();
            switch (m_regIndex) {
                case 0: setMem16(inc16(getMem16())); break;
                case 1: setMem16(dec16(getMem16())); break;
                case 2: callNear(getMem16()); break;
                case 3: {
                    if (m_memIsReg) fault(Fault::InvalidOpcode);
                    RMAddr a = m_memAddr;
                    s16 nip = m_mem.readWord(a);
                    RMAddr a2(a.segment, (s16)(a.offset + 2));
                    s16 ncs = m_mem.readWord(a2);
                    callFar(ncs, nip);
                    break;
                }
                case 4: m_state.ip = getMem16(); break;
                case 5: {
                    if (m_memIsReg) fault(Fault::InvalidOpcode);
                    RMAddr a = m_memAddr;
                    s16 nip = m_mem.readWord(a);
                    RMAddr a2(a.segment, (s16)(a.offset + 2));
                    s16 ncs = m_mem.readWord(a2);
                    m_state.cs = ncs; m_state.ip = nip;
                    break;
                }
                case 6: push(getMem16()); break;
                case 7: fault(Fault::InvalidOpcode);
            }
            break;
    }
}

} // namespace cw
