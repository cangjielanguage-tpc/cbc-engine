#include "cbc/decoder.h"
#include "cbc/isa.h"
#include "engine/terms.h"
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

    void Nop() override { stream << "nop" << endl; }

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

    void MovBasePtr(IReg dst, bool local) override
    {
        stream << "mov.base.ptr" << (local ? ".local" : ".global") << " " << dst.ToStr() << endl;
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

    void LoadStackRec(IReg r, uint16_t ts) override { stream << "ld.stack.rec" << " " << r << ", " << ts << endl; }

    void LoadStatic(AnyReg r, uint16_t field) override { stream << "ld.static" << " " << r << ", " << field << endl; }

    void StoreStatic(AnyReg r, uint16_t field) override { stream << "st.static" << " " << r << ", " << field << endl; }

    void LoadField(IReg rb, AnyReg rs, uint16_t field) override
    {
        stream << "ld.obj" << " " << rb.ToStr() << ", " << rs << ", " << field << endl;
    }

    void StoreField(IReg rb, AnyReg rd, uint16_t field) override
    {
        stream << "st.obj" << " " << rb.ToStr() << ", " << rd << ", " << field << endl;
    }

    void LoadTypeInfoGeneric(IReg dst, uint16_t typeId) override
    {
        stream << "load.typeinfo.generic" << " " << dst.ToStr() << ", " << typeId << endl;
    }

    void LoadTypeInfoSig(IReg dst, uint16_t type) override
    {
        stream << "load.typeinfo.sig" << " " << dst.ToStr() << ", " << type << endl;
    }

    void Offset(IReg dst, IReg ti, uint16_t field, bool accumulate) override
    {
        auto name = accumulate ? "add.offs" : "offs";
        stream << name << " " << dst.ToStr() << ", " << ti.ToStr() << " " << field << endl;
    }

    void TagGeneric(IReg dst, IReg src, IReg ti, uint16_t typeId) override
    {
        stream << "tag.g " << dst.ToStr() << ", " << src.ToStr() << ", " << ti.ToStr() << ", " << typeId << endl;
    }

    void PayloadGeneric(IReg dst, IReg src, IReg underlyingTypeInfo, IReg optionTypeInfo, uint16_t optionTypeInfoId)
        override
    {
        stream << "payload.g " << dst.ToStr() << ", " << src.ToStr() << ", " << underlyingTypeInfo.ToStr();
        stream << "< " << optionTypeInfo.ToStr() << ", " << optionTypeInfoId << endl;
    }

    void NewNoneGeneric(IReg dst, IReg underlyingTypeInfo, IReg optionTypeInfo, uint16_t optionTypeInfoId) override
    {
        stream << "new.none.g " << dst.ToStr() << ", " << underlyingTypeInfo.ToStr() << ", " << optionTypeInfo.ToStr()
               << ", " << optionTypeInfoId << endl;
    }

    void NewSomeGeneric(IReg dst, IReg src, IReg underlyingTypeInfo, IReg optionTypeInfo, uint16_t optionTypeInfoId)
        override
    {
        stream << "new.some.g " << dst.ToStr() << ", " << src.ToStr() << ", " << underlyingTypeInfo.ToStr();
        stream << ", " << optionTypeInfo.ToStr() << ", " << optionTypeInfoId << endl;
    }

    void NewObj(IReg dst, uint16_t type) override { stream << "newobj" << " " << dst.ToStr() << ", " << type << endl; }

    void NewClosure(IReg dst, uint16_t type) override
    {
        stream << "new.closure" << " " << dst.ToStr() << ", " << type << endl;
    }

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

    void Spawn(IReg closure, uint16_t type) override
    {
        stream << "spawn" << " " << closure.ToStr() << ", " << type << endl;
    }

    void SpawnFuture(IReg future, uint16_t type) override
    {
        stream << "spawn.future" << " " << future.ToStr() << ", " << type << endl;
    }

    void CallClosure(IReg dst, uint16_t type) override
    {
        stream << "call.closure" << " " << dst.ToStr() << ", " << type << endl;
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

    void NullCheck(IReg reg) override { stream << "nullcheck" << " " << reg.ToStr() << endl; }

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

    void LoadTyped(AnyReg dst, uint16_t ts, uint16_t fieldId) override
    {
        stream << "load.typed" << " " << dst << ", " << ts << ", " << fieldId << endl;
    }

    void StoreTyped(AnyReg src, uint16_t ts, uint16_t fieldId) override
    {
        stream << "store.typed" << " " << ts << ", " << fieldId << ", " << src << endl;
    }

    void StoreTypedImm(uint64_t imm, uint16_t ts, uint16_t fieldId) override
    {
        stream << "store.typed.imm" << " " << ts << ", " << fieldId << ", " << imm << endl;
    }

    void LoadArray(AnyReg dst, Format::LoadAccessKind ldk, IReg arr, IReg idx) override
    {
        stream << "ldarr." << ldk.ToStr() << " " << Fmt(dst, ldk.IsFloat()) << ", " << arr.ToStr() << ", " << idx.ToStr() << endl;
    }

    void StoreArray(AnyReg src, Format::StoreAccessKind stk, IReg arr, IReg idx) override
    {
        stream << "starr." << stk.ToStr() << " " << arr.ToStr() << ", " << idx.ToStr() << ", " << Fmt(src, stk.IsFloat()) << endl;
    }

    void TypeArg(IReg ti, int idx, IReg dst) override
    {
        stream << "type.arg " << ti.ToStr() << ", " << idx << ", " << dst.ToStr() << endl;
    }

    void Box(AnyReg src, IReg dst, uint16_t tk) override
    {
        stream << "box." << (uint8_t)tk << " " << src << ", " << dst.ToStr() << endl; // TODO: prettify
    }

    void BoxT(uint16_t srcTs, IReg dst) override { stream << "box.t " << srcTs << ", " << dst.ToStr() << endl; }

    void Unbox(AnyReg dst, IReg src, uint16_t tk) override
    {
        stream << "unbox." << (uint8_t)tk << " " << dst << ", " << src.ToStr() << endl; // TODO: prettify
    }

    void UnboxT(uint16_t dstTs, IReg src) override { stream << "unbox.t " << dstTs << ", " << src.ToStr() << endl; }

    class PrintingMemSpace : public MemSpace {
    public:
        PrintingMemSpace(Stream::Output& stream) : stream(stream) { stream << "{ "; }

        ~PrintingMemSpace() override { stream << " }" << endl; }

    private:
        Stream::Output& stream;
    };

    std::unique_ptr<MemSpace> OpenMemSpace() override { return std::make_unique<PrintingMemSpace>(stream); }

    void MemHeadReg(MemSpace& ms, IReg base, bool isRef) override
    {
        stream << "mem.reg" << (isRef ? ".ref" : ".rec") << " " << base.ToStr() << endl;
    }

    void MemHeadField(MemSpace& ms, IReg base, uint16_t field) override
    {
        stream << "mem.field" << " " << base.ToStr() << ", " << field << endl;
    }

    void MemHeadStatic(MemSpace& ms, uint16_t field) override
    {
        stream << "mem.static" << " " << field << endl;
    }

    void MemHeadHandle(MemSpace& ms, IReg base, IReg derived) override
    {
        stream << "mem.handle" << " " << base.ToStr() << ", " << derived.ToStr() << endl;
    }

    void MemHeadTyped(MemSpace& ms, uint16_t ts) override
    {
        stream << "mem.typed" << " " << ts << endl;
    }

    void MemBodyField1(MemSpace& ms, uint16_t f1) override
    {
        PrintMemPos();
        stream << "mem.field1" << " " << f1 << endl;
    }

    void MemBodyField2(MemSpace& ms, uint16_t f1, uint16_t f2) override
    {
        PrintMemPos();
        stream << "mem.field2" << " " << f1 << " " << f2 << endl;
    }

    void MemBodyField3(MemSpace& ms, uint16_t f1, uint16_t f2, uint16_t f3) override
    {
        PrintMemPos();
        stream << "mem.field3" << " " << f1 << " " << f2 << " " << f3 << endl;
    }

    void MemBodyField4(MemSpace& ms, uint16_t f1, uint16_t f2, uint16_t f3, uint16_t f4) override
    {
        PrintMemPos();
        stream << "mem.field4" << " " << f1 << " " << f2 << " " << f3 << " " << f4 << endl;
    }

    void MemBodyIndex(MemSpace& ms, IReg reg, uint16_t elemType, bool checked) override
    {
        PrintMemPos();
        stream << "mem.index" << " " << reg.ToStr() << ", " << elemType << ", " << checked << endl;
    }

    void MemBodyConstIndex(MemSpace& ms, int64_t idx, uint16_t refType) override
    {
        PrintMemPos();
        stream << "mem.const.index" << " " << idx << ", " << refType << ", " << endl;
    }

    void Refs(std::vector<uint16_t> refs)
    {
        stream << "[ ";
        for (auto ref : refs) {
            stream << ref << " ";
        }
        stream << "]";
    }

    void MemTailLoad(MemSpace& ms, IReg dst, std::vector<uint16_t> refs) override
    {
        PrintMemPos();
        stream << "mem.load" << " " << dst.ToStr() << ", ";
        Refs(refs);
    }

    void MemTailStore(MemSpace& ms, IReg src, std::vector<uint16_t> refs) override
    {
        PrintMemPos();
        stream << "mem.store" << " " << src.ToStr() << ", ";
        Refs(refs);
    }

    void MemTailStoreImm(MemSpace& ms, uint64_t imm) override
    {
        PrintMemPos();
        stream << "mem.store.imm" << " " << imm;
    }

    void MemTailCopyReg(MemSpace& ms, IReg dst, uint16_t recType) override
    {
        PrintMemPos();
        stream << "mem.copy.reg" << " " << dst.ToStr() << ", " << recType;
    }

    void MemTailCopyInterior(MemSpace& ms, IReg dst, std::vector<uint16_t> refs) override
    {
        PrintMemPos();
        stream << "mem.copy.interior" << " " << dst.ToStr() << ", ";
        Refs(refs);
    }

    void MemTailCopyInteriorArr(MemSpace& ms, IReg dst, IReg idx, std::vector<uint16_t> refs) override
    {
        PrintMemPos();
        stream << "mem.copy.interior.arr" << " " << dst.ToStr() << ", " << idx.ToStr() << ", ";
        Refs(refs);
    }

    void MemTailCopyStatic(MemSpace& ms, std::vector<uint16_t> refs) override
    {
        PrintMemPos();
        stream << "mem.copy.static" << " ";
        Refs(refs);
    }

    void MemTailCopyTyped(MemSpace& ms, uint16_t ts, std::vector<uint16_t> refs) override
    {
        PrintMemPos();
        stream << "mem.copy.typed" << " " << ts << ", ";
        Refs(refs);
    }

    void MemBodyOffset(MemSpace& ms, IReg offset) override
    {
        PrintMemPos();
        stream << "mem.body.offs" << " " << offset.ToStr() << endl;
    }

    void MemTailCopyHandle(MemSpace& ms, IReg base, IReg offset) override
    {
        PrintMemPos();
        stream << "mem.copy.handle" << " " << base.ToStr() << ", " << offset.ToStr();
    }

    void MemBodyConstIndexGeneric(MemSpace& ms, int64_t idx, uint16_t elemType, IReg ti) override
    {
        PrintMemPos();
        stream << "mem.const.index.g" << " " << idx << ", " << elemType << ", " << endl;
    }

    void MemBodyIndexGeneric(MemSpace& ms, IReg reg, uint16_t elemType, IReg ti) override
    {
        PrintMemPos();
        stream << "mem.index.g" << " " << reg.ToStr() << ", " << elemType << ", " << ti.ToStr() << endl;
    }

    void MemBodyFieldGeneric(MemSpace& ms, uint16_t field, IReg ti) override
    {
        PrintMemPos();
        stream << "mem.field.g" << " " << field << " " << ti.ToStr() << endl;
    }

    void MemTailStoreGeneric(MemSpace& ms, IReg src, IReg ti) override
    {
        PrintMemPos();
        stream << "mem.store.g" << " " << src << " " << ti.ToStr();
    }

    void MemTailLoadGeneric(MemSpace& ms, IReg dst, IReg ti) override
    {
        PrintMemPos();
        stream << "mem.load.g" << " " << dst << " " << ti.ToStr();
    }

    void PrintMemPos()
    {
        PrintPos();
        stream << "  ";
    }

    void PrintPos()
    {
        auto position = reader.Cursor() - reader.Start();
        stream.PrintFmt("%*lld: ", log10Size, position);
    }

    void ParseOne() override
    {
        PrintPos();
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
        stream << "call.virtual " << dst.ToStr() << ", " << method;
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
