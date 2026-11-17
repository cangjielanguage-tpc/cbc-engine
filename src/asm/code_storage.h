#pragma once

#include <cstdint>

#include "asm/code_segment.h"
#include "asm/insn.h"
#include "utils/assertion.h"
#include "utils/span.h"
#include "utils/vector.h"

namespace Asm {

// Instruction storage, duck-typed: the assembler and the fixup resolver call
// these members directly, so a storage is a complete type whose layout is known
// to its users, not a base class.
//
//   void        Write(Insn insn);                  // append one instruction word
//   uint32_t    Position() const;                  // current byte offset
//   void        Patch(uint32_t offset, Insn insn); // overwrite an earlier word
//   CodeSegment Finalize();                        // publish the written words
//
// Offsets are byte offsets from the start of the stream. Instruction words are
// kept in target byte order; every supported host is little-endian, as is
// aarch64, so a word is stored as encoded.
//
// Assembling needs Write and Position, and fixup resolution needs Patch; the
// assembler never publishes, so the words become code only when their owner asks
// the storage for a segment.

// The default storage: words accumulate in a growable buffer, and Finalize()
// publishes all of them at once as an executable segment.
class CodeStorage {
public:
    static constexpr uint32_t WordSize = sizeof(uint32_t);

    CodeStorage()                              = default;
    CodeStorage(CodeStorage&&)                 = default;
    CodeStorage& operator=(CodeStorage&&)      = default;
    CodeStorage(const CodeStorage&)            = delete;
    CodeStorage& operator=(const CodeStorage&) = delete;

    void Write(Insn insn)
    {
        ASSERT(!finalized_);
        words_.PushBack(insn.Bits());
    }

    uint32_t Position() const { return static_cast<uint32_t>(words_.Size() * WordSize); }

    void Patch(uint32_t offset, Insn insn)
    {
        ASSERT(!finalized_);
        ASSERT(offset % WordSize == 0);
        ASSERT(offset < Position());
        words_[offset / WordSize] = insn.Bits();
    }

    // The written words, for inspection. The view is into the storage's own
    // buffer, so it does not survive finalization.
    Utils::Span<const uint32_t> Words() const { return words_; }

    // Publishes the words and spends the storage: the segment owns the code, and
    // a finalized storage is never written or patched again. A storage with no
    // words finalizes to a segment holding nothing; when the mapping cannot be
    // created, the storage keeps its words and may be finalized again.
    CodeSegment Finalize()
    {
        ASSERT(!finalized_);
        CodeSegment segment = CodeSegment::Publish(words_);
        if (segment.Ok()) {
            finalized_ = true;
            words_     = {};
        }
        return segment;
    }

private:
    Utils::Vector<uint32_t> words_;
    bool finalized_ = false;
};

} // namespace Asm
