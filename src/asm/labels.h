#pragma once

#include <cstdint>

#include "asm/insn.h"
#include "utils/assertion.h"
#include "utils/span.h"
#include "utils/vector.h"

namespace Asm {

// A label handle, issued by a LabelManager and unique within it. The handle
// carries no position of its own: a label takes on a position when the manager
// is told which position it marks, and any number of branch sites may refer to
// it.
struct Label {
    uint32_t index;
};

// The shape of a branch site, telling the resolver how to patch the word emitted
// there.
enum class FixupKind : uint8_t {
    Branch26,     // b, bl: signed offset in words, 26 bits
    CondBranch19, // b.cond: signed offset in words, 19 bits
    CmpBranch19,  // cbz, cbnz: signed offset in words, 19 bits
    TestBranch14, // tbz, tbnz: signed offset in words, 14 bits
    Adr21,        // adr: signed offset in bytes, 21 bits
};

// A branch site to patch once its target label is bound. The record keeps the
// word as emitted, so resolution never re-derives the base encoding, and storage
// needs no way to read a word back.
struct Fixup {
    FixupKind kind;
    uint32_t site; // byte offset of the word in the storage
    Insn word;     // the branch word as emitted, before resolution
    Label label;   // the label the word refers to
};

// Label bookkeeping for one assembler: issues handles, remembers the position
// each label was bound to, and collects branch fixups for the resolver. Labels
// are not part of the emission path, so an implementation may allocate freely
// and may store its state however suits it.
class LabelManager {
public:
    virtual ~LabelManager() = default;

    // Issues an unbound label handle. A handle stays valid for the life of the
    // manager that issued it.
    virtual Label NewLabel() = 0;

    // Sets the position the label marks. Binding one label twice is misuse.
    virtual void Bind(Label label, uint32_t position) = 0;

    // Records a branch site waiting for its target label to be bound.
    virtual void AddFixup(FixupKind kind, uint32_t site, Insn word, Label label) = 0;

    // Every recorded fixup, in the order the sites were emitted.
    virtual Utils::Span<const Fixup> Fixups() const = 0;

    // Writes the position of a bound label; answers false while the label is
    // unbound.
    virtual bool TryPosition(Label label, uint32_t& position) const = 0;

    // The number of labels issued so far.
    virtual uint32_t LabelCount() const = 0;
};

// A manager holding one position per label in a dense array, so handles are
// indices into it, and collecting fixups in a growable array of their own.
class BasicLabelManager : public LabelManager {
public:
    Label NewLabel() override
    {
        positions_.PushBack(Unbound);
        return Label { static_cast<uint32_t>(positions_.Size() - 1) };
    }

    void Bind(Label label, uint32_t position) override
    {
        ASSERT(label.index < positions_.Size());
        ASSERT(positions_[label.index] == Unbound);
        positions_[label.index] = position;
    }

    void AddFixup(FixupKind kind, uint32_t site, Insn word, Label label) override
    {
        ASSERT(label.index < positions_.Size());
        fixups_.PushBack(Fixup { kind, site, word, label });
    }

    Utils::Span<const Fixup> Fixups() const override { return fixups_; }

    bool TryPosition(Label label, uint32_t& position) const override
    {
        ASSERT(label.index < positions_.Size());
        if (positions_[label.index] == Unbound) {
            return false;
        }
        position = positions_[label.index];
        return true;
    }

    uint32_t LabelCount() const override { return static_cast<uint32_t>(positions_.Size()); }

private:
    // Positions are byte offsets into a storage, which cannot itself reach this
    // value, so it serves as the unbound mark.
    static constexpr uint32_t Unbound = UINT32_MAX;

    Utils::Vector<uint32_t> positions_;
    Utils::Vector<Fixup> fixups_;
};

} // namespace Asm
