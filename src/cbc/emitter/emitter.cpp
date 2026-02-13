#include <utility>
#include <cstring>

#include "emitter.h"
#include "cbc/isa_rt.h"
#include "utils/math.h"

namespace Cbc {
namespace Emitter {

using Width = Format::Width;
using CC = Format::CC;
using Common = Format::Common;

Symbol Emitter::NewAddressSym(uintptr_t ptr)
{
    return symbols.Address(ptr);
}

Label Emitter::NewLabel()
{
    return symbols.NewLabel();
}

void Emitter::Bind(Label label)
{
    symbols.Bind(label, segment.Pos());
}

EmitterSnapshot Emitter::Snapshot()
{
    return EmitterSnapshot {
        .segmentSnapshot = segment.Snapshot(),
        .fixupCount = fixups.size(),
    };
}

void Emitter::Apply(EmitterSnapshot snapshot)
{
    segment.Apply(snapshot.segmentSnapshot);
    fixups.resize(snapshot.fixupCount);
}

void Emitter::AddFixup(std::unique_ptr<Fixup> fixup)
{
    auto size = fixup->Size();
    fixup->position = segment.Pos();
    fixups.push_back(std::move(fixup));
    // Fill the fixup position with zeroes.
    for (size_t i = 0; i < size; i++) {
        segment.AddW8(0);
    }
}

Interpretation::Code Emitter::Build(std::pmr::memory_resource& heap)
{
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
    Literal12Fixup(Format::Imm4 _i4, Symbol _sym)
        : Fixup(_sym), i4(_i4) {}

    static_assert(LiteralTableBuilder::MAX_SIZE == RT::LIT_TABLE_SIZE);

    int32_t Size() const override { return 2; }

    void Resolve(
        Segment& segment, Symbols& symbols,
        std::function<uint16_t(Symbol)> const& relocationConverter
    ) const override
    {
        assert(position >= 0);
        Segment::View buf = segment.At(static_cast<size_t>(position));
        Encode(buf, Format::XImm12 {
            .imm4 = i4,
            .imm12 = relocationConverter(symbol),
        });
    }

private:
    Format::Imm4 i4;
};

class JmpFixup : public Fixup {
public:
    JmpFixup(Symbol _sym): Fixup(_sym) {}

    int32_t Size() const override { return RT::B5i32::SIZE; }

    void Resolve(
        Segment& segment, Symbols& symbols,
        std::function<uint16_t(Symbol)> const& relocationConverter
    ) const override
    {
        int32_t distance = Distance(symbols, this->symbol);

        Segment::View buf = segment.At(static_cast<size_t>(position));
        Encode(buf, RT::B5i32 {
            .opc = RT::Opcode::JMP32, // TODO: support short jump instruction
            .imm32 = Format::Imm32 {
                .imm = static_cast<uint32_t>(distance)
            }
        });
    }
};

class BccFixup : public Fixup {
public:
    BccFixup(Symbol _sym, CC _cc, Width _width, IReg _left, IReg _right)
        : Fixup(_sym), cc(_cc), width(_width), left(_left), right(_right) {}

    int32_t Size() const override { return RT::B4xi12rr::SIZE; }

    static RT::Opcode opcode(bool isImm, Format::Width width)
    {
        switch (width) {
            case Format::Width::W32: return isImm ? RT::Opcode::BCC32I : RT::Opcode::BCC32L;
            case Format::Width::W64: return isImm ? RT::Opcode::BCC64I : RT::Opcode::BCC64L;
            default: assert(false); return RT::Opcode::BCC32I;
        }
    }

    void Resolve(
        Segment& segment, Symbols& symbols,
        std::function<uint16_t(Symbol)> const& relocationConverter
    ) const override
    {
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

class BccImmFixup : public Fixup {
public:
    BccImmFixup(Symbol _sym, CC _cc, Width _width, IReg _left, uint64_t _right)
        : Fixup(_sym), cc(_cc), width(_width), left(_left), right(_right) {}

    int32_t Size() const override { return RT::B5xi12ri12::SIZE; }

    static RT::Opcode opcode(bool isImmOffset, bool isImmValue, Format::Width width)
    {
        if (width == Format::Width::W32) {
            return isImmOffset ? (isImmValue ? RT::Opcode::BCCI32I : RT::Opcode::BCCL32I)
                               : (isImmValue ? RT::Opcode::BCCI32L : RT::Opcode::BCCL32L);
        } else {
            ASSERTION(width == Format::Width::W64, "Unexpected width");
            return isImmOffset ? (isImmValue ? RT::Opcode::BCCI64I : RT::Opcode::BCCL64I)
                               : (isImmValue ? RT::Opcode::BCCI64L : RT::Opcode::BCCL64L);
        }
    }

    void Resolve(
        Segment& segment, Symbols& symbols,
        std::function<uint16_t(Symbol)> const& relocationConverter
    ) const override
    {
        int32_t distance = Distance(symbols, this->symbol);

        bool isImmOffset = MathUtils::IsNBitsSigned(distance, 12);
        uint16_t immOffset = isImmOffset
            ? static_cast<uint16_t>(distance & 0xfff)
            : relocationConverter(symbols.Value(distance));

        bool isImmValue = MathUtils::IsNBitsSigned(right, 12);
        uint16_t immValue = isImmValue
            ? static_cast<uint16_t>(right & 0xfff)
            : relocationConverter(symbols.Value(right));

        Segment::View buf = segment.At(static_cast<size_t>(position));
        Encode(buf, RT::B5xi12ri12 {
            .opc = opcode(isImmOffset, isImmValue, width),
            .xi12 = {
                .imm4 = cc,
                .imm12 = immOffset,
            },
            .ri12 = {
                .r = left,
                .imm12 = immValue
            },
        });
    }

private:
    CC cc;
    Width width;
    IReg left;
    uint64_t right;
};

// region instructions

void Emitter::Binary(Format::Common op, Format::Width width, IReg d, IReg l, IReg r)
{
    assert(width == Format::Width::W32 || width == Format::Width::W64);
    auto opcode = width == Format::Width::W32
        ? RT::Opcode::BIN32
        : RT::Opcode::BIN64;

    Encode(segment, RT::B3xrrr {
        .opc = opcode,
        .xr = Format::XR {
            .imm = Format::Imm4(op),
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

void Emitter::BinaryImm(Format::Common op, Format::Width width, IReg d, IReg l, uint64_t imm)
{
    assert(width == Format::Width::W32 || width == Format::Width::W64);

    bool isImm = MathUtils::IsNBitsSigned(imm, 12);
    if (isImm) {
        uint16_t immediate = static_cast<uint16_t>(imm & 0xfff);

        auto opcode = width == Format::Width::W32
            ? RT::Opcode::BINI32I
            : RT::Opcode::BINI64I;

        Encode(segment, RT::B4xi12rr {
            .opc = opcode,
            .xi12 = Format::XImm12 {
                .imm4 = Format::Imm4(op),
                .imm12 = Format::Imm12(immediate & 0xfff),
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
        AddFixup(std::make_unique<Literal12Fixup>(Format::Imm4(op), immediate));
        Encode(segment, Format::RR {
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

void Emitter::Binary(Format::FloatOperations op, Format::Width width, FReg d, FReg l, FReg r)
{
    assert(width == Format::Width::W32 || width == Format::Width::W64);
    assert(op.IsBasic());

    auto opcode = width == Format::Width::W32
        ? RT::Opcode::FBIN32
        : RT::Opcode::FBIN64;

    Encode(segment, RT::B3xrrr {
        .opc = opcode,
        .xr = Format::XR {
            .imm = Format::Imm4(op),
            .r = d,
        },
        .rr = {
            .x = l,
            .y = r
        },
    });
}

void Emitter::Add(Width width, FReg d, FReg l, FReg r) { Binary(FloatOperations::FADD, width, d, l, r); }
void Emitter::Sub(Width width, FReg d, FReg l, FReg r) { Binary(FloatOperations::FSUB, width, d, l, r); }
void Emitter::Mul(Width width, FReg d, FReg l, FReg r) { Binary(FloatOperations::FMUL, width, d, l, r); }
void Emitter::Div(Width width, FReg d, FReg l, FReg r) { Binary(FloatOperations::FDIV, width, d, l, r); }

void Emitter::Mov(RT::Opcode opcode, Format::Reg d, Format::Reg s)
{
    Encode(segment, RT::B2rr {
        .opc = opcode,
        .rr = Format::RR {
            .x = d,
            .y = s
        }
    });
}

void Emitter::Mov(IReg d, IReg s) { Emitter::Mov(RT::Opcode::MOV, d, s); }
void Emitter::Mov(FReg d, FReg s) { Emitter::Mov(RT::Opcode::FMOV, d, s); }
void Emitter::Mov(FReg d, IReg s) { Emitter::Mov(RT::Opcode::MOVI2F, d, s); }
void Emitter::Mov(IReg d, FReg s) { Emitter::Mov(RT::Opcode::MOVF2I, d, s); }

void Emitter::MovImm(Width width, IReg d, uint64_t imm)
{
    assert(width == Format::Width::W32 || width == Format::Width::W64);

    bool isImm = MathUtils::IsNBitsSigned(imm, 4);
    if (isImm) {
        uint8_t immediate = static_cast<uint8_t>(imm & 0xf);

        Encode(segment, RT::B2xr {
            .opc = RT::Opcode::MOVI,
            .xr = Format::XR {
                .imm = Format::Imm4(immediate),
                .r = d
            }
        });

    } else {
        AddI(width, d, IReg::IRZ, imm);
    }
}

void Emitter::FMovI32(FReg d, float imm)
{
    Encode(segment, RT::B6xri32 {
        .opc = RT::Opcode::FMOVI32,
        .xr = Format::XR {
            .imm = 0,
            .r = d
        },
        .imm32 = Format::Imm32 {
            .fimm = imm
        }
    });
}

void Emitter::FMovI64(FReg d, double imm)
{
    Encode(segment, RT::B10xri64 {
        .opc = RT::Opcode::FMOVI64,
        .xr = Format::XR {
            .imm = 0,
            .r = d
        },
        .imm64 = Format::Imm64 {
            .dimm = imm
        }
    });
}

void Emitter::MovRef(IReg d, IReg s)
{
    Encode(segment, RT::B2rr {
        .opc = RT::Opcode::MOVR,
        .rr = Format::RR {
            .x = d,
            .y = s
        }
    });
}


void Emitter::Bcc(CC cc, Width width, IReg l, IReg r, Label label)
{
    ASSERT(width == Width::W32 || width == Width::W64);
    AddFixup(std::make_unique<BccFixup>(label, cc, width, l, r));
}

void Emitter::BccImm(CC cc, Width width, IReg l, uint64_t r, Label label)
{
    ASSERT(width == Width::W32 || width == Width::W64);
    AddFixup(std::make_unique<BccImmFixup>(label, cc, width, l, r));
}

void Emitter::Jmp(Label label)
{
    AddFixup(std::make_unique<JmpFixup>(label));
}

void Emitter::Ret()
{
    Encode(segment, RT::B1{RT::Opcode::RET});
}


void Emitter::NewObj(IReg d, Symbol sym)
{
    segment.AddW8(RT::Opcode::NEWOBJ);
    Format::Imm4 i4(d);
    AddFixup(std::make_unique<Literal12Fixup>(i4, sym));
}

void Emitter::LoadObj(Format::LoadAccessKind ldk, Format::Reg dst, IReg base, uint32_t offset)
{
    if (MathUtils::IsNBits(offset, 12)) {
        Encode(segment, RT::B4xi12rr {
            .opc = RT::Opcode::LOAD_OBJ,
            .xi12 = {
                .imm4 = Format::Imm4(ldk),
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

void Emitter::StoreObj(Format::StoreAccessKind stk, Format::Reg src, IReg base, uint32_t offset)
{
    if (MathUtils::IsNBits(offset, 12)) {
        Encode(segment, RT::B4xi12rr {
            .opc = RT::Opcode::STORE_OBJ,
            .xi12 = {
                .imm4 = Format::Imm4(stk),
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

void Emitter::LoadRec(Format::LoadAccessKind ldk, Format::Reg dst, IReg base, uint32_t offset)
{
    if (MathUtils::IsNBits(offset, 12)) {
        LoadStore(ldk, dst, base, offset, RT::Opcode::LOAD_REC);
    } else {
        auto ms = OpenMemSpace();
        ms.Offset(offset);
        ms.LoadRec(ldk, dst, base);
    }
}

void Emitter::StoreRec(Format::StoreAccessKind stk, Format::Reg src, IReg base, uint32_t offset)
{
    if (MathUtils::IsNBits(offset, 12)) {
        LoadStore(stk, src, base, offset, RT::Opcode::STORE_REC);
    } else {
        auto ms = OpenMemSpace();
        ms.Offset(offset);
        ms.StoreRec(stk, src, base);
    }
}


void Emitter::LoadFrame(Format::LoadAccessKind ldk, RT::Reg dst, uint32_t offset)
{
    if (MathUtils::IsNBits(offset, 12)) {
        Load(ldk, dst, IReg::IRZ, offset, RT::Opcode::LOAD_FRAME);
    } else {
        auto ms = OpenMemSpace();
        ms.Offset(offset);
        ms.LoadFrame(ldk, dst);
    }
}

void Emitter::StoreFrame(Format::StoreAccessKind stk, RT::Reg src, uint32_t offset)
{
    if (MathUtils::IsNBits(offset, 12)) {
        Store(stk, src, IReg::IRZ, offset, RT::Opcode::STORE_FRAME);
    } else {
        auto ms = OpenMemSpace();
        ms.Offset(offset);
        ms.StoreFrame(stk, src);
    }
}

} // namespace Emitter
} // namespace Cbc
