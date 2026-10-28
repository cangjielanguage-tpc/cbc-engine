#pragma once

#include "utils/vector.h"
#include <cstdint>
#include <memory>
#include <string_view>

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

class Symbols;

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

    void Binary(Common op, Width width, IReg d, IReg l, IReg r);
    void Add(Width width, IReg d, IReg l, IReg r);

    void BinaryImm(Common op, Width width, IReg d, IReg l, uint64_t imm);
    void AddI(Width width, IReg d, IReg l, uint64_t imm);

    void Binary(Checked op, Width width, IReg d, IReg l, IReg r);

    void BinaryImm(Checked op, Width width, IReg d, IReg l, uint64_t imm);

    void SatBinary(Format::Saturating op, Width width, IReg d, IReg l, IReg r);
    void SatBinaryImm(Format::Saturating op, Width width, IReg d, IReg l, uint64_t imm);

    void Binary(FloatOperations op, Width width, FReg d, FReg l, FReg r);

    void Unary(FloatOperations op, Width Width, FReg d, FReg s);
    void FMathUnary(FloatMathOp op, Width width, FReg d, FReg s);

    void Ret();
    void Mov(IReg d, IReg s);
    void Mov(FReg d, FReg s);
    void Mov(IReg d, FReg s);
    void Mov(FReg d, IReg s);
    void MovImm(Width width, IReg d, uint64_t imm);
    void FMovI32(FReg d, float imm);
    void FMovI64(FReg d, double imm);

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
    void NewArrGeneric(IReg ti);
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
    void StoreGeneric(IReg src, IReg baseRef, IReg base, IReg ti, uint32_t offset);
    void LeaGeneric(Reg dst, IReg base, IReg ti, uint32_t offset);
    void InitNoneGenericOpt0(IReg underlyingTi);
    void InitSomeGeneric(IReg src, IReg underlyingTi, bool opt0);
    void TypeArg(IReg dst, IReg typeInfo, int idx);
    void CopyDerived(IReg dstBase, IReg dst, IReg srcBase, IReg src, RTSupport::TypeInfo ti);
    void CopyDerivedGeneric(IReg dstBase, IReg dst, IReg srcBase, IReg src, IReg ti);
    void CopyLocal(IReg dst, IReg src, RTSupport::TypeInfo ti);
    void CopyToLocal(IReg dst, IReg srcBase, IReg src, RTSupport::TypeInfo ti);
    void CopyFromLocal(IReg dstBase, IReg dst, IReg src, RTSupport::TypeInfo ti);
    void CopyLocalGeneric(IReg dst, IReg src, IReg ti);
    void CopyToLocalGeneric(IReg dst, IReg srcBase, IReg src, IReg ti);
    void CopyFromLocalGeneric(IReg dstBase, IReg dst, IReg src, IReg ti);
    void CopyTypeSlot(int32_t dstOffset, int32_t srcOffset, RTSupport::TypeInfo ti);
    void CopyToTypeSlot(int32_t dstOffset, IReg srcBase, IReg src, RTSupport::TypeInfo ti);
    void CopyFromTypeSlot(IReg dstBase, IReg dst, int32_t srcOffset, RTSupport::TypeInfo ti);
    void CopyTypeSlotGeneric(int32_t dstOffset, int32_t srcOffset, IReg ti);
    void CopyToTypeSlotGeneric(int32_t dstOffset, IReg srcBase, IReg src, IReg ti);
    void CopyFromTypeSlotGeneric(IReg dstBase, IReg dst, int32_t srcOffset, IReg ti);
    void LeaIndex(IReg dst, IReg src, IReg idx, RTSupport::TypeInfo ti, bool isCangjeiArray);
    void LeaIndexGeneric(IReg dst, IReg src, IReg idx, IReg ti);
    void LeaConstIndexGeneric(IReg dst, IReg src, int64_t idx, IReg ti);

    void LoadFrame(LoadAccessKind ldk, Reg dst, int32_t offset);
    void StoreFrame(StoreAccessKind stk, Reg src, int32_t offset);
    void StoreFrameImm(StoreAccessKind stk, uint64_t imm, int32_t offset);

    void SCC(CC cc, Width width, IReg d, IReg l, IReg r);
    void SCC(CC cc, Width width, IReg d, FReg l, FReg r);
    void SCCImm(CC cc, Width width, IReg d, IReg l, uint64_t imm);

    void Convert(ConvertType toType, ConvertType fromType, Reg to, Reg from);
    void CheckedConvert(ConvertType toType, ConvertType fromType, Reg to, Reg from);

    void BFXS(IReg dst, IReg src, uint8_t offset, uint8_t size);
    void BFXZ(IReg dst, IReg src, uint8_t offset, uint8_t size);

    void GcPoint();

    void CallClosure(bool sret);
    void CallClosureGeneric();

    void DirectCall2i(Symbol fuh);
    void DirectCall2c(Symbol target);
    void DirectCall2n(Symbol target, uint8_t stackSlots, uint8_t vecRegCount);
    void CallCFunc(IReg src, uint8_t stackSlots, uint8_t vecRegCount);

    void VirtualCall(uint16_t vnum, uint16_t extDefNum, bool sret);
    void InterfaceCall(
        uint16_t methodNum, RTSupport::TypeInfo typeInfo, bool sret, Interpretation::StaticCallTypeInfoArgs typeInfoArgs
    );
    void InterfaceCallGeneric(uint16_t methodNum, bool sret, Interpretation::StaticCallTypeInfoArgs typeInfoArgs);

    void StringLit(Interpretation::StringStorage* literal, int32_t frameOffs);

    void DivCheck(IReg r);
    void NullCheck(IReg r);

    void InstanceOf(IReg dst, IReg obj, RTSupport::TypeInfo typeInfo);
    void LoadGenericTypeInfo(uintptr_t termData);

    void NewBox(Interpretation::BuiltinType t);
    void NewBox(RTSupport::TypeInfo typeInfo);
    void UnboxT(IReg src, int32_t frameOffs);
    void Offset(IReg dst, int ordinal, IReg typeInfo);

    void WriteStructField(IReg src, IReg base, uint32_t offset, RTSupport::TypeInfo ti);
    void ReadStructField(IReg dst, IReg base, uint32_t offset, RTSupport::TypeInfo ti);

    void AtomicLoad(IReg dst, Format::LoadAccessKind ldk, IReg obj, uint16_t offset);
    void AtomicStore(IReg src, Format::StoreAccessKind stk, IReg obj, uint16_t offset);

    void CAS(RT::Opcode opc, IReg dst, IReg obj, IReg expected, IReg newVal, uint16_t offset);
    void AtomicOp(RT::Opcode opc, IReg dst, IReg obj, IReg src, uint16_t offset);

    void Throw(IReg dst);
    void Catch(IReg dst);

    void AssignGeneric(IReg dst, IReg src, IReg ti);
    void InstanceOfGeneric(IReg dst, IReg obj, IReg ti);

    void LogInstruction(std::string_view string);
    void LogInstruction(char* string);

    void StackParam(IReg src, uint16_t offset);
    void StackParamF(FReg src, uint16_t dst);
    void StackParamS16(int16_t src, uint16_t dst);
    void StackParamS32(int32_t src, uint16_t dst);
    void LeaStackParam(IReg dst, uint16_t offset);

private:
    void AddFixup(std::unique_ptr<Fixup> fixup);
    void Mov(RT::Opcode opcode, Reg d, Reg s);
    void BFX(RT::Opcode opcode, IReg dst, IReg src, uint8_t offset, uint8_t size);

    template <typename AccessKind> void LoadStore(AccessKind akind, Reg v, IReg base, int32_t offset, RT::Opcode opc)
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
    void LoadStoreLong(AccessKind akind, Reg v, IReg baseRef, IReg base, int32_t offset, RT::Opcode opc)
    {
        Encode(
            segment,
            RT::B7xrrri32 { .opc   = opc,
                            .xr    = { .imm = Format::Imm4(akind), .r = v },
                            .rr    = { .x = baseRef, .y = base },
                            .imm32 = { .imm = static_cast<uint32_t>(offset) } }
        );
    }

    Symbols symbols;
    Segment segment;

    Utils::Vector<std::unique_ptr<Fixup>> fixups;
};

} // namespace Emitter
} // namespace Cbc
