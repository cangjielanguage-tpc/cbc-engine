#include "api/resolver.h"
#include "cbc/decoder.h"
#include "cbc/isa.h"
#include "isa_parser.h"
#include "utils/ostream.h"
#include "utils/options.h"
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <ostream>

namespace Cbc {

using namespace Stream;

static std::string_view Sz(Format::Width w)
{
    switch (w) {
        case Format::Width::W8:  return "8";
        case Format::Width::W16: return "16";
        case Format::Width::W32: return "32";
        case Format::Width::W64: return "64";
    }
}

static uint32_t SizeOfOffset(uint8_t* start, uint8_t* end)
{
    auto log10size = static_cast<int>(1.0 + std::log10(end - start));
    log10size      = std::max(log10size, 1);
    return log10size;
}

/// Disasm implementation for CBC bytecode.
/// This implementation doesn't rely on resolution.
struct IsaDisasm : public IsaParser {
    Stream::Output& stream;
    uint32_t log10Size;

    IsaDisasm(Stream::Output& stream, Decoder::FatByteReader reader)
        : IsaParser(reader),
          stream(stream),
          log10Size(SizeOfOffset(reader.Start(), reader.End()))
    {}

    void Bcc(Format::Width width, Format::CC cc, AnyReg l, AnyReg r, int64_t delta) override
    {
        stream << "bcc." << Sz(width) << " " << cc.ToStr() << ", ";
        if (cc.IsFloatingPoint()) {
            stream << FReg::From(l).ToStr() << ", " << FReg::From(r).ToStr();
        } else {
            stream << IReg::From(l).ToStr() << ", " << IReg::From(r).ToStr();
        }
        stream << ", " << delta << endl;
    }

    void BccImm(Format::Width width, Format::CC cc, IReg l, uint64_t imm, int64_t delta) override
    {
        stream << "bcci." << Sz(width) << " " << cc.ToStr() << " ";
        stream << IReg::From(l).ToStr() << ", " << imm << ", " << delta << endl;
    }

    void Jump(int64_t delta) override { stream << "jmp" << " " << delta << endl; }

    void Mov(Format::Width width, IReg d, IReg s) override
    {
        stream << "mov." << Sz(width) << " " << d.ToStr() << ", " << s.ToStr() << endl;
    }

    void FMov(Format::Width width, FReg d, FReg s) override
    {
        stream << "fmov." << Sz(width) << " " << d.ToStr() << ", " << s.ToStr() << endl;
    }

    void FloatToInt(Format::Width width, IReg d, FReg s) override
    {
        stream << "f2i." << Sz(width) << " " << d.ToStr() << ", " << s.ToStr() << endl;
    }

    void IntToFloat(Format::Width width, FReg d, IReg s) override
    {
        stream << "i2f." << Sz(width) << " " << d.ToStr() << ", " << s.ToStr() << endl;
    }

    void MovRef(IReg d, IReg s) override { stream << "mov.ref" << " " << d.ToStr() << ", " << s.ToStr() << endl; }

    void MovImm(Format::Width width, IReg d, uint64_t value) override
    {
        stream << "mov." << Sz(width) << " " << d.ToStr() << ", " << value << endl;
    }

    virtual void FMovImm(Format::Width width, FReg d, double value) override
    {
        stream << "fmov." << Sz(width) << " " << d.ToStr() << ", " << value << endl;
    }

    void Binary(Format::Common op, Format::Width width, IReg d, IReg l, IReg r) override
    {
        stream << op.ToStr() << Sz(width) << " " << d.ToStr() << ", ";
        stream << l.ToStr() << ", " << r.ToStr() << endl;
    }

    void BinaryImm(Format::Common op, Format::Width width, IReg d, IReg l, uint64_t value) override
    {
        stream << op.ToStr() << "i" << "." << Sz(width) << " " << d.ToStr();
        stream << ", " << l.ToStr() << ", " << value << endl;
    }

    // TODO: add enum
    void FloatBinary(uint8_t op, Format::Width width, FReg d, FReg l, FReg r) override
    {
        stream << "fbin" << op << " " << Sz(width) << " " << d.ToStr();
        stream << " " << l.ToStr() << " " << r.ToStr() << endl;
    }

    void Convert(Format::ConvertType toType, Format::ConvertType fromType, AnyReg to, AnyReg from) override
    {
        stream << "convert" << " " << toType.ToStr() << "_" << fromType.ToStr();
        stream << ", " << to << ", " << from << endl;
    }

    void PrepareRecord(uint16_t ts) override { stream << "prepare.record" << " " << ts << endl; }

    void NewArr(IReg dst, IReg len, uint16_t type) override
    {
        stream << "newarr" << " " << dst.ToStr() << ", " << len.ToStr() << ", " << type << endl;
    }

    void GcPoint() override { stream << "gcpoint" << endl; }

    void LoadTypeInfoFtc(IReg dst, uint16_t ftc) override
    {
        stream << "load.typeinfo.ftc" << " " << dst.ToStr() << ", " << ftc << endl;
    }

    void LoadTypeInfoSig(IReg dst, uint16_t type) override
    {
        stream << "load.typeinfo.sig" << " " << dst.ToStr() << ", " << type << endl;
    }

    void NewObj(IReg dst, uint16_t type) override { stream << "newobj" << " " << dst.ToStr() << ", " << type << endl; }

    void CallDirect(IReg dst, uint16_t method) override
    {
        stream << "call.direct" << " " << dst.ToStr() << ", " << method << endl;
    }

    void CallVirtual(IReg dst, uint16_t method) override
    {
        stream << "call.virtual" << " " << dst.ToStr() << ", " << method << endl;
    }

    void CallInterf(IReg dst, uint16_t method) override
    {
        stream << "call.interf" << " " << dst.ToStr() << ", " << method << endl;
    }

    void Scc(Format::Width width, Format::CC cc, IReg d, AnyReg l, AnyReg r) override
    {
        stream << "scc." << Sz(width) << " " << cc.ToStr() << ", " << d.ToStr() << ", ";
        if (cc.IsFloatingPoint()) {
            stream << FReg::From(l).ToStr() << ", " << FReg::From(r).ToStr();
        } else {
            stream << IReg::From(l).ToStr() << ", " << IReg::From(r).ToStr();
        }
        stream << endl;
    }

    void SccImm(Format::Width width, Format::CC cc, IReg d, IReg l, uint64_t imm) override
    {
        stream << "scci." << Sz(width) << " " << cc.ToStr() << ", ";
        stream << d.ToStr() << ", " << l.ToStr() << ", " << imm << endl;
    }

    void Ret(Format::Width width, IReg dst) override { stream << "ret." << Sz(width) << " " << dst.ToStr() << endl; }

    void FRet(Format::Width width, FReg dst) override { stream << "fret." << Sz(width) << " " << dst.ToStr() << endl; }

    void DivCheck(IReg reg) override { stream << "divcheck" << " " << reg.ToStr() << endl; }

    void Catch(IReg reg) override { stream << "catch" << " " << reg.ToStr() << endl; }

    void Throw(IReg reg) override { stream << "throw" << " " << reg.ToStr() << endl; }

    void ZeroRefs(uint16_t ts) override { stream << "zerorefs" << " " << ts << endl; }

    void InstanceOf(IReg dst, IReg obj, uint16_t type) override
    {
        stream << "iof" << " " << dst.ToStr() << ", " << obj.ToStr() << ", " << type << endl;
    }

    void LoadTypeInfoObj(IReg dst, IReg obj) override
    {
        stream << "load.typeinfo" << " " << dst.ToStr() << ", " << obj.ToStr() << endl;
    }

    void InitObj(uint16_t ts) override { stream << "initobj" << " " << ts << endl; }

    void InitString(uint16_t ts, uint32_t offset) override
    {
        stream << "initstr" << " " << ts << ", " << offset << endl;
    }

    void ArrayLength(IReg dst, IReg arr) override
    {
        stream << "arrlen" << " " << dst.ToStr() << ", " << arr.ToStr() << endl;
    }

    void ArrayIndexCheck(IReg length, IReg index) override
    {
        stream << "aic" << " " << length.ToStr() << ", " << index.ToStr() << endl;
    }

    void ParseOne() override
    {
        auto position = reader.Cursor() - reader.Start();
        stream.PrintFmt("%*lld: ", log10Size, position);
        IsaParser::ParseOne();
    }
};

struct IsaResolvingDisasm : IsaDisasm {
    API::Resolver& resolver;

    using MethodIndex = Symlevel::Index<Symlevel::MethodReference>;

    IsaResolvingDisasm(Stream::Output& stream, Decoder::FatByteReader reader, API::Resolver& resolver)
        : IsaDisasm(stream, reader),
          resolver(resolver)
    {}

    /// FIXME:
    ///  - remove duplication between rewriter and disasm.
    ///  - MethodIndex and others MUST HAVE separate representation for API users
    ///    and API implementation. Knowledge about region is known only resolver.
    ///  - Usage of Symlevel::XYZIndex here do not make sense.
    MethodIndex Method(uint16_t index) { return MethodIndex { .region = 0, .index = index }; }

    void CallVirtual(IReg dst, uint16_t method) override
    {
        auto m = resolver.ResolveVirtualMethod(Method(method));
        stream << "call.virtual" << " " << dst.ToStr() << ", " << m->ExtDefNum() << ", " << m->VNum() << endl;
    }

    // TODO: implement rest.
};

void EnableRawDisasm() { Options::SetOption("raw_dasm", true); }

void EnableDisasm() { Options::SetOption("dasm", true); }

bool IsRawDisasmEnabled() { return Options::raw_dasm; }

bool IsDisasmEnabled() { return Options::raw_dasm || Options::dasm; }

std::unique_ptr<IsaParser> RawDisasm(Stream::Output& stream, Decoder::FatByteReader reader)
{
    return std::make_unique<IsaDisasm>(stream, reader);
}

std::unique_ptr<IsaParser> RawDisasm(Stream::Output& stream, Cbc::MethodCode code)
{
    auto start = code.CodePtr();
    auto end   = start + code.CodeSize();
    return RawDisasm(stream, Decoder::FatByteReader(start, start, end));
}

std::unique_ptr<IsaParser> RawDisasm(Stream::Output& stream, uint8_t* start, uint8_t* end)
{
    return RawDisasm(stream, Decoder::FatByteReader(start, start, end));
}

std::unique_ptr<IsaParser> Disasm(Stream::Output& stream, Decoder::FatByteReader reader, API::Resolver* resolver)
{
    if (!IsRawDisasmEnabled() && resolver != nullptr) {
        return std::make_unique<IsaResolvingDisasm>(stream, reader, *resolver);
    } else {
        return RawDisasm(stream, reader);
    }
}

std::unique_ptr<IsaParser> Disasm(Stream::Output& stream, Cbc::MethodCode code, API::Resolver* resolver)
{
    auto start = code.CodePtr();
    auto end   = start + code.CodeSize();
    return Disasm(stream, Decoder::FatByteReader(start, start, end), resolver);
}

std::unique_ptr<IsaParser> Disasm(Stream::Output& stream, uint8_t* start, uint8_t* end, API::Resolver* resolver)
{
    return Disasm(stream, Decoder::FatByteReader(start, start, end), resolver);
}

} // namespace Cbc
