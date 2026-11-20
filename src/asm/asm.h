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
