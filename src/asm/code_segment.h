#pragma once

#include <sys/mman.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "utils/span.h"

namespace Asm {

// A block of published code: a private anonymous mapping that holds a copy of
// the emitted words and is readable and executable, never writable. The mapping
// belongs to this object alone and is released with it.
//
// Publication is one-way: the words are copied into the mapping once and the
// mapping is never written to again, so code that is running is never rewritten.
class CodeSegment {
public:
    static constexpr size_t WordSize = sizeof(uint32_t);

    CodeSegment() = default;

    ~CodeSegment() { Release(); }

    // The mapping follows the move; the moved-from segment holds nothing.
    CodeSegment(CodeSegment&& other) noexcept
        : code_ { other.code_ }, size_ { other.size_ }, mappingSize_ { other.mappingSize_ }
    {
        other.Reset();
    }

    CodeSegment& operator=(CodeSegment&& other) noexcept
    {
        if (this == &other) {
            return *this;
        }
        Release();
        code_        = other.code_;
        size_        = other.size_;
        mappingSize_ = other.mappingSize_;
        other.Reset();
        return *this;
    }

    CodeSegment(const CodeSegment&)            = delete;
    CodeSegment& operator=(const CodeSegment&) = delete;

    // Copies the words into a fresh mapping and makes it readable and
    // executable. Answers a segment holding nothing when there are no words, or
    // when the mapping could not be created.
    static CodeSegment Publish(Utils::Span<const uint32_t> words)
    {
        if (words.Empty()) {
            return {};
        }
        const size_t bytes = words.Size() * WordSize;
        void* mapping      = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapping == MAP_FAILED) {
            return {};
        }
        std::memcpy(mapping, words.Data(), bytes);
        if (mprotect(mapping, bytes, PROT_READ | PROT_EXEC) != 0) {
            munmap(mapping, bytes);
            return {};
        }
        // The words went in through the data cache, so the instruction cache may
        // still hold a stale view of the addresses the mapping occupies.
        __builtin___clear_cache(static_cast<char*>(mapping), static_cast<char*>(mapping) + bytes);
        return CodeSegment { mapping, bytes, bytes };
    }

    // The start of the mapping: the entry point of the code, to be called as a
    // function. Null when the segment holds nothing.
    void* Code() const { return code_; }

    // Bytes of code, without the padding to the mapping's page granularity.
    size_t Size() const { return size_; }

    // Whether the segment holds published code.
    bool Ok() const { return code_ != nullptr; }

    // The published words, as they lie in the mapping.
    Utils::Span<const uint32_t> Words() const
    {
        return { static_cast<const uint32_t*>(code_), size_ / WordSize };
    }

private:
    CodeSegment(void* code, size_t size, size_t mappingSize) : code_ { code }, size_ { size }, mappingSize_ { mappingSize }
    {}

    void Release()
    {
        if (code_ != nullptr) {
            munmap(code_, mappingSize_);
            Reset();
        }
    }

    void Reset()
    {
        code_        = nullptr;
        size_        = 0;
        mappingSize_ = 0;
    }

    void* code_ = nullptr;   // the mapping, null while the segment holds nothing
    size_t size_ = 0;        // bytes of code
    size_t mappingSize_ = 0; // bytes mapped, as passed to munmap
};

} // namespace Asm
