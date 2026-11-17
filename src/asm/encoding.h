#pragma once

#include <cstdint>

#include "asm/insn.h"
#include "asm/labels.h"
#include "asm/operands.h"
#include "utils/assertion.h"

namespace Asm {

// Aarch64 instruction encoders. Each function returns Insn: the 32-bit word of
// one instruction form. The comment above a form is that form's encoding
// template from the Arm Architecture Reference Manual (ARM ARM), most
// significant bit on the left, where literal 0s and 1s are fixed template bits
// and named fields are filled in by the encoder:
//
//   sf    - operand width: 1 = 64-bit operands, 0 = 32-bit
//   op    - family-specific operation selector; in add/sub: 0 = add, 1 = subtract
//   S     - flag update: 1 = ADDS/SUBS
//   sh    - immediate shift: 1 = imm12 is shifted left by 12
//   shift - shift type applied to Rm: 0 = LSL, 1 = LSR, 2 = ASR (3 = ROR, not modeled)
//   imm6  - shift amount: 0..63 for sf=1, 0..31 for sf=0
//   imm12 - unsigned 12-bit immediate
//   imm26/imm19/imm14 - branch offsets in instruction words, signed
//   immhi/immlo - adr offset in bytes: bits [23:5] and bits [30:29] of the offset
//                 divided by 4
//   cond  - condition code
//   b5/b40 - tested bit number: its bit 5 and its low 5 bits
//   Rt    - register operand of loads, stores and compare-and-branch
//   Rm    - register operand 2
//   Rn    - register operand 1
//   Rd    - destination register

namespace Detail {

constexpr uint32_t CodeOf(XReg reg) { return reg.Code(); }

constexpr uint32_t CodeOf(WReg reg) { return reg.Code(); }

constexpr uint32_t CodeOf(SpReg) { return 31; }

// The helpers take the form's fixed bits: the template literals plus the
// selectors that pick this form (sf, op, S).

// ADD/SUB (immediate): sf op S 100010 sh imm12 Rn Rd
template <typename D, typename N> constexpr Insn AddSubImm(uint32_t formBits, D rd, N rn, uint32_t imm12, uint32_t sh)
{
    return (Op { formBits } | Field<22, 1> { sh } | Field<10, 12> { imm12 } | Field<5, 5> { CodeOf(rn) } |
            Field<0, 5> { CodeOf(rd) })
        .Finish();
}

// ADD/SUB (shifted register): sf op S 01011 shift 0 imm6 Rm Rn Rd
template <typename D, typename N, typename M>
constexpr Insn AddSubShifted(uint32_t formBits, D rd, N rn, M rm, Shift shift, uint32_t imm6, uint32_t imm6Limit)
{
    ASSERT(imm6 <= imm6Limit);
    return (Op { formBits } | Field<22, 2> { static_cast<uint32_t>(shift) } | Field<16, 5> { CodeOf(rm) } |
            Field<10, 6> { imm6 } | Field<5, 5> { CodeOf(rn) } | Field<0, 5> { CodeOf(rd) })
        .Finish();
}

// BR/BLR/RET (register): 1101011 0 0 op 11111 000000 Rn 00000
constexpr Insn BranchReg(uint32_t formBits, uint32_t rn)
{
    return (Op { formBits } | Field<5, 5> { rn }).Finish();
}

// TBZ/TBNZ: b5 011011 op 0 b40 imm14 Rt
constexpr Insn TestBranch(uint32_t formBits, uint32_t rt, uint32_t bit, uint32_t bitLimit)
{
    ASSERT(bit <= bitLimit);
    return (Op { formBits } | Field<31, 1> { bit >> 5 } | Field<19, 5> { bit & 0x1F } | Field<0, 5> { rt }).Finish();
}

// Puts the distance to a branch target into the offset field of a branch word:
// the offset field of a branch is the signed distance scaled down by Scale. The
// scaled distance is truncated to the field width, so a negative distance is
// stored in two's complement form.
template <uint32_t Shift, uint32_t Width, uint32_t Scale> constexpr Insn BranchFixup(Insn word, int64_t delta)
{
    constexpr int64_t Limit = int64_t { 1 } << (Width - 1);
    ASSERT(delta % Scale == 0);
    ASSERT(delta >= -Limit * Scale && delta < Limit * Scale);
    return (Op { word.Bits() } | Field<Shift, Width> { static_cast<uint32_t>(delta / Scale) & Field<Shift, Width>::Mask })
        .Finish();
}

} // namespace Detail

namespace Encode {

constexpr Insn Add(XReg dst, XReg src, uint32_t imm12) { return Detail::AddSubImm(0x91000000, dst, src, imm12, 0); }

constexpr Insn Add(XReg dst, XReg src, uint32_t imm12, Lsl12Tag)
{
    return Detail::AddSubImm(0x91000000, dst, src, imm12, 1);
}

constexpr Insn Add(XReg dst, SpReg src, uint32_t imm12) { return Detail::AddSubImm(0x91000000, dst, src, imm12, 0); }

constexpr Insn Add(XReg dst, SpReg src, uint32_t imm12, Lsl12Tag)
{
    return Detail::AddSubImm(0x91000000, dst, src, imm12, 1);
}

constexpr Insn Add(SpReg dst, XReg src, uint32_t imm12) { return Detail::AddSubImm(0x91000000, dst, src, imm12, 0); }

constexpr Insn Add(SpReg dst, SpReg src, uint32_t imm12) { return Detail::AddSubImm(0x91000000, dst, src, imm12, 0); }

constexpr Insn Add(WReg dst, WReg src, uint32_t imm12) { return Detail::AddSubImm(0x11000000, dst, src, imm12, 0); }

constexpr Insn Add(WReg dst, WReg src, uint32_t imm12, Lsl12Tag)
{
    return Detail::AddSubImm(0x11000000, dst, src, imm12, 1);
}

constexpr Insn Add(XReg dst, XReg src, XReg src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
{
    return Detail::AddSubShifted(0x8B000000, dst, src, src2, shift, amount, 63);
}

constexpr Insn Add(WReg dst, WReg src, WReg src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
{
    return Detail::AddSubShifted(0x0B000000, dst, src, src2, shift, amount, 31);
}

constexpr Insn Sub(XReg dst, XReg src, uint32_t imm12) { return Detail::AddSubImm(0xD1000000, dst, src, imm12, 0); }

constexpr Insn Sub(XReg dst, XReg src, uint32_t imm12, Lsl12Tag)
{
    return Detail::AddSubImm(0xD1000000, dst, src, imm12, 1);
}

constexpr Insn Sub(XReg dst, SpReg src, uint32_t imm12) { return Detail::AddSubImm(0xD1000000, dst, src, imm12, 0); }

constexpr Insn Sub(XReg dst, SpReg src, uint32_t imm12, Lsl12Tag)
{
    return Detail::AddSubImm(0xD1000000, dst, src, imm12, 1);
}

constexpr Insn Sub(SpReg dst, XReg src, uint32_t imm12) { return Detail::AddSubImm(0xD1000000, dst, src, imm12, 0); }

constexpr Insn Sub(SpReg dst, SpReg src, uint32_t imm12) { return Detail::AddSubImm(0xD1000000, dst, src, imm12, 0); }

constexpr Insn Sub(WReg dst, WReg src, uint32_t imm12) { return Detail::AddSubImm(0x51000000, dst, src, imm12, 0); }

constexpr Insn Sub(WReg dst, WReg src, uint32_t imm12, Lsl12Tag)
{
    return Detail::AddSubImm(0x51000000, dst, src, imm12, 1);
}

constexpr Insn Sub(XReg dst, XReg src, XReg src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
{
    return Detail::AddSubShifted(0xCB000000, dst, src, src2, shift, amount, 63);
}

constexpr Insn Sub(WReg dst, WReg src, WReg src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
{
    return Detail::AddSubShifted(0x4B000000, dst, src, src2, shift, amount, 31);
}

// RET (register): 1101011 0 0 10 11111 000000 Rn 00000
constexpr Insn Ret(XReg reg = X30) { return Detail::BranchReg(0xD65F0000, Detail::CodeOf(reg)); }

// BR (register): 1101011 0 0 00 11111 000000 Rn 00000
constexpr Insn Br(XReg reg) { return Detail::BranchReg(0xD61F0000, Detail::CodeOf(reg)); }

// BLR (register): 1101011 0 0 01 11111 000000 Rn 00000
constexpr Insn Blr(XReg reg) { return Detail::BranchReg(0xD63F0000, Detail::CodeOf(reg)); }

// B: 000101 imm26
constexpr Insn B() { return Detail::Op { 0x14000000 }.Finish(); }

// BL: 100101 imm26
constexpr Insn Bl() { return Detail::Op { 0x94000000 }.Finish(); }

// B.cond: 0101010 0 imm19 0 cond
constexpr Insn B(Cc cc)
{
    return (Detail::Op { 0x54000000 } | Detail::Field<0, 4> { static_cast<uint32_t>(cc) }).Finish();
}

// CBZ: sf 011010 0 imm19 Rt
constexpr Insn Cbz(XReg rt) { return (Detail::Op { 0xB4000000 } | Detail::Field<0, 5> { rt.Code() }).Finish(); }

constexpr Insn Cbz(WReg rt) { return (Detail::Op { 0x34000000 } | Detail::Field<0, 5> { rt.Code() }).Finish(); }

// CBNZ: sf 011010 1 imm19 Rt
constexpr Insn Cbnz(XReg rt) { return (Detail::Op { 0xB5000000 } | Detail::Field<0, 5> { rt.Code() }).Finish(); }

constexpr Insn Cbnz(WReg rt) { return (Detail::Op { 0x35000000 } | Detail::Field<0, 5> { rt.Code() }).Finish(); }

// TBZ: b5 011011 0 0 b40 imm14 Rt
constexpr Insn Tbz(XReg rt, uint32_t bit) { return Detail::TestBranch(0x36000000, rt.Code(), bit, 63); }

constexpr Insn Tbz(WReg rt, uint32_t bit) { return Detail::TestBranch(0x36000000, rt.Code(), bit, 31); }

// TBNZ: b5 011011 1 0 b40 imm14 Rt
constexpr Insn Tbnz(XReg rt, uint32_t bit) { return Detail::TestBranch(0x37000000, rt.Code(), bit, 63); }

constexpr Insn Tbnz(WReg rt, uint32_t bit) { return Detail::TestBranch(0x37000000, rt.Code(), bit, 31); }

// ADR: 0 immlo 10000 immhi Rd
constexpr Insn Adr(XReg rd) { return (Detail::Op { 0x10000000 } | Detail::Field<0, 5> { rd.Code() }).Finish(); }

// Fills the offset field of a word emitted for a branch at a known site with the
// distance from that site to the target, in bytes.
constexpr Insn ApplyFixup(Insn word, FixupKind kind, int64_t delta)
{
    switch (kind) {
    case FixupKind::Branch26:
        return Detail::BranchFixup<0, 26, 4>(word, delta);
    case FixupKind::CondBranch19:
    case FixupKind::CmpBranch19:
        return Detail::BranchFixup<5, 19, 4>(word, delta);
    case FixupKind::TestBranch14:
        return Detail::BranchFixup<5, 14, 4>(word, delta);
    case FixupKind::Adr21: {
        ASSERT(delta % 4 == 0);
        ASSERT(delta >= -(int64_t { 1 } << 20) && delta < (int64_t { 1 } << 20));
        const uint32_t offset = static_cast<uint32_t>(delta) & 0x1FFFFF;
        return (Detail::Op { word.Bits() } | Detail::Field<5, 19> { offset >> 2 } | Detail::Field<29, 2> { offset & 0x3 })
            .Finish();
    }
    }
    return word;
}

} // namespace Encode

} // namespace Asm
