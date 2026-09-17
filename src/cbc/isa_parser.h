#pragma once

#include <cstdint>

#include "cbc/decoder.h"
#include "cbc/isa.h"
#include "engine/terms.h"

namespace Cbc {

using MethodCode = Image::Code;

class IsaParser {
public:
    IsaParser(Cbc::MethodCode& code);
    IsaParser(Decoder::FatByteReader reader);
    IsaParser(uint8_t* start, uint8_t* end);

    virtual ~IsaParser() = default;

    using AnyReg = uint8_t;

    virtual void ParseOne();
    virtual void End();
    void ParseAll();

protected:
    virtual void Bcc(Format::Width width, Format::CC cc, AnyReg l, AnyReg r, int64_t delta)          = 0;
    virtual void BccImm(Format::Width width, Format::CC cc, IReg l, uint64_t imm, int64_t delta)     = 0;
    virtual void Jump(int64_t delta)                                                                 = 0;
    virtual void Nop()                                                                               = 0;
    virtual void Mov(Format::Width width, IReg d, IReg s)                                            = 0;
    virtual void FMov(Format::Width width, FReg d, FReg s)                                           = 0;
    virtual void FloatToInt(Format::Width width, IReg d, FReg s)                                     = 0;
    virtual void IntToFloat(Format::Width width, FReg d, IReg s)                                     = 0;
    virtual void MovRef(IReg d, IReg s)                                                              = 0;
    virtual void MovImm(Format::Width width, IReg d, uint64_t value)                                 = 0;
    virtual void Binary(Format::Common op, Format::Width width, IReg d, IReg l, IReg r)              = 0;
    virtual void BinaryImm(Format::Common op, Format::Width width, IReg d, IReg l, uint64_t value)   = 0;
    virtual void CBinary(Format::Checked op, Format::Width width, IReg d, IReg l, IReg r)            = 0;
    virtual void CBinaryImm(Format::Checked op, Format::Width width, IReg d, IReg l, uint64_t value) = 0;
    virtual void SBinary(Format::Saturating op, Format::Width width, IReg d, IReg l, IReg r)         = 0;
    virtual void SBinaryImm(Format::Saturating op, Format::Width width, IReg d, IReg l, uint64_t value) = 0;
    virtual void FMovImm(Format::Width width, FReg d, double value)                                  = 0;
    virtual void FBinary(Format::FloatOperations op, Format::Width width, FReg d, FReg l, FReg r)    = 0;
    virtual void FUnary(Format::FloatOperations op, Format::Width width, FReg d, FReg s)             = 0;

    virtual void Convert(Format::ConvertType toType, Format::ConvertType fromType, AnyReg to, AnyReg from) = 0;

    virtual void MovBasePtr(IReg dst, bool local) = 0;

    virtual void BFX(
        IReg dst, IReg src, Format::Width resW, Format::Width argW, bool sx, uint8_t offset, uint8_t size
    ) = 0;

    virtual void PrepareRecord(uint16_t ts)                = 0;
    virtual void NewArr(IReg dst, IReg len, uint32_t type) = 0;

    virtual void GcPoint() = 0;

    virtual void LoadTailParam(AnyReg dst, IReg tailReg, int64_t number, Format::LoadAccessKind ldk) = 0;
    virtual void LoadRawMemory(AnyReg dst, IReg base, int64_t offset, Format::LoadAccessKind ldk)    = 0;
    virtual void StoreRawMemory(AnyReg src, IReg base, int64_t offset, Format::StoreAccessKind stk)  = 0;

    virtual void LoadStackRec(IReg r, uint16_t ts)              = 0;
    virtual void LoadStatic(AnyReg r, uint32_t field)           = 0;
    virtual void StoreStatic(AnyReg r, uint32_t field)          = 0;
    virtual void LoadField(IReg rb, AnyReg rs, uint32_t field)  = 0;
    virtual void StoreField(IReg rb, AnyReg rd, uint32_t field) = 0;

    virtual void AssignGeneric(IReg dst, IReg src, IReg ti)     = 0;
    virtual void InstanceOfGeneric(IReg dst, IReg obj, IReg ti) = 0;

    virtual void LoadTypeInfoGeneric(IReg dst, uint32_t typeId) = 0;

    virtual void LoadTypeInfoSig(IReg dst, uint32_t type)            = 0;
    virtual void NewObj(IReg dst, uint32_t type)                     = 0;
    virtual void CallDirect(IReg dst, uint32_t method)               = 0;
    virtual void CallVirtual(IReg dst, uint32_t method)              = 0;
    virtual void CallInterf(IReg dst, uint32_t method)               = 0;
    virtual void CallInterfGeneric(uint16_t argnum, uint32_t method) = 0;
    virtual void Spawn(IReg closure, uint32_t type)                  = 0;
    virtual void SpawnFuture(IReg future, uint32_t type)             = 0;
    virtual void CallClosure(IReg dst, uint32_t type, bool generic)  = 0;
    virtual void NewClosure(IReg dst, uint32_t type)                 = 0;
    virtual void NewClosureGeneric(IReg ti, uint32_t typeId)         = 0;
    virtual void NewObjGeneric(IReg ti, uint32_t typeId)             = 0;

    virtual void Scc(Format::Width width, Format::CC cc, IReg d, AnyReg l, AnyReg r)      = 0;
    virtual void SccImm(Format::Width width, Format::CC cc, IReg d, IReg l, uint64_t imm) = 0;

    virtual void Ret(Format::Width width, IReg src)  = 0;
    virtual void FRet(Format::Width width, FReg src) = 0;
    virtual void RetRef(IReg src)                    = 0;
    virtual void DivCheck(IReg reg)                  = 0;
    virtual void NullCheck(IReg reg)                 = 0;
    virtual void Catch(IReg reg)                     = 0;
    virtual void Throw(IReg reg)                     = 0;

    virtual void InstanceOf(IReg dst, IReg obj, uint32_t type) = 0;
    virtual void LoadTypeInfoObj(IReg dst, IReg obj)           = 0;
    virtual void InitObj(uint16_t ts)                          = 0;
    virtual void InitString(uint16_t ts, uint32_t offset)      = 0;

    virtual void ArrayLength(IReg dst, IReg arr)          = 0;
    virtual void ArrayIndexCheck(IReg length, IReg index) = 0;

    virtual void LoadUntyped(AnyReg dst, Format::LoadAccessKind ldk, uint16_t us)   = 0;
    virtual void StoreUntyped(AnyReg src, Format::StoreAccessKind stk, uint16_t us) = 0;
    virtual void StoreUntypedImm(uint64_t imm, uint16_t us)                         = 0;

    virtual void LoadTyped(AnyReg dst, uint16_t ts, uint16_t field)       = 0;
    virtual void StoreTyped(AnyReg src, uint16_t ts, uint16_t field)      = 0;
    virtual void StoreTypedImm(uint64_t imm, uint16_t ts, uint16_t field) = 0;

    virtual void LoadArray(AnyReg dst, Format::LoadAccessKind ldk, IReg arr, IReg idx)   = 0;
    virtual void StoreArray(AnyReg src, Format::StoreAccessKind stk, IReg arr, IReg idx) = 0;

    virtual void TypeArg(IReg ti, int idx, IReg dst)      = 0;
    virtual void Box(AnyReg src, IReg dst, uint32_t type) = 0;
    virtual void BoxT(uint16_t srcTs, IReg dst)           = 0;

    virtual void Unbox(AnyReg dst, IReg src, uint32_t type) = 0;
    virtual void UnboxT(uint16_t dstTs, IReg src)           = 0;

    virtual void Offset(IReg dst, IReg ti, uint32_t field, bool accumulate)  = 0;
    virtual void TagGeneric(IReg dst, IReg src, IReg tiReg, uint32_t typeId) = 0;
    virtual void PayloadGeneric(
        IReg dst, IReg src, IReg underlyingTypeInfo, IReg optionTypeInfo, uint32_t optionTypeInfoId
    )                                                                                                              = 0;
    virtual void NewNoneGeneric(IReg dst, IReg underlyingTypeInfo, IReg optionTypeInfo, uint32_t optionTypeInfoId) = 0;
    virtual void NewSomeGeneric(
        IReg dst, IReg src, IReg underlyingTypeInfo, IReg optionTypeInfo, uint32_t optionTypeInfoId
    ) = 0;

    virtual void AtomicLoad(IReg dst, IReg obj, uint32_t fieldId)  = 0;
    virtual void AtomicStore(IReg src, IReg obj, uint32_t fieldId) = 0;

    virtual void CAS(IReg dst, IReg obj, IReg expected, IReg newVal, uint32_t fieldId) = 0;

    virtual void AtomicSwap(IReg dst, IReg obj, IReg src, uint32_t fieldId) = 0;

    virtual void AtomicFetchAdd(IReg dst, IReg obj, IReg src, uint32_t fieldId) = 0;
    virtual void AtomicFetchSub(IReg dst, IReg obj, IReg src, uint32_t fieldId) = 0;
    virtual void AtomicFetchAnd(IReg dst, IReg obj, IReg src, uint32_t fieldId) = 0;
    virtual void AtomicFetchOr(IReg dst, IReg obj, IReg src, uint32_t fieldId)  = 0;
    virtual void AtomicFetchXor(IReg dst, IReg obj, IReg src, uint32_t fieldId) = 0;

    virtual void Ld(AnyReg dst, IReg base, uint32_t field) = 0;
    virtual void LdStatic(AnyReg dst, uint32_t field)      = 0;
    virtual void LdTyped(AnyReg dst, uint16_t slot, uint32_t field)                         = 0;
    virtual void LdDerived(AnyReg dst, IReg baseRef, IReg derived, uint32_t field)          = 0;
    virtual void LdGeneric(AnyReg dst, IReg baseRef, IReg derived, IReg ti, uint32_t field) = 0;
    virtual void Lea(IReg dst, IReg base, uint32_t field)  = 0;
    virtual void LeaStatic(IReg dst, IReg dstBaseRef, uint32_t field)                       = 0;
    virtual void LeaGeneric(IReg dst, IReg base, IReg ti, uint32_t field)                   = 0;
    virtual void LeaBox(IReg dst, IReg base)                                                = 0;
    virtual void St(AnyReg src, IReg base, uint32_t field) = 0;
    virtual void StStatic(AnyReg src, uint32_t field)      = 0;
    virtual void StTyped(AnyReg src, uint16_t slot, uint32_t field)                         = 0;
    virtual void StDerived(AnyReg src, IReg baseRef, IReg derived, uint32_t field)          = 0;
    virtual void StGeneric(AnyReg src, IReg baseRef, IReg derived, IReg ti, uint32_t field) = 0;

    class MemSpace {
    public:
        virtual ~MemSpace() = default;
    };

    virtual std::unique_ptr<MemSpace> OpenMemSpace() = 0;

    virtual void MemHeadReg(MemSpace& ms, IReg base, bool isRef)       = 0;
    virtual void MemHeadField(MemSpace& ms, IReg base, uint32_t field) = 0;
    virtual void MemHeadStatic(MemSpace& ms, uint32_t field)           = 0;
    virtual void MemHeadHandle(MemSpace& ms, IReg base, IReg derived)  = 0;
    virtual void MemHeadTyped(MemSpace& ms, uint16_t ts)               = 0;

    virtual void MemBodyField1(MemSpace& ms, uint32_t f1)                                        = 0;
    virtual void MemBodyField2(MemSpace& ms, uint32_t f1, uint32_t f2)                           = 0;
    virtual void MemBodyField3(MemSpace& ms, uint32_t f1, uint32_t f2, uint32_t f3)              = 0;
    virtual void MemBodyField4(MemSpace& ms, uint32_t f1, uint32_t f2, uint32_t f3, uint32_t f4) = 0;
    virtual void MemBodyIndex(MemSpace& ms, IReg reg, uint32_t elemType, bool checked)           = 0;
    virtual void MemBodyConstIndex(MemSpace& ms, int64_t idx, uint32_t elemType)                 = 0;

    virtual void MemTailLoad(MemSpace& ms, IReg dst, std::vector<uint32_t> refs)  = 0;
    virtual void MemTailStore(MemSpace& ms, IReg src, std::vector<uint32_t> refs) = 0;
    virtual void MemTailStoreImm(MemSpace& ms, uint64_t imm)                      = 0;
    virtual void MemTailCopyRegTo(MemSpace& ms, IReg to, uint32_t recType)        = 0;
    virtual void MemTailCopyRegFrom(MemSpace& ms, IReg from, uint32_t recType)    = 0;

    virtual void MemBodyOffset(MemSpace& ms, IReg offset)                                        = 0;
    virtual void MemBodyConstIndexGeneric(MemSpace& ms, int64_t idx, uint32_t elemType, IReg ti) = 0;
    virtual void MemBodyIndexGeneric(MemSpace& ms, IReg reg, uint32_t elemType, IReg ti)         = 0;
    virtual void MemBodyFieldGeneric(MemSpace& ms, uint32_t field, IReg ti)                      = 0;
    virtual void MemTailStoreGeneric(MemSpace& ms, IReg src, IReg ti)                            = 0;
    virtual void MemTailLoadGeneric(MemSpace& ms, IReg dst, IReg ti)                             = 0;

    friend class IsaParserImpl;
    Decoder::FatByteReader reader;
};

} // namespace Cbc
