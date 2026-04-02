#include "cbc/isa.h"
#include "isa_parser.h"
#include <ostream>

namespace Cbc {

struct IsaDisasm : public IsaParser {
    std::ostream stream;

    void Bcc(Format::Width width, Format::CC cc, AnyReg l, AnyReg r, int64_t delta) override
    {
        stream << "BCC." << width.ToStr() << " " << cc.ToStr() << " ";
        if (cc.IsFloatingPoint()) {
            stream << FReg::From(l).ToStr() << " " << FReg::From(r).ToStr();
        } else {
            stream << IReg::From(l).ToStr() << " " << IReg::From(r).ToStr();
        }
        stream << " " << delta << std::endl;
    }

    void BccImm(Format::Width width, Format::CC cc, IReg l, uint64_t imm, int64_t delta) override
    {
        stream << "BCCI." << width.ToStr() << " " << cc.ToStr() << " ";
        stream << IReg::From(l).ToStr() << " " << imm << " " << delta << std::endl;
    }

    void Jump(int64_t delta) override { stream << "JMP" << delta << std::endl; }

    void Mov(Format::Width width, IReg d, IReg s) override
    {
        stream << "MOV." << width.ToStr() << " " << d.ToStr() << " " << s.ToStr() << std::endl;
    }

    void FMov(Format::Width width, FReg d, FReg s) override
    {
        stream << "FMOV." << width.ToStr() << " " << d.ToStr() << " " << s.ToStr() << std::endl;
    }

    void FloatToInt(Format::Width width, IReg d, FReg s) override
    {
        stream << "F2I." << width.ToStr() << " " << d.ToStr() << " " << s.ToStr() << std::endl;
    }

    void IntToFloat(Format::Width width, FReg d, IReg s) override
    {
        stream << "I2F." << width.ToStr() << " " << d.ToStr() << " " << s.ToStr() << std::endl;
    }

    void MovRef(IReg d, IReg s) override { stream << "MOV.REF" << " " << d.ToStr() << " " << s.ToStr() << std::endl; }

    void MovImm(Format::Width width, IReg d, uint64_t value) override
    {
        stream << "MOVI." << " " << width.ToStr() << " " << d.ToStr() << " " << value << std::endl;
    }

    void Binary(Format::Common op, Format::Width width, IReg d, IReg l, IReg r) override
    {
        stream << op.ToStr() << "." << " " << width.ToStr() << " " << d.ToStr() << " " << l.ToStr() << " " << r.ToStr()
               << std::endl;
    }

    void BinaryImm(Format::Common op, Format::Width width, IReg d, IReg l, uint64_t value) override
    {
        stream << op.ToStr() << "I." << " " << width.ToStr() << " " << d.ToStr() << " " << l.ToStr() << " " << value
               << std::endl;
    }

    // TODO: add enum
    void FloatBinary(uint8_t op, Format::Width width, FReg d, FReg l, FReg r) override
    {
        stream << "FBIN" << op << " " << width.ToStr() << " " << d.ToStr() << " " << l.ToStr() << " " << r.ToStr()
               << std::endl;
    }

    // TODO: add enum
    void Cast(int8_t fromType, int8_t toType, AnyReg d, AnyReg s) override
    {
        stream << "CAST" << " " << fromType << " " << toType << " " << d << " " << s << std::endl;
    }

    void PrepareRecord(uint16_t ts) override { stream << "PREPARE.RECORD" << " " << ts << std::endl; }

    void NewArr(IReg dst, IReg len, uint16_t type) override
    {
        stream << "NEWARR" << " " << dst.ToStr() << " " << len.ToStr() << " " << type << std::endl;
    }

    void GcPoint() override { stream << "gcpoint" << std::endl; }

    void LoadTypeInfoFtc(IReg dst, uint16_t ftc) override
    {
        stream << "LOAD.TYPEINFO.FTC" << " " << dst.ToStr() << " " << ftc << std::endl;
    }

    void LoadTypeInfoSig(IReg dst, uint16_t type) override
    {
        stream << "LOAD.TYPEINFO.SIG" << " " << dst.ToStr() << " " << type << std::endl;
    }

    void NewObj(IReg dst, uint16_t type) override
    {
        stream << "NEWOBJ" << " " << dst.ToStr() << " " << type << std::endl;
    }

    void CallDirect(IReg dst, uint16_t method) override
    {
        stream << "CALL.DIRECT" << " " << dst.ToStr() << " " << method << std::endl;
    }

    void CallVirtual(IReg dst, uint16_t method) override
    {
        stream << "CALL.VIRTUAL" << " " << dst.ToStr() << " " << method << std::endl;
    }

    void CallInterf(IReg dst, uint16_t method) override
    {
        stream << "CALL.INTERF" << " " << dst.ToStr() << " " << method << std::endl;
    }

    void Scc(Format::Width width, Format::CC cc, IReg d, AnyReg l, AnyReg r) override
    {
        stream << "SCC." << width.ToStr() << " " << cc.ToStr() << " " << d.ToStr() << " ";
        if (cc.IsFloatingPoint()) {
            stream << FReg::From(l).ToStr() << " " << FReg::From(r).ToStr();
        } else {
            stream << IReg::From(l).ToStr() << " " << IReg::From(r).ToStr();
        }
        stream << std::endl;
    }

    void SccImm(Format::Width width, Format::CC cc, IReg d, IReg l, uint64_t imm) override
    {
        stream << "SCCI." << width.ToStr() << " " << cc.ToStr() << " ";
        stream << d.ToStr() << " " << l.ToStr() << " " << imm << std::endl;
    }

    void Ret(Format::Width width, IReg dst) override
    {
        stream << "RET." << width.ToStr() << " " << dst.ToStr() << std::endl;
    }

    void FRet(Format::Width width, FReg dst) override
    {
        stream << "FRET." << width.ToStr() << " " << dst.ToStr() << std::endl;
    }

    void DivCheck(IReg reg) override { stream << "DIVCHECK" << " " << reg.ToStr() << std::endl; }

    void Catch(IReg reg) override { stream << "CATCH" << " " << reg.ToStr() << std::endl; }

    void Throw(IReg reg) override { stream << "THROW" << " " << reg.ToStr() << std::endl; }

    void ZeroRefs(uint16_t ts) override { stream << "ZEROREFS" << " " << ts << std::endl; }

    void InstanceOf(IReg dst, IReg obj, uint16_t type) override
    {
        stream << "IOF" << " " << dst.ToStr() << " " << obj.ToStr() << " " << type << std::endl;
    }

    void LoadTypeInfoObj(IReg dst, IReg obj) override
    {
        stream << "LOAD.TYPEINFO" << " " << dst.ToStr() << " " << obj.ToStr() << std::endl;
    }

    void InitObj(uint16_t ts) override { stream << "INITOBJ" << " " << ts << std::endl; }

    void InitString(uint16_t ts, uint32_t offset) override
    {
        stream << "INITSTR" << " " << ts << " " << offset << std::endl;
    }

    void ArrayLength(IReg dst, IReg arr) override
    {
        stream << "ARRLEN" << " " << dst.ToStr() << " " << arr.ToStr() << std::endl;
    }

    void ArrayIndexCheck(IReg length, IReg index) override
    {
        stream << "AIC" << " " << length.ToStr() << " " << index.ToStr() << std::endl;
    }
};

} // namespace Cbc
