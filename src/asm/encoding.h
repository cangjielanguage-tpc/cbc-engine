#pragma once

#include <cstdint>
#include <type_traits>

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
//   op2   - data-processing (2 source) selector: 0 = LSL, 1 = LSR, 2 = ASR
//           (3 = ROR, not modeled)
//   immr  - bitfield: rotate the source right by immr
//   imms  - bitfield: keep the low imms+1 bits after the rotation
//   imm12 - unsigned 12-bit immediate
//   imm9  - load/store byte offset, unscaled, signed
//   imm7  - load/store pair offset in units of the access size, signed
//   imm26/imm19/imm14 - branch offsets in instruction words, signed
//   immhi/immlo - adr offset in bytes: bits [23:5] and bits [30:29] of the offset
//                 divided by 4
//   cond  - condition code
//   b5/b40 - tested bit number: its bit 5 and its low 5 bits
//   option - load/store register-offset extension: 2 = UXTW, 3 = LSL, 6 = SXTW,
//            7 = SXTX
//   S     - load/store register-offset scale: 1 = the index register is shifted
//           left by log2(access size), so it counts elements instead of bytes
//   Rt    - register operand of loads, stores and compare-and-branch
//   Rt2   - second register operand of a load/store pair
//   Rm    - register operand 2
//   Rn    - register operand 1
//   Rd    - destination register

namespace Detail {

constexpr uint32_t CodeOf(XReg reg) { return reg.Code(); }

constexpr uint32_t CodeOf(WReg reg) { return reg.Code(); }

constexpr uint32_t CodeOf(XzrReg) { return 31; }

constexpr uint32_t CodeOf(WzrReg) { return 31; }

constexpr uint32_t CodeOf(SpReg) { return 31; }

// The operand-type traits live next to the operand types in operands.h; the
// aliases below are the enable_if forms the encoders and assembler methods use
// to constrain their operands.
template <typename R> using EnableXReg = std::enable_if_t<IsXRegOperand<R>::value, int>;

template <typename R> using EnableWReg = std::enable_if_t<IsWRegOperand<R>::value, int>;

template <typename D, typename N, typename M>
using EnableXOperands =
    std::enable_if_t<IsXRegOperand<D>::value && IsXRegOperand<N>::value && IsXRegOperand<M>::value, int>;

template <typename D, typename N, typename M>
using EnableWOperands =
    std::enable_if_t<IsWRegOperand<D>::value && IsWRegOperand<N>::value && IsWRegOperand<M>::value, int>;

// The register field classes a two-register form reads as <Xd>/<Xn>, and a form
// that reads its second register as a 32-bit one while writing a 64-bit one
// (the sign-extending forms).
template <typename D, typename N>
using EnableXPair = std::enable_if_t<IsXRegOperand<D>::value && IsXRegOperand<N>::value, int>;

template <typename D, typename N>
using EnableWPair = std::enable_if_t<IsWRegOperand<D>::value && IsWRegOperand<N>::value, int>;

template <typename D, typename N>
using EnableXFromW = std::enable_if_t<IsXRegOperand<D>::value && IsWRegOperand<N>::value, int>;

// The helpers take the form's fixed bits: the template literals plus the
// selectors that pick this form (sf, op, S), and the register numbers, so the
// caller decides what each register field means.

// ADD/SUB (immediate): sf op S 100010 sh imm12 Rn Rd
constexpr Insn AddSubImm(uint32_t formBits, uint32_t rd, uint32_t rn, uint32_t imm12, uint32_t sh)
{
    return (Op { formBits } | Field<22, 1> { sh } | Field<10, 12> { imm12 } | Field<5, 5> { rn } | Field<0, 5> { rd })
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

// AND/ORR/EOR/ANDS (shifted register): sf opc 01010 shift 0 imm6 Rm Rn Rd
template <typename D, typename N, typename M>
constexpr Insn LogicalShifted(uint32_t formBits, D rd, N rn, M rm, Shift shift, uint32_t imm6, uint32_t imm6Limit)
{
    ASSERT(imm6 <= imm6Limit);
    return (Op { formBits } | Field<22, 2> { static_cast<uint32_t>(shift) } | Field<16, 5> { CodeOf(rm) } |
            Field<10, 6> { imm6 } | Field<5, 5> { CodeOf(rn) } | Field<0, 5> { CodeOf(rd) })
        .Finish();
}

// Number of set bits of value: the length of the run of ones in a bitmask
// immediate's element.
constexpr unsigned PopCount(uint64_t value)
{
    unsigned count = 0;
    while (value != 0) {
        count  += static_cast<unsigned>(value & 1);
        value >>= 1;
    }
    return count;
}

// Encodes `imm` as a bitmask immediate for a `width`-bit form and produces the
// N, immr and imms fields. Such an immediate is a single run of ones replicated
// at some power-of-two period: the element of `esize = 2^len` bits is one
// rotation of `Ones(ones)`, and the value repeats it every esize bits. Returns
// false when no encoding exists (the value is 0, all-ones, or not such a
// repetition). `imm` is only read to `width` bits.
constexpr bool EncodeLogicalImm(uint64_t imm, uint32_t width, uint32_t& n, uint32_t& immr, uint32_t& imms)
{
    const uint64_t value   = (width == 32) ? (imm & 0xFFFFFFFFull) : imm;
    const uint64_t allOnes = (width == 64) ? ~UINT64_C(0) : UINT64_C(0xFFFFFFFF);
    if (value == 0 || value == allOnes) {
        return false;
    }
    const unsigned maxLen = (width == 64) ? 6 : 5;
    for (unsigned len = 1; len <= maxLen; ++len) {
        const unsigned esize    = 1u << len;
        const uint64_t elemMask = (esize == 64) ? allOnes : ((UINT64_C(1) << esize) - 1);
        const uint64_t element  = value & elemMask;
        uint64_t replica        = 0;
        for (unsigned i = 0; i < width; i += esize) {
            replica |= element << i;
        }
        if (replica != value) {
            continue;
        }
        const unsigned ones = PopCount(element);
        if (ones == 0 || ones == esize) {
            continue;
        }
        // The element must be a rotation of Ones(ones), that is, its ones must
        // form exactly one contiguous run. Such a run has exactly one start: the
        // 1 whose predecessor is 0.
        unsigned runs  = 0;
        unsigned start = 0;
        for (unsigned i = 0; i < esize; ++i) {
            const unsigned prev = (i + esize - 1) % esize;
            if (((element >> i) & 1u) != 0 && ((element >> prev) & 1u) == 0) {
                start = i;
                ++runs;
            }
        }
        if (runs != 1) {
            continue;
        }
        // element == ROR(Ones(ones), rotate), so immr is the rotation that moves
        // the run's start to bit 0.
        n    = (len == 6) ? 1u : 0u;
        immr = (esize - start) % esize;
        imms = (len == 6) ? (ones - 1) : ((64u - (2u << len)) | (ones - 1));
        return true;
    }
    return false;
}

// AND/ORR/EOR/ANDS (immediate): sf opc 100100 N immr imms Rn Rd
constexpr Insn LogicalImm(uint32_t formBits, uint32_t rd, uint32_t rn, uint64_t imm, uint32_t width)
{
    uint32_t n           = 0;
    uint32_t immr        = 0;
    uint32_t imms        = 0;
    const bool encodable = EncodeLogicalImm(imm, width, n, immr, imms);
    ASSERT(encodable);
    return (Op { formBits } | Field<22, 1> { n } | Field<16, 6> { immr } | Field<10, 6> { imms } | Field<5, 5> { rn } |
            Field<0, 5> { rd })
        .Finish();
}

// LSLV/LSRV/ASRV (data-processing, 2 source):
// sf 0 0 11010110 Rm 0010 00 op2 Rn Rd
constexpr Insn DataProc2Source(uint32_t formBits, uint32_t rd, uint32_t rn, uint32_t rm, uint32_t op2)
{
    return (Op { formBits } | Field<16, 5> { rm } | Field<10, 2> { op2 } | Field<5, 5> { rn } | Field<0, 5> { rd })
        .Finish();
}

// SBFM/BFM/UBFM (bitfield): sf opc 100110 N immr imms Rn Rd. immr and imms are
// 6-bit fields, but in a 32-bit form only their low 5 bits are meaningful, so
// the limit is the width-dependent one rather than the field width.
constexpr Insn Bitfield(uint32_t formBits, uint32_t rd, uint32_t rn, uint32_t immr, uint32_t imms, uint32_t limit)
{
    ASSERT(immr <= limit);
    ASSERT(imms <= limit);
    return (Op { formBits } | Field<16, 6> { immr } | Field<10, 6> { imms } | Field<5, 5> { rn } | Field<0, 5> { rd })
        .Finish();
}

// LDR/STR (immediate, unsigned offset): size 111 V 01 opc imm12 Rn Rt. imm12
// counts accessed elements, so the byte offset must be a multiple of the access
// size and below 4096 elements.
constexpr Insn LoadStoreUnsigned(uint32_t formBits, uint32_t rt, const MemImm& mem, uint32_t elementSize)
{
    const int32_t offset = mem.Offset();
    ASSERT(offset >= 0);
    ASSERT(offset % static_cast<int32_t>(elementSize) == 0);
    ASSERT(static_cast<uint32_t>(offset) / elementSize <= 4095);
    return (Op { formBits } | Field<10, 12> { static_cast<uint32_t>(offset) / elementSize } |
            Field<5, 5> { mem.Base() } | Field<0, 5> { rt })
        .Finish();
}

// LDUR/STUR (immediate, unscaled): size 111 V 00 opc 00 imm9 00 Rn Rt. The byte
// offset is not scaled, so any value that fits its 9 signed bits is encodable.
constexpr Insn LoadStoreUnscaled(uint32_t formBits, uint32_t rt, const MemImm& mem)
{
    const int32_t offset = mem.Offset();
    ASSERT(offset >= -256 && offset <= 255);
    return (Op { formBits } | Field<12, 9> { static_cast<uint32_t>(offset) & 0x1FF } | Field<5, 5> { mem.Base() } |
            Field<0, 5> { rt })
        .Finish();
}

// LDR/STR (register): size 111 V 00 opc 1 Rm option S 10 Rn Rt. The index is
// either a byte offset or, with S set, shifted left by log2(access size); the
// option field selects how the index register is read (a 64-bit one through LSL
// or SXTX, a 32-bit one through UXTW or SXTW).
constexpr Insn LoadStoreRegister(uint32_t formBits, uint32_t rt, const MemIndex& mem)
{
    return (Op { formBits } | Field<16, 5> { mem.Index() } | Field<13, 3> { static_cast<uint32_t>(mem.Extend()) } |
            Field<12, 1> { mem.IsScaled() ? 1u : 0u } | Field<5, 5> { mem.Base() } | Field<0, 5> { rt })
        .Finish();
}

// LDP/STP (immediate, signed offset): opc 101 V 000 L imm7 Rt2 Rn Rt. imm7
// counts accessed elements, so the byte offset must be a multiple of the access
// size (the size of one register of the pair) and within 64 elements. A loading
// pair reads its two registers from one address, so its two registers must
// differ or the result is unpredictable.
constexpr Insn LoadStorePair(
    uint32_t formBits, uint32_t rt, uint32_t rt2, const MemImm& mem, uint32_t elementSize, bool loading
)
{
    const int32_t offset = mem.Offset();
    ASSERT(offset % static_cast<int32_t>(elementSize) == 0);
    ASSERT(offset / static_cast<int32_t>(elementSize) >= -64);
    ASSERT(offset / static_cast<int32_t>(elementSize) <= 63);
    ASSERT(!loading || rt != rt2);
    const uint32_t imm7 = static_cast<uint32_t>(offset / static_cast<int32_t>(elementSize)) & 0x7F;
    return (Op { formBits } | Field<15, 7> { imm7 } | Field<10, 5> { rt2 } | Field<5, 5> { mem.Base() } |
            Field<0, 5> { rt })
        .Finish();
}

// BR/BLR/RET (register): 1101011 0 0 op 11111 000000 Rn 00000
constexpr Insn BranchReg(uint32_t formBits, uint32_t rn) { return (Op { formBits } | Field<5, 5> { rn }).Finish(); }

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
    return (Op { word.Bits() } |
            Field<Shift, Width> { static_cast<uint32_t>(delta / Scale) & Field<Shift, Width>::Mask })
        .Finish();
}

} // namespace Detail

namespace Encode {

constexpr Insn Add(XReg dst, XReg src, uint32_t imm12)
{
    return Detail::AddSubImm(0x91000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm12, 0);
}

constexpr Insn Add(XReg dst, XReg src, uint32_t imm12, Lsl12Tag)
{
    return Detail::AddSubImm(0x91000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm12, 1);
}

constexpr Insn Add(XReg dst, SpReg src, uint32_t imm12)
{
    return Detail::AddSubImm(0x91000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm12, 0);
}

constexpr Insn Add(XReg dst, SpReg src, uint32_t imm12, Lsl12Tag)
{
    return Detail::AddSubImm(0x91000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm12, 1);
}

constexpr Insn Add(SpReg dst, XReg src, uint32_t imm12)
{
    return Detail::AddSubImm(0x91000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm12, 0);
}

constexpr Insn Add(SpReg dst, SpReg src, uint32_t imm12)
{
    return Detail::AddSubImm(0x91000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm12, 0);
}

constexpr Insn Add(WReg dst, WReg src, uint32_t imm12)
{
    return Detail::AddSubImm(0x11000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm12, 0);
}

constexpr Insn Add(WReg dst, WReg src, uint32_t imm12, Lsl12Tag)
{
    return Detail::AddSubImm(0x11000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm12, 1);
}

// ADD (shifted register): sf op S 01011 shift 0 imm6 Rm Rn Rd. The register
// fields are <Xd>/<Xn>/<Xm>, so any of them may be the zero register.
template <typename D, typename N, typename M, Detail::EnableXOperands<D, N, M> = 0>
constexpr Insn Add(D dst, N src, M src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
{
    return Detail::AddSubShifted(0x8B000000, dst, src, src2, shift, amount, 63);
}

template <typename D, typename N, typename M, Detail::EnableWOperands<D, N, M> = 0>
constexpr Insn Add(D dst, N src, M src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
{
    return Detail::AddSubShifted(0x0B000000, dst, src, src2, shift, amount, 31);
}

constexpr Insn Sub(XReg dst, XReg src, uint32_t imm12)
{
    return Detail::AddSubImm(0xD1000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm12, 0);
}

constexpr Insn Sub(XReg dst, XReg src, uint32_t imm12, Lsl12Tag)
{
    return Detail::AddSubImm(0xD1000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm12, 1);
}

constexpr Insn Sub(XReg dst, SpReg src, uint32_t imm12)
{
    return Detail::AddSubImm(0xD1000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm12, 0);
}

constexpr Insn Sub(XReg dst, SpReg src, uint32_t imm12, Lsl12Tag)
{
    return Detail::AddSubImm(0xD1000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm12, 1);
}

constexpr Insn Sub(SpReg dst, XReg src, uint32_t imm12)
{
    return Detail::AddSubImm(0xD1000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm12, 0);
}

constexpr Insn Sub(SpReg dst, SpReg src, uint32_t imm12)
{
    return Detail::AddSubImm(0xD1000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm12, 0);
}

constexpr Insn Sub(WReg dst, WReg src, uint32_t imm12)
{
    return Detail::AddSubImm(0x51000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm12, 0);
}

constexpr Insn Sub(WReg dst, WReg src, uint32_t imm12, Lsl12Tag)
{
    return Detail::AddSubImm(0x51000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm12, 1);
}

template <typename D, typename N, typename M, Detail::EnableXOperands<D, N, M> = 0>
constexpr Insn Sub(D dst, N src, M src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
{
    return Detail::AddSubShifted(0xCB000000, dst, src, src2, shift, amount, 63);
}

template <typename D, typename N, typename M, Detail::EnableWOperands<D, N, M> = 0>
constexpr Insn Sub(D dst, N src, M src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
{
    return Detail::AddSubShifted(0x4B000000, dst, src, src2, shift, amount, 31);
}

// ADDS/SUBS (immediate): sf op S 100010 sh imm12 Rn Rd, with op/S selecting
// adds (0/1) and subs (1/1). The destination is <Xd>, which accepts the zero
// register (cmp/cmn use it), while the source is <Xn|SP>, which accepts SP.
constexpr Insn Adds(XReg dst, XReg src, uint32_t imm12)
{
    return Detail::AddSubImm(0xB1000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm12, 0);
}

constexpr Insn Adds(XReg dst, XReg src, uint32_t imm12, Lsl12Tag)
{
    return Detail::AddSubImm(0xB1000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm12, 1);
}

constexpr Insn Adds(XzrReg dst, XReg src, uint32_t imm12)
{
    return Detail::AddSubImm(0xB1000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm12, 0);
}

constexpr Insn Adds(XzrReg dst, XReg src, uint32_t imm12, Lsl12Tag)
{
    return Detail::AddSubImm(0xB1000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm12, 1);
}

constexpr Insn Adds(XReg dst, SpReg src, uint32_t imm12)
{
    return Detail::AddSubImm(0xB1000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm12, 0);
}

constexpr Insn Adds(XReg dst, SpReg src, uint32_t imm12, Lsl12Tag)
{
    return Detail::AddSubImm(0xB1000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm12, 1);
}

constexpr Insn Adds(XzrReg dst, SpReg src, uint32_t imm12)
{
    return Detail::AddSubImm(0xB1000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm12, 0);
}

constexpr Insn Adds(XzrReg dst, SpReg src, uint32_t imm12, Lsl12Tag)
{
    return Detail::AddSubImm(0xB1000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm12, 1);
}

constexpr Insn Adds(WReg dst, WReg src, uint32_t imm12)
{
    return Detail::AddSubImm(0x31000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm12, 0);
}

constexpr Insn Adds(WReg dst, WReg src, uint32_t imm12, Lsl12Tag)
{
    return Detail::AddSubImm(0x31000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm12, 1);
}

constexpr Insn Adds(WzrReg dst, WReg src, uint32_t imm12)
{
    return Detail::AddSubImm(0x31000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm12, 0);
}

constexpr Insn Adds(WzrReg dst, WReg src, uint32_t imm12, Lsl12Tag)
{
    return Detail::AddSubImm(0x31000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm12, 1);
}

// ADDS/SUBS (shifted register): sf op S 01011 shift 0 imm6 Rm Rn Rd.
template <typename D, typename N, typename M, Detail::EnableXOperands<D, N, M> = 0>
constexpr Insn Adds(D dst, N src, M src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
{
    return Detail::AddSubShifted(0xAB000000, dst, src, src2, shift, amount, 63);
}

template <typename D, typename N, typename M, Detail::EnableWOperands<D, N, M> = 0>
constexpr Insn Adds(D dst, N src, M src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
{
    return Detail::AddSubShifted(0x2B000000, dst, src, src2, shift, amount, 31);
}

constexpr Insn Subs(XReg dst, XReg src, uint32_t imm12)
{
    return Detail::AddSubImm(0xF1000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm12, 0);
}

constexpr Insn Subs(XReg dst, XReg src, uint32_t imm12, Lsl12Tag)
{
    return Detail::AddSubImm(0xF1000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm12, 1);
}

constexpr Insn Subs(XzrReg dst, XReg src, uint32_t imm12)
{
    return Detail::AddSubImm(0xF1000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm12, 0);
}

constexpr Insn Subs(XzrReg dst, XReg src, uint32_t imm12, Lsl12Tag)
{
    return Detail::AddSubImm(0xF1000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm12, 1);
}

constexpr Insn Subs(XReg dst, SpReg src, uint32_t imm12)
{
    return Detail::AddSubImm(0xF1000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm12, 0);
}

constexpr Insn Subs(XReg dst, SpReg src, uint32_t imm12, Lsl12Tag)
{
    return Detail::AddSubImm(0xF1000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm12, 1);
}

constexpr Insn Subs(XzrReg dst, SpReg src, uint32_t imm12)
{
    return Detail::AddSubImm(0xF1000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm12, 0);
}

constexpr Insn Subs(XzrReg dst, SpReg src, uint32_t imm12, Lsl12Tag)
{
    return Detail::AddSubImm(0xF1000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm12, 1);
}

constexpr Insn Subs(WReg dst, WReg src, uint32_t imm12)
{
    return Detail::AddSubImm(0x71000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm12, 0);
}

constexpr Insn Subs(WReg dst, WReg src, uint32_t imm12, Lsl12Tag)
{
    return Detail::AddSubImm(0x71000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm12, 1);
}

constexpr Insn Subs(WzrReg dst, WReg src, uint32_t imm12)
{
    return Detail::AddSubImm(0x71000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm12, 0);
}

constexpr Insn Subs(WzrReg dst, WReg src, uint32_t imm12, Lsl12Tag)
{
    return Detail::AddSubImm(0x71000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm12, 1);
}

template <typename D, typename N, typename M, Detail::EnableXOperands<D, N, M> = 0>
constexpr Insn Subs(D dst, N src, M src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
{
    return Detail::AddSubShifted(0xEB000000, dst, src, src2, shift, amount, 63);
}

template <typename D, typename N, typename M, Detail::EnableWOperands<D, N, M> = 0>
constexpr Insn Subs(D dst, N src, M src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
{
    return Detail::AddSubShifted(0x6B000000, dst, src, src2, shift, amount, 31);
}

// AND/ORR/EOR/ANDS (immediate). The immediate is a bitmask immediate (a run of
// ones replicated at a power-of-two period). The destination is <Xd|SP> for
// and/orr/eor and <Xd> (accepting the zero register) for ands; the source is
// <Xn>. A 32-bit form checks the immediate against the 32-bit width.
constexpr Insn And(XReg dst, XReg src, uint64_t imm)
{
    return Detail::LogicalImm(0x92000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm, 64);
}

constexpr Insn And(SpReg dst, XReg src, uint64_t imm)
{
    return Detail::LogicalImm(0x92000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm, 64);
}

constexpr Insn And(WReg dst, WReg src, uint32_t imm)
{
    return Detail::LogicalImm(0x12000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm, 32);
}

constexpr Insn Orr(XReg dst, XReg src, uint64_t imm)
{
    return Detail::LogicalImm(0xB2000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm, 64);
}

constexpr Insn Orr(SpReg dst, XReg src, uint64_t imm)
{
    return Detail::LogicalImm(0xB2000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm, 64);
}

constexpr Insn Orr(WReg dst, WReg src, uint32_t imm)
{
    return Detail::LogicalImm(0x32000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm, 32);
}

constexpr Insn Eor(XReg dst, XReg src, uint64_t imm)
{
    return Detail::LogicalImm(0xD2000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm, 64);
}

constexpr Insn Eor(SpReg dst, XReg src, uint64_t imm)
{
    return Detail::LogicalImm(0xD2000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm, 64);
}

constexpr Insn Eor(WReg dst, WReg src, uint32_t imm)
{
    return Detail::LogicalImm(0x52000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm, 32);
}

constexpr Insn Ands(XReg dst, XReg src, uint64_t imm)
{
    return Detail::LogicalImm(0xF2000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm, 64);
}

constexpr Insn Ands(XzrReg dst, XReg src, uint64_t imm)
{
    return Detail::LogicalImm(0xF2000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm, 64);
}

constexpr Insn Ands(WReg dst, WReg src, uint32_t imm)
{
    return Detail::LogicalImm(0x72000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm, 32);
}

constexpr Insn Ands(WzrReg dst, WReg src, uint32_t imm)
{
    return Detail::LogicalImm(0x72000000, Detail::CodeOf(dst), Detail::CodeOf(src), imm, 32);
}

// AND/ORR/EOR/ANDS (shifted register): sf opc 01010 shift 0 imm6 Rm Rn Rd. As
// in the shifted add/sub forms, the register fields accept the zero register.
template <typename D, typename N, typename M, Detail::EnableXOperands<D, N, M> = 0>
constexpr Insn And(D dst, N src, M src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
{
    return Detail::LogicalShifted(0x8A000000, dst, src, src2, shift, amount, 63);
}

template <typename D, typename N, typename M, Detail::EnableWOperands<D, N, M> = 0>
constexpr Insn And(D dst, N src, M src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
{
    return Detail::LogicalShifted(0x0A000000, dst, src, src2, shift, amount, 31);
}

template <typename D, typename N, typename M, Detail::EnableXOperands<D, N, M> = 0>
constexpr Insn Orr(D dst, N src, M src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
{
    return Detail::LogicalShifted(0xAA000000, dst, src, src2, shift, amount, 63);
}

template <typename D, typename N, typename M, Detail::EnableWOperands<D, N, M> = 0>
constexpr Insn Orr(D dst, N src, M src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
{
    return Detail::LogicalShifted(0x2A000000, dst, src, src2, shift, amount, 31);
}

template <typename D, typename N, typename M, Detail::EnableXOperands<D, N, M> = 0>
constexpr Insn Eor(D dst, N src, M src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
{
    return Detail::LogicalShifted(0xCA000000, dst, src, src2, shift, amount, 63);
}

template <typename D, typename N, typename M, Detail::EnableWOperands<D, N, M> = 0>
constexpr Insn Eor(D dst, N src, M src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
{
    return Detail::LogicalShifted(0x4A000000, dst, src, src2, shift, amount, 31);
}

template <typename D, typename N, typename M, Detail::EnableXOperands<D, N, M> = 0>
constexpr Insn Ands(D dst, N src, M src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
{
    return Detail::LogicalShifted(0xEA000000, dst, src, src2, shift, amount, 63);
}

template <typename D, typename N, typename M, Detail::EnableWOperands<D, N, M> = 0>
constexpr Insn Ands(D dst, N src, M src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
{
    return Detail::LogicalShifted(0x6A000000, dst, src, src2, shift, amount, 31);
}

// LSLV/LSRV/ASRV (data-processing, 2 source): dst = src shifted by the low bits
// of `amount`. Every register field accepts the zero register, so the three
// operands are templates constrained to one width. The shift is the amount
// taken modulo the width, so an amount at or above the width shifts everything
// out (LSL/LSR) or fills with the sign bit (ASR); no amount is rejected.
template <typename D, typename N, typename M, Detail::EnableXOperands<D, N, M> = 0>
constexpr Insn Lsl(D dst, N src, M amount)
{
    return Detail::DataProc2Source(0x9AC02000, Detail::CodeOf(dst), Detail::CodeOf(src), Detail::CodeOf(amount), 0);
}

template <typename D, typename N, typename M, Detail::EnableWOperands<D, N, M> = 0>
constexpr Insn Lsl(D dst, N src, M amount)
{
    return Detail::DataProc2Source(0x1AC02000, Detail::CodeOf(dst), Detail::CodeOf(src), Detail::CodeOf(amount), 0);
}

template <typename D, typename N, typename M, Detail::EnableXOperands<D, N, M> = 0>
constexpr Insn Lsr(D dst, N src, M amount)
{
    return Detail::DataProc2Source(0x9AC02000, Detail::CodeOf(dst), Detail::CodeOf(src), Detail::CodeOf(amount), 1);
}

template <typename D, typename N, typename M, Detail::EnableWOperands<D, N, M> = 0>
constexpr Insn Lsr(D dst, N src, M amount)
{
    return Detail::DataProc2Source(0x1AC02000, Detail::CodeOf(dst), Detail::CodeOf(src), Detail::CodeOf(amount), 1);
}

template <typename D, typename N, typename M, Detail::EnableXOperands<D, N, M> = 0>
constexpr Insn Asr(D dst, N src, M amount)
{
    return Detail::DataProc2Source(0x9AC02000, Detail::CodeOf(dst), Detail::CodeOf(src), Detail::CodeOf(amount), 2);
}

template <typename D, typename N, typename M, Detail::EnableWOperands<D, N, M> = 0>
constexpr Insn Asr(D dst, N src, M amount)
{
    return Detail::DataProc2Source(0x1AC02000, Detail::CodeOf(dst), Detail::CodeOf(src), Detail::CodeOf(amount), 2);
}

// LSL/LSR/ASR (immediate): aliases of the bitfield forms, so they keep the same
// register rules (zero register allowed) and the same immediate rules (the
// shift is below the operand width):
//   LSL #s = UBFM #(-s MOD width), #(width-1-s)
//   LSR #s = UBFM #s, #(width-1)
//   ASR #s = SBFM #s, #(width-1)
template <typename D, typename N, Detail::EnableXPair<D, N> = 0> constexpr Insn Lsl(D dst, N src, uint32_t shift)
{
    ASSERT(shift <= 63);
    return Detail::Bitfield(0xD3400000, Detail::CodeOf(dst), Detail::CodeOf(src), (64u - shift) % 64u, 63u - shift, 63);
}

template <typename D, typename N, Detail::EnableWPair<D, N> = 0> constexpr Insn Lsl(D dst, N src, uint32_t shift)
{
    ASSERT(shift <= 31);
    return Detail::Bitfield(0x53000000, Detail::CodeOf(dst), Detail::CodeOf(src), (32u - shift) % 32u, 31u - shift, 31);
}

template <typename D, typename N, Detail::EnableXPair<D, N> = 0> constexpr Insn Lsr(D dst, N src, uint32_t shift)
{
    ASSERT(shift <= 63);
    return Detail::Bitfield(0xD3400000, Detail::CodeOf(dst), Detail::CodeOf(src), shift, 63, 63);
}

template <typename D, typename N, Detail::EnableWPair<D, N> = 0> constexpr Insn Lsr(D dst, N src, uint32_t shift)
{
    ASSERT(shift <= 31);
    return Detail::Bitfield(0x53000000, Detail::CodeOf(dst), Detail::CodeOf(src), shift, 31, 31);
}

template <typename D, typename N, Detail::EnableXPair<D, N> = 0> constexpr Insn Asr(D dst, N src, uint32_t shift)
{
    ASSERT(shift <= 63);
    return Detail::Bitfield(0x93400000, Detail::CodeOf(dst), Detail::CodeOf(src), shift, 63, 63);
}

template <typename D, typename N, Detail::EnableWPair<D, N> = 0> constexpr Insn Asr(D dst, N src, uint32_t shift)
{
    ASSERT(shift <= 31);
    return Detail::Bitfield(0x13000000, Detail::CodeOf(dst), Detail::CodeOf(src), shift, 31, 31);
}

// SBFM/UBFM (bitfield): the general form the LSR/ASR/Ubfx/Sbfx aliases below
// stand for. immr rotates the source right, imms keeps the low imms+1 bits of
// the result; a 32-bit form takes both below 32. BFM (the insert form) is not
// modeled, so the opc field is only 00 (SBFM) and 10 (UBFM) here.
template <typename D, typename N, Detail::EnableXPair<D, N> = 0>
constexpr Insn Ubfm(D dst, N src, uint32_t immr, uint32_t imms)
{
    return Detail::Bitfield(0xD3400000, Detail::CodeOf(dst), Detail::CodeOf(src), immr, imms, 63);
}

template <typename D, typename N, Detail::EnableWPair<D, N> = 0>
constexpr Insn Ubfm(D dst, N src, uint32_t immr, uint32_t imms)
{
    return Detail::Bitfield(0x53000000, Detail::CodeOf(dst), Detail::CodeOf(src), immr, imms, 31);
}

template <typename D, typename N, Detail::EnableXPair<D, N> = 0>
constexpr Insn Sbfm(D dst, N src, uint32_t immr, uint32_t imms)
{
    return Detail::Bitfield(0x93400000, Detail::CodeOf(dst), Detail::CodeOf(src), immr, imms, 63);
}

template <typename D, typename N, Detail::EnableWPair<D, N> = 0>
constexpr Insn Sbfm(D dst, N src, uint32_t immr, uint32_t imms)
{
    return Detail::Bitfield(0x13000000, Detail::CodeOf(dst), Detail::CodeOf(src), immr, imms, 31);
}

// UBFX/SBFX: extract `width` bits starting at `lsb` (so SBFX sign-extends what
// UBFX zero-extends). The field is UBFM/SBFM with imms = lsb + width - 1; the
// extraction must stay inside the operand width.
template <typename D, typename N, Detail::EnableXPair<D, N> = 0>
constexpr Insn Ubfx(D dst, N src, uint32_t lsb, uint32_t width)
{
    ASSERT(width >= 1);
    ASSERT(lsb + width <= 64);
    return Ubfm(dst, src, lsb, lsb + width - 1);
}

template <typename D, typename N, Detail::EnableWPair<D, N> = 0>
constexpr Insn Ubfx(D dst, N src, uint32_t lsb, uint32_t width)
{
    ASSERT(width >= 1);
    ASSERT(lsb + width <= 32);
    return Ubfm(dst, src, lsb, lsb + width - 1);
}

template <typename D, typename N, Detail::EnableXPair<D, N> = 0>
constexpr Insn Sbfx(D dst, N src, uint32_t lsb, uint32_t width)
{
    ASSERT(width >= 1);
    ASSERT(lsb + width <= 64);
    return Sbfm(dst, src, lsb, lsb + width - 1);
}

template <typename D, typename N, Detail::EnableWPair<D, N> = 0>
constexpr Insn Sbfx(D dst, N src, uint32_t lsb, uint32_t width)
{
    ASSERT(width >= 1);
    ASSERT(lsb + width <= 32);
    return Sbfm(dst, src, lsb, lsb + width - 1);
}

// SXTB/SXTH/SXTW: sign-extend the low byte/halfword/word of a 32-bit source
// into a 64-bit destination. The source is a W register in an otherwise 64-bit
// form (SBFM #0, #7/#15/#31), so these do not fit the Ubfm/Sbfm overloads and
// build their word directly.
template <typename D, typename N, Detail::EnableXFromW<D, N> = 0> constexpr Insn Sxtb(D dst, N src)
{
    return Detail::Bitfield(0x93400000, Detail::CodeOf(dst), Detail::CodeOf(src), 0, 7, 63);
}

template <typename D, typename N, Detail::EnableXFromW<D, N> = 0> constexpr Insn Sxth(D dst, N src)
{
    return Detail::Bitfield(0x93400000, Detail::CodeOf(dst), Detail::CodeOf(src), 0, 15, 63);
}

template <typename D, typename N, Detail::EnableXFromW<D, N> = 0> constexpr Insn Sxtw(D dst, N src)
{
    return Detail::Bitfield(0x93400000, Detail::CodeOf(dst), Detail::CodeOf(src), 0, 31, 63);
}

// UXTB/UXTH: zero-extend into a 32-bit destination (there is no 64-bit form of
// these two; the 64-bit zero extension is Ubfx or Uxtw, the latter not modeled).
template <typename D, typename N, Detail::EnableWPair<D, N> = 0> constexpr Insn Uxtb(D dst, N src)
{
    return Ubfm(dst, src, 0, 7);
}

template <typename D, typename N, Detail::EnableWPair<D, N> = 0> constexpr Insn Uxth(D dst, N src)
{
    return Ubfm(dst, src, 0, 15);
}

// Loads and stores. The address operand is Mem(base, #offset) for the
// immediate forms and Mem(base, index[, extend][, Scaled]) for the
// register-offset ones; the mnemonic picks the access size and the signedness.
// A mnemonic whose offset field is scaled takes a non-negative multiple of the
// access size and one whose field is unscaled takes any value that fits it, so
// the two address forms of one access are named separately (LDR against LDUR).

// LDR (immediate): Rt = zero-extended memory[base + offset].
template <typename Rt, Detail::EnableXReg<Rt> = 0> constexpr Insn Ldr(Rt rt, const MemImm& mem)
{
    return Detail::LoadStoreUnsigned(0xF9400000, Detail::CodeOf(rt), mem, 8);
}

template <typename Rt, Detail::EnableWReg<Rt> = 0> constexpr Insn Ldr(Rt rt, const MemImm& mem)
{
    return Detail::LoadStoreUnsigned(0xB9400000, Detail::CodeOf(rt), mem, 4);
}

// LDR (register).
template <typename Rt, Detail::EnableXReg<Rt> = 0> constexpr Insn Ldr(Rt rt, const MemIndex& mem)
{
    return Detail::LoadStoreRegister(0xF8600800, Detail::CodeOf(rt), mem);
}

template <typename Rt, Detail::EnableWReg<Rt> = 0> constexpr Insn Ldr(Rt rt, const MemIndex& mem)
{
    return Detail::LoadStoreRegister(0xB8600800, Detail::CodeOf(rt), mem);
}

// STR (immediate): memory[base + offset] = the low half or all of Rt.
template <typename Rt, Detail::EnableXReg<Rt> = 0> constexpr Insn Str(Rt rt, const MemImm& mem)
{
    return Detail::LoadStoreUnsigned(0xF9000000, Detail::CodeOf(rt), mem, 8);
}

template <typename Rt, Detail::EnableWReg<Rt> = 0> constexpr Insn Str(Rt rt, const MemImm& mem)
{
    return Detail::LoadStoreUnsigned(0xB9000000, Detail::CodeOf(rt), mem, 4);
}

// STR (register).
template <typename Rt, Detail::EnableXReg<Rt> = 0> constexpr Insn Str(Rt rt, const MemIndex& mem)
{
    return Detail::LoadStoreRegister(0xF8200800, Detail::CodeOf(rt), mem);
}

template <typename Rt, Detail::EnableWReg<Rt> = 0> constexpr Insn Str(Rt rt, const MemIndex& mem)
{
    return Detail::LoadStoreRegister(0xB8200800, Detail::CodeOf(rt), mem);
}

// LDRB: Rt = zero-extended byte.
template <typename Rt, Detail::EnableWReg<Rt> = 0> constexpr Insn Ldrb(Rt rt, const MemImm& mem)
{
    return Detail::LoadStoreUnsigned(0x39400000, Detail::CodeOf(rt), mem, 1);
}

template <typename Rt, Detail::EnableWReg<Rt> = 0> constexpr Insn Ldrb(Rt rt, const MemIndex& mem)
{
    return Detail::LoadStoreRegister(0x38600800, Detail::CodeOf(rt), mem);
}

// STRB: memory[base + offset] = the low byte of Rt.
template <typename Rt, Detail::EnableWReg<Rt> = 0> constexpr Insn Strb(Rt rt, const MemImm& mem)
{
    return Detail::LoadStoreUnsigned(0x39000000, Detail::CodeOf(rt), mem, 1);
}

template <typename Rt, Detail::EnableWReg<Rt> = 0> constexpr Insn Strb(Rt rt, const MemIndex& mem)
{
    return Detail::LoadStoreRegister(0x38200800, Detail::CodeOf(rt), mem);
}

// LDRH: Rt = zero-extended halfword.
template <typename Rt, Detail::EnableWReg<Rt> = 0> constexpr Insn Ldrh(Rt rt, const MemImm& mem)
{
    return Detail::LoadStoreUnsigned(0x79400000, Detail::CodeOf(rt), mem, 2);
}

template <typename Rt, Detail::EnableWReg<Rt> = 0> constexpr Insn Ldrh(Rt rt, const MemIndex& mem)
{
    return Detail::LoadStoreRegister(0x78600800, Detail::CodeOf(rt), mem);
}

// STRH: memory[base + offset] = the low halfword of Rt.
template <typename Rt, Detail::EnableWReg<Rt> = 0> constexpr Insn Strh(Rt rt, const MemImm& mem)
{
    return Detail::LoadStoreUnsigned(0x79000000, Detail::CodeOf(rt), mem, 2);
}

template <typename Rt, Detail::EnableWReg<Rt> = 0> constexpr Insn Strh(Rt rt, const MemIndex& mem)
{
    return Detail::LoadStoreRegister(0x78200800, Detail::CodeOf(rt), mem);
}

// LDRSB: Rt = sign-extended byte, into a 64-bit or a 32-bit destination.
template <typename Rt, Detail::EnableXReg<Rt> = 0> constexpr Insn Ldrsb(Rt rt, const MemImm& mem)
{
    return Detail::LoadStoreUnsigned(0x39800000, Detail::CodeOf(rt), mem, 1);
}

template <typename Rt, Detail::EnableWReg<Rt> = 0> constexpr Insn Ldrsb(Rt rt, const MemImm& mem)
{
    return Detail::LoadStoreUnsigned(0x39C00000, Detail::CodeOf(rt), mem, 1);
}

template <typename Rt, Detail::EnableXReg<Rt> = 0> constexpr Insn Ldrsb(Rt rt, const MemIndex& mem)
{
    return Detail::LoadStoreRegister(0x38A00800, Detail::CodeOf(rt), mem);
}

template <typename Rt, Detail::EnableWReg<Rt> = 0> constexpr Insn Ldrsb(Rt rt, const MemIndex& mem)
{
    return Detail::LoadStoreRegister(0x38E00800, Detail::CodeOf(rt), mem);
}

// LDRSH: Rt = sign-extended halfword, into a 64-bit or a 32-bit destination.
template <typename Rt, Detail::EnableXReg<Rt> = 0> constexpr Insn Ldrsh(Rt rt, const MemImm& mem)
{
    return Detail::LoadStoreUnsigned(0x79800000, Detail::CodeOf(rt), mem, 2);
}

template <typename Rt, Detail::EnableWReg<Rt> = 0> constexpr Insn Ldrsh(Rt rt, const MemImm& mem)
{
    return Detail::LoadStoreUnsigned(0x79C00000, Detail::CodeOf(rt), mem, 2);
}

template <typename Rt, Detail::EnableXReg<Rt> = 0> constexpr Insn Ldrsh(Rt rt, const MemIndex& mem)
{
    return Detail::LoadStoreRegister(0x78A00800, Detail::CodeOf(rt), mem);
}

template <typename Rt, Detail::EnableWReg<Rt> = 0> constexpr Insn Ldrsh(Rt rt, const MemIndex& mem)
{
    return Detail::LoadStoreRegister(0x78E00800, Detail::CodeOf(rt), mem);
}

// LDRSW: Rt = sign-extended word. Only a 64-bit destination exists.
template <typename Rt, Detail::EnableXReg<Rt> = 0> constexpr Insn Ldrsw(Rt rt, const MemImm& mem)
{
    return Detail::LoadStoreUnsigned(0xB9800000, Detail::CodeOf(rt), mem, 4);
}

template <typename Rt, Detail::EnableXReg<Rt> = 0> constexpr Insn Ldrsw(Rt rt, const MemIndex& mem)
{
    return Detail::LoadStoreRegister(0xB8A00800, Detail::CodeOf(rt), mem);
}

// LDUR/STUR (unscaled): the same accesses as LDR/STR with an offset that is not
// scaled by the access size, which is what makes an unaligned byte address
// expressible.
template <typename Rt, Detail::EnableXReg<Rt> = 0> constexpr Insn Ldur(Rt rt, const MemImm& mem)
{
    return Detail::LoadStoreUnscaled(0xF8400000, Detail::CodeOf(rt), mem);
}

template <typename Rt, Detail::EnableWReg<Rt> = 0> constexpr Insn Ldur(Rt rt, const MemImm& mem)
{
    return Detail::LoadStoreUnscaled(0xB8400000, Detail::CodeOf(rt), mem);
}

template <typename Rt, Detail::EnableXReg<Rt> = 0> constexpr Insn Stur(Rt rt, const MemImm& mem)
{
    return Detail::LoadStoreUnscaled(0xF8000000, Detail::CodeOf(rt), mem);
}

template <typename Rt, Detail::EnableWReg<Rt> = 0> constexpr Insn Stur(Rt rt, const MemImm& mem)
{
    return Detail::LoadStoreUnscaled(0xB8000000, Detail::CodeOf(rt), mem);
}

template <typename Rt, Detail::EnableWReg<Rt> = 0> constexpr Insn Ldurb(Rt rt, const MemImm& mem)
{
    return Detail::LoadStoreUnscaled(0x38400000, Detail::CodeOf(rt), mem);
}

template <typename Rt, Detail::EnableWReg<Rt> = 0> constexpr Insn Sturb(Rt rt, const MemImm& mem)
{
    return Detail::LoadStoreUnscaled(0x38000000, Detail::CodeOf(rt), mem);
}

template <typename Rt, Detail::EnableWReg<Rt> = 0> constexpr Insn Ldurh(Rt rt, const MemImm& mem)
{
    return Detail::LoadStoreUnscaled(0x78400000, Detail::CodeOf(rt), mem);
}

template <typename Rt, Detail::EnableWReg<Rt> = 0> constexpr Insn Sturh(Rt rt, const MemImm& mem)
{
    return Detail::LoadStoreUnscaled(0x78000000, Detail::CodeOf(rt), mem);
}

template <typename Rt, Detail::EnableXReg<Rt> = 0> constexpr Insn Ldursb(Rt rt, const MemImm& mem)
{
    return Detail::LoadStoreUnscaled(0x38800000, Detail::CodeOf(rt), mem);
}

template <typename Rt, Detail::EnableWReg<Rt> = 0> constexpr Insn Ldursb(Rt rt, const MemImm& mem)
{
    return Detail::LoadStoreUnscaled(0x38C00000, Detail::CodeOf(rt), mem);
}

template <typename Rt, Detail::EnableXReg<Rt> = 0> constexpr Insn Ldursh(Rt rt, const MemImm& mem)
{
    return Detail::LoadStoreUnscaled(0x78800000, Detail::CodeOf(rt), mem);
}

template <typename Rt, Detail::EnableWReg<Rt> = 0> constexpr Insn Ldursh(Rt rt, const MemImm& mem)
{
    return Detail::LoadStoreUnscaled(0x78C00000, Detail::CodeOf(rt), mem);
}

template <typename Rt, Detail::EnableXReg<Rt> = 0> constexpr Insn Ldursw(Rt rt, const MemImm& mem)
{
    return Detail::LoadStoreUnscaled(0xB8800000, Detail::CodeOf(rt), mem);
}

// LDP/STP (pair): two registers moved to or from one address. A loading pair
// reads both registers from the same address, which the architecture leaves
// unpredictable when they are the same register, so Ldp (and Ldpsw, which also
// loads) rejects that; a storing pair may repeat one.
template <typename Rt, typename Rt2, Detail::EnableXReg<Rt> = 0, Detail::EnableXReg<Rt2> = 0>
constexpr Insn Ldp(Rt rt, Rt2 rt2, const MemImm& mem)
{
    ASSERT(Detail::RegCode(rt) != Detail::RegCode(rt2));
    return Detail::LoadStorePair(0xA9400000, Detail::CodeOf(rt), Detail::CodeOf(rt2), mem, 8, true);
}

template <typename Rt, typename Rt2, Detail::EnableWReg<Rt> = 0, Detail::EnableWReg<Rt2> = 0>
constexpr Insn Ldp(Rt rt, Rt2 rt2, const MemImm& mem)
{
    ASSERT(Detail::RegCode(rt) != Detail::RegCode(rt2));
    return Detail::LoadStorePair(0x29400000, Detail::CodeOf(rt), Detail::CodeOf(rt2), mem, 4, true);
}

template <typename Rt, typename Rt2, Detail::EnableXReg<Rt> = 0, Detail::EnableXReg<Rt2> = 0>
constexpr Insn Stp(Rt rt, Rt2 rt2, const MemImm& mem)
{
    return Detail::LoadStorePair(0xA9000000, Detail::CodeOf(rt), Detail::CodeOf(rt2), mem, 8, false);
}

template <typename Rt, typename Rt2, Detail::EnableWReg<Rt> = 0, Detail::EnableWReg<Rt2> = 0>
constexpr Insn Stp(Rt rt, Rt2 rt2, const MemImm& mem)
{
    return Detail::LoadStorePair(0x29000000, Detail::CodeOf(rt), Detail::CodeOf(rt2), mem, 4, false);
}

// LDPSW: two sign-extended words from one address into two 64-bit registers.
template <typename Rt, typename Rt2, Detail::EnableXReg<Rt> = 0, Detail::EnableXReg<Rt2> = 0>
constexpr Insn Ldpsw(Rt rt, Rt2 rt2, const MemImm& mem)
{
    return Detail::LoadStorePair(0x69400000, Detail::CodeOf(rt), Detail::CodeOf(rt2), mem, 4, true);
}

// NOP: 1101010100 0 00 110 mmmmm 00000 nnnnn
constexpr Insn Nop() { return Detail::Op { 0xD503201F }.Finish(); }

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
        case FixupKind::Branch26:     return Detail::BranchFixup<0, 26, 4>(word, delta);
        case FixupKind::CondBranch19:
        case FixupKind::CmpBranch19:  return Detail::BranchFixup<5, 19, 4>(word, delta);
        case FixupKind::TestBranch14: return Detail::BranchFixup<5, 14, 4>(word, delta);
        case FixupKind::Adr21:        {
            ASSERT(delta % 4 == 0);
            ASSERT(delta >= -(int64_t { 1 } << 20) && delta < (int64_t { 1 } << 20));
            const uint32_t offset = static_cast<uint32_t>(delta) & 0x1FFFFF;
            return (Detail::Op { word.Bits() } | Detail::Field<5, 19> { offset >> 2 } |
                    Detail::Field<29, 2> { offset & 0x3 })
                .Finish();
        }
    }
    return word;
}

} // namespace Encode

} // namespace Asm
