#pragma once

#include <cstddef>
#include <cstdint>

#include "asm/insn.h"
#include "utils/span.h"

// Emitted code is inspected through its bytes. These read the instruction words
// back for the assertions that reason about the encoding: the stream is a
// sequence of little-endian words, one Insn::Size each.
inline uint32_t WordAt(Utils::Span<const uint8_t> bytes, size_t index)
{
    const uint8_t* word = bytes.Data() + index * Asm::Insn::Size;
    return static_cast<uint32_t>(word[0]) | (static_cast<uint32_t>(word[1]) << 8) |
           (static_cast<uint32_t>(word[2]) << 16) | (static_cast<uint32_t>(word[3]) << 24);
}

inline size_t WordCount(Utils::Span<const uint8_t> bytes) { return bytes.Size() / Asm::Insn::Size; }
