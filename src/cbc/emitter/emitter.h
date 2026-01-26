#ifndef CBC_EMITTER_EMITTER_H
#define CBC_EMITTER_EMITTER_H

#include <vector>
#include <functional>
#include <cstdint>
#include <memory_resource>
#include <memory>

#include "utils/assertion.h"
#include "cbc/isa.h"
#include "cbc/emitter/symbols.h"
#include "cbc/emitter/segment.h"

namespace Cbc {
namespace Emitter {

class CbcEmitter;
class Symbols;

struct EmitterSnapshot {
    SegmentSnapshot segmentSnapshot;
    size_t fixupCount;
};

struct Code {
    size_t bytecodeSize;
    uint8_t* bytecode;
    Interpretation::LiteralTable *literals;
};

class Emitter {
public:
    using Width = Format::Width;
    using CC = Format::CC;
    using Common = Format::Common;
    using Bits = Format::Bits;

    Emitter() = default;

    Symbol NewAddressSym(uintptr_t ptr);
    Label NewLabel();
    void Bind(Label label);
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

    void Bcc(CC cc, Width width, IReg l, IReg r, Label label);


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
    void AddFixup(std::unique_ptr<Fixup> fixup);

    Symbols symbols;
    Segment segment;

    std::vector<std::unique_ptr<Fixup>> fixups;
};

} // namespace Emitter
} // namespace Cbc
#endif // CBC_EMITTER_EMITTER_H
