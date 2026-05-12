#include "cbc/decoder.h"
#include "cbc/isa.h"
#include "isa_parser.h"
#include "resolution/resolution.h"
#include "utils/ostream.h"
#include <cmath>
#include <cstdint>

namespace Cbc {

using namespace Stream;
using namespace Resolution;

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

    std::string_view Fmt(AnyReg reg, bool isFloat)
    {
        if (isFloat) {
            return FReg::From(reg).ToStr();
        } else {
            return IReg::From(reg).ToStr();
        }
    }

    void Bcc(Format::Width width, Format::CC cc, AnyReg l, AnyReg r, int64_t delta) override
    {
        stream << "bcc." << Sz(width) << " " << cc.ToStr() << ", ";
        auto fp = cc.IsFloatingPoint();
        stream << Fmt(l, fp) << ", " << Fmt(r, fp) << ", " << delta << endl;
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

    void FBinary(Format::FloatOperations op, Format::Width width, FReg d, FReg l, FReg r) override
    {
        stream << op.ToStr() << "." << Sz(width) << " ";
        stream << d.ToStr() << ", " << l.ToStr() << ", " << r.ToStr() << endl;
    }

    void FUnary(Format::FloatOperations op, Format::Width width, FReg d, FReg s) override
    {
        stream << op.ToStr() << "." << Sz(width) << " ";
        stream << d.ToStr() << ", " << s.ToStr() << endl;
    }

    void Convert(Format::ConvertType toType, Format::ConvertType fromType, AnyReg to, AnyReg from) override
    {
        stream << "convert" << " " << toType.ToStr() << "_" << fromType.ToStr();
        stream << ", " << to << ", " << from << endl;
    }

    void BFX(IReg dst, IReg src, Format::Width resW, Format::Width argW, bool sx, uint8_t offset, uint8_t size) override
    {
        stream << "bfx" << " " << dst.ToStr() << ", " << src.ToStr() << ", ";
        stream << Sz(resW) << ", " << Sz(argW) << ", " << sx << ", ";
        stream << offset << ", " << size << endl;
    }

    void PrepareRecord(uint16_t ts) override { stream << "prepare.record" << " " << ts << endl; }

    void NewArr(IReg dst, IReg len, uint16_t type) override
    {
        stream << "newarr" << " " << dst.ToStr() << ", " << len.ToStr() << ", " << type << endl;
    }

    void GcPoint() override { stream << "gcpoint" << endl; }

    void LoadStatic(AnyReg r, uint16_t field) override { stream << "ld.static" << " " << r << ", " << field << endl; }

    void StoreStatic(AnyReg r, uint16_t field) override { stream << "st.static" << " " << r << ", " << field << endl; }

    void LoadObj(IReg rb, AnyReg rs, uint16_t field) override
    {
        stream << "ld.obj" << " " << rb.ToStr() << ", " << rs << ", " << field << endl;
    }

    void StoreObj(IReg rb, AnyReg rd, uint16_t field) override
    {
        stream << "st.obj" << " " << rb.ToStr() << ", " << rd << ", " << field << endl;
    }

    void LoadRec(IReg rb, AnyReg rs, uint16_t field) override
    {
        stream << "ld.rec" << " " << rb.ToStr() << ", " << rs << ", " << field << endl;
    }

    void StoreRec(IReg rb, AnyReg rd, uint16_t field) override
    {
        stream << "st.rec" << " " << rb.ToStr() << ", " << rd << ", " << field << endl;
    }

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
        auto fp = cc.IsFloatingPoint();
        stream << Fmt(l, fp) << ", " << Fmt(r, fp) << endl;
    }

    void SccImm(Format::Width width, Format::CC cc, IReg d, IReg l, uint64_t imm) override
    {
        stream << "scci." << Sz(width) << " " << cc.ToStr() << ", ";
        stream << d.ToStr() << ", " << l.ToStr() << ", " << imm << endl;
    }

    void Ret(Format::Width width, IReg src) override { stream << "ret." << Sz(width) << " " << src.ToStr() << endl; }

    void FRet(Format::Width width, FReg src) override { stream << "fret." << Sz(width) << " " << src.ToStr() << endl; }

    void RetRef(IReg src) override { stream << "ret.ref " << src.ToStr() << endl; }

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

    void LoadUntyped(AnyReg dst, Format::LoadAccessKind ldk, uint16_t us) override
    {
        stream << "load.untyped." << ldk.ToStr() << " " << Fmt(dst, ldk.IsFloat()) << ", " << us << endl;
    }

    void StoreUntyped(AnyReg src, Format::StoreAccessKind stk, uint16_t us) override
    {
        stream << "store.untyped." << stk.ToStr() << " " << us << ", " << Fmt(src, stk.IsFloat()) << endl;
    }

    void StoreUntypedImm(uint64_t imm, uint16_t us) override
    {
        stream << "store.untyped.imm" << " " << us << ", " << imm << endl;
    }

    void ParseOne() override
    {
        auto position = reader.Cursor() - reader.Start();
        stream.PrintFmt("%*lld: ", log10Size, position);
        IsaParser::ParseOne();
    }
};

struct IsaResolvingDisasm : IsaDisasm {
    Resolution::Resolver& resolver;

    using MethodIndex = Symlevel::RefId<Symlevel::MethodReference>;

    IsaResolvingDisasm(Stream::Output& stream, Decoder::FatByteReader reader, Resolution::Resolver& resolver)
        : IsaDisasm(stream, reader),
          resolver(resolver)
    {}

    void CallVirtual(IReg dst, uint16_t methodId) override
    {
        auto m = resolver.Query(Index<VirtualCall>(methodId));
        if (!m.has_value()) {
            return;
        }
        auto method = m.value();
        stream << "call.virtual " << dst.ToStr() << ", " << *method;
        stream << " (" << method->extDefNum << "," << method->methodNum << ")";
        stream << endl;
    }

    // TODO: implement rest.
};

bool g_IsRawDisasmEnabled = false;

void EnableRawDisasm() { g_IsRawDisasmEnabled = true; }

bool IsRawDisasmEnabled() { return g_IsRawDisasmEnabled; }

void RawDisasm(Stream::Output& stream, Decoder::FatByteReader reader) { IsaDisasm(stream, reader).ParseAll(); }

void RawDisasm(Stream::Output& stream, Cbc::MethodCode code)
{
    auto start = code.CodePtr();
    auto end   = start + code.CodeSize();
    RawDisasm(stream, Decoder::FatByteReader(start, start, end));
}

void RawDisasm(Stream::Output& stream, uint8_t* start, uint8_t* end)
{
    RawDisasm(stream, Decoder::FatByteReader(start, start, end));
}

void Disasm(Stream::Output& stream, Decoder::FatByteReader reader, Resolution::Resolver* resolver)
{
    if (!g_IsRawDisasmEnabled && resolver != nullptr) {
        IsaResolvingDisasm(stream, reader, *resolver).ParseAll();
    } else {
        RawDisasm(stream, reader);
    }
}

void Disasm(Stream::Output& stream, Cbc::MethodCode code, Resolution::Resolver* resolver)
{
    auto start = code.CodePtr();
    auto end   = start + code.CodeSize();
    Disasm(stream, Decoder::FatByteReader(start, start, end), resolver);
}

void Disasm(Stream::Output& stream, uint8_t* start, uint8_t* end, Resolution::Resolver* resolver)
{
    Disasm(stream, Decoder::FatByteReader(start, start, end), resolver);
}

} // namespace Cbc
