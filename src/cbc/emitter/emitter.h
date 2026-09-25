#pragma once

#include <cstdint>
#include <memory>
#include <string_view>
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
    using Checked     = Format::Checked;

    using FloatOperations = Format::FloatOperations;
    using FloatMathOp     = Format::FloatMathOp;
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

        void CopyRecFromObj(Reg from, Reg to, RTSupport::TypeInfo);
        void CopyRecFromRec(Reg from, Reg to, RTSupport::TypeInfo);
        void CopyRecFromDerived(Reg base, Reg derived, Reg to, RTSupport::TypeInfo);

        void CopyRecToObj(Reg from, Reg to, RTSupport::TypeInfo);
        void CopyRecToRec(Reg from, Reg to, RTSupport::TypeInfo);
        void CopyRecToDerived(Reg base, Reg derived, Reg from, RTSupport::TypeInfo);

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

        void CopyFrom(Reg from, Reg to, RTSupport::TypeInfo ti, RT::MemOpcode opc);
        void CopyTo(Reg from, Reg to, RTSupport::TypeInfo ti, RT::MemOpcode opc);

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

    void Binary(Checked op, Width width, IReg d, IReg l, IReg r);
    void CAdd(Width width, IReg d, IReg l, IReg r);
    void CSub(Width width, IReg d, IReg l, IReg r);
    void CMul(Width width, IReg d, IReg l, IReg r);
    void CDiv(Width width, IReg d, IReg l, IReg r);
    void CUAdd(Width width, IReg d, IReg l, IReg r);
    void CUSub(Width width, IReg d, IReg l, IReg r);
    void CUMul(Width width, IReg d, IReg l, IReg r);
    void CPow(Width width, IReg d, IReg l, IReg r);

    void BinaryImm(Checked op, Width width, IReg d, IReg l, uint64_t imm);
    void CAddI(Width width, IReg d, IReg l, uint64_t imm);
    void CSubI(Width width, IReg d, IReg l, uint64_t imm);
    void CMulI(Width width, IReg d, IReg l, uint64_t imm);
    void CUAddI(Width width, IReg d, IReg l, uint64_t imm);
    void CUSubI(Width width, IReg d, IReg l, uint64_t imm);
    void CUMulI(Width width, IReg d, IReg l, uint64_t imm);
    void CPowI(Width width, IReg d, IReg l, uint64_t imm);

    void SatBinary(Format::Saturating op, Width width, IReg d, IReg l, IReg r);
    void SatBinaryImm(Format::Saturating op, Width width, IReg d, IReg l, uint64_t imm);
    void SatAdd(Width width, IReg d, IReg l, IReg r);
    void SatSub(Width width, IReg d, IReg l, IReg r);
    void SatMul(Width width, IReg d, IReg l, IReg r);
    void SatDiv(Width width, IReg d, IReg l, IReg r);
    void SatMod(Width width, IReg d, IReg l, IReg r);
    void SatPow(Width width, IReg d, IReg l, IReg r);
    void SatShl(Width width, IReg d, IReg l, IReg r);
    void SatShr(Width width, IReg d, IReg l, IReg r);

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
    void FMathUnary(FloatMathOp op, Width width, FReg d, FReg s);
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

    void BranchIfRef(IReg typeInfo, Label label);
    void Bcc(CC cc, Width width, Reg l, Reg r, Label label);
    void BccImm(CC cc, Width width, IReg l, uint64_t r, Label label);
    void Nop();
    void Jmp(Label label);

    void InitClosure(bool instantiatedSret);
    void Spawn(RTSupport::TypeInfo typeInfo);
    void SpawnFuture();

    void NewObjGeneric(IReg ti, bool pinned = false);
    void NewObjGenericOnAcc(IReg ti);
    void NewObj(RTSupport::TypeInfo typeInfo, bool pinned = false);
    void NewArr(RTSupport::TypeInfo typeInfo);
    void LoadObj(LoadAccessKind ldk, Reg dst, IReg base, uint32_t offset);
    void StoreObj(StoreAccessKind stk, Reg src, IReg base, uint32_t offset);
    void LoadArray(LoadAccessKind ldk, Reg dst, IReg base, IReg idx);
    void StoreArray(StoreAccessKind stk, Reg src, IReg base, IReg idx);
    void LoadStatic(LoadAccessKind ldk, Reg dst, Symbol offSym);
    void StoreStatic(StoreAccessKind sdk, Reg src, Symbol offSym);
    void LoadRec(LoadAccessKind ldk, Reg dst, IReg base, uint32_t offset);
    void StoreRec(StoreAccessKind stk, Reg src, IReg base, uint32_t offset);
    void LoadDerived(LoadAccessKind ldk, Reg dst, IReg baseRef, IReg base, uint32_t offset);
    void StoreDerived(StoreAccessKind stk, Reg src, IReg baseRef, IReg base, uint32_t offset);
    void LoadGeneric(Reg dst, IReg baseRef, IReg base, IReg ti);
    void StoreGeneric(Reg src, IReg baseRef, IReg base, IReg ti);
    void LeaGeneric(Reg dst, IReg base, IReg ti, uint32_t offset);
    void TypeArg(IReg dst, IReg typeInfo, int idx);
    void CopyDerived(IReg dstBase, IReg dst, IReg srcBase, IReg src, RTSupport::TypeInfo ti);
    void CopyDerivedGeneric(IReg dstBase, IReg dst, IReg srcBase, IReg src, IReg ti);
    void LeaIndex(IReg dst, IReg src, IReg idx, RTSupport::TypeInfo ti, bool isCangjeiArray);
    void LeaIndexGeneric(IReg dst, IReg src, IReg idx, IReg ti);

    void LoadFrame(LoadAccessKind ldk, Reg dst, uint32_t offset);
    void StoreFrame(StoreAccessKind stk, Reg src, uint32_t offset);
    void StoreFrameImm(StoreAccessKind stk, uint64_t imm, uint32_t offset);

    void PrepareTyped(uint64_t size, uint32_t offset);

    void SCC(CC cc, Width width, IReg d, IReg l, IReg r);
    void SCC(CC cc, Width width, IReg d, FReg l, FReg r);
    void SCCImm(CC cc, Width width, IReg d, IReg l, uint64_t imm);

    void Convert(ConvertType toType, ConvertType fromType, Reg to, Reg from);

    void BFXS(IReg dst, IReg src, uint8_t offset, uint8_t size);
    void BFXZ(IReg dst, IReg src, uint8_t offset, uint8_t size);

    void GcPoint();

    void CallClosure(bool sret);
    void CallClosureGeneric();

    void DirectCall2i(Symbol fuh);
    void DirectCall2c(Symbol target);

    void VirtualCall(uint16_t vnum, uint16_t extDefNum, bool sret);
    void InterfaceCall(uint16_t methodNum, RTSupport::TypeInfo typeInfo, bool sret);
    void InterfaceCallGeneric(uint16_t methodNum, uint16_t argnum, bool sret);

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

    void AtomicLoad(IReg dst, Format::LoadAccessKind ldk, IReg obj, uint16_t offset);
    void AtomicStore(IReg src, Format::StoreAccessKind stk, IReg obj, uint16_t offset);

    void CAS(IReg dst, Width width, IReg obj, IReg src1, IReg src2, uint16_t offset);
    void CASRef(IReg dst, IReg obj, IReg src1, IReg src2, uint16_t offset);
    void CAS(RT::Opcode opc, IReg dst, IReg obj, IReg expected, IReg newVal, uint16_t offset);
    void AtomicOp(RT::Opcode opc, IReg dst, IReg obj, IReg src, uint16_t offset);

    void Throw(IReg dst);
    void Catch(IReg dst);

    void AssignGeneric(IReg dst, IReg src, IReg ti);
    void InstanceOfGeneric(IReg dst, IReg obj, IReg ti);

    void LogInstruction(std::string_view string);
    void LogInstruction(char* string);

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

    template <typename AccessKind>
    void LoadStoreLong(AccessKind akind, Reg v, IReg baseRef, IReg base, uint32_t offset, RT::Opcode opc)
    {
        Encode(
            segment,
            RT::B7xrrri32 { .opc   = opc,
                            .xr    = { .imm = Format::Imm4(akind), .r = v },
                            .rr    = { .x = baseRef, .y = base },
                            .imm32 = { .imm = offset } }
        );
    }

    Symbols symbols;
    Segment segment;

    std::vector<std::unique_ptr<Fixup>> fixups;
};

} // namespace Emitter
} // namespace Cbc
