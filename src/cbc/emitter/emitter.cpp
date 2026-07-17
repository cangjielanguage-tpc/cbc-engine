#include <cstdint>
#include <cstring>
#include <utility>

#include "cbc/emitter/encoding_rt.h"
#include "cbc/isa.h"
#include "cbc/isa_rt.h"
#include "emitter.h"
#include "runtimesupport/runtime.h"
#include "utils/assertion.h"
#include "utils/heap.h"
#include "utils/math.h"

namespace Cbc {
namespace Emitter {

using namespace Format;

Symbol Emitter::NewAddressSym(uintptr_t ptr) { return symbols.Address(ptr); }

Label Emitter::NewLabel() { return symbols.NewLabel(); }

void Emitter::Bind(Label label) { symbols.Bind(label, segment.Pos()); }

int32_t Emitter::LabelPosition(Label label) const { return symbols.LabelPosition(label); }

EmitterSnapshot Emitter::Snapshot()
{
    return EmitterSnapshot {
        .segmentSnapshot = segment.Snapshot(),
        .fixupCount      = fixups.size(),
    };
}

void Emitter::Apply(EmitterSnapshot snapshot)
{
    segment.Apply(snapshot.segmentSnapshot);
    fixups.resize(snapshot.fixupCount);
}

void Emitter::AddFixup(std::unique_ptr<Fixup> fixup)
{
    auto size       = fixup->Size();
    fixup->position = segment.Pos();
    fixups.push_back(std::move(fixup));
    // Fill the fixup position with zeroes.
    for (size_t i = 0; i < size; i++) {
        segment.AddW8(0);
    }
}

Interpretation::Code Emitter::Build(Memory::Heap& heap)
{
    auto segment = std::exchange(this->segment, {});
    auto fixups  = std::exchange(this->fixups, {});

    LiteralTableBuilder litBuilder(this->symbols);

    auto relocationConverter = [&litBuilder, &segment](Symbol sym) {
        ASSERTION(sym.kind != SymbolKind::LABEL, "Labels should be processed as part of fixup resolution");
        return litBuilder.UseSymbol(sym);
    };

    for (auto& fixup : fixups) {
        fixup->Resolve(segment, litBuilder.symbols, relocationConverter);
    }

    auto segmentCode = segment.Finish();

    auto bytecode     = (uint8_t*)heap.Allocate(segmentCode.size());
    auto bytecodeSize = segmentCode.size();
    std::copy(segmentCode.begin(), segmentCode.end(), bytecode);

    return Interpretation::Code {
        .bytecodeSize = bytecodeSize,
        .bytecode     = bytecode,
        .literals     = litBuilder.BuildTable(heap),
    };
}

// region fixups

class LiteralFixup : public Fixup {
public:
    LiteralFixup(Symbol _sym) : Fixup(_sym) {}

    int32_t Size() const override { return 2; }

    void Resolve(Segment& segment, Symbols& symbols, std::function<uint16_t(Symbol)> const& relocationConverter)
        const override
    {
        ASSERT(position >= 0);
        Segment::View buf = segment.At(static_cast<size_t>(position));
        buf.AddW16(relocationConverter(symbol));
    }
};

class Literal12Fixup : public Fixup {
public:
    Literal12Fixup(Imm4 _i4, Symbol _sym) : Fixup(_sym), i4(_i4) {}

    static_assert(LiteralTableBuilder::MAX_SIZE == RT::LIT_TABLE_SIZE);

    int32_t Size() const override { return 2; }

    void Resolve(Segment& segment, Symbols& symbols, std::function<uint16_t(Symbol)> const& relocationConverter)
        const override
    {
        ASSERT(position >= 0);
        Segment::View buf = segment.At(static_cast<size_t>(position));
        Encode(
            buf,
            XImm12 {
                .imm4  = i4,
                .imm12 = relocationConverter(symbol),
            }
        );
    }

private:
    Imm4 i4;
};

class JmpFixup : public Fixup {
public:
    JmpFixup(Symbol _sym) : Fixup(_sym) {}

    int32_t Size() const override { return RT::B5i32::SIZE; }

    void Resolve(Segment& segment, Symbols& symbols, std::function<uint16_t(Symbol)> const& relocationConverter)
        const override
    {
        int32_t distance = Distance(symbols, this->symbol);

        Segment::View buf = segment.At(static_cast<size_t>(position));
        Encode(
            buf,
            RT::B5i32 { .opc   = RT::Opcode::JMP32, // TODO: support short jump instruction
                        .imm32 = Imm32 { .imm = static_cast<uint32_t>(distance) } }
        );
    }
};

class BrIfRef : public Fixup {
public:
    BrIfRef(Symbol sym, IReg typeInfo) : Fixup(sym), typeInfo(typeInfo) {}

    int32_t Size() const override { return RT::B3xi12::SIZE; }

    void Resolve(Segment& segment, Symbols& symbols, std::function<uint16_t(Symbol)> const& relocationConverter)
        const override
    {
        int32_t distance = Distance(symbols, this->symbol);
        ASSERTION(MathUtils::IsNBitsSigned(distance, 12), "has only short encoding");

        Segment::View buf = segment.At(static_cast<size_t>(position));
        RT::B3xi12 command { .opc = RT::Opcode::BRANCH_IS_REF, .xi12 = { .imm4 = { typeInfo }, .imm12 = distance } };
        Encode(buf, command);
    }

private:
    IReg typeInfo;
};

class BccFixup : public Fixup {
public:
    BccFixup(Symbol _sym, CC _cc, Width _width, Reg _left, Reg _right)
        : Fixup(_sym),
          cc(_cc),
          width(_width),
          left(_left),
          right(_right)
    {}

    int32_t Size() const override { return RT::B4xi12rr::SIZE; }

    static RT::Opcode opcode(bool isImm, Width width)
    {
        switch (width) {
            case Width::W32: return isImm ? RT::Opcode::BCC32I : RT::Opcode::BCC32L;
            case Width::W64: return isImm ? RT::Opcode::BCC64I : RT::Opcode::BCC64L;
            default:         FATAL("unexpected Width: %s", width.CStr()); return RT::Opcode::BCC32I;
        }
    }

    void Resolve(Segment& segment, Symbols& symbols, std::function<uint16_t(Symbol)> const& relocationConverter)
        const override
    {
        int32_t distance = Distance(symbols, this->symbol);

        bool isImm = MathUtils::IsNBitsSigned(distance, 12);
        uint16_t immediate =
            isImm ? static_cast<uint16_t>(distance & 0xfff) : relocationConverter(symbols.Value(distance));

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
    Reg left;
    Reg right;
};

class BccImmFixup : public Fixup {
public:
    BccImmFixup(Symbol _sym, CC _cc, Width _width, IReg _left, uint64_t _right)
        : Fixup(_sym),
          cc(_cc),
          width(_width),
          left(_left),
          right(_right)
    {}

    int32_t Size() const override { return RT::B5xi12ri12::SIZE; }

    static RT::Opcode opcode(bool isImmOffset, bool isImmValue, Width width)
    {
        if (width == Width::W32) {
            return isImmOffset ? (isImmValue ? RT::Opcode::BCCI32I : RT::Opcode::BCCL32I)
                               : (isImmValue ? RT::Opcode::BCCI32L : RT::Opcode::BCCL32L);
        } else {
            ASSERTION(width == Width::W64, "Unexpected width: %s", width.CStr());
            return isImmOffset ? (isImmValue ? RT::Opcode::BCCI64I : RT::Opcode::BCCL64I)
                               : (isImmValue ? RT::Opcode::BCCI64L : RT::Opcode::BCCL64L);
        }
    }

    void Resolve(Segment& segment, Symbols& symbols, std::function<uint16_t(Symbol)> const& relocationConverter)
        const override
    {
        int32_t distance = Distance(symbols, this->symbol);

        bool isImmOffset = MathUtils::IsNBitsSigned(distance, 12);
        uint16_t immOffset =
            isImmOffset ? static_cast<uint16_t>(distance & 0xfff) : relocationConverter(symbols.Value(distance));

        bool isImmValue = MathUtils::IsNBitsSigned(right, 12);
        uint16_t immValue =
            isImmValue ? static_cast<uint16_t>(right & 0xfff) : relocationConverter(symbols.Value(right));

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

void Emitter::Binary(Common op, Width width, IReg d, IReg l, IReg r)
{
    ASSERT(width == Width::W32 || width == Width::W64);
    auto opcode = width == Width::W32 ? RT::Opcode::BIN32 : RT::Opcode::BIN64;

    Encode(
        segment,
        RT::B3xrrr {
            .opc = opcode,
            .xr =
                XR {
                    .imm = Imm4(op),
                    .r   = d,
                },
            .rr = { .x = l, .y = r },
        }
    );
}

void Emitter::Add(Width width, IReg d, IReg l, IReg r) { Binary(Common::ADD, width, d, l, r); }

void Emitter::Sub(Width width, IReg d, IReg l, IReg r) { Binary(Common::SUB, width, d, l, r); }

void Emitter::Mul(Width width, IReg d, IReg l, IReg r) { Binary(Common::MUL, width, d, l, r); }

void Emitter::And(Width width, IReg d, IReg l, IReg r) { Binary(Common::AND, width, d, l, r); }

void Emitter::Or(Width width, IReg d, IReg l, IReg r) { Binary(Common::OR, width, d, l, r); }

void Emitter::Xor(Width width, IReg d, IReg l, IReg r) { Binary(Common::XOR, width, d, l, r); }

void Emitter::Div(Width width, IReg d, IReg l, IReg r) { Binary(Common::SDIV, width, d, l, r); }

void Emitter::Rem(Width width, IReg d, IReg l, IReg r) { Binary(Common::SREM, width, d, l, r); }

void Emitter::UDiv(Width width, IReg d, IReg l, IReg r) { Binary(Common::UDIV, width, d, l, r); }

void Emitter::URem(Width width, IReg d, IReg l, IReg r) { Binary(Common::UREM, width, d, l, r); }

void Emitter::Lsl(Width width, IReg d, IReg l, IReg r) { Binary(Common::LSL, width, d, l, r); }

void Emitter::Lsr(Width width, IReg d, IReg l, IReg r) { Binary(Common::LSR, width, d, l, r); }

void Emitter::Asr(Width width, IReg d, IReg l, IReg r) { Binary(Common::ASR, width, d, l, r); }

void Emitter::Neg(Width width, IReg d, IReg s) { Binary(Common::SUB, width, d, IReg::IRZ, s); }

void Emitter::BinaryImm(Common op, Width width, IReg d, IReg l, uint64_t imm)
{
    ASSERT(width == Width::W32 || width == Width::W64);

    if (MathUtils::IsNBitsSigned(imm, 12)) {
        uint16_t immediate = static_cast<uint16_t>(imm & 0xfff);

        auto opcode = width == Width::W32 ? RT::Opcode::BINI32I : RT::Opcode::BINI64I;

        Encode(
            segment,
            RT::B4xi12rr {
                .opc = opcode,
                .xi12 =
                    XImm12 {
                        .imm4  = Imm4(op),
                        .imm12 = Imm12(immediate),
                    },
                .rr = { .x = d, .y = l },
            }
        );

    } else {
        Symbol immediate = symbols.Value(imm);

        RT::Opcode opcode = width == Width::W32 ? RT::Opcode::BINI32L : RT::Opcode::BINI64L;

        Encode(segment, opcode);
        AddFixup(std::make_unique<Literal12Fixup>(Imm4(op), immediate));
        Encode(segment, RR { .x = d, .y = l });
    }
}

void Emitter::AddI(Width width, IReg d, IReg l, uint64_t imm) { BinaryImm(Common::ADD, width, d, l, imm); }

void Emitter::SubI(Width width, IReg d, IReg l, uint64_t imm) { BinaryImm(Common::SUB, width, d, l, imm); }

void Emitter::MulI(Width width, IReg d, IReg l, uint64_t imm) { BinaryImm(Common::MUL, width, d, l, imm); }

void Emitter::AndI(Width width, IReg d, IReg l, uint64_t imm) { BinaryImm(Common::AND, width, d, l, imm); }

void Emitter::OrI(Width width, IReg d, IReg l, uint64_t imm) { BinaryImm(Common::OR, width, d, l, imm); }

void Emitter::XorI(Width width, IReg d, IReg l, uint64_t imm) { BinaryImm(Common::XOR, width, d, l, imm); }

void Emitter::DivI(Width width, IReg d, IReg l, uint64_t imm) { BinaryImm(Common::SDIV, width, d, l, imm); }

void Emitter::RemI(Width width, IReg d, IReg l, uint64_t imm) { BinaryImm(Common::SREM, width, d, l, imm); }

void Emitter::UDivI(Width width, IReg d, IReg l, uint64_t imm) { BinaryImm(Common::UDIV, width, d, l, imm); }

void Emitter::URemI(Width width, IReg d, IReg l, uint64_t imm) { BinaryImm(Common::UREM, width, d, l, imm); }

void Emitter::LslI(Width width, IReg d, IReg l, uint64_t imm) { BinaryImm(Common::LSL, width, d, l, imm); }

void Emitter::LsrI(Width width, IReg d, IReg l, uint64_t imm) { BinaryImm(Common::LSR, width, d, l, imm); }

void Emitter::AsrI(Width width, IReg d, IReg l, uint64_t imm) { BinaryImm(Common::ASR, width, d, l, imm); }

void Emitter::Binary(FloatOperations op, Width width, FReg d, FReg l, FReg r)
{
    ASSERT(width == Width::W32 || width == Width::W64);
    ASSERT(op.IsBasic());

    auto opcode = width == Width::W32 ? RT::Opcode::FBIN32 : RT::Opcode::FBIN64;

    Encode(
        segment,
        RT::B3xrrr {
            .opc = opcode,
            .xr =
                XR {
                    .imm = Imm4(op),
                    .r   = d,
                },
            .rr = { .x = l, .y = r },
        }
    );
}

void Emitter::Add(Width width, FReg d, FReg l, FReg r) { Binary(FloatOperations::FADD, width, d, l, r); }

void Emitter::Sub(Width width, FReg d, FReg l, FReg r) { Binary(FloatOperations::FSUB, width, d, l, r); }

void Emitter::Mul(Width width, FReg d, FReg l, FReg r) { Binary(FloatOperations::FMUL, width, d, l, r); }

void Emitter::Div(Width width, FReg d, FReg l, FReg r) { Binary(FloatOperations::FDIV, width, d, l, r); }

void Emitter::Mov(Width width, FReg d, FReg l, FReg r) { Binary(FloatOperations::FMOV, width, d, l, r); }

void Emitter::Neg(Width width, FReg d, FReg l, FReg r) { Binary(FloatOperations::FNEG, width, d, l, r); }

void Emitter::Abs(Width width, FReg d, FReg l, FReg r) { Binary(FloatOperations::FABS, width, d, l, r); }

void Emitter::Sqrt(Width width, FReg d, FReg l, FReg r) { Binary(FloatOperations::FSQRT, width, d, l, r); }

void Emitter::Unary(FloatOperations op, Width width, FReg d, FReg s)
{
    ASSERT(width == Width::W32 || width == Width::W64);

    auto opcode = width == Width::W32 ? RT::Opcode::FUN32 : RT::Opcode::FUN64;

    Encode(
        segment,
        RT::B3xrrr {
            .opc = opcode,
            .xr =
                XR {
                    .imm = Imm4(op),
                    .r   = d,
                },
            .rr = { .x = d, .y = s },
        }
    );
}

void Emitter::Sqrt(Width width, FReg d, FReg s) { Unary(FloatOperations::FSQRT, width, d, s); }

void Emitter::Abs(Width width, FReg d, FReg s) { Unary(FloatOperations::FABS, width, d, s); }

void Emitter::Neg(Width width, FReg d, FReg s) { Unary(FloatOperations::FNEG, width, d, s); }

void Emitter::Mov(RT::Opcode opcode, Reg d, Reg s)
{
    Encode(segment, RT::B2rr { .opc = opcode, .rr = RR { .x = d, .y = s } });
}

void Emitter::Mov(IReg d, IReg s) { Emitter::Mov(RT::Opcode::MOV, d, s); }

void Emitter::Mov(FReg d, FReg s) { Emitter::Mov(RT::Opcode::FMOV, d, s); }

void Emitter::Mov(FReg d, IReg s) { Emitter::Mov(RT::Opcode::MOVI2F, d, s); }

void Emitter::Mov(IReg d, FReg s) { Emitter::Mov(RT::Opcode::MOVF2I, d, s); }

void Emitter::MovImm(Width width, IReg d, uint64_t imm)
{
    ASSERT(width == Width::W32 || width == Width::W64);

    bool isImm = MathUtils::IsNBitsSigned(imm, 4);
    if (isImm) {
        uint8_t immediate = static_cast<uint8_t>(imm & 0xf);

        Encode(segment, RT::B2xr { .opc = RT::Opcode::MOVI, .xr = XR { .imm = Imm4(immediate), .r = d } });

    } else {
        AddI(width, d, IReg::IRZ, imm);
    }
}

void Emitter::FMovI32(FReg d, float imm)
{
    Encode(
        segment,
        RT::B6xri32 { .opc = RT::Opcode::FMOVI32, .xr = XR { .imm = 0, .r = d }, .imm32 = Imm32 { .fimm = imm } }
    );
}

void Emitter::FMovI64(FReg d, double imm)
{
    Encode(
        segment,
        RT::B10xri64 { .opc = RT::Opcode::FMOVI64, .xr = XR { .imm = 0, .r = d }, .imm64 = Imm64 { .dimm = imm } }
    );
}

void Emitter::Bcc(CC cc, Width width, Reg l, Reg r, Label label)
{
    ASSERT(width == Width::W32 || width == Width::W64);
    AddFixup(std::make_unique<BccFixup>(label, cc, width, l, r));
}

void Emitter::BranchIfRef(IReg typeInfo, Label label) { AddFixup(std::make_unique<BrIfRef>(label, typeInfo)); }

void Emitter::BccImm(CC cc, Width width, IReg l, uint64_t r, Label label)
{
    ASSERT(width == Width::W32 || width == Width::W64);
    AddFixup(std::make_unique<BccImmFixup>(label, cc, width, l, r));
}

void Emitter::Nop() { Encode(segment, RT::B1 { RT::Opcode::NOP }); }

void Emitter::Jmp(Label label) { AddFixup(std::make_unique<JmpFixup>(label)); }

void Emitter::Ret() { Encode(segment, RT::B1 { RT::Opcode::RET }); }

void Emitter::NewObjGenericOnAcc(IReg ti) { Encode(segment, RT::B2rr { .opc = RT::Opcode::NEWOBJ_G, .rr = { ti, ti } }); }

void Emitter::NewObj(RTSupport::TypeInfo typeInfo)
{
    Encode(
        segment, RT::B9i64 { .opc = RT::Opcode::NEWOBJ, .imm64 = { .imm = reinterpret_cast<uint64_t>(typeInfo.Raw()) } }
    );
}

void Emitter::NewArr(RTSupport::TypeInfo typeInfo)
{
    Encode(
        segment, RT::B9i64 { .opc = RT::Opcode::NEWARR, .imm64 = { .imm = reinterpret_cast<uint64_t>(typeInfo.Raw()) } }
    );
}

void Emitter::InitClosure() { Encode(segment, RT::B1 { RT::Opcode::INITCLOSURE }); }

void Emitter::Spawn(RTSupport::TypeInfo typeInfo)
{
    Encode(
        segment, RT::B9i64 { .opc = RT::Opcode::SPAWN, .imm64 = { .imm = reinterpret_cast<uint64_t>(typeInfo.Raw()) } }
    );
}

void Emitter::LoadStatic(LoadAccessKind ldk, Reg dst, Symbol offSym)
{
    LoadAccessKind::Value kind = ldk;
    Encode(segment, RT::B2xr { .opc = RT::Opcode::LOAD_ADDR, .xr = { .imm = kind, .r = dst } });
    AddFixup(std::make_unique<LiteralFixup>(offSym));
}

void Emitter::StoreStatic(StoreAccessKind sdk, Reg src, Symbol offSym)
{
    StoreAccessKind::Value kind = sdk;
    Encode(segment, RT::B2xr { .opc = RT::Opcode::STORE_ADDR, .xr = { .imm = kind, .r = src } });
    AddFixup(std::make_unique<LiteralFixup>(offSym));
}

void Emitter::LoadObj(LoadAccessKind ldk, Reg dst, IReg base, uint32_t offset)
{
    if (MathUtils::IsNBits(offset, 12)) {
        auto opc = !ldk.IsFloat() ? RT::Opcode::LOAD_OBJ : RT::Opcode::LOAD_OBJ_F;
        Encode(segment, RT::B4xi12rr {
            .opc = opc,
            .xi12 = {
                .imm4 = Imm4(ldk),
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

void Emitter::StoreObj(StoreAccessKind stk, Reg src, IReg base, uint32_t offset)
{
    if (MathUtils::IsNBits(offset, 12)) {
        auto opc = !stk.IsFloat() ? RT::Opcode::STORE_OBJ : RT::Opcode::STORE_OBJ_F;
        Encode(segment, RT::B4xi12rr {
            .opc = opc,
            .xi12 = {
                .imm4 = Imm4(stk),
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

void Emitter::LoadArray(LoadAccessKind ldk, Reg dst, IReg base, IReg idx)
{
    auto opc = !ldk.IsFloat() ? RT::Opcode::LOAD_ARR : RT::Opcode::LOAD_ARR_F;
    Encode(segment, RT::B3xrrr {
        .opc = opc,
        .xr = {
            .imm = Imm4(ldk),
            .r = dst,
        },
        .rr = {
            .x = base,
            .y = idx,
        }
    });
}

void Emitter::StoreArray(StoreAccessKind stk, Reg src, IReg base, IReg idx)
{
    auto opc = !stk.IsFloat() ? RT::Opcode::STORE_ARR : RT::Opcode::STORE_ARR_F;
    Encode(segment, RT::B3xrrr {
        .opc = opc,
        .xr = {
            .imm = Imm4(stk),
            .r = src,
        },
        .rr = {
            .x = base,
            .y = idx,
        }
    });
}

void Emitter::TypeArg(IReg dst, IReg typeInfo, int idx)
{
    RT::B4xi12rr command = {
        // FIXME: use i16
        .opc  = RT::Opcode::TYPE_ARG,
        .xi12 = { 0, idx },
        .rr   = { dst, typeInfo },
    };
    Encode(segment, command);
}

void Emitter::LoadRec(LoadAccessKind ldk, Reg dst, IReg base, uint32_t offset)
{
    if (MathUtils::IsNBits(offset, 12)) {
        auto opc = !ldk.IsFloat() ? RT::Opcode::LOAD_REC : RT::Opcode::LOAD_REC_F;
        LoadStore(ldk, dst, base, offset, opc);
    } else {
        auto ms = OpenMemSpace();
        ms.Offset(offset);
        ms.LoadRec(ldk, dst, base);
    }
}

void Emitter::StoreRec(StoreAccessKind stk, Reg src, IReg base, uint32_t offset)
{
    if (MathUtils::IsNBits(offset, 12)) {
        auto opc = !stk.IsFloat() ? RT::Opcode::STORE_REC : RT::Opcode::STORE_REC_F;
        LoadStore(stk, src, base, offset, opc);
    } else {
        auto ms = OpenMemSpace();
        ms.Offset(offset);
        ms.StoreRec(stk, src, base);
    }
}

void Emitter::LoadFrame(LoadAccessKind ldk, Reg dst, uint32_t offset)
{
    if (MathUtils::IsNBits(offset, 12)) {
        auto opc = !ldk.IsFloat() ? RT::Opcode::LOAD_FRAME : RT::Opcode::LOAD_FRAME_F;
        LoadStore(ldk, dst, IReg::IRZ, offset, RT::Opcode::LOAD_FRAME);
    } else {
        auto ms = OpenMemSpace();
        ms.Offset(offset);
        ms.LoadFrame(ldk, dst);
    }
}

void Emitter::StoreFrame(StoreAccessKind stk, Reg src, uint32_t offset)
{
    if (MathUtils::IsNBits(offset, 12)) {
        auto opc = !stk.IsFloat() ? RT::Opcode::STORE_FRAME : RT::Opcode::STORE_FRAME_F;
        LoadStore(stk, src, IReg::IRZ, offset, opc);
    } else {
        auto ms = OpenMemSpace();
        ms.Offset(offset);
        ms.StoreFrame(stk, src);
    }
}

void Emitter::StoreFrameImm(StoreAccessKind stk, uint64_t imm, uint32_t offset)
{
    auto ms = OpenMemSpace();
    ms.Offset(offset);
    ms.StoreFrameImm(stk, imm);
}

void Emitter::PrepareTyped(RTSupport::TypeInfo typeInfo, uint32_t offset)
{
    Encode(segment, RT::B13i64i32 {
        .opc = RT::Opcode::PREP_TYPED,
        .imm64 = { .imm = reinterpret_cast<uint64_t>(typeInfo.Raw()) },
        .imm32 = { .imm = offset }
    });
}

void Emitter::SCC(CC cc, Width width, IReg d, IReg l, IReg r)
{
    ASSERT(width == Width::W32 || width == Width::W64);
    auto opc = width == Width::W32 ? RT::Opcode::SCC32 : RT::Opcode::SCC64;
    Encode(segment, RT::B3xrrr {
        .opc = opc,
        .xr = {
            .imm = cc,
            .r = d,
        },
        .rr = {
            .x = l,
            .y = r,
        },
    });
}

void Emitter::SCC(CC cc, Width width, IReg d, FReg l, FReg r)
{
    ASSERT(width == Width::W32 || width == Width::W64);
    auto opc = width == Width::W32 ? RT::Opcode::FSCC32 : RT::Opcode::FSCC64;
    Encode(segment, RT::B3xrrr {
        .opc = opc,
        .xr = {
            .imm = cc,
            .r = d,
        },
        .rr = {
            .x = l,
            .y = r,
        },
    });
}

void Emitter::SCCImm(CC cc, Width width, IReg d, IReg l, uint64_t imm)
{
    ASSERT(width == Width::W32 || width == Width::W64);

    bool isImm = MathUtils::IsNBitsSigned(imm, 12);
    if (isImm) {
        uint16_t immediate = static_cast<uint16_t>(imm & 0xfff);

        auto opcode = width == Width::W32 ? RT::Opcode::SCCI32I : RT::Opcode::SCCI64I;

        Encode(
            segment,
            RT::B4xi12rr {
                .opc = opcode,
                .xi12 =
                    XImm12 {
                        .imm4  = Imm4(cc),
                        .imm12 = Imm12(immediate),
                    },
                .rr = { .x = d, .y = l },
            }
        );

    } else {
        Symbol immediate = symbols.Value(imm);

        RT::Opcode opcode = width == Width::W32 ? RT::Opcode::SCCI32L : RT::Opcode::SCCI64L;

        Encode(segment, opcode);
        AddFixup(std::make_unique<Literal12Fixup>(Imm4(cc), immediate));
        Encode(segment, RR { .x = d, .y = l });
    }
}

void Emitter::Convert(ConvertType toType, ConvertType fromType, Reg to, Reg from)
{
    Encode(segment, RT::B3xxrr {
        .opc = RT::Opcode::CONVERT,
        .xx = {
            .imm1 = Imm4(toType),
            .imm2 = Imm4(fromType),
        },
        .rr = {
            .x = to,
            .y = from,
        },
    });
}

void Emitter::BFXS(IReg dst, IReg src, uint8_t offset, uint8_t size)
{
    BFX(RT::Opcode::BFXS, dst, src, offset, size);
}

void Emitter::BFXZ(IReg dst, IReg src, uint8_t offset, uint8_t size)
{
    BFX(RT::Opcode::BFXZ, dst, src, offset, size);
}

void Emitter::BFX(RT::Opcode opcode, IReg dst, IReg src, uint8_t offset, uint8_t size)
{
    Encode(segment, RT::BFX {
        .opc = opcode,
        .rr = {
            .x = dst,
            .y = src
        },
        .offs = offset,
        .size = size
    });
}

void Emitter::GcPoint()
{
    Encode(
        segment,
        RT::B1 {
            .opc = RT::Opcode::GC_POINT,
        }
    );
}

void Emitter::CallClosure(bool sret)
{
    if (sret) {
        segment.AddW8(RT::Opcode::CALL_CLOSURE_SRET);
    } else {
        segment.AddW8(RT::Opcode::CALL_CLOSURE);
    }
}

void Emitter::CallClosureGeneric() { segment.AddW8(RT::Opcode::CALL_CLOSURE_GENERIC); }

void Emitter::DirectCall2i(Symbol fuh)
{
    segment.AddW8(RT::Opcode::DIRECT_CALL_2I);
    Imm4 i4(0);
    AddFixup(std::make_unique<Literal12Fixup>(i4, fuh));
}

void Emitter::DirectCall2c(Symbol target)
{
    segment.AddW8(RT::Opcode::DIRECT_CALL_2C);
    Imm4 i4(0);
    AddFixup(std::make_unique<Literal12Fixup>(i4, target));
}

void Emitter::VirtualCall(uint16_t vnum, uint16_t extDefNum, bool sret)
{
    Encode(
        segment,
        RT::VirtualCall {
            .opc  = RT::Opcode::VIRTUAL_CALL,
            .vnum = vnum,
            .edef = extDefNum,
            .sret = static_cast<uint8_t>(sret),
        }
    );
}

void Emitter::InterfaceCall(uint16_t methodNum, RTSupport::TypeInfo typeInfo, bool sret)
{
    Encode(
        segment,
        RT::InterfaceCall {
            .opc  = RT::Opcode::INTERFACE_CALL,
            .vnum = methodNum,
            .ti   = reinterpret_cast<uint64_t>(typeInfo.Raw()),
            .sret = static_cast<uint8_t>(sret),
        }
    );
}

void Emitter::InterfaceCallGeneric(uint16_t methodNum, IReg interfaceTi, bool sret)
{
    Encode(
        segment,
        RT::InterfaceCallGeneric {
            .opc  = RT::Opcode::INTERFACE_CALL_GENERIC,
            .vnum = methodNum,
            .xr   = { sret, interfaceTi },
        }
    );
}

void Emitter::StringLit(Interpretation::StringStorage* literal, uint32_t frameOffs)
{
    Encode(
        segment,
        RT::B13i64i32 { .opc   = RT::Opcode::STRING_INIT,
                        .imm64 = { .imm = reinterpret_cast<uint64_t>(literal) },
                        .imm32 = { .imm = frameOffs } }
    );
}

void Emitter::DivCheck(IReg r)
{
    Encode(segment, RT::B2xr { .opc = RT::Opcode::DIVCHECK, .xr = { .imm = 0, .r = r } });
}

void Emitter::NullCheck(IReg r)
{
    Encode(segment, RT::B2xr { .opc = RT::Opcode::NULLCHECK, .xr = { .imm = 0, .r = r } });
}

void Emitter::InstanceOf(IReg dst, IReg obj, RTSupport::TypeInfo typeInfo)
{
    Encode(segment, RT::IOF { .opc = RT::Opcode::IOF, .rr = { .x = dst, .y = obj }, .imm64 = reinterpret_cast<uint64_t>(typeInfo.Raw()) });
}

void Emitter::Throw(IReg reg)
{
    Encode(segment, RT::B2xr { .opc = RT::Opcode::THROW, .xr = { .imm = 0, .r = reg } });
}

void Emitter::Catch(IReg reg) { Encode(segment, RT::B2xr { .opc = RT::Opcode::CATCH, .xr = { .imm = 0, .r = reg } }); }

void Emitter::LoadGenericTypeInfo(uintptr_t termData)
{
    Encode(segment, RT::B9i64 { .opc = RT::Opcode::LOAD_GENERIC_TI, .imm64 = { termData } });
}

void Emitter::LoadTypeInfo(RTSupport::TypeInfo typeInfo)
{
    auto d = reinterpret_cast<uintptr_t>(typeInfo.Raw());
    Encode(segment, RT::B9i64 { .opc = RT::Opcode::LOAD_TI, .imm64 = { d } });
}

void Emitter::NewBox(Interpretation::BuiltinType t)
{
    Encode(segment, RT::B2xr { .opc = RT::Opcode::NEWBOX, .xr = { .imm = t, .r = IReg::IRZ } });
}

void Emitter::NewBox(RTSupport::TypeInfo typeInfo)
{
    Encode(segment, RT::B9i64 { .opc = RT::Opcode::NEWBOX2, .imm64 = { reinterpret_cast<uint64_t>(typeInfo.Raw()) } });
}

void Emitter::Offset(IReg dst, int ordinal, IReg typeInfo)
{
    segment.AddW8(RT::Opcode::OFFSET);
    Encode(segment, Format::RR { dst, typeInfo });
    segment.AddW32(ordinal); // TODO: encode efficiently
}

void Emitter::ReadStructField(IReg dst, IReg base, IReg field, RTSupport::TypeInfo ti)
{
    RT::StructFieldOp command = {
        .opc = RT::Opcode::READ_STRUCT_FIELD, .rr = { dst, base }, .field = { field, field }, .ti = ti
    };
    Encode(segment, command);
}

void Emitter::WriteStructField(IReg src, IReg base, IReg field, RTSupport::TypeInfo ti)
{
    RT::StructFieldOp command = {
        .opc = RT::Opcode::WRITE_STRUCT_FIELD, .rr = { src, base }, .field = { field, field }, .ti = ti
    };
    Encode(segment, command);
}

} // namespace Emitter
} // namespace Cbc
