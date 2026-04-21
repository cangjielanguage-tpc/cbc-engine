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
    virtual void Mov(Format::Width width, IReg d, IReg s)                                          = 0;
    virtual void FMov(Format::Width width, FReg d, FReg s)                                         = 0;
    virtual void FloatToInt(Format::Width width, IReg d, FReg s)                                   = 0;
    virtual void IntToFloat(Format::Width width, FReg d, IReg s)                                   = 0;
    virtual void MovRef(IReg d, IReg s)                                                            = 0;
    virtual void MovImm(Format::Width width, IReg d, uint64_t value)                               = 0;
    virtual void Binary(Format::Common op, Format::Width width, IReg d, IReg l, IReg r)            = 0;
    virtual void BinaryImm(Format::Common op, Format::Width width, IReg d, IReg l, uint64_t value) = 0;
    virtual void FMovImm(Format::Width width, FReg d, double value)                                = 0;

    // TODO: add enum
    virtual void FloatBinary(uint8_t op, Format::Width width, FReg d, FReg l, FReg r) = 0;

    virtual void Convert(Format::ConvertType toType, Format::ConvertType fromType, AnyReg to, AnyReg from) = 0;

    virtual void PrepareRecord(uint16_t ts)                = 0;
    virtual void NewArr(IReg dst, IReg len, uint16_t type) = 0;

    virtual void GcPoint() = 0;

    virtual void LoadTypeInfoFtc(IReg dst, uint16_t ftc)  = 0;
    virtual void LoadTypeInfoSig(IReg dst, uint16_t type) = 0;
    virtual void NewObj(IReg dst, uint16_t type)          = 0;
    virtual void CallDirect(IReg dst, uint16_t method)    = 0;
    virtual void CallVirtual(IReg dst, uint16_t method)   = 0;
    virtual void CallInterf(IReg dst, uint16_t method)    = 0;

    virtual void Scc(Format::Width width, Format::CC cc, IReg d, AnyReg l, AnyReg r)      = 0;
    virtual void SccImm(Format::Width width, Format::CC cc, IReg d, IReg l, uint64_t imm) = 0;

    virtual void Ret(Format::Width width, IReg dst)  = 0;
    virtual void FRet(Format::Width width, FReg dst) = 0;
    virtual void DivCheck(IReg reg)                  = 0;
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

    friend class IsaParserImpl;
    Decoder::FatByteReader reader;
};

} // namespace Cbc
