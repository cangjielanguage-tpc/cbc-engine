#include <utility>
#include <cstring>

#include "emitter.h"
#include "cbc/isa_rt.h"
#include "utils/math.h"
#include "encoding_rt.h"

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

using namespace Format;

// region fixups

class Literal12Fixup : public Fixup {
public:
    Literal12Fixup(RT::Imm4 _i4, Symbol _sym)
        : Fixup(_sym), i4(_i4) {}

    static_assert(LiteralTableBuilder::MAX_SIZE == RT::LIT_TABLE_SIZE);

    int32_t Size() const override {
        return 2;
    }

    void Resolve(Segment& segment, Symbols& symbols,
            std::function<uint16_t(Symbol)> const& relocationConverter) const override {
        assert(position >= 0);
        Segment::View buf = segment.At(static_cast<size_t>(position));
        Encode(buf, RT::XImm12 {
            .imm4 = i4,
            .imm12 = relocationConverter(symbol),
        });
    }

private:
    RT::Imm4 i4;
};

class BccFixup : public Fixup {
public:
    BccFixup(Symbol _sym, CC _cc, Width _width, IReg _left, IReg _right)
        : Fixup(_sym), cc(_cc), width(_width), left(_left), right(_right) {}

    int32_t Size() const override {
        return RT::B4xi12rr::SIZE;
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

        bool isImm = MathUtils::IsNBitsSigned(distance, 12);
        uint16_t immediate = isImm
            ? static_cast<uint16_t>(distance & 0xfff)
            : relocationConverter(symbols.Value(distance));

        Segment::View buf = segment.At(static_cast<size_t>(position));
        Encode(buf, RT::B4xi12rr {
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

void Emitter::BinaryImm(Format::Common op, Format::Width width, IReg d, IReg l, uint64_t imm) {
    assert(width == Format::Width::W32 || width == Format::Width::W64);

    bool isImm = MathUtils::IsNBitsSigned(imm, 12);
    if (isImm) {
        uint16_t immediate = static_cast<uint16_t>(imm & 0xfff);

        auto opcode = width == Format::Width::W32
            ? RT::Opcode::BINI32I
            : RT::Opcode::BINI64I;

        Encode(segment, RT::B4xi12rr {
            .opc = opcode,
            .xi12 = RT::XImm12 {
                .imm4 = RT::Imm4(op),
                .imm12 = RT::Imm12(immediate & 0xfff),
            },
            .rr = {
                .x = d,
                .y = l
            },
        });

    } else {
        Symbol immediate = symbols.Value(imm);

        RT::Opcode opcode = width == Format::Width::W32
            ? RT::Opcode::BINI32L
            : RT::Opcode::BINI64L;

        Encode(segment, opcode);
        AddFixup(std::make_unique<Literal12Fixup>(RT::Imm4(op), immediate));
        Encode(segment, RT::RR {
            .x = d,
            .y = l
        });
    }
}

void Emitter::AddI (Width width, IReg d, IReg l, uint64_t imm) { BinaryImm(Common::ADD,  width, d, l, imm); }
void Emitter::SubI (Width width, IReg d, IReg l, uint64_t imm) { BinaryImm(Common::SUB,  width, d, l, imm); }
void Emitter::MulI (Width width, IReg d, IReg l, uint64_t imm) { BinaryImm(Common::MUL,  width, d, l, imm); }
void Emitter::AndI (Width width, IReg d, IReg l, uint64_t imm) { BinaryImm(Common::AND,  width, d, l, imm); }
void Emitter::OrI  (Width width, IReg d, IReg l, uint64_t imm) { BinaryImm(Common::OR,   width, d, l, imm); }
void Emitter::XorI (Width width, IReg d, IReg l, uint64_t imm) { BinaryImm(Common::XOR,  width, d, l, imm); }
void Emitter::DivI (Width width, IReg d, IReg l, uint64_t imm) { BinaryImm(Common::SDIV, width, d, l, imm); }
void Emitter::RemI (Width width, IReg d, IReg l, uint64_t imm) { BinaryImm(Common::SREM, width, d, l, imm); }
void Emitter::UDivI(Width width, IReg d, IReg l, uint64_t imm) { BinaryImm(Common::UDIV, width, d, l, imm); }
void Emitter::URemI(Width width, IReg d, IReg l, uint64_t imm) { BinaryImm(Common::UREM, width, d, l, imm); }
void Emitter::LslI (Width width, IReg d, IReg l, uint64_t imm) { BinaryImm(Common::LSL,  width, d, l, imm); }
void Emitter::LsrI (Width width, IReg d, IReg l, uint64_t imm) { BinaryImm(Common::LSR,  width, d, l, imm); }
void Emitter::AsrI (Width width, IReg d, IReg l, uint64_t imm) { BinaryImm(Common::ASR,  width, d, l, imm); }

void Emitter::Mov(IReg d, IReg s) {
    Encode(segment, RT::B2rr {
        .opc = RT::Opcode::MOV,
        .rr = RT::RR {
            .x = d,
            .y = s
        }
    });
}

void Emitter::MovImm(Width width, IReg d, uint64_t imm) {
    assert(width == Format::Width::W32 || width == Format::Width::W64);

    bool isImm = MathUtils::IsNBitsSigned(imm, 4);
    if (isImm) {
        uint8_t immediate = static_cast<uint8_t>(imm & 0xf);

        Encode(segment, RT::B2xr {
            .opc = RT::Opcode::MOVI,
            .xr = RT::XR {
                .imm = RT::Imm4(immediate),
                .r = d
            }
        });

    } else {
        AddI(width, d, IReg::IRZ, imm);
    }
}

void Emitter::MovRef(IReg d, IReg s) {
    Encode(segment, RT::B2rr {
        .opc = RT::Opcode::MOVR,
        .rr = RT::RR {
            .x = d,
            .y = s
        }
    });
}


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

void Emitter::LoadObj(Format::LoadAccessKind ldk, IReg dst, IReg base, uint32_t offset) {
    if (MathUtils::IsNBits(offset, 12)) {
        Encode(segment, RT::B4xi12rr {
            .opc = RT::Opcode::LOAD_OBJ,
            .xi12 = {
                .imm4 = RT::Imm4(ldk),
                .imm12 = static_cast<uint16_t>(offset),
            },
            .rr = {
                .x = dst,
                .y = base,
            }
        });
    } else {
        auto ms = OpenMemSpace();
        ms.Offset(offset);
        ms.LoadObj(ldk, dst, base);
    }
}

void Emitter::StoreObj(Format::StoreAccessKind stk, IReg src, IReg base, uint32_t offset) {
    if (MathUtils::IsNBits(offset, 12)) {
        Encode(segment, RT::B4xi12rr {
            .opc = RT::Opcode::STORE_OBJ,
            .xi12 = {
                .imm4 = RT::Imm4(stk),
                .imm12 = static_cast<uint16_t>(offset),
            },
            .rr = {
                .x = src,
                .y = base,
            }
        });
    } else {
        auto ms = OpenMemSpace();
        ms.Offset(offset);
        ms.StoreObj(stk, src, base);
    }
}

} // namespace Emitter
} // namespace Cbc
