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

    // add/sub, immediate forms.
    void Add(XReg dst, XReg src, uint32_t imm12) { Emit(Encode::Add(dst, src, imm12)); }

    void Add(XReg dst, XReg src, uint32_t imm12, Lsl12Tag tag) { Emit(Encode::Add(dst, src, imm12, tag)); }

    void Add(XReg dst, SpReg src, uint32_t imm12) { Emit(Encode::Add(dst, src, imm12)); }

    void Add(XReg dst, SpReg src, uint32_t imm12, Lsl12Tag tag) { Emit(Encode::Add(dst, src, imm12, tag)); }

    void Add(SpReg dst, XReg src, uint32_t imm12) { Emit(Encode::Add(dst, src, imm12)); }

    void Add(SpReg dst, SpReg src, uint32_t imm12) { Emit(Encode::Add(dst, src, imm12)); }

    void Add(WReg dst, WReg src, uint32_t imm12) { Emit(Encode::Add(dst, src, imm12)); }

    void Add(WReg dst, WReg src, uint32_t imm12, Lsl12Tag tag) { Emit(Encode::Add(dst, src, imm12, tag)); }

    // add/sub, shifted-register forms.
    void Add(XReg dst, XReg src, XReg src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
    {
        Emit(Encode::Add(dst, src, src2, shift, amount));
    }

    void Add(WReg dst, WReg src, WReg src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
    {
        Emit(Encode::Add(dst, src, src2, shift, amount));
    }

    void Sub(XReg dst, XReg src, uint32_t imm12) { Emit(Encode::Sub(dst, src, imm12)); }

    void Sub(XReg dst, XReg src, uint32_t imm12, Lsl12Tag tag) { Emit(Encode::Sub(dst, src, imm12, tag)); }

    void Sub(XReg dst, SpReg src, uint32_t imm12) { Emit(Encode::Sub(dst, src, imm12)); }

    void Sub(XReg dst, SpReg src, uint32_t imm12, Lsl12Tag tag) { Emit(Encode::Sub(dst, src, imm12, tag)); }

    void Sub(SpReg dst, XReg src, uint32_t imm12) { Emit(Encode::Sub(dst, src, imm12)); }

    void Sub(SpReg dst, SpReg src, uint32_t imm12) { Emit(Encode::Sub(dst, src, imm12)); }

    void Sub(WReg dst, WReg src, uint32_t imm12) { Emit(Encode::Sub(dst, src, imm12)); }

    void Sub(WReg dst, WReg src, uint32_t imm12, Lsl12Tag tag) { Emit(Encode::Sub(dst, src, imm12, tag)); }

    void Sub(XReg dst, XReg src, XReg src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
    {
        Emit(Encode::Sub(dst, src, src2, shift, amount));
    }

    void Sub(WReg dst, WReg src, WReg src2, Shift shift = Shift::Lsl, uint32_t amount = 0)
    {
        Emit(Encode::Sub(dst, src, src2, shift, amount));
    }

    // Control flow. Methods that name a label record a fixup: the word is
    // emitted with an empty offset, and Finalize() fills in the distance once
    // every label has a position.
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
