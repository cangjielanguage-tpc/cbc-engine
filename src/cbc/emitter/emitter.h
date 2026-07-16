#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "cbc/emitter/segment.h"
#include "cbc/emitter/symbols.h"
#include "cbc/isa.h"
#include "encoding_rt.h"
#include "interpreter/code.h"
#include "interpreter/interpretation_loop.h"
#include "interpreter/literals.h"
#include "runtimesupport/runtime.h"
#include "utils/heap.h"

namespace Cbc {
namespace Emitter {

class CbcEmitter;
class Symbols;

struct EmitterSnapshot {
    SegmentSnapshot segmentSnapshot;
    size_t fixupCount;
};

class Emitter {
public:
    using Reg         = Format::Reg;
    using Width       = Format::Width;
    using CC          = Format::CC;
    using Common      = Format::Common;
    using ConvertType = Format::ConvertType;

    using FloatOperations = Format::FloatOperations;
    using LoadAccessKind  = Format::LoadAccessKind;
    using StoreAccessKind = Format::StoreAccessKind;

    class MemSpace {
    public:
        MemSpace(Emitter& _emitter) : segment(_emitter.segment), symbols(_emitter.symbols), emitter(_emitter) {}

        void Offset(uint64_t offset);
        void OffsetReg(IReg reg);
        void OffsetRegIdx(IReg reg, uint64_t size);

        // tail instructions
        void WriteStructFieldObj(IReg src, IReg base, RTSupport::TypeInfo structTypeInfo);
        void ReadStructFieldObj(IReg dst, IReg base, RTSupport::TypeInfo structTypeInfo);

        void LoadObj(LoadAccessKind ldk, Reg dst, IReg base);
        void StoreObj(StoreAccessKind stk, Reg src, IReg base);
        void StoreObjImm(StoreAccessKind stk, Reg base, uint64_t imm);

        void LoadDerived(LoadAccessKind ldk, Reg dst, IReg base, IReg derived);
        void StoreDerived(StoreAccessKind stk, Reg src, IReg base, IReg derived);
        void StoreDerivedImm(StoreAccessKind stk, IReg base, IReg derived, uint64_t imm);

        void LoadRec(LoadAccessKind ldk, Reg dst, IReg base);
        void StoreRec(StoreAccessKind stk, Reg src, IReg base);
        void StoreRecImm(StoreAccessKind stk, Reg base, uint64_t imm);

        void LoadFrame(LoadAccessKind ldk, Reg dst);
        void StoreFrame(StoreAccessKind stk, Reg src);
        void StoreFrameImm(StoreAccessKind stk, uint64_t imm);

        void LoadGeneric(IReg dst, IReg base, IReg typeInfo);
        void StoreGeneric(IReg src, IReg base, IReg typeInfo);
        void LoadDerivedGeneric(IReg dst, IReg base, IReg derived, IReg typeInfo);
        void StoreDerivedGeneric(IReg src, IReg base, IReg derived, IReg typeInfo);

        void GenericField(int ordinal, IReg typeInfo);

    private:
        template <typename AccessKind> void LoadStore(AccessKind akind, Reg v, IReg base, RT::MemOpcode opc)
        {
            Encode(
                segment,
                RT::M2rr {
                    .opc = opc,
                    .rr  = Format::RR { .x = v, .y = base },
                }
            );
        }

        Segment& segment;
        Symbols& symbols;
        Emitter& emitter;
    };

    Emitter() = default;

    Symbol NewAddressSym(uintptr_t ptr);
    Label NewLabel();
    void Bind(Label label);
    // TODO: add symbol kind to store arbitrary-size values.

    int32_t LabelPosition(Label label) const;

    /// Build `Code` in given `heap`.
    ///
    /// This procedure resolves all existring fixups and
    /// creates literal table (unused symbols or fixups will be discarded).
    Interpretation::Code Build(Memory::Heap& heap);

    EmitterSnapshot Snapshot();
    void Apply(EmitterSnapshot snapshot);

    void Binary(Common op, Width width, IReg d, IReg l, IReg r);
    void Add(Width width, IReg d, IReg l, IReg r);
    void Sub(Width width, IReg d, IReg l, IReg r);
    void Mul(Width width, IReg d, IReg l, IReg r);
    void And(Width width, IReg d, IReg l, IReg r);
    void Or(Width width, IReg d, IReg l, IReg r);
    void Xor(Width width, IReg d, IReg l, IReg r);
    void Div(Width width, IReg d, IReg l, IReg r);
    void Rem(Width width, IReg d, IReg l, IReg r);
    void UDiv(Width width, IReg d, IReg l, IReg r);
    void URem(Width width, IReg d, IReg l, IReg r);
    void Lsl(Width width, IReg d, IReg l, IReg r);
    void Lsr(Width width, IReg d, IReg l, IReg r);
    void Asr(Width width, IReg d, IReg l, IReg r);
    void Neg(Width width, IReg d, IReg s);

    void BinaryImm(Common op, Width width, IReg d, IReg l, uint64_t imm);
    void AddI(Width width, IReg d, IReg l, uint64_t imm);
    void SubI(Width width, IReg d, IReg l, uint64_t imm);
    void MulI(Width width, IReg d, IReg l, uint64_t imm);
    void AndI(Width width, IReg d, IReg l, uint64_t imm);
    void OrI(Width width, IReg d, IReg l, uint64_t imm);
    void XorI(Width width, IReg d, IReg l, uint64_t imm);
    void DivI(Width width, IReg d, IReg l, uint64_t imm);
    void RemI(Width width, IReg d, IReg l, uint64_t imm);
    void UDivI(Width width, IReg d, IReg l, uint64_t imm);
    void URemI(Width width, IReg d, IReg l, uint64_t imm);
    void LslI(Width width, IReg d, IReg l, uint64_t imm);
    void LsrI(Width width, IReg d, IReg l, uint64_t imm);
    void AsrI(Width width, IReg d, IReg l, uint64_t imm);

    void Binary(FloatOperations op, Width width, FReg d, FReg l, FReg r);
    void Add(Width width, FReg d, FReg l, FReg r);
    void Sub(Width width, FReg d, FReg l, FReg r);
    void Mul(Width width, FReg d, FReg l, FReg r);
    void Div(Width width, FReg d, FReg l, FReg r);
    void Mov(Width width, FReg d, FReg l, FReg r);
    void Neg(Width width, FReg d, FReg l, FReg r);
    void Abs(Width width, FReg d, FReg l, FReg r);
    void Sqrt(Width width, FReg d, FReg l, FReg r);

    void Unary(FloatOperations op, Width Width, FReg d, FReg s);
    void Sqrt(Width width, FReg d, FReg s);
    void Abs(Width width, FReg d, FReg s);
    void Neg(Width width, FReg d, FReg s);

    void Ret();
    void Mov(IReg d, IReg s);
    void Mov(FReg d, FReg s);
    void Mov(IReg d, FReg s);
    void Mov(FReg d, IReg s);
    void MovImm(Width width, IReg d, uint64_t imm);
    void FMovI32(FReg d, float imm);
    void FMovI64(FReg d, double imm);
    void MovRef(IReg d, IReg s);

    void Bcc(CC cc, Width width, Reg l, Reg r, Label label);
    void BccImm(CC cc, Width width, IReg l, uint64_t r, Label label);
    void Nop();
    void Jmp(Label label);

    void InitClosure(bool instantiatedSret);
    void Spawn(RTSupport::TypeInfo typeInfo);

    void NewObj(RTSupport::TypeInfo typeInfo);
    void NewArr(RTSupport::TypeInfo typeInfo);
    void LoadObj(LoadAccessKind ldk, Reg dst, IReg base, uint32_t offset);
    void StoreObj(StoreAccessKind stk, Reg src, IReg base, uint32_t offset);
    void LoadArray(LoadAccessKind ldk, Reg dst, IReg base, IReg idx);
    void StoreArray(StoreAccessKind stk, Reg src, IReg base, IReg idx);
    void LoadStatic(LoadAccessKind ldk, Reg dst, Symbol offSym);
    void StoreStatic(StoreAccessKind sdk, Reg src, Symbol offSym);
    void LoadRec(LoadAccessKind ldk, Reg dst, IReg base, uint32_t offset);
    void StoreRec(StoreAccessKind stk, Reg src, IReg base, uint32_t offset);
    void TypeArg(IReg dst, IReg typeInfo, int idx);

    void LoadFrame(LoadAccessKind ldk, Reg dst, uint32_t offset);
    void StoreFrame(StoreAccessKind stk, Reg src, uint32_t offset);
    void StoreFrameImm(StoreAccessKind stk, uint64_t imm, uint32_t offset);

    void PrepareTyped(RTSupport::TypeInfo typeInfo, uint32_t offset);

    void SCC(CC cc, Width width, IReg d, IReg l, IReg r);
    void SCC(CC cc, Width width, IReg d, FReg l, FReg r);
    void SCCImm(CC cc, Width width, IReg d, IReg l, uint64_t imm);

    void Convert(ConvertType toType, ConvertType fromType, Reg to, Reg from);

    void BFXS(IReg dst, IReg src, uint8_t offset, uint8_t size);
    void BFXZ(IReg dst, IReg src, uint8_t offset, uint8_t size);

    void GcPoint();

    void DirectCall2i(Symbol fuh);
    void DirectCall2c(Symbol target);

    void VirtualCall(uint16_t vnum, uint16_t extDefNum, bool sret);
    void InterfaceCall(uint16_t methodNum, RTSupport::TypeInfo typeInfo, bool sret);

    void StringLit(Interpretation::StringStorage* literal, uint32_t frameOffs);

    void DivCheck(IReg r);
    void NullCheck(IReg r);

    void InstanceOf(IReg dst, IReg obj, RTSupport::TypeInfo typeInfo);
    void LoadGenericTypeInfo(uintptr_t termData);
    void LoadTypeInfo(RTSupport::TypeInfo typeInfo);

    void NewBox(Interpretation::BuiltinType t);
    void NewBox(RTSupport::TypeInfo typeInfo);
    void Offset(IReg dst, int ordinal, IReg typeInfo);

    void WriteStructField(IReg src, IReg base, IReg field, RTSupport::TypeInfo ti);
    void ReadStructField(IReg dst, IReg base, IReg field, RTSupport::TypeInfo ti);

    void Throw(IReg dst);
    void Catch(IReg dst);

    MemSpace OpenMemSpace();

private:
    void AddFixup(std::unique_ptr<Fixup> fixup);
    void Mov(RT::Opcode opcode, Reg d, Reg s);
    void BFX(RT::Opcode opcode, IReg dst, IReg src, uint8_t offset, uint8_t size);

    template <typename AccessKind> void LoadStore(AccessKind akind, Reg v, IReg base, uint32_t offset, RT::Opcode opc)
    {
        Encode(segment, RT::B4xi12rr {
            .opc = opc,
            .xi12 = {
                .imm4 = Format::Imm4(akind),
                .imm12 = static_cast<uint16_t>(offset),
            },
            .rr = {
                .x = v,
                .y = base,
            }
        });
    }

    Symbols symbols;
    Segment segment;

    std::vector<std::unique_ptr<Fixup>> fixups;
};

} // namespace Emitter
} // namespace Cbc
