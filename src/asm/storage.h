#pragma once

#include <sys/mman.h>

#include <cstdint>

#include "asm/insn.h"
#include "utils/assertion.h"
#include "utils/span.h"

namespace Asm {

// Instruction storage, duck-typed: the assembler and the fixup resolver call
// these members directly, so a storage is a complete type whose layout is known
// to its users, not a base class.
//
//   void     Write(Insn insn);                  // append one instruction word
//   uint32_t Position() const;                  // current byte offset
//   void     Patch(uint32_t offset, Insn insn); // overwrite an earlier word
//   void     Flush();                           // make code visible to the CPU
//
// Offsets are byte offsets from the start of the stream. Instruction words are
// kept in target byte order; every supported host is little-endian, as is
// aarch64, so a word is stored as encoded.
//
// Assembling needs Write and Position, fixup resolution needs Patch, and Flush
// belongs to whoever owns executable memory: the assembler never calls it.

// Storage over a private anonymous mapping that is readable, writable and
// executable, so the emitted words can be called as a function. The mapping
// belongs to this object alone and is released with it.
class ExecStorage {
public:
    static constexpr uint32_t WordSize = sizeof(uint32_t);

    explicit ExecStorage(uint32_t bytes) : words_ { static_cast<uint32_t*>(Map(bytes)) }, capacity_ { bytes } {}

    ~ExecStorage() { Release(); }

    // The mapping follows the move; the moved-from storage owns nothing.
    ExecStorage(ExecStorage&& other) noexcept
        : words_ { other.words_ },
          count_ { other.count_ },
          capacity_ { other.capacity_ }
    {
        other.words_    = nullptr;
        other.count_    = 0;
        other.capacity_ = 0;
    }

    ExecStorage& operator=(ExecStorage&& other) noexcept
    {
        if (this == &other) {
            return *this;
        }
        Release();
        words_          = other.words_;
        count_          = other.count_;
        capacity_       = other.capacity_;
        other.words_    = nullptr;
        other.count_    = 0;
        other.capacity_ = 0;
        return *this;
    }

    ExecStorage(const ExecStorage&)            = delete;
    ExecStorage& operator=(const ExecStorage&) = delete;

    void Write(Insn insn)
    {
        ASSERT(Position() < capacity_);
        words_[count_++] = insn.Bits();
    }

    uint32_t Position() const { return count_ * WordSize; }

    void Patch(uint32_t offset, Insn insn)
    {
        ASSERT(offset % WordSize == 0);
        ASSERT(offset < Position());
        words_[offset / WordSize] = insn.Bits();
    }

    // Publishes the written words to the instruction cache. On hosts whose
    // instruction and data caches are not coherent, code must be flushed before
    // it is called.
    void Flush() { __builtin___clear_cache(reinterpret_cast<char*>(words_), reinterpret_cast<char*>(words_ + count_)); }

    // The start of the mapping: the entry point of the emitted code.
    void* Code() { return words_; }

    const void* Code() const { return words_; }

    // The written words, for inspection.
    Utils::Span<const uint32_t> Words() const { return { words_, count_ }; }

private:
    static void* Map(uint32_t bytes)
    {
        ASSERT(bytes > 0);
        ASSERT(bytes % WordSize == 0);
        void* mapping = mmap(nullptr, bytes, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapping == MAP_FAILED) {
            FATAL("Failed to map %u bytes of executable memory", bytes);
        }
        return mapping;
    }

    void Release()
    {
        if (words_ != nullptr) {
            munmap(words_, capacity_);
        }
    }

    uint32_t* words_;
    uint32_t count_ = 0;
    uint32_t capacity_;
};

} // namespace Asm
