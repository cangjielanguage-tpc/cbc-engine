#include <utility>
#include <cstring>

#include "emitter.h"
#include "cbc/isa_rt.h"

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

Interpretation::Code Emitter::Build(std::pmr::memory_resource& heap) {
    auto segment = std::exchange(this->segment, {});
    auto fixups = std::exchange(this->fixups, {});

    LiteralTableBuilder litBuilder(std::exchange(this->symbols, {}));

    auto relocationConverter = [&litBuilder, &segment](Symbol sym) {
        ASSERTION(sym.kind != SymbolKind::LABEL, "Labels should be processed as part of fixup resolution");
        return litBuilder.UseSymbol(sym);
    };

    for (auto &fixup : fixups) {
        fixup->Resolve(segment, litBuilder.symbols, relocationConverter);
    }

    auto segmentCode = segment.Finish();

    auto bytecode = (uint8_t*) heap.allocate(segmentCode.size());
    auto bytecodeSize = segmentCode.size();
    std::memcpy(bytecode, &segmentCode[0], bytecodeSize);

    return Interpretation::Code {
        .bytecodeSize = bytecodeSize,
        .bytecode = bytecode,
        .literals = litBuilder.BuildTable(heap),
    };
}

// region encoding

void Encode(Segment& segment, RT::Opcode opc) {
    segment.AddW8(opc);
}

void Encode(Segment& segment, RT::RR rr) {
    segment.AddW8(static_cast<uint32_t>(rr.x | (rr.y << 4)));
}

void Encode(Segment& segment, RT::XR xr) {
    segment.AddW8(static_cast<uint32_t>(xr.imm | (xr.r << 4)));
}

void Encode(Segment& segment, RT::Imm16 i16) {
    segment.AddW16(i16.imm);
}

void Encode(Segment& segment, RT::XImm12 xi12) {
    segment.AddW16(static_cast<uint16_t>(xi12.imm4 | (xi12.imm12 << 4)));
}

void Encode(Segment& segment, RT::B1 command) {
    Encode(segment, command.opc);
}

void Encode(Segment& segment, RT::B2rr command) {
    Encode(segment, command.opc);
    Encode(segment, command.rr);
}

void Encode(Segment& segment, RT::B3xrrr command) {
    Encode(segment, command.opc);
    Encode(segment, command.xr);
    Encode(segment, command.rr);
}

void Encode(Segment& segment, RT::B4xi12rr command) {
    Encode(segment, command.opc);
    Encode(segment, command.xi12);
    Encode(segment, command.rr);
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

// region fixups

class Literal12Fixup : public Fixup {
public:
    Literal12Fixup(RT::Imm4 _i4, Symbol _sym)
        : Fixup(_sym), i4(_i4) {}

    int32_t Size() const override {
        return 2;
    }

    void Resolve(Segment& segment, Symbols& symbols,
            std::function<uint16_t(Symbol)> const& relocationConverter) const override {
        assert(position >= 0);
        RT::XImm12 value {
            .imm4 = i4,
            .imm12 = relocationConverter(symbol),
        };
        segment.SetW16(static_cast<size_t>(position), relocationConverter(symbol));
    }

private:
    RT::Imm4 i4;
};

class BccFixup : public Fixup {
public:
    static_assert(Format::ExtBrr::INSTRUCTION_SIZE == 4);

    BccFixup(Symbol _sym, CC _cc, Width _width, IReg _left, IReg _right)
        : Fixup(_sym), cc(_cc), width(_width), left(_left), right(_right) {}

    int32_t Size() const override {
        return Format::ExtBrr::INSTRUCTION_SIZE;
    }

    static RT::Opcode opcode(bool isImm, Format::Width width) {
        switch (width) {
            case Format::Width::W32: return isImm ? RT::Opcode::BCC32I : RT::Opcode::BCC32L;
            case Format::Width::W64: return isImm ? RT::Opcode::BCC64I : RT::Opcode::BCC64L;
            default: assert(false); return RT::Opcode::BCC32I;
        }
    }

    void Resolve(Segment& segment, Symbols& symbols,
            std::function<uint16_t(Symbol)> const& relocationConverter) const override {
        int32_t distance = Distance(symbols, this->symbol);

        bool isImm = IsNBitsSigned(distance, 12);
        uint16_t immediate = isImm
            ? static_cast<uint16_t>(distance)
            : relocationConverter(symbols.Value(distance));

        Encode(segment, RT::B4xi12rr {
            .opc = opcode(isImm, width),
            .xi12 = {
                .imm4 = cc,
                .imm12 = immediate,
            },
            .rr = {
                .x = left,
                .y = right
            },
        });
    }

private:
    CC cc;
    Width width;
    IReg left;
    IReg right;
};

// region instructions

void Emitter::Binary(Format::Common op, Format::Width width, IReg d, IReg l, IReg r) {
    assert(width == Format::Width::W32 || width == Format::Width::W64);
    auto opcode = width == Format::Width::W32
        ? RT::Opcode::BIN32
        : RT::Opcode::BIN64;

    Encode(segment, RT::B3xrrr {
        .opc = opcode,
        .xr = RT::XR {
            .imm = RT::Imm4(op),
            .r = d,
        },
        .rr = {
            .x = l,
            .y = r
        },
    });
}

void Emitter::Add (Width width, IReg d, IReg l, IReg r) { Binary(Common::ADD,  width, d, l, r); }
void Emitter::Sub (Width width, IReg d, IReg l, IReg r) { Binary(Common::SUB,  width, d, l, r); }
void Emitter::Mul (Width width, IReg d, IReg l, IReg r) { Binary(Common::MUL,  width, d, l, r); }
void Emitter::And (Width width, IReg d, IReg l, IReg r) { Binary(Common::AND,  width, d, l, r); }
void Emitter::Or  (Width width, IReg d, IReg l, IReg r) { Binary(Common::OR,   width, d, l, r); }
void Emitter::Xor (Width width, IReg d, IReg l, IReg r) { Binary(Common::XOR,  width, d, l, r); }
void Emitter::Div (Width width, IReg d, IReg l, IReg r) { Binary(Common::SDIV, width, d, l, r); }
void Emitter::Rem (Width width, IReg d, IReg l, IReg r) { Binary(Common::SREM, width, d, l, r); }
void Emitter::UDiv(Width width, IReg d, IReg l, IReg r) { Binary(Common::UDIV, width, d, l, r); }
void Emitter::URem(Width width, IReg d, IReg l, IReg r) { Binary(Common::UREM, width, d, l, r); }
void Emitter::Lsl (Width width, IReg d, IReg l, IReg r) { Binary(Common::LSL,  width, d, l, r); }
void Emitter::Lsr (Width width, IReg d, IReg l, IReg r) { Binary(Common::LSR,  width, d, l, r); }
void Emitter::Asr (Width width, IReg d, IReg l, IReg r) { Binary(Common::ASR,  width, d, l, r); }

void Emitter::Bcc(CC cc, Width width, IReg l, IReg r, Label label) {
    ASSERT(width == Width::W32 || width == Width::W64);
    AddFixup(std::make_unique<BccFixup>(label, cc, width, l, r));
}

void Emitter::Ret() {
    Encode(segment, RT::B1{RT::Opcode::RET});
}


void Emitter::NewObj(IReg d, Symbol sym) {
    segment.AddW8(RT::Opcode::NEWOBJ);
    RT::Imm4 i4(d);
    AddFixup(std::make_unique<Literal12Fixup>(i4, sym));
}

// endregion isa12

} // namespace Emitter
} // namespace Cbc
