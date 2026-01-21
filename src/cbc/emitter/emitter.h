#ifndef CBC_EMITTER_EMITTER_H
#define CBC_EMITTER_EMITTER_H

#include <unordered_map>
#include <vector>
#include <functional>
#include <cstdint>
#include <memory_resource>

#include "utils/assertion.h"
#include "cbc/isa.h"

namespace Cbc {
namespace Emitter {

class CbcEmitter;
class Symbols;

enum class SymbolKind {
    LABEL,
    ADDRESS,
    VALUE,
};

struct Symbol {
    SymbolKind kind;
    uint32_t id;
};

struct Fixup {
    Symbol symbol;
    size_t position;
};

struct SegmentSnapshot {
    size_t dataSize;
};

struct EmitterSnapshot {
    SegmentSnapshot segmentSnapshot;
    size_t addressFixupsCount;
    size_t jumpFixupsCount;
};

class SymbolHash {
public:
    size_t operator()(const Symbol& s);
};

class Symbols {
public:
    Symbols() = default;
    Symbol Address(uintptr_t ptr);
    Symbol NewLabel();
    void Bind(Symbol label, size_t position);
private:
    std::unordered_map<uintptr_t, uint32_t> ptrToSym;
    std::vector<uintptr_t> symToPtr;
    uint32_t ptrSymCount = 0;

    std::vector<size_t> labelToPosition;
    uint32_t labelCount = 0;
};

class LiteralTable {
    size_t literal_count;
    uintptr_t *literals;

    // TODO: reference offsets
};

class Segment {
public:
    Segment() = default;
    size_t Position() const;
    void AddW8(uint32_t value);
    void AddW16(uint32_t value);
    void AddW32(uint32_t value);
    void AddW64(uint64_t value);

    SegmentSnapshot Snapshot() const;
    void Apply(SegmentSnapshot snapshot);

    std::vector<uint8_t> Finish();
private:
    std::vector<uint8_t> data;
};

struct Code {
    size_t bytecodeSize;
    uint8_t* bytecode;
    size_t literalTableSize;
    size_t* literalTable;
};

class Emitter {
public:
    using Width = Format::Width;
    using CC = Format::CC;
    using Common = Format::Common;
    using Bits = Format::Bits;

    Emitter() = default;

    Symbol Address(uintptr_t ptr);
    Symbol NewLabel();
    void Bind(Symbol label);
    // TODO: add symbol kind to store arbitrary-size values.

    /// Build `Code` in given `heap`.
    ///
    /// This procedure resolves all existring fixups and
    /// creates literal table (unused symbols or fixups will be discarded).
    Code Build(std::pmr::memory_resource& heap);

    EmitterSnapshot Snapshot();
    void Apply(EmitterSnapshot snapshot);

    void Add (Width width, IReg d, IReg l, IReg r);
    void Sub (Width width, IReg d, IReg l, IReg r);
    void Mul (Width width, IReg d, IReg l, IReg r);
    void And (Width width, IReg d, IReg l, IReg r);
    void Or  (Width width, IReg d, IReg l, IReg r);
    void Xor (Width width, IReg d, IReg l, IReg r);
    void Div (Width width, IReg d, IReg l, IReg r);
    void Rem (Width width, IReg d, IReg l, IReg r);
    void UDiv(Width width, IReg d, IReg l, IReg r);
    void URem(Width width, IReg d, IReg l, IReg r);
    void Lsl (Width width, IReg d, IReg l, IReg r);
    void Lsr (Width width, IReg d, IReg l, IReg r);
    void Asr (Width width, IReg d, IReg l, IReg r);

    void Mov(IReg d, IReg s, Width width);
    void MovRef(IReg d, IReg s);

    void Bcc(CC cc, Width width, IReg l, IReg r, Symbol label);


private:
    struct B3xrr_parts {
        Bits low3BitsOfFormatByte;
        Bits low4BitsOfSecondByte;
    };

    static B3xrr_parts PrepareBitsForB3Formats(Common op, Bits b1);
    static B3xrr_parts PrepareBitsForB3Formats(Common op, Width width);
    void GenCommon(Common common, Width width, IReg d, IReg l, IReg r, bool prohibitB2r = false);
    void GenB2rr(IReg d, IReg r, Common common, Width width);
    void GenB3xrrr(IReg d, IReg l, IReg r, B3xrr_parts parts);


    Symbols symbols;
    Segment segment;

    std::vector<Fixup> addressFixups;
    std::vector<Fixup> jumpFixups;
};

} // namespace Emitter
} // namespace Cbc
#endif // CBC_EMITTER_EMITTER_H
