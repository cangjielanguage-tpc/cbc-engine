#pragma once

#include <cstdint>
#include <memory>
#include <utility>

#include "asm/encoding.h"
#include "asm/insn.h"
#include "asm/labels.h"
#include "asm/operands.h"
#include "utils/span.h"

namespace Asm {

// Emits aarch64 instructions into a storage, and tracks labels through a label
// manager it owns.
//
// The storage type is a template parameter because emission is the hot path:
// every Emit is a direct call to the storage's Write, with no indirection. The
// label manager is an abstract interface behind a unique_ptr, because label
// bookkeeping is not on that path and its representation may change.
template <class Storage> class Assembler {
public:
    Assembler() : Assembler { Storage {} } {}

    explicit Assembler(Storage storage)
        : storage_ { std::move(storage) },
          labels_ { std::make_unique<BasicLabelManager>() }
    {}

    Assembler(Storage storage, std::unique_ptr<LabelManager> labels)
        : storage_ { std::move(storage) },
          labels_ { std::move(labels) }
    {}

    Assembler(Assembler&&)                 = default;
    Assembler& operator=(Assembler&&)      = default;
    Assembler(const Assembler&)            = delete;
    Assembler& operator=(const Assembler&) = delete;

    // ADD (immediate): dst = src + (imm12 << (Lsl12 ? 12 : 0)); the flags are
    // not updated. dst and src are <Xd|SP>: dst may be SP, as may a src passed
    // as SpReg; a src passed as XReg is a real register. imm12 is unsigned.
    void Add(XReg dst, XReg src, uint32_t imm12) { Emit(Encode::Add(dst, src, imm12)); }

    void Add(XReg dst, XReg src, uint32_t imm12, Lsl12Tag tag) { Emit(Encode::Add(dst, src, imm12, tag)); }

    void Add(XReg dst, SpReg src, uint32_t imm12) { Emit(Encode::Add(dst, src, imm12)); }

    void Add(XReg dst, SpReg src, uint32_t imm12, Lsl12Tag tag) { Emit(Encode::Add(dst, src, imm12, tag)); }

    void Add(SpReg dst, XReg src, uint32_t imm12) { Emit(Encode::Add(dst, src, imm12)); }

    void Add(SpReg dst, SpReg src, uint32_t imm12) { Emit(Encode::Add(dst, src, imm12)); }

    void Add(WReg dst, WReg src, uint32_t imm12) { Emit(Encode::Add(dst, src, imm12)); }

    void Add(WReg dst, WReg src, uint32_t imm12, Lsl12Tag tag) { Emit(Encode::Add(dst, src, imm12, tag)); }

    // ADD (shifted register): dst = src + (src2 shifted by amount); the flags
    // are not updated. The register operands may be the zero register, so they
    // are templates over the operand types, constrained to one width.
    template <typename D, typename N, typename M, Detail::EnableXOperands<D, N, M> = 0>
    void Add(D dst, N src, M src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
    {
        Emit(Encode::Add(dst, src, src2, shift, amount));
    }

    template <typename D, typename N, typename M, Detail::EnableWOperands<D, N, M> = 0>
    void Add(D dst, N src, M src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
    {
        Emit(Encode::Add(dst, src, src2, shift, amount));
    }

    // SUB (immediate): dst = src - (imm12 << (Lsl12 ? 12 : 0)); the flags are
    // not updated. Operands as in Add immediate.
    void Sub(XReg dst, XReg src, uint32_t imm12) { Emit(Encode::Sub(dst, src, imm12)); }

    void Sub(XReg dst, XReg src, uint32_t imm12, Lsl12Tag tag) { Emit(Encode::Sub(dst, src, imm12, tag)); }

    void Sub(XReg dst, SpReg src, uint32_t imm12) { Emit(Encode::Sub(dst, src, imm12)); }

    void Sub(XReg dst, SpReg src, uint32_t imm12, Lsl12Tag tag) { Emit(Encode::Sub(dst, src, imm12, tag)); }

    void Sub(SpReg dst, XReg src, uint32_t imm12) { Emit(Encode::Sub(dst, src, imm12)); }

    void Sub(SpReg dst, SpReg src, uint32_t imm12) { Emit(Encode::Sub(dst, src, imm12)); }

    void Sub(WReg dst, WReg src, uint32_t imm12) { Emit(Encode::Sub(dst, src, imm12)); }

    void Sub(WReg dst, WReg src, uint32_t imm12, Lsl12Tag tag) { Emit(Encode::Sub(dst, src, imm12, tag)); }

    // SUB (shifted register): dst = src - (src2 shifted by amount); the flags
    // are not updated.
    template <typename D, typename N, typename M, Detail::EnableXOperands<D, N, M> = 0>
    void Sub(D dst, N src, M src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
    {
        Emit(Encode::Sub(dst, src, src2, shift, amount));
    }

    template <typename D, typename N, typename M, Detail::EnableWOperands<D, N, M> = 0>
    void Sub(D dst, N src, M src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
    {
        Emit(Encode::Sub(dst, src, src2, shift, amount));
    }

    // ADDS/SUBS (immediate and shifted register): dst = src ± operand, and set
    // the condition flags (NZCV) from the result. Operands are as in ADD/SUB,
    // except the immediate destination is <Xd>, which may be the zero register
    // — that is how a flag update with no result is written (see Cmp/Cmn).
    void Adds(XReg dst, XReg src, uint32_t imm12) { Emit(Encode::Adds(dst, src, imm12)); }

    void Adds(XReg dst, XReg src, uint32_t imm12, Lsl12Tag tag) { Emit(Encode::Adds(dst, src, imm12, tag)); }

    void Adds(XzrReg dst, XReg src, uint32_t imm12) { Emit(Encode::Adds(dst, src, imm12)); }

    void Adds(XzrReg dst, XReg src, uint32_t imm12, Lsl12Tag tag) { Emit(Encode::Adds(dst, src, imm12, tag)); }

    void Adds(XReg dst, SpReg src, uint32_t imm12) { Emit(Encode::Adds(dst, src, imm12)); }

    void Adds(XReg dst, SpReg src, uint32_t imm12, Lsl12Tag tag) { Emit(Encode::Adds(dst, src, imm12, tag)); }

    void Adds(XzrReg dst, SpReg src, uint32_t imm12) { Emit(Encode::Adds(dst, src, imm12)); }

    void Adds(XzrReg dst, SpReg src, uint32_t imm12, Lsl12Tag tag) { Emit(Encode::Adds(dst, src, imm12, tag)); }

    void Adds(WReg dst, WReg src, uint32_t imm12) { Emit(Encode::Adds(dst, src, imm12)); }

    void Adds(WReg dst, WReg src, uint32_t imm12, Lsl12Tag tag) { Emit(Encode::Adds(dst, src, imm12, tag)); }

    void Adds(WzrReg dst, WReg src, uint32_t imm12) { Emit(Encode::Adds(dst, src, imm12)); }

    void Adds(WzrReg dst, WReg src, uint32_t imm12, Lsl12Tag tag) { Emit(Encode::Adds(dst, src, imm12, tag)); }

    template <typename D, typename N, typename M, Detail::EnableXOperands<D, N, M> = 0>
    void Adds(D dst, N src, M src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
    {
        Emit(Encode::Adds(dst, src, src2, shift, amount));
    }

    template <typename D, typename N, typename M, Detail::EnableWOperands<D, N, M> = 0>
    void Adds(D dst, N src, M src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
    {
        Emit(Encode::Adds(dst, src, src2, shift, amount));
    }

    void Subs(XReg dst, XReg src, uint32_t imm12) { Emit(Encode::Subs(dst, src, imm12)); }

    void Subs(XReg dst, XReg src, uint32_t imm12, Lsl12Tag tag) { Emit(Encode::Subs(dst, src, imm12, tag)); }

    void Subs(XzrReg dst, XReg src, uint32_t imm12) { Emit(Encode::Subs(dst, src, imm12)); }

    void Subs(XzrReg dst, XReg src, uint32_t imm12, Lsl12Tag tag) { Emit(Encode::Subs(dst, src, imm12, tag)); }

    void Subs(XReg dst, SpReg src, uint32_t imm12) { Emit(Encode::Subs(dst, src, imm12)); }

    void Subs(XReg dst, SpReg src, uint32_t imm12, Lsl12Tag tag) { Emit(Encode::Subs(dst, src, imm12, tag)); }

    void Subs(XzrReg dst, SpReg src, uint32_t imm12) { Emit(Encode::Subs(dst, src, imm12)); }

    void Subs(XzrReg dst, SpReg src, uint32_t imm12, Lsl12Tag tag) { Emit(Encode::Subs(dst, src, imm12, tag)); }

    void Subs(WReg dst, WReg src, uint32_t imm12) { Emit(Encode::Subs(dst, src, imm12)); }

    void Subs(WReg dst, WReg src, uint32_t imm12, Lsl12Tag tag) { Emit(Encode::Subs(dst, src, imm12, tag)); }

    void Subs(WzrReg dst, WReg src, uint32_t imm12) { Emit(Encode::Subs(dst, src, imm12)); }

    void Subs(WzrReg dst, WReg src, uint32_t imm12, Lsl12Tag tag) { Emit(Encode::Subs(dst, src, imm12, tag)); }

    template <typename D, typename N, typename M, Detail::EnableXOperands<D, N, M> = 0>
    void Subs(D dst, N src, M src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
    {
        Emit(Encode::Subs(dst, src, src2, shift, amount));
    }

    template <typename D, typename N, typename M, Detail::EnableWOperands<D, N, M> = 0>
    void Subs(D dst, N src, M src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
    {
        Emit(Encode::Subs(dst, src, src2, shift, amount));
    }

    // CMP/CMN: compute src - operand (CMP) or src + operand (CMN), discard the
    // result, and set the flags from it. Encoded as SUBS/ADDS with the zero
    // register destination; the shift applies to a register operand only.
    void Cmp(XReg src, uint32_t imm12) { Emit(Encode::Subs(Xzr, src, imm12)); }

    void Cmp(XReg src, XReg src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
    {
        Emit(Encode::Subs(Xzr, src, src2, shift, amount));
    }

    void Cmp(WReg src, uint32_t imm12) { Emit(Encode::Subs(Wzr, src, imm12)); }

    // The stack pointer is the one register CMP/CMN compares through its
    // stack-pointer operand form, which the zero-register destination cannot use.
    void Cmp(SpReg src, uint32_t imm12) { Emit(Encode::Subs(Xzr, src, imm12)); }

    void Cmp(WReg src, WReg src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
    {
        Emit(Encode::Subs(Wzr, src, src2, shift, amount));
    }

    void Cmn(XReg src, uint32_t imm12) { Emit(Encode::Adds(Xzr, src, imm12)); }

    void Cmn(XReg src, XReg src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
    {
        Emit(Encode::Adds(Xzr, src, src2, shift, amount));
    }

    void Cmn(WReg src, uint32_t imm12) { Emit(Encode::Adds(Wzr, src, imm12)); }

    void Cmn(SpReg src, uint32_t imm12) { Emit(Encode::Adds(Xzr, src, imm12)); }

    void Cmn(WReg src, WReg src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
    {
        Emit(Encode::Adds(Wzr, src, src2, shift, amount));
    }

    // AND/ORR/EOR (immediate): dst = src &/|/^ imm, where imm is a bitmask
    // immediate. The destination is <Xd|SP>; the source is <Xn>. ANDS (immediate)
    // instead sets the flags and takes <Xd>, so it accepts the zero register.
    void And(XReg dst, XReg src, uint64_t imm) { Emit(Encode::And(dst, src, imm)); }

    void And(SpReg dst, XReg src, uint64_t imm) { Emit(Encode::And(dst, src, imm)); }

    void And(WReg dst, WReg src, uint32_t imm) { Emit(Encode::And(dst, src, imm)); }

    void Orr(XReg dst, XReg src, uint64_t imm) { Emit(Encode::Orr(dst, src, imm)); }

    void Orr(SpReg dst, XReg src, uint64_t imm) { Emit(Encode::Orr(dst, src, imm)); }

    void Orr(WReg dst, WReg src, uint32_t imm) { Emit(Encode::Orr(dst, src, imm)); }

    void Eor(XReg dst, XReg src, uint64_t imm) { Emit(Encode::Eor(dst, src, imm)); }

    void Eor(SpReg dst, XReg src, uint64_t imm) { Emit(Encode::Eor(dst, src, imm)); }

    void Eor(WReg dst, WReg src, uint32_t imm) { Emit(Encode::Eor(dst, src, imm)); }

    void Ands(XReg dst, XReg src, uint64_t imm) { Emit(Encode::Ands(dst, src, imm)); }

    void Ands(XzrReg dst, XReg src, uint64_t imm) { Emit(Encode::Ands(dst, src, imm)); }

    void Ands(WReg dst, WReg src, uint32_t imm) { Emit(Encode::Ands(dst, src, imm)); }

    void Ands(WzrReg dst, WReg src, uint32_t imm) { Emit(Encode::Ands(dst, src, imm)); }

    // AND/ORR/EOR/ANDS (shifted register): the register field of each operand
    // accepts the zero register, so the three operands are templates constrained
    // to one operand width.
    template <typename D, typename N, typename M, Detail::EnableXOperands<D, N, M> = 0>
    void And(D dst, N src, M src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
    {
        Emit(Encode::And(dst, src, src2, shift, amount));
    }

    template <typename D, typename N, typename M, Detail::EnableWOperands<D, N, M> = 0>
    void And(D dst, N src, M src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
    {
        Emit(Encode::And(dst, src, src2, shift, amount));
    }

    template <typename D, typename N, typename M, Detail::EnableXOperands<D, N, M> = 0>
    void Orr(D dst, N src, M src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
    {
        Emit(Encode::Orr(dst, src, src2, shift, amount));
    }

    template <typename D, typename N, typename M, Detail::EnableWOperands<D, N, M> = 0>
    void Orr(D dst, N src, M src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
    {
        Emit(Encode::Orr(dst, src, src2, shift, amount));
    }

    template <typename D, typename N, typename M, Detail::EnableXOperands<D, N, M> = 0>
    void Eor(D dst, N src, M src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
    {
        Emit(Encode::Eor(dst, src, src2, shift, amount));
    }

    template <typename D, typename N, typename M, Detail::EnableWOperands<D, N, M> = 0>
    void Eor(D dst, N src, M src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
    {
        Emit(Encode::Eor(dst, src, src2, shift, amount));
    }

    template <typename D, typename N, typename M, Detail::EnableXOperands<D, N, M> = 0>
    void Ands(D dst, N src, M src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
    {
        Emit(Encode::Ands(dst, src, src2, shift, amount));
    }

    template <typename D, typename N, typename M, Detail::EnableWOperands<D, N, M> = 0>
    void Ands(D dst, N src, M src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
    {
        Emit(Encode::Ands(dst, src, src2, shift, amount));
    }

    // TST: ANDS with the zero-register destination; sets the flags only.
    void Tst(XReg src, uint64_t imm) { Emit(Encode::Ands(Xzr, src, imm)); }

    void Tst(XReg src, XReg src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
    {
        Emit(Encode::Ands(Xzr, src, src2, shift, amount));
    }

    void Tst(WReg src, uint32_t imm) { Emit(Encode::Ands(Wzr, src, imm)); }

    void Tst(WReg src, WReg src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
    {
        Emit(Encode::Ands(Wzr, src, src2, shift, amount));
    }

    // LSL/LSR/ASR (register): dst = src shifted by the low bits of `amount`.
    // Every register field accepts the zero register, so the operands are
    // templates constrained to one operand width. The hardware takes the amount
    // modulo the width, so any value is encodable and none is rejected.
    template <typename D, typename N, typename M, Detail::EnableXOperands<D, N, M> = 0> void Lsl(D dst, N src, M amount)
    {
        Emit(Encode::Lsl(dst, src, amount));
    }

    template <typename D, typename N, typename M, Detail::EnableWOperands<D, N, M> = 0> void Lsl(D dst, N src, M amount)
    {
        Emit(Encode::Lsl(dst, src, amount));
    }

    template <typename D, typename N, typename M, Detail::EnableXOperands<D, N, M> = 0> void Lsr(D dst, N src, M amount)
    {
        Emit(Encode::Lsr(dst, src, amount));
    }

    template <typename D, typename N, typename M, Detail::EnableWOperands<D, N, M> = 0> void Lsr(D dst, N src, M amount)
    {
        Emit(Encode::Lsr(dst, src, amount));
    }

    template <typename D, typename N, typename M, Detail::EnableXOperands<D, N, M> = 0> void Asr(D dst, N src, M amount)
    {
        Emit(Encode::Asr(dst, src, amount));
    }

    template <typename D, typename N, typename M, Detail::EnableWOperands<D, N, M> = 0> void Asr(D dst, N src, M amount)
    {
        Emit(Encode::Asr(dst, src, amount));
    }

    // LSL/LSR/ASR (immediate): the shift must be below the operand width, which
    // the encoder asserts. Same register rules as the register form.
    template <typename D, typename N, Detail::EnableXPair<D, N> = 0> void Lsl(D dst, N src, uint32_t shift)
    {
        Emit(Encode::Lsl(dst, src, shift));
    }

    template <typename D, typename N, Detail::EnableWPair<D, N> = 0> void Lsl(D dst, N src, uint32_t shift)
    {
        Emit(Encode::Lsl(dst, src, shift));
    }

    template <typename D, typename N, Detail::EnableXPair<D, N> = 0> void Lsr(D dst, N src, uint32_t shift)
    {
        Emit(Encode::Lsr(dst, src, shift));
    }

    template <typename D, typename N, Detail::EnableWPair<D, N> = 0> void Lsr(D dst, N src, uint32_t shift)
    {
        Emit(Encode::Lsr(dst, src, shift));
    }

    template <typename D, typename N, Detail::EnableXPair<D, N> = 0> void Asr(D dst, N src, uint32_t shift)
    {
        Emit(Encode::Asr(dst, src, shift));
    }

    template <typename D, typename N, Detail::EnableWPair<D, N> = 0> void Asr(D dst, N src, uint32_t shift)
    {
        Emit(Encode::Asr(dst, src, shift));
    }

    // SBFM/UBFM: rotate the source right by immr, then keep the low imms+1 bits
    // of the rotated value (sign- or zero-extending them to the destination
    // width). The aliases above are the readable spellings of the usual cases;
    // these are the escape hatch for the rest. immr and imms are below the
    // operand width, which the encoder asserts. BFM is not modeled.
    template <typename D, typename N, Detail::EnableXPair<D, N> = 0>
    void Sbfm(D dst, N src, uint32_t immr, uint32_t imms)
    {
        Emit(Encode::Sbfm(dst, src, immr, imms));
    }

    template <typename D, typename N, Detail::EnableWPair<D, N> = 0>
    void Sbfm(D dst, N src, uint32_t immr, uint32_t imms)
    {
        Emit(Encode::Sbfm(dst, src, immr, imms));
    }

    template <typename D, typename N, Detail::EnableXPair<D, N> = 0>
    void Ubfm(D dst, N src, uint32_t immr, uint32_t imms)
    {
        Emit(Encode::Ubfm(dst, src, immr, imms));
    }

    template <typename D, typename N, Detail::EnableWPair<D, N> = 0>
    void Ubfm(D dst, N src, uint32_t immr, uint32_t imms)
    {
        Emit(Encode::Ubfm(dst, src, immr, imms));
    }

    // SBFX/UBFX: extract `width` bits starting at `lsb`, sign- or zero-extending
    // the result to the destination width; the extraction must stay inside the
    // operand width, which the encoder asserts.
    template <typename D, typename N, Detail::EnableXPair<D, N> = 0>
    void Sbfx(D dst, N src, uint32_t lsb, uint32_t width)
    {
        Emit(Encode::Sbfx(dst, src, lsb, width));
    }

    template <typename D, typename N, Detail::EnableWPair<D, N> = 0>
    void Sbfx(D dst, N src, uint32_t lsb, uint32_t width)
    {
        Emit(Encode::Sbfx(dst, src, lsb, width));
    }

    template <typename D, typename N, Detail::EnableXPair<D, N> = 0>
    void Ubfx(D dst, N src, uint32_t lsb, uint32_t width)
    {
        Emit(Encode::Ubfx(dst, src, lsb, width));
    }

    template <typename D, typename N, Detail::EnableWPair<D, N> = 0>
    void Ubfx(D dst, N src, uint32_t lsb, uint32_t width)
    {
        Emit(Encode::Ubfx(dst, src, lsb, width));
    }

    // SXTB/SXTH/SXTW: sign-extend the low byte/halfword/word of a W source into
    // an X destination.
    template <typename D, typename N, Detail::EnableXFromW<D, N> = 0> void Sxtb(D dst, N src)
    {
        Emit(Encode::Sxtb(dst, src));
    }

    template <typename D, typename N, Detail::EnableXFromW<D, N> = 0> void Sxth(D dst, N src)
    {
        Emit(Encode::Sxth(dst, src));
    }

    template <typename D, typename N, Detail::EnableXFromW<D, N> = 0> void Sxtw(D dst, N src)
    {
        Emit(Encode::Sxtw(dst, src));
    }

    // UXTB/UXTH: zero-extend the low byte/halfword into a W destination.
    template <typename D, typename N, Detail::EnableWPair<D, N> = 0> void Uxtb(D dst, N src)
    {
        Emit(Encode::Uxtb(dst, src));
    }

    template <typename D, typename N, Detail::EnableWPair<D, N> = 0> void Uxth(D dst, N src)
    {
        Emit(Encode::Uxth(dst, src));
    }

    // MADD/MSUB: dst = src * src2 + the fourth register, or MSUB's
    // dst = the fourth register - src * src2. Every operand may be the zero
    // register, so they are templates constrained to one width. MUL is the alias
    // with a zero addend, and SDIV/UDIV divide: signed (truncated toward zero)
    // or unsigned. A division by zero yields zero rather than trapping, so a
    // caller that must reject it has to test the divisor.
    template <typename D, typename N, typename M, typename A, Detail::EnableXQuad<D, N, M, A> = 0>
    void Madd(D dst, N src, M src2, A addend)
    {
        Emit(Encode::Madd(dst, src, src2, addend));
    }

    template <typename D, typename N, typename M, typename A, Detail::EnableWQuad<D, N, M, A> = 0>
    void Madd(D dst, N src, M src2, A addend)
    {
        Emit(Encode::Madd(dst, src, src2, addend));
    }

    template <typename D, typename N, typename M, typename A, Detail::EnableXQuad<D, N, M, A> = 0>
    void Msub(D dst, N src, M src2, A minuend)
    {
        Emit(Encode::Msub(dst, src, src2, minuend));
    }

    template <typename D, typename N, typename M, typename A, Detail::EnableWQuad<D, N, M, A> = 0>
    void Msub(D dst, N src, M src2, A minuend)
    {
        Emit(Encode::Msub(dst, src, src2, minuend));
    }

    template <typename D, typename N, typename M, Detail::EnableXOperands<D, N, M> = 0> void Mul(D dst, N src, M src2)
    {
        Emit(Encode::Mul(dst, src, src2));
    }

    template <typename D, typename N, typename M, Detail::EnableWOperands<D, N, M> = 0> void Mul(D dst, N src, M src2)
    {
        Emit(Encode::Mul(dst, src, src2));
    }

    template <typename D, typename N, typename M, Detail::EnableXOperands<D, N, M> = 0> void Sdiv(D dst, N src, M src2)
    {
        Emit(Encode::Sdiv(dst, src, src2));
    }

    template <typename D, typename N, typename M, Detail::EnableWOperands<D, N, M> = 0> void Sdiv(D dst, N src, M src2)
    {
        Emit(Encode::Sdiv(dst, src, src2));
    }

    template <typename D, typename N, typename M, Detail::EnableXOperands<D, N, M> = 0> void Udiv(D dst, N src, M src2)
    {
        Emit(Encode::Udiv(dst, src, src2));
    }

    template <typename D, typename N, typename M, Detail::EnableWOperands<D, N, M> = 0> void Udiv(D dst, N src, M src2)
    {
        Emit(Encode::Udiv(dst, src, src2));
    }

    // MOVZ/MOVN/MOVK: one 16-bit immediate into one halfword slot, with the
    // other halfwords zeroed, one, or unchanged, respectively. `shift` names the
    // slot: 0, 16, 32 or 48 for the 64-bit form and 0 or 16 for the 32-bit one.
    // None of the three touches the flags, and the destination may be the zero
    // register, whose write is discarded.
    template <typename D, Detail::EnableXReg<D> = 0> void Movz(D dst, uint32_t imm16, uint32_t shift = 0)
    {
        Emit(Encode::Movz(dst, imm16, shift));
    }

    template <typename D, Detail::EnableWReg<D> = 0> void Movz(D dst, uint32_t imm16, uint32_t shift = 0)
    {
        Emit(Encode::Movz(dst, imm16, shift));
    }

    template <typename D, Detail::EnableXReg<D> = 0> void Movn(D dst, uint32_t imm16, uint32_t shift = 0)
    {
        Emit(Encode::Movn(dst, imm16, shift));
    }

    template <typename D, Detail::EnableWReg<D> = 0> void Movn(D dst, uint32_t imm16, uint32_t shift = 0)
    {
        Emit(Encode::Movn(dst, imm16, shift));
    }

    template <typename D, Detail::EnableXReg<D> = 0> void Movk(D dst, uint32_t imm16, uint32_t shift = 0)
    {
        Emit(Encode::Movk(dst, imm16, shift));
    }

    template <typename D, Detail::EnableWReg<D> = 0> void Movk(D dst, uint32_t imm16, uint32_t shift = 0)
    {
        Emit(Encode::Movk(dst, imm16, shift));
    }

    // MOV (register): dst = src. Between two general registers this is ORR with
    // a zero source; with SP on either side it is ADD of zero, because ORR
    // cannot read SP and ADD's operand position that ORR uses for the zero
    // register is the one SP is read from. Neither form touches the flags.
    void Mov(XReg dst, XReg src) { Emit(Encode::Orr(dst, Xzr, src)); }

    void Mov(WReg dst, WReg src) { Emit(Encode::Orr(dst, Wzr, src)); }

    void Mov(XReg dst, SpReg src) { Emit(Encode::Add(dst, src, 0)); }

    void Mov(SpReg dst, XReg src) { Emit(Encode::Add(dst, src, 0)); }

    // MOV (immediate): dst = imm, as the fewest move-wide instructions that
    // produce it — one to four words, and one or two for the 32-bit form, which
    // zero-extends into the upper half of the destination register.
    void Mov(XReg dst, uint64_t imm) { EmitMoveWide(dst, imm, 4); }

    void Mov(WReg dst, uint32_t imm) { EmitMoveWide(dst, imm, 2); }

    // Loads and stores. The address operand is Mem(base, #offset) for the
    // immediate forms and Mem(base, index[, extend][, Scaled]) for the
    // register-offset ones. The access size and the signedness come from the
    // mnemonic and the destination register: Ldr moves 64 bits into an X
    // register and 32 into a W one, Ldrb/Ldrh zero-extend into a W register,
    // Ldrsb/Ldrsh/Ldrsw sign-extend, and the Ldur family is the same access with
    // an offset that is not scaled by the access size. A loading pair reads both
    // registers from one address, so they must differ.
    template <typename Rt, Detail::EnableXReg<Rt> = 0> void Ldr(Rt rt, const MemImm& mem)
    {
        Emit(Encode::Ldr(rt, mem));
    }

    template <typename Rt, Detail::EnableWReg<Rt> = 0> void Ldr(Rt rt, const MemImm& mem)
    {
        Emit(Encode::Ldr(rt, mem));
    }

    template <typename Rt, Detail::EnableXReg<Rt> = 0> void Ldr(Rt rt, const MemIndex& mem)
    {
        Emit(Encode::Ldr(rt, mem));
    }

    template <typename Rt, Detail::EnableWReg<Rt> = 0> void Ldr(Rt rt, const MemIndex& mem)
    {
        Emit(Encode::Ldr(rt, mem));
    }

    template <typename Rt, Detail::EnableXReg<Rt> = 0> void Str(Rt rt, const MemImm& mem)
    {
        Emit(Encode::Str(rt, mem));
    }

    template <typename Rt, Detail::EnableWReg<Rt> = 0> void Str(Rt rt, const MemImm& mem)
    {
        Emit(Encode::Str(rt, mem));
    }

    template <typename Rt, Detail::EnableXReg<Rt> = 0> void Str(Rt rt, const MemIndex& mem)
    {
        Emit(Encode::Str(rt, mem));
    }

    template <typename Rt, Detail::EnableWReg<Rt> = 0> void Str(Rt rt, const MemIndex& mem)
    {
        Emit(Encode::Str(rt, mem));
    }

    template <typename Rt, Detail::EnableWReg<Rt> = 0> void Ldrb(Rt rt, const MemImm& mem)
    {
        Emit(Encode::Ldrb(rt, mem));
    }

    template <typename Rt, Detail::EnableWReg<Rt> = 0> void Ldrb(Rt rt, const MemIndex& mem)
    {
        Emit(Encode::Ldrb(rt, mem));
    }

    template <typename Rt, Detail::EnableWReg<Rt> = 0> void Strb(Rt rt, const MemImm& mem)
    {
        Emit(Encode::Strb(rt, mem));
    }

    template <typename Rt, Detail::EnableWReg<Rt> = 0> void Strb(Rt rt, const MemIndex& mem)
    {
        Emit(Encode::Strb(rt, mem));
    }

    template <typename Rt, Detail::EnableWReg<Rt> = 0> void Ldrh(Rt rt, const MemImm& mem)
    {
        Emit(Encode::Ldrh(rt, mem));
    }

    template <typename Rt, Detail::EnableWReg<Rt> = 0> void Ldrh(Rt rt, const MemIndex& mem)
    {
        Emit(Encode::Ldrh(rt, mem));
    }

    template <typename Rt, Detail::EnableWReg<Rt> = 0> void Strh(Rt rt, const MemImm& mem)
    {
        Emit(Encode::Strh(rt, mem));
    }

    template <typename Rt, Detail::EnableWReg<Rt> = 0> void Strh(Rt rt, const MemIndex& mem)
    {
        Emit(Encode::Strh(rt, mem));
    }

    template <typename Rt, Detail::EnableXReg<Rt> = 0> void Ldrsb(Rt rt, const MemImm& mem)
    {
        Emit(Encode::Ldrsb(rt, mem));
    }

    template <typename Rt, Detail::EnableWReg<Rt> = 0> void Ldrsb(Rt rt, const MemImm& mem)
    {
        Emit(Encode::Ldrsb(rt, mem));
    }

    template <typename Rt, Detail::EnableXReg<Rt> = 0> void Ldrsb(Rt rt, const MemIndex& mem)
    {
        Emit(Encode::Ldrsb(rt, mem));
    }

    template <typename Rt, Detail::EnableWReg<Rt> = 0> void Ldrsb(Rt rt, const MemIndex& mem)
    {
        Emit(Encode::Ldrsb(rt, mem));
    }

    template <typename Rt, Detail::EnableXReg<Rt> = 0> void Ldrsh(Rt rt, const MemImm& mem)
    {
        Emit(Encode::Ldrsh(rt, mem));
    }

    template <typename Rt, Detail::EnableWReg<Rt> = 0> void Ldrsh(Rt rt, const MemImm& mem)
    {
        Emit(Encode::Ldrsh(rt, mem));
    }

    template <typename Rt, Detail::EnableXReg<Rt> = 0> void Ldrsh(Rt rt, const MemIndex& mem)
    {
        Emit(Encode::Ldrsh(rt, mem));
    }

    template <typename Rt, Detail::EnableWReg<Rt> = 0> void Ldrsh(Rt rt, const MemIndex& mem)
    {
        Emit(Encode::Ldrsh(rt, mem));
    }

    template <typename Rt, Detail::EnableXReg<Rt> = 0> void Ldrsw(Rt rt, const MemImm& mem)
    {
        Emit(Encode::Ldrsw(rt, mem));
    }

    template <typename Rt, Detail::EnableXReg<Rt> = 0> void Ldrsw(Rt rt, const MemIndex& mem)
    {
        Emit(Encode::Ldrsw(rt, mem));
    }

    template <typename Rt, Detail::EnableXReg<Rt> = 0> void Ldur(Rt rt, const MemImm& mem)
    {
        Emit(Encode::Ldur(rt, mem));
    }

    template <typename Rt, Detail::EnableWReg<Rt> = 0> void Ldur(Rt rt, const MemImm& mem)
    {
        Emit(Encode::Ldur(rt, mem));
    }

    template <typename Rt, Detail::EnableXReg<Rt> = 0> void Stur(Rt rt, const MemImm& mem)
    {
        Emit(Encode::Stur(rt, mem));
    }

    template <typename Rt, Detail::EnableWReg<Rt> = 0> void Stur(Rt rt, const MemImm& mem)
    {
        Emit(Encode::Stur(rt, mem));
    }

    template <typename Rt, Detail::EnableWReg<Rt> = 0> void Ldurb(Rt rt, const MemImm& mem)
    {
        Emit(Encode::Ldurb(rt, mem));
    }

    template <typename Rt, Detail::EnableWReg<Rt> = 0> void Sturb(Rt rt, const MemImm& mem)
    {
        Emit(Encode::Sturb(rt, mem));
    }

    template <typename Rt, Detail::EnableWReg<Rt> = 0> void Ldurh(Rt rt, const MemImm& mem)
    {
        Emit(Encode::Ldurh(rt, mem));
    }

    template <typename Rt, Detail::EnableWReg<Rt> = 0> void Sturh(Rt rt, const MemImm& mem)
    {
        Emit(Encode::Sturh(rt, mem));
    }

    template <typename Rt, Detail::EnableXReg<Rt> = 0> void Ldursb(Rt rt, const MemImm& mem)
    {
        Emit(Encode::Ldursb(rt, mem));
    }

    template <typename Rt, Detail::EnableWReg<Rt> = 0> void Ldursb(Rt rt, const MemImm& mem)
    {
        Emit(Encode::Ldursb(rt, mem));
    }

    template <typename Rt, Detail::EnableXReg<Rt> = 0> void Ldursh(Rt rt, const MemImm& mem)
    {
        Emit(Encode::Ldursh(rt, mem));
    }

    template <typename Rt, Detail::EnableWReg<Rt> = 0> void Ldursh(Rt rt, const MemImm& mem)
    {
        Emit(Encode::Ldursh(rt, mem));
    }

    template <typename Rt, Detail::EnableXReg<Rt> = 0> void Ldursw(Rt rt, const MemImm& mem)
    {
        Emit(Encode::Ldursw(rt, mem));
    }

    template <typename Rt, typename Rt2, Detail::EnableXReg<Rt> = 0, Detail::EnableXReg<Rt2> = 0>
    void Ldp(Rt rt, Rt2 rt2, const MemImm& mem)
    {
        Emit(Encode::Ldp(rt, rt2, mem));
    }

    template <typename Rt, typename Rt2, Detail::EnableWReg<Rt> = 0, Detail::EnableWReg<Rt2> = 0>
    void Ldp(Rt rt, Rt2 rt2, const MemImm& mem)
    {
        Emit(Encode::Ldp(rt, rt2, mem));
    }

    template <typename Rt, typename Rt2, Detail::EnableXReg<Rt> = 0, Detail::EnableXReg<Rt2> = 0>
    void Stp(Rt rt, Rt2 rt2, const MemImm& mem)
    {
        Emit(Encode::Stp(rt, rt2, mem));
    }

    template <typename Rt, typename Rt2, Detail::EnableWReg<Rt> = 0, Detail::EnableWReg<Rt2> = 0>
    void Stp(Rt rt, Rt2 rt2, const MemImm& mem)
    {
        Emit(Encode::Stp(rt, rt2, mem));
    }

    template <typename Rt, typename Rt2, Detail::EnableXReg<Rt> = 0, Detail::EnableXReg<Rt2> = 0>
    void Ldpsw(Rt rt, Rt2 rt2, const MemImm& mem)
    {
        Emit(Encode::Ldpsw(rt, rt2, mem));
    }

    // NOP: does nothing.
    void Nop() { Emit(Encode::Nop()); }

    // Control flow. Methods that name a label record a fixup: the word is
    // emitted with an empty offset, and Finalize() fills in the distance once
    // every label has a position.
    //
    // B: branch to target, unconditionally. B(cc): branch if cc holds. Bl:
    // branch and leave the address of the next instruction in X30. Cbz/Cbnz:
    // branch if Rt is zero / not zero. Tbz/Tbnz: branch if bit `bit` of Rt is
    // zero / one. Adr: dst = address of target. Br: branch to the address in a
    // register, without touching X30. Blr: Br that leaves the return address in
    // X30. Ret: Br with the register defaulting to X30 — the usual return.
    void B(Label target) { EmitFixup(Encode::B(), FixupKind::Branch26, target); }

    void Bl(Label target) { EmitFixup(Encode::Bl(), FixupKind::Branch26, target); }

    void B(Cc cc, Label target) { EmitFixup(Encode::B(cc), FixupKind::CondBranch19, target); }

    void Cbz(XReg rt, Label target) { EmitFixup(Encode::Cbz(rt), FixupKind::CmpBranch19, target); }

    void Cbz(WReg rt, Label target) { EmitFixup(Encode::Cbz(rt), FixupKind::CmpBranch19, target); }

    void Cbnz(XReg rt, Label target) { EmitFixup(Encode::Cbnz(rt), FixupKind::CmpBranch19, target); }

    void Cbnz(WReg rt, Label target) { EmitFixup(Encode::Cbnz(rt), FixupKind::CmpBranch19, target); }

    void Tbz(XReg rt, uint32_t bit, Label target) { EmitFixup(Encode::Tbz(rt, bit), FixupKind::TestBranch14, target); }

    void Tbz(WReg rt, uint32_t bit, Label target) { EmitFixup(Encode::Tbz(rt, bit), FixupKind::TestBranch14, target); }

    void Tbnz(XReg rt, uint32_t bit, Label target)
    {
        EmitFixup(Encode::Tbnz(rt, bit), FixupKind::TestBranch14, target);
    }

    void Tbnz(WReg rt, uint32_t bit, Label target)
    {
        EmitFixup(Encode::Tbnz(rt, bit), FixupKind::TestBranch14, target);
    }

    void Adr(XReg dst, Label target) { EmitFixup(Encode::Adr(dst), FixupKind::Adr21, target); }

    void Br(XReg target) { Emit(Encode::Br(target)); }

    void Blr(XReg target) { Emit(Encode::Blr(target)); }

    void Ret(XReg target = X30) { Emit(Encode::Ret(target)); }

    // Issues a label through the owned manager.
    Label NewLabel() { return labels_->NewLabel(); }

    // Binds a label to the position the storage will be written at next.
    void Bind(Label label) { labels_->Bind(label, storage_.Position()); }

    // Bytes emitted so far.
    uint32_t Position() const { return storage_.Position(); }

    // The written words, for inspection; the view does not survive finalization.
    Utils::Span<const uint32_t> Words() const { return storage_.Words(); }

    // Resolves the branches recorded against labels, then publishes the words as
    // an executable segment and spends the storage: the segment owns the code,
    // and the assembler may not take more words afterwards. A branch to an
    // unbound label is a fatal error.
    auto Finalize()
    {
        Resolve();
        return storage_.Finalize();
    }

private:
    // Writes one encoded word. Encoders are the usual source of words.
    void Emit(Insn insn) { storage_.Write(insn); }

    // The 16-bit halfword of `imm` that halfword slot `slot` holds.
    static constexpr uint32_t Halfword(uint64_t imm, uint32_t slot)
    {
        return static_cast<uint32_t>((imm >> (16 * slot)) & 0xFFFF);
    }

    // The MOV immediate pseudo. Only MOVZ/MOVN clear or fill a slot they do not
    // name (zeroing the others, or complementing them to ones), so the first
    // instruction has to seed the register and the slots the seed got wrong are
    // then patched with MOVK:
    //
    //   MOVZ #imm16, lsl #16k   slot k = imm16, the rest 0
    //   MOVN #imm16, lsl #16k   slot k = ~imm16, the rest 0xFFFF
    //   MOVK #imm16, lsl #16k   slot k = imm16, the rest untouched
    //
    // A MOVZ seed therefore needs a MOVK for every nonzero halfword, a MOVN seed
    // one for every halfword that is not all ones; the seed is counted in its own
    // total rather than added to it. The cheaper seed is used, a tie going to
    // MOVZ because a seed of zeros does not rely on the remaining slots being
    // overwritten. Zero words of that total means the seed alone is the value:
    // `imm` is zero or all ones, and the seed is the immediate zero. Slots are
    // filled from the low half up.
    template <typename D> void EmitMoveWide(D dst, uint64_t imm, uint32_t halves)
    {
        unsigned zeros = 0;
        unsigned ones  = 0;
        for (uint32_t slot = 0; slot < halves; ++slot) {
            const uint32_t half  = Halfword(imm, slot);
            zeros               += half == 0 ? 1u : 0u;
            ones                += half == 0xFFFF ? 1u : 0u;
        }
        const unsigned movzCount = halves - zeros == 0 ? 1 : halves - zeros;
        const unsigned movnCount = halves - ones == 0 ? 1 : halves - ones;
        const bool useMovn       = movnCount < movzCount;

        // The slots the seed itself gets right are the ones it already produced,
        // so they take no MOVK and the first slot to emit seeds the register.
        bool seeded = false;
        for (uint32_t slot = 0; slot < halves; ++slot) {
            const uint32_t half = Halfword(imm, slot);
            if (useMovn ? half == 0xFFFF : half == 0) {
                continue;
            }
            const uint32_t imm16 = useMovn ? (~half & 0xFFFF) : half;
            const uint32_t shift = 16 * slot;
            if (!seeded) {
                if (useMovn) {
                    Emit(Encode::Movn(dst, imm16, shift));
                } else {
                    Emit(Encode::Movz(dst, imm16, shift));
                }
                seeded = true;
                continue;
            }
            Emit(Encode::Movk(dst, imm16, shift));
        }
        if (!seeded) {
            if (useMovn) {
                Emit(Encode::Movn(dst, 0, 0));
            } else {
                Emit(Encode::Movz(dst, 0, 0));
            }
        }
    }

    // Records the site a branch word is about to be written at, then writes it.
    void EmitFixup(Insn word, FixupKind kind, Label target)
    {
        labels_->AddFixup(kind, storage_.Position(), word, target);
        Emit(word);
    }

    // Patches every recorded branch word with the distance to its label.
    void Resolve()
    {
        for (const Fixup& fixup : labels_->Fixups()) {
            uint32_t position = 0;
            if (!labels_->TryPosition(fixup.label, position)) {
                FATAL("Branch to an unbound label");
            }
            const int64_t delta = static_cast<int64_t>(position) - static_cast<int64_t>(fixup.site);
            storage_.Patch(fixup.site, Encode::ApplyFixup(fixup.word, fixup.kind, delta));
        }
    }

    Storage storage_;
    std::unique_ptr<LabelManager> labels_ { std::make_unique<BasicLabelManager>() };
};

} // namespace Asm
