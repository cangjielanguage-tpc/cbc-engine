#include <utility>
#include <cstring>

#include "emitter.h"

namespace Cbc {
namespace Emitter {

using Width = Format::Width;
using CC = Format::CC;
using Common = Format::Common;

Symbol Emitter::NewAddressSym(uintptr_t ptr) {
    return symbols.Address(ptr);
}

Label Emitter::NewLabel() {
    return symbols.NewLabel();
}

void Emitter::Bind(Label label) {
    symbols.Bind(label, segment.Pos());
}

EmitterSnapshot Emitter::Snapshot() {
    return EmitterSnapshot {
        .segmentSnapshot = segment.Snapshot(),
        .fixupCount = fixups.size(),
    };
}

void Emitter::Apply(EmitterSnapshot snapshot) {
    segment.Apply(snapshot.segmentSnapshot);
    fixups.resize(snapshot.fixupCount);
}

void Emitter::AddFixup(std::unique_ptr<Fixup> fixup) {
    auto size = fixup->Size();
    fixup->position = segment.Pos();
    fixups.push_back(std::move(fixup));
    // Fill the fixup position with zeroes.
    for (size_t i = 0; i < size; i++) {
        segment.AddW8(0);
    }
}

Code Emitter::Build(std::pmr::memory_resource& heap) {
    auto segment = std::exchange(this->segment, {});
    auto fixups = std::exchange(this->fixups, {});

    LiteralTableBuilder litBuilder(std::exchange(this->symbols, {}));

    auto relocationConverter = [&litBuilder, &segment](size_t position, Symbol sym) {
        ASSERTION(sym.kind != SymbolKind::LABEL, "Labels should be processed as part of fixup resolution");
        uint16_t value = litBuilder.UseSymbol(sym);
        segment.SetW16(position, value);
    };

    for (auto &fixup : fixups) {
        fixup->Resolve(segment, litBuilder.symbols, relocationConverter);
    }

    auto segmentCode = segment.Finish();

    auto bytecode = (uint8_t*) heap.allocate(segmentCode.size());
    auto bytecodeSize = segmentCode.size();
    std::memcpy(bytecode, &segmentCode[0], bytecodeSize);

    return Code {
        .bytecodeSize = bytecodeSize,
        .bytecode = bytecode,
        .literals = litBuilder.BuildTable(heap),
    };
}

// region isa12

using namespace Format;

Bits Pack8(Bits low4, Bits high4) {
    return high4.In(4).Shift(4) | low4.In(4);
}

Bits Pack8(IReg r1, IReg r2) {
    return Pack8(Bits(r1), Bits(r2));
}

Bits Pack8(Bits v1, IReg r2) {
    return Pack8(v1, Bits(r2));
}

Bits Pack8(IReg r1, Bits v2) {
    return Pack8(Bits(r1), v2);
}

bool IsNBitsSigned(int32_t value, uint32_t bits) {
    if (bits == 32) {
        return true;
    } else {
        // C++ have implementation-defined right shift for signed numbers until C++20.
        // We expect arithmetic shift.
        static_assert((-1 >> 16) == -1);
        auto extension = value >> (bits - 1);
        return (extension == 0) || (extension == -1);
    }
}

ImmKind ImmKindOf(int32_t value) {
    return IsNBitsSigned(value, 16) ? ImmKind::VALUE : ImmKind::LITERAL;
}

Emitter::B3xrr_parts Emitter::PrepareBitsForB3Formats(Common op, Width width) {
    return PrepareBitsForB3Formats(op, width.Common());
}

Emitter::B3xrr_parts Emitter::PrepareBitsForB3Formats(Common op, Bits b1) {
    auto page = Bits(op >> 3);
    return B3xrr_parts {
        .low3BitsOfFormatByte = Bits(OP7A::COMMON).In(2).Shift(1) | page.In(1),
        .low4BitsOfSecondByte = (Bits(op) & 0x7).Shift(1) | b1.In(1),
    };
}

// region fixups

class LiteralFixup : public Fixup {
public:
    LiteralFixup(Symbol _sym)
        : Fixup(_sym) {}

    int32_t Size() const override {
        return 2;
    }

    void Resolve(Segment& segment, Symbols& symbols,
            std::function<void(size_t, Symbol)> const& relocationConverter) const override {
        relocationConverter((size_t) position, symbol);
    }
};

class BccFixup : public Fixup {
public:
    static_assert(Format::ExtBrr::INSTRUCTION_SIZE == 4);

    BccFixup(Symbol _sym, CC _cc, Width _width, IReg _left, IReg _right)
        : Fixup(_sym), cc(_cc), width(_width), left(_left), right(_right) {}

    int32_t Size() const override {
        return Format::ExtBrr::INSTRUCTION_SIZE;
    }

    void Resolve(Segment& segment, Symbols& symbols,
            std::function<void(size_t, Symbol)> const& relocationConverter) const override {
        int32_t distance = Distance(symbols, this->symbol);
        auto immKind = ImmKindOf(distance);

        // TODO: Remove B2rrd8 formats in main byte-size opcode space.
        //       Use freed locations for these runtime-specific instructions
        //       to avoid double-dispatch.
        auto opcode = Format::ExtBrr::Fmt(immKind, width, cc);
        auto pos = (size_t) position;
        segment.SetW8(pos + 0, opcode.Raw());
        segment.SetW8(pos + 1, Pack8(left, right).Raw());
        if (immKind == ImmKind::LITERAL) {
            relocationConverter(pos + 2, symbols.Value(distance));
        } else {
            uint16_t offsetValue = (uint16_t) distance;
            segment.SetW16(pos + 2, offsetValue);
        }
    }

private:
    CC cc;
    Width width;
    IReg left;
    IReg right;
};

// region instructions

void Emitter::Add (Width width, IReg d, IReg l, IReg r) { GenCommon(Common::ADD,  width, d, l, r); }
void Emitter::Sub (Width width, IReg d, IReg l, IReg r) { GenCommon(Common::SUB,  width, d, l, r); }
void Emitter::Mul (Width width, IReg d, IReg l, IReg r) { GenCommon(Common::MUL,  width, d, l, r); }
void Emitter::And (Width width, IReg d, IReg l, IReg r) { GenCommon(Common::AND,  width, d, l, r); }
void Emitter::Or  (Width width, IReg d, IReg l, IReg r) { GenCommon(Common::OR,   width, d, l, r); }
void Emitter::Xor (Width width, IReg d, IReg l, IReg r) { GenCommon(Common::XOR,  width, d, l, r); }
void Emitter::Div (Width width, IReg d, IReg l, IReg r) { GenCommon(Common::SDIV, width, d, l, r, true); }
void Emitter::Rem (Width width, IReg d, IReg l, IReg r) { GenCommon(Common::SREM, width, d, l, r, true); }
void Emitter::UDiv(Width width, IReg d, IReg l, IReg r) { GenCommon(Common::UDIV, width, d, l, r); }
void Emitter::URem(Width width, IReg d, IReg l, IReg r) { GenCommon(Common::UREM, width, d, l, r); }
void Emitter::Lsl (Width width, IReg d, IReg l, IReg r) { GenCommon(Common::LSL,  width, d, l, r); }
void Emitter::Lsr (Width width, IReg d, IReg l, IReg r) { GenCommon(Common::LSR,  width, d, l, r); }
void Emitter::Asr (Width width, IReg d, IReg l, IReg r) { GenCommon(Common::ASR,  width, d, l, r); }

void Emitter::GenCommon(Common common, Width width, IReg d, IReg l, IReg r, bool prohibitB2r) {
    if (d == l) {
        GenB2rr(d, r, common, width);
    } else {
        GenB3xrrr(d, l, r, PrepareBitsForB3Formats(common, width));
    }
}

void Emitter::GenB2rr(IReg d, IReg r, Common common, Width width) {
    segment.AddW8(Format::B2rr::Fmt(common, width).Raw());
    segment.AddW8(Pack8(d, r).Raw());
}

void Emitter::GenB3xrrr(IReg d, IReg l, IReg r, B3xrr_parts parts) {
    segment.AddW8(Format::B3xrrr::Fmt(parts.low3BitsOfFormatByte).Raw());
    segment.AddW8(Pack8(parts.low4BitsOfSecondByte, d).Raw());
    segment.AddW8(Pack8(l, r).Raw());
}

void Emitter::Bcc(CC cc, Width width, IReg l, IReg r, Label label) {
    ASSERT(width == Width::W32 || width == Width::W64);
    AddFixup(std::make_unique<BccFixup>(label, cc, width, l, r));
}

void Emitter::Ret () {
    segment.AddW8(Format::ExtRet::Fmt().Raw());
}


void Emitter::NewObj(IReg d, Symbol sym) {
    segment.AddW8(Format::B2xrI::Opc1011::OPCODE.Raw());
    segment.AddW8(Pack8(Bits(Format::B2xrI::Opc1011::NEWOBJ), d).Raw());
    AddFixup(std::make_unique<LiteralFixup>(sym));
}

// endregion isa12

} // namespace Emitter
} // namespace Cbc
