#pragma once

#include <cstdint>

#include "cbc/decoder.h"
#include "cbc/isa.h"
#include "engine/symlevel/code.h"

namespace Cbc {

using MethodCode = Symlevel::Code;

class IsaParser {
public:
    IsaParser(Cbc::MethodCode code);
    IsaParser(Decoder::FatByteReader reader);
    IsaParser(uint8_t* start, uint8_t* end);

    virtual ~IsaParser() = default;

    using AnyReg = uint8_t;

    virtual void ParseOne();
    void ParseAll();

protected:
    virtual void Bcc(Format::Width width, Format::CC cc, AnyReg l, AnyReg r, int64_t delta)        = 0;
    virtual void BccImm(Format::Width width, Format::CC cc, IReg l, uint64_t imm, int64_t delta)   = 0;
    virtual void Jump(int64_t delta)                                                               = 0;
    virtual void Nop()                                                                             = 0;
    virtual void Mov(Format::Width width, IReg d, IReg s)                                          = 0;
    virtual void FMov(Format::Width width, FReg d, FReg s)                                         = 0;
    virtual void FloatToInt(Format::Width width, IReg d, FReg s)                                   = 0;
    virtual void IntToFloat(Format::Width width, FReg d, IReg s)                                   = 0;
    virtual void MovRef(IReg d, IReg s)                                                            = 0;
    virtual void MovImm(Format::Width width, IReg d, uint64_t value)                               = 0;
    virtual void Binary(Format::Common op, Format::Width width, IReg d, IReg l, IReg r)            = 0;
    virtual void BinaryImm(Format::Common op, Format::Width width, IReg d, IReg l, uint64_t value) = 0;
    virtual void FMovImm(Format::Width width, FReg d, double value)                                = 0;
    virtual void FBinary(Format::FloatOperations op, Format::Width width, FReg d, FReg l, FReg r)  = 0;
    virtual void FUnary(Format::FloatOperations op, Format::Width width, FReg d, FReg s)           = 0;

    virtual void Convert(Format::ConvertType toType, Format::ConvertType fromType, AnyReg to, AnyReg from) = 0;

    virtual void BFX(IReg dst, IReg src, Format::Width resW, Format::Width argW, bool sx, uint8_t offset, uint8_t size) = 0;

    virtual void PrepareRecord(uint16_t ts)                = 0;
    virtual void NewArr(IReg dst, IReg len, uint16_t type) = 0;

    virtual void GcPoint() = 0;

    virtual void LoadStackRec(IReg r, uint16_t ts)              = 0;
    virtual void LoadStatic(AnyReg r, uint16_t field)           = 0;
    virtual void StoreStatic(AnyReg r, uint16_t field)          = 0;
    virtual void LoadField(IReg rb, AnyReg rs, uint16_t field)  = 0;
    virtual void StoreField(IReg rb, AnyReg rd, uint16_t field) = 0;

    virtual void LoadTypeInfoFtc(IReg dst, uint16_t ftc)  = 0;
    virtual void LoadTypeInfoSig(IReg dst, uint16_t type) = 0;
    virtual void NewObj(IReg dst, uint16_t type)          = 0;
    virtual void CallDirect(IReg dst, uint16_t method)    = 0;
    virtual void CallVirtual(IReg dst, uint16_t method)   = 0;
    virtual void CallInterf(IReg dst, uint16_t method)    = 0;

    virtual void Scc(Format::Width width, Format::CC cc, IReg d, AnyReg l, AnyReg r)      = 0;
    virtual void SccImm(Format::Width width, Format::CC cc, IReg d, IReg l, uint64_t imm) = 0;

    virtual void Ret(Format::Width width, IReg src)  = 0;
    virtual void FRet(Format::Width width, FReg src) = 0;
    virtual void RetRef(IReg src)                    = 0;
    virtual void DivCheck(IReg reg)                  = 0;
    virtual void NullCheck(IReg reg)                 = 0;
    virtual void Catch(IReg reg)                     = 0;
    virtual void Throw(IReg reg)                     = 0;

    virtual void ZeroRefs(uint16_t ts) = 0;

    virtual void InstanceOf(IReg dst, IReg obj, uint16_t type) = 0;
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

    virtual void LoadArray(AnyReg dst, Format::LoadAccessKind ldk, IReg arr, IReg idx) = 0;
    virtual void StoreArray(AnyReg src, Format::StoreAccessKind stk, IReg arr, IReg idx) = 0;

    friend class IsaParserImpl;
    Decoder::FatByteReader reader;
};

} // namespace Cbc
