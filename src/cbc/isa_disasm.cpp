#include "cbc/decoder.h"
#include "cbc/isa.h"
#include "engine/resolving_output.h"
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
        auto fp = cc.IsFloatingPoint();
        stream.PrintLn("bcc.{} {}, {}, {}, {}", width, cc, Fmt(l, fp), Fmt(r, fp), delta);
    }

    void BccImm(Format::Width width, Format::CC cc, IReg l, uint64_t imm, int64_t delta) override
    {
        stream.PrintLn("bcci.{} {}, {}, {}, {}", width, cc, l, imm, delta);
    }

    void Nop() override { stream.PrintLn("nop"); }

    void Jump(int64_t delta) override { stream.PrintLn("jmp {}", delta); }

    void Mov(Format::Width width, IReg d, IReg s) override { stream.PrintLn("mov.{} {}, {}", width, d, s); }

    void FMov(Format::Width width, FReg d, FReg s) override { stream.PrintLn("fmov.{} {}, {}", width, d, s); }

    void FloatToInt(Format::Width width, IReg d, FReg s) override { stream.PrintLn("f2i.{} {}, {}", width, d, s); }

    void IntToFloat(Format::Width width, FReg d, IReg s) override { stream.PrintLn("i2f.{} {}, {}", width, d, s); }

    void MovRef(IReg d, IReg s) override { stream.PrintLn("mov.ref {}, {}", d, s); }

    void MovImm(Format::Width width, IReg d, uint64_t value) override
    {
        stream.PrintLn("mov.{} {}, {}", width, d, value);
    }

    virtual void FMovImm(Format::Width width, FReg d, double value) override
    {
        stream.PrintLn("fmov.{} {}, {}", width, d, value);
    }

    void Binary(Format::Common op, Format::Width width, IReg d, IReg l, IReg r) override
    {
        stream.PrintLn("{}.{} {}, {}, {}", op, width, d, l, r);
    }

    void BinaryImm(Format::Common op, Format::Width width, IReg d, IReg l, uint64_t value) override
    {
        stream.PrintLn("{}i.{} {}, {}, {}", op, width, d, l, value);
    }

    void CBinary(Format::Checked op, Format::Width width, IReg d, IReg l, IReg r) override
    {
        stream << op.ToStr() << Sz(width) << " " << d.ToStr() << ", ";
        stream << l.ToStr() << ", " << r.ToStr() << endl;
    }

    void CBinaryImm(Format::Checked op, Format::Width width, IReg d, IReg l, uint64_t value) override
    {
        stream << op.ToStr() << "i" << "." << Sz(width) << " " << d.ToStr();
        stream << ", " << l.ToStr() << ", " << value << endl;
    }

    void FBinary(Format::FloatOperations op, Format::Width width, FReg d, FReg l, FReg r) override
    {
        stream.PrintLn("{}.{} {}, {}, {}", op, width, d, l, r);
    }

    void FUnary(Format::FloatOperations op, Format::Width width, FReg d, FReg s) override
    {
        stream.PrintLn("{}.{} {}, {}", op, width, d, s);
    }

    void Convert(Format::ConvertType toType, Format::ConvertType fromType, AnyReg to, AnyReg from) override
    {
        stream.PrintLn(
            "convert {}, {}, {}, {}",
            toType.ToStr(),
            fromType.ToStr(),
            Fmt(to, toType.IsFloatingPoint()),
            Fmt(from, fromType.IsFloatingPoint())
        );
    }

    void MovBasePtr(IReg dst, bool local) override { stream.PrintLn("mov.base.{}", local ? ".local" : ".global", dst); }

    void BFX(IReg dst, IReg src, Format::Width resW, Format::Width argW, bool sx, uint8_t offset, uint8_t size) override
    {
        stream.PrintLn("bfx {}, {}, {}, {}, {}, {}, {}", dst, src, resW, argW, sx, offset, size);
    }

    void PrepareRecord(uint16_t ts) override { stream.PrintLn("prepare.record {}", ts); }

    void NewArr(IReg dst, IReg len, uint32_t type) override { stream.PrintLn("newarr {}, {}, {}", dst, len, type); }

    void GcPoint() override { stream.PrintLn("gcpoint"); }

    void LoadStackRec(IReg r, uint16_t ts) override { stream.PrintLn("ld.stack.rec {}, {}", r, ts); }

    void LoadRawMemory(AnyReg dst, IReg base, int64_t offset, Format::LoadAccessKind ldk) override
    {
        stream.PrintLn("ld.raw.mem.{} {}, [{} + {}]", ldk, Fmt(dst, ldk.IsFloat()), base, offset);
    }

    void StoreRawMemory(AnyReg src, IReg base, int64_t offset, Format::StoreAccessKind stk) override
    {
        stream.PrintLn("st.raw.mem.{} {}, [{} + {}]", stk, Fmt(src, stk.IsFloat()), base, offset);
    }

    void LoadStatic(AnyReg r, uint32_t field) override { stream.PrintLn("ld.static R{} {}", r, field); }

    void StoreStatic(AnyReg r, uint32_t field) override { stream.PrintLn("st.static R{} {}", r, field); }

    void LoadField(IReg rb, AnyReg rs, uint32_t field) override
    {
        stream.PrintLn("ld.field R{}, [{} @{}]", rs, rb, field);
    }

    void StoreField(IReg rb, AnyReg rd, uint32_t field) override
    {
        stream.PrintLn("lst.field R{}, [{} @{}]", rd, rb, field);
    }

    void LoadTypeInfoGeneric(IReg dst, uint32_t typeId) override { stream.PrintLn("load.ti.g {}, @{}", dst, typeId); }

    void LoadTypeInfoSig(IReg dst, uint32_t type) override { stream.PrintLn("load.ti {}, @{}", dst, type); }

    void Offset(IReg dst, IReg ti, uint32_t field, bool accumulate) override
    {
        auto name = accumulate ? "add.offs" : "offs";
        stream.PrintLn("{} {}, {}, @{}", name, dst, ti, field);
    }

    void TagGeneric(IReg dst, IReg src, IReg ti, uint32_t typeId) override
    {
        stream.PrintLn("tag.g {}, {}, {}, @{}", dst, src, ti, typeId);
    }

    void PayloadGeneric(IReg dst, IReg src, IReg underlyingTypeInfo, IReg optionTypeInfo, uint32_t optionTypeInfoId)
        override
    {
        stream.PrintLn("payload.g {}, {}, {}, {}, @{}", dst, src, underlyingTypeInfo, optionTypeInfo, optionTypeInfoId);
    }

    void NewNoneGeneric(IReg dst, IReg underlyingTypeInfo, IReg optionTypeInfo, uint32_t optionTypeInfoId) override
    {
        stream.PrintLn("new.nonge.g {}, {}, {}, @{}", dst, underlyingTypeInfo, optionTypeInfo, optionTypeInfoId);
    }

    void NewSomeGeneric(IReg dst, IReg src, IReg underlyingTypeInfo, IReg optionTypeInfo, uint32_t optionTypeInfoId)
        override
    {
        stream.PrintLn(
            "new.some.g {}, {}, {}, {}, @{}", dst, src, underlyingTypeInfo, optionTypeInfo, optionTypeInfoId
        );
    }

    void AssignGeneric(IReg dst, IReg src, IReg ti) override { stream.PrintLn("assign.g {}, {}, {}", dst, src, ti); }

    void InstanceOfGeneric(IReg dst, IReg obj, IReg ti) override { stream.PrintLn("iof.g {}, {}, {}", dst, obj, ti); }

    void AtomicLoad(IReg dst, IReg obj, uint32_t fieldId) override
    {
        stream.PrintLn("atomic.load {}, [{} @{}]", dst, obj, fieldId);
    }

    void AtomicStore(IReg src, IReg obj, uint32_t fieldId) override
    {
        stream.PrintLn("atomic.store {}, [{} @{}]", src, obj, fieldId);
    }

    void CAS(IReg dst, IReg obj, IReg expected, IReg newVal, uint32_t fieldId) override
    {
        stream.PrintLn("atomic.cas {}, {}, {}, [{} @{}]", dst, expected, newVal, obj, fieldId);
    }

    void AtomicSwap(IReg dst, IReg obj, IReg src, uint32_t fieldId) override
    {
        stream.PrintLn("atomic.swap {}, {}, [{} @{}]", dst, src, obj, fieldId);
    }

    void AtomicFetchAdd(IReg dst, IReg obj, IReg src, uint32_t fieldId) override
    {
        stream.PrintLn("atomic.fetch.add {}, {}, [{} @{}]", dst, src, obj, fieldId);
    }

    void AtomicFetchSub(IReg dst, IReg obj, IReg src, uint32_t fieldId) override
    {
        stream.PrintLn("atomic.fetch.sub {}, {}, [{} @{}]", dst, src, obj, fieldId);
    }

    void AtomicFetchAnd(IReg dst, IReg obj, IReg src, uint32_t fieldId) override
    {
        stream.PrintLn("atomic.fetch.and {}, {}, [{} @{}]", dst, src, obj, fieldId);
    }

    void AtomicFetchOr(IReg dst, IReg obj, IReg src, uint32_t fieldId) override
    {
        stream.PrintLn("atomic.fetch.and {}, {}, [{} @{}]", dst, src, obj, fieldId);
    }

    void AtomicFetchXor(IReg dst, IReg obj, IReg src, uint32_t fieldId) override
    {
        stream.PrintLn("atomic.fetch.xor {}, {}, [{} @{}]", dst, src, obj, fieldId);
    }

    void NewObj(IReg dst, uint32_t type) override { stream.PrintLn("newobj {}, @{}", dst, type); }

    void NewClosure(IReg dst, uint32_t type) override { stream.PrintLn("new.closure {}, @{}", dst, type); }

    void CallDirect(IReg dst, uint32_t method) override { stream.PrintLn("call.direct {}, @{}", dst, method); }

    void CallVirtual(IReg dst, uint32_t method) override { stream.PrintLn("call.virtual {}, @{}", dst, method); }

    void CallInterf(IReg dst, uint32_t method) override { stream.PrintLn("call.interf {}, @{}", dst, method); }

    void CallInterfGeneric(uint16_t argnum, uint32_t method) override
    {
        stream.PrintLn("call.interf.g {}, @{}", argnum, method);
    }

    void Spawn(IReg closure, uint32_t type) override { stream.PrintLn("spawn {}, @{}", closure, type); }

    void SpawnFuture(IReg future, uint32_t type) override { stream.PrintLn("spawn.future {}, @{}", future, type); }

    void CallClosure(IReg dst, uint32_t type, bool generic) override
    {
        auto suffix = generic ? ".g" : "";
        stream.PrintLn("call.closure{} {}, @{}", suffix, dst, type);
    }

    void Scc(Format::Width width, Format::CC cc, IReg d, AnyReg l, AnyReg r) override
    {
        auto fp = cc.IsFloatingPoint();
        stream.PrintLn("scc.{} {}, {}, {}, {}, {}", width, cc, d, Fmt(l, fp), Fmt(r, fp));
    }

    void SccImm(Format::Width width, Format::CC cc, IReg d, IReg l, uint64_t imm) override
    {
        stream.PrintLn("scci.{} {}, {}, {}, {}, {}", width, cc, d, l, imm);
    }

    void Ret(Format::Width width, IReg src) override { stream.PrintLn("ret.{} {}", width, src); }

    void FRet(Format::Width width, FReg src) override { stream.PrintLn("fret.{} {}", width, src); }

    void RetRef(IReg src) override { stream.PrintLn("ret.ref {}", src); }

    void DivCheck(IReg reg) override { stream.PrintLn("divcheck {}", reg); }

    void NullCheck(IReg reg) override { stream.PrintLn("nullcheck {}", reg); }

    void Catch(IReg reg) override { stream.PrintLn("catch {}", reg); }

    void Throw(IReg reg) override { stream.PrintLn("throw {}", reg); }

    void InstanceOf(IReg dst, IReg obj, uint32_t type) override { stream.PrintLn("iof {}, {}, {}", dst, obj, type); }

    void LoadTypeInfoObj(IReg dst, IReg obj) override { stream.PrintLn("load.ti.obj {}, {}", dst, obj); }

    void InitObj(uint16_t ts) override { stream.PrintLn("initobj {}", ts); }

    void InitString(uint16_t ts, uint32_t offset) override { stream.PrintLn("initstr {}, #{}", ts, offset); }

    void ArrayLength(IReg dst, IReg arr) override { stream.PrintLn("arrlen {}, {}", dst, arr); }

    void ArrayIndexCheck(IReg length, IReg index) override { stream.PrintLn("aic {}, {}", length, index); }

    void LoadUntyped(AnyReg dst, Format::LoadAccessKind ldk, uint16_t us) override
    {
        stream.PrintLn("ld.untyped.{} {}, [u{}]", ldk, Fmt(dst, ldk.IsFloat()), us);
    }

    void StoreUntyped(AnyReg src, Format::StoreAccessKind stk, uint16_t us) override
    {
        stream.PrintLn("st.untyped.{} {}, [u{}]", stk, Fmt(src, stk.IsFloat()), us);
    }

    void StoreUntypedImm(uint64_t imm, uint16_t us) override { stream.PrintLn("st.untyped.imm {}, [u{}]", imm, us); }

    void LoadTyped(AnyReg dst, uint16_t ts, uint16_t fieldId) override
    {
        stream.PrintLn("ld.typed R{}, [t{} @{}]", dst, ts, fieldId);
    }

    void StoreTyped(AnyReg src, uint16_t ts, uint16_t fieldId) override
    {
        stream.PrintLn("st.typed R{}, [t{} @{}]", src, ts, fieldId);
    }

    void StoreTypedImm(uint64_t imm, uint16_t ts, uint16_t fieldId) override
    {
        stream.PrintLn("st.typed.imm {}, [t{} @{}]", imm, ts, fieldId);
    }

    void LoadArray(AnyReg dst, Format::LoadAccessKind ldk, IReg arr, IReg idx) override
    {
        stream.PrintLn("ld.arr.{} {}, {}[{}]", ldk, Fmt(dst, ldk.IsFloat()), arr, idx);
    }

    void StoreArray(AnyReg src, Format::StoreAccessKind stk, IReg arr, IReg idx) override
    {
        stream.PrintLn("st.arr.{} {}, {}[{}]", stk, Fmt(src, stk.IsFloat()), arr, idx);
    }

    void TypeArg(IReg ti, int idx, IReg dst) override { stream.PrintLn("type.arg {}, {}[{}]", dst, ti, idx); }

    void Box(AnyReg src, IReg dst, uint32_t tk) override { stream.PrintLn("box {}, R{}, @{}", dst, src, tk); }

    void BoxT(uint16_t srcTs, IReg dst) override { stream.PrintLn("box {}, t{}", dst, srcTs); }

    void Unbox(AnyReg dst, IReg src, uint32_t tk) override { stream.PrintLn("unbox R{}, {}, @{}", dst, tk); }

    void UnboxT(uint16_t dstTs, IReg src) override { stream.PrintLn("box t{}, {}", dstTs, src); }

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
        stream.PrintLn("ms.hd.{} {}", (isRef ? "ref" : "rec"), base);
    }

    void MemHeadField(MemSpace& ms, IReg base, uint32_t field) override
    {
        stream.PrintLn("ms.hd.field {}, @{}", base, field);
    }

    void MemHeadStatic(MemSpace& ms, uint32_t field) override { stream.PrintLn("ms.hd.field @{}", field); }

    void MemHeadHandle(MemSpace& ms, IReg base, IReg derived) override
    {
        stream.PrintLn("ms.hd.handle {}, {}", base, derived);
    }

    void MemHeadTyped(MemSpace& ms, uint16_t ts) override { stream.PrintLn("ms.hd.typed t{}", ts); }

    void MemBodyField1(MemSpace& ms, uint32_t f1) override
    {
        PrintMemPos();
        stream.PrintLn("field @{}", f1);
    }

    void MemBodyField2(MemSpace& ms, uint32_t f1, uint32_t f2) override
    {
        PrintMemPos();
        stream.PrintLn("field @{}, @{}", f1, f2);
    }

    void MemBodyField3(MemSpace& ms, uint32_t f1, uint32_t f2, uint32_t f3) override
    {
        PrintMemPos();
        stream.PrintLn("field @{}, @{}, @{}", f1, f2, f3);
    }

    void MemBodyField4(MemSpace& ms, uint32_t f1, uint32_t f2, uint32_t f3, uint32_t f4) override
    {
        PrintMemPos();
        stream.PrintLn("field @{}, @{}, @{}, @{}", f1, f2, f3, f4);
    }

    void MemBodyIndex(MemSpace& ms, IReg reg, uint32_t elemType, bool checked) override
    {
        PrintMemPos();
        stream.PrintLn("index{} {}, @{}", checked ? ".checked" : "", reg, elemType);
    }

    void MemBodyConstIndex(MemSpace& ms, int64_t idx, uint32_t refType) override
    {
        PrintMemPos();
        stream.PrintLn("const.index {}, @{}", idx, refType);
    }

    void Refs(std::vector<uint32_t> refs)
    {
        stream << "[ ";
        for (auto ref : refs) {
            stream << ref << " ";
        }
        stream << "]" << endl;
    }

    void MemTailLoad(MemSpace& ms, IReg dst, std::vector<uint32_t> refs) override
    {
        PrintMemPos();
        stream.Print("load {}", dst);
        Refs(refs);
    }

    void MemTailStore(MemSpace& ms, IReg src, std::vector<uint32_t> refs) override
    {
        PrintMemPos();
        stream.Print("store {}", src);
        Refs(refs);
    }

    void MemTailStoreImm(MemSpace& ms, uint64_t imm) override
    {
        PrintMemPos();
        stream.PrintLn("store.imm {}", imm);
    }

    void MemTailCopyReg(MemSpace& ms, IReg dst, uint32_t recType) override
    {
        PrintMemPos();
        stream.PrintLn("copy.reg {}, @{}", dst, recType);
    }

    void MemTailCopyInterior(MemSpace& ms, IReg dst, std::vector<uint32_t> refs) override
    {
        PrintMemPos();
        stream.Print("copy.interior {}, ", dst);
        Refs(refs);
    }

    void MemTailCopyInteriorArr(MemSpace& ms, IReg dst, IReg idx, std::vector<uint32_t> refs) override
    {
        PrintMemPos();
        stream.Print("copy.interior.arr {}, ", dst, idx);
        Refs(refs);
    }

    void MemTailCopyStatic(MemSpace& ms, std::vector<uint32_t> refs) override
    {
        PrintMemPos();
        stream.Print("copy.static ");
        Refs(refs);
    }

    void MemTailCopyTyped(MemSpace& ms, uint32_t ts, std::vector<uint32_t> refs) override
    {
        PrintMemPos();
        stream.Print("copy.types t{}, ", ts);
        Refs(refs);
    }

    void MemBodyOffset(MemSpace& ms, IReg offset) override
    {
        PrintMemPos();
        stream.PrintLn("offset {}", offset);
    }

    void MemTailCopyHandle(MemSpace& ms, IReg base, IReg derived) override
    {
        PrintMemPos();
        stream.PrintLn("copy.handle {}, {}", base, derived);
    }

    void MemBodyConstIndexGeneric(MemSpace& ms, int64_t idx, uint32_t elemType, IReg ti) override
    {
        PrintMemPos();
        stream.PrintLn("const.index.g {}, {}, @{}", idx, ti, elemType);
    }

    void MemBodyIndexGeneric(MemSpace& ms, IReg reg, uint32_t elemType, IReg ti) override
    {
        PrintMemPos();
        stream.PrintLn("index.g {}, {}, @{}", reg, ti, elemType);
    }

    void MemBodyFieldGeneric(MemSpace& ms, uint32_t field, IReg ti) override
    {
        PrintMemPos();
        stream.PrintLn("field.g {}, @{}", ti, field);
    }

    void MemTailStoreGeneric(MemSpace& ms, IReg src, IReg ti) override
    {
        PrintMemPos();
        stream.PrintLn("st.g {}, {}", src, ti);
    }

    void MemTailLoadGeneric(MemSpace& ms, IReg dst, IReg ti) override
    {
        PrintMemPos();
        stream.PrintLn("ld.g {}, {}", dst, ti);
    }

    void PrintMemPos()
    {
        PrintPos();
        stream.Print("  ");
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

    void CallVirtual(IReg dst, uint32_t methodId) override
    {
        auto m = resolver.Query(Index<VirtualCall>(methodId));
        if (!m.has_value()) {
            return;
        }
        auto method = m.value();
        stream.PrintLn("call.virtual {}, {} ({}, {})", dst, method, method->extDefNum, method->methodNum);
    }

    void NewObj(IReg dst, uint32_t type) override
    {
        auto t = resolver.Query(Index<Type>(type));
        if (t) {
            stream.PrintLn("newobj {}, {}", dst, *t);
        } else {
            IsaDisasm::NewObj(dst, type);
        }
    }

    void NewClosure(IReg dst, uint32_t type) override
    {
        auto t = resolver.Query(Index<Type>(type));
        if (t) {
            stream.PrintLn("new.closure {}, {}", dst, *t);
        } else {
            IsaDisasm::NewObj(dst, type);
        }
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
