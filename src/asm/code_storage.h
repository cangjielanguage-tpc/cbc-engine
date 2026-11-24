#pragma once

#include <cstdint>
#include <cstring>

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
//   void        Write(Insn insn);                  // append one instruction
//   uint32_t    Position() const;                  // current byte offset
//   void        Patch(uint32_t offset, Insn insn); // overwrite an earlier instruction
//   CodeSegment Finalize();                        // publish the written bytes
//
// Offsets are byte offsets from the start of the stream. Instructions are held
// as their target bytes (little-endian, the order of every supported host and of
// aarch64), so a word is stored as encoded.
//
// Assembling needs Write and Position, and fixup resolution needs Patch; the
// assembler never publishes, so the bytes become code only when their owner asks
// the storage for a segment.

// The default storage: instruction bytes accumulate in a growable buffer, and
// Finalize() publishes all of them at once as an executable segment.
class CodeStorage {
public:
    CodeStorage()                              = default;
    CodeStorage(CodeStorage&&)                 = default;
    CodeStorage& operator=(CodeStorage&&)      = default;
    CodeStorage(const CodeStorage&)            = delete;
    CodeStorage& operator=(const CodeStorage&) = delete;

    void Write(Insn insn)
    {
        ASSERT(!finalized_);
        // Appending through PushBack grows the buffer geometrically; the
        // vector's Resize grows to the exact requested size, which would realloc
        // once per instruction.
        for (uint8_t byte : insn.Bytes()) {
            bytes_.PushBack(byte);
        }
    }

    uint32_t Position() const { return static_cast<uint32_t>(bytes_.Size()); }

    void Patch(uint32_t offset, Insn insn)
    {
        ASSERT(!finalized_);
        ASSERT(offset % Insn::Size == 0);
        ASSERT(offset + Insn::Size <= bytes_.Size());
        std::memcpy(bytes_.Data() + offset, insn.Bytes().data(), Insn::Size);
    }

    // The written bytes, for inspection. The view is into the storage's own
    // buffer, so it does not survive finalization.
    Utils::Span<const uint8_t> Bytes() const { return bytes_; }

    // Publishes the bytes and spends the storage: the segment owns the code, and
    // a finalized storage is never written or patched again. A storage with no
    // bytes finalizes to a segment holding nothing; when the mapping cannot be
    // created, the storage keeps its bytes and may be finalized again.
    CodeSegment Finalize()
    {
        ASSERT(!finalized_);
        CodeSegment segment = CodeSegment::Publish(bytes_);
        if (segment.Ok()) {
            finalized_ = true;
            bytes_     = {};
        }
        return segment;
    }

private:
    Utils::Vector<uint8_t> bytes_;
    bool finalized_ = false;
};

} // namespace Asm
