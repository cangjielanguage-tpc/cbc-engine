#include <cstdint>

#include "engine/terms.h"
#include "isa.h"
#include "isa_opcodes.h"
#include "isa_parser.h"
#include "utils/assertion.h"
#include "utils/math.h"

namespace Cbc {

using namespace Format;

class Value {
public:
    Value(uint64_t value) : value(value) {}

    inline operator int64_t() { return static_cast<int64_t>(value); }

    inline operator int32_t()
    {
        ASSERT(MathUtils::IsNBitsSigned(value, 32));
        return static_cast<int32_t>(value);
    }

    inline operator int16_t()
    {
        ASSERT(MathUtils::IsNBitsSigned(value, 16));
        return static_cast<int16_t>(value);
    }

    inline operator int8_t()
    {
        ASSERT(MathUtils::IsNBitsSigned(value, 8));
        return static_cast<int8_t>(value);
    }

    inline operator uint64_t() { return value; }

    inline operator uint32_t()
    {
        ASSERT(MathUtils::IsNBits(value, 32));
        return static_cast<uint32_t>(value);
    }

    inline operator uint16_t()
    {
        ASSERT(MathUtils::IsNBits(value, 16));
        return static_cast<uint16_t>(value);
    }

    inline operator double()
    {
        double mem;
        static_assert(sizeof(mem) == 8);
        std::memcpy(&mem, &value, sizeof(mem));
        return mem;
    }

    inline operator float()
    {
        ASSERT(MathUtils::IsNBits(value, 32));
        float mem;
        static_assert(sizeof(mem) == 4);
        std::memcpy(&mem, &value, sizeof(mem));
        return mem;
    }

    inline operator uint8_t()
    {
        ASSERT(MathUtils::IsNBits(value, 8));
        return static_cast<uint8_t>(value);
    }

    inline operator bool()
    {
        ASSERT(MathUtils::IsNBits(value, 1));
        return static_cast<bool>(value);
    }

    inline operator Engine::TermKind()
    {
        ASSERT(value < Engine::FIRST_NON_PRIMITIVE);
        return static_cast<Engine::TermKind>(value);
    }

    inline operator IReg() { return IReg::From(*this); }

    inline operator FReg() { return FReg::From(*this); }

    inline operator CC() { return CC::From(*this); }

    inline operator LoadAccessKind() { return LoadAccessKind::From(*this); }

    inline operator StoreAccessKind() { return StoreAccessKind::From(*this); }

    inline operator Common() { return Common::From(*this); }

    inline operator RegSymGroup() { return RegSymGroup::From(*this); }

    inline operator RegGroup() { return RegGroup::From(*this); }

private:
    uint64_t value;
};

template <typename... Ts> struct ByteReaderM;
template <typename... Ts> struct ByteReaderHalf;

template <typename... Ts> struct ByteReaderM {
public:
    Decoder::FatByteReader& reader;
    std::tuple<Ts...> data;

    ByteReaderM(Decoder::FatByteReader& rreader) : reader(rreader), data() {}

    ByteReaderM(Decoder::FatByteReader& rreader, std::tuple<Ts...>&& base) : reader(rreader), data(std::move(base)) {}

    ByteReaderM(const ByteReaderM<Ts...>&)                   = delete;
    ByteReaderM<Ts...>& operator=(const ByteReaderM<Ts...>&) = delete;

    auto ReadU4() && -> decltype(auto)
    {
        auto val      = reader.Read8();
        auto new_data = std::tuple_cat(data, std::make_tuple(Value(static_cast<uint8_t>((val >> 4) & 0xF))));
        return ByteReaderHalf<Ts..., Value>(reader, val & 0xF, std::move(new_data));
    }

    auto ReadU4Skip4() && -> decltype(auto)
    {
        auto val      = reader.Read8();
        auto new_data = std::tuple_cat(data, std::make_tuple(Value(static_cast<uint8_t>((val & 0xF)))));
        return ByteReaderM<Ts..., Value>(reader, std::move(new_data));
    }

    auto ReadU8() && -> decltype(auto)
    {
        auto val      = Value(reader.Read8());
        auto new_data = std::tuple_cat(data, std::make_tuple(val));
        return ByteReaderM<Ts..., Value>(reader, std::move(new_data));
    }

    auto ReadU16() && -> decltype(auto)
    {
        auto val      = Value(reader.Read16());
        auto new_data = std::tuple_cat(data, std::make_tuple(val));
        return ByteReaderM<Ts..., Value>(reader, std::move(new_data));
    }

    auto ReadU32() && -> decltype(auto)
    {
        auto val      = Value(reader.Read32());
        auto new_data = std::tuple_cat(data, std::make_tuple(val));
        return ByteReaderM<Ts..., Value>(reader, std::move(new_data));
    }

    auto ReadU64() && -> decltype(auto)
    {
        auto val      = Value(reader.Read64());
        auto new_data = std::tuple_cat(data, std::make_tuple(val));
        return ByteReaderM<Ts..., Value>(reader, std::move(new_data));
    }

    auto ReadS8() && -> decltype(auto)
    {
        auto val      = Value(MathUtils::SignExtend((uint64_t)reader.Read8(), 8));
        auto new_data = std::tuple_cat(data, std::make_tuple(val));
        return ByteReaderM<Ts..., Value>(reader, std::move(new_data));
    }

    auto ReadS16() && -> decltype(auto)
    {
        auto val      = Value(MathUtils::SignExtend((uint64_t)reader.Read16(), 16));
        auto new_data = std::tuple_cat(data, std::make_tuple(val));
        return ByteReaderM<Ts..., Value>(reader, std::move(new_data));
    }

    auto ReadS32() && -> decltype(auto)
    {
        auto val      = Value(MathUtils::SignExtend((uint64_t)reader.Read32(), 32));
        auto new_data = std::tuple_cat(data, std::make_tuple(val));
        return ByteReaderM<Ts..., Value>(reader, std::move(new_data));
    }

    auto ReadSLEB() && -> decltype(auto)
    {
        auto val      = Value(reader.ReadSLEB());
        auto new_data = std::tuple_cat(data, std::make_tuple(val));
        return ByteReaderM<Ts..., Value>(reader, std::move(new_data));
    }

    auto Get() && -> decltype(auto) { return std::move(data); }
};

template <typename... Ts> struct ByteReaderHalf {
public:
    Decoder::FatByteReader& reader;
    uint8_t last;
    std::tuple<Ts...> data;

    ByteReaderHalf(Decoder::FatByteReader& rreader) : reader(rreader) {}

    ByteReaderHalf(Decoder::FatByteReader& rreader, uint8_t alast, std::tuple<Ts...>&& base)
        : reader(rreader),
          last(std::move(alast)),
          data(std::move(base))
    {}

    ByteReaderHalf(const ByteReaderHalf<Ts...>&)                   = delete;
    ByteReaderHalf<Ts...>& operator=(const ByteReaderHalf<Ts...>&) = delete;

    auto ReadU4() && -> decltype(auto)
    {
        auto new_data = std::tuple_cat(data, std::make_tuple(Value(last)));
        return ByteReaderM<Ts..., Value>(reader, std::move(new_data));
    }

    auto Get() && -> decltype(auto) { return std::move(data); }
};

struct IsaParserImpl {
    using ParseFunction = void (*)(IsaParser&);

#define PARSE_ONE_CASES(opc, func)                                                                                     \
    case Opcode::opc: func(parser); break;

    static void ParseOne(IsaParser& parser)
    {
        int w    = 0; // branch offsets are wide or not.
        auto opc = parser.reader.Read8();
        if (opc == Opcode::WidePrefix) {
            w   = 1;
            opc = parser.reader.Read8();
        }
        switch (opc) {
            ISA_OPCODES(PARSE_ONE_CASES)
        }
    }

#define PARSE_ONE_MEM_CASES(opc, func)                                                                                     \
    case MemOpcode::opc: end = func(parser, ms); break;

    static void ParseMemExpr(IsaParser& parser, IsaParser::MemSpace& ms)
    {
        bool end = false;
        while (!end) {
            auto opc = parser.reader.Read8();
            switch (opc) {
                ISA_MEM_OPCODES(PARSE_ONE_MEM_CASES)
            }
        }

    }

    static int64_t MergeLowHi(uint8_t low4, int64_t hi) { return static_cast<int64_t>((hi << 4) | low4); }

    static Width width64Or32(bool w64) { return w64 ? Width::W64 : Width::W32; }

    template <Width::Value width, CC::Value cc> static void BranchSpecializedDefault(IsaParser& parser)
    {
        auto [lhs, rhs, offset] = ByteReaderM(parser.reader).ReadU4().ReadU4().ReadS16().Get();
        parser.Bcc(width, cc, lhs, rhs, offset);
    }

    template <Width::Value width, CC::Value cc> static void BranchSpecializedWide(IsaParser& parser)
    {
        auto [lhs, rhs, offset] = ByteReaderM(parser.reader).ReadU4().ReadU4().ReadS32().Get();
        parser.Bcc(width, cc, lhs, rhs, offset);
    }

    template <Width::Value width> static void BranchGenericDefault(IsaParser& parser)
    {
        auto [cc, lhs, rhs, lowOffset, hiOffset] =
            ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU4().ReadU4().ReadS16().Get();
        parser.Bcc(width, cc, lhs, rhs, MergeLowHi(lowOffset, hiOffset));
    }

    template <Width::Value width> static void BranchGenericWide(IsaParser& parser)
    {
        auto [cc, lhs, rhs, lowOffset, hiOffset] =
            ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU4().ReadU4().ReadS32().Get();
        parser.Bcc(width, cc, lhs, rhs, MergeLowHi(lowOffset, hiOffset));
    }

    template <Width::Value width> static void BranchImmDefault(IsaParser& parser)
    {
        auto [cc, lhs, imm, offset] = ByteReaderM(parser.reader).ReadU4().ReadU4().ReadSLEB().ReadS16().Get();
        parser.BccImm(width, cc, lhs, imm, offset);
    }

    template <Width::Value width> static void BranchImmWide(IsaParser& parser)
    {
        auto [cc, lhs, imm, offset] = ByteReaderM(parser.reader).ReadU4().ReadU4().ReadSLEB().ReadS32().Get();
        parser.BccImm(width, cc, lhs, imm, offset);
    }

    static void Nop(IsaParser& parser) { parser.Nop(); }

    static void JumpDefault(IsaParser& parser)
    {
        auto [offset] = ByteReaderM(parser.reader).ReadS16().Get();
        parser.Jump(offset);
    }

    static void JumpWide(IsaParser& parser)
    {
        auto [offset] = ByteReaderM(parser.reader).ReadS32().Get();
        parser.Jump(offset);
    }

    template <Width::Value width> static void Mov(IsaParser& parser)
    {
        auto [dst, src] = ByteReaderM(parser.reader).ReadU4().ReadU4().Get();
        parser.Mov(width, dst, src);
    }

    template <Width::Value width> static void FMov(IsaParser& parser)
    {
        auto [dst, src] = ByteReaderM(parser.reader).ReadU4().ReadU4().Get();
        parser.FMov(width, dst, src);
    }

    static void MovRef(IsaParser& parser)
    {
        auto [dst, src] = ByteReaderM(parser.reader).ReadU4().ReadU4().Get();
        parser.MovRef(dst, src);
    }

    template <Width::Value width> static void MovImm(IsaParser& parser)
    {
        auto [dst, low4, hibits] = ByteReaderM(parser.reader).ReadU4().ReadU4().ReadSLEB().Get();
        parser.MovImm(width, dst, MergeLowHi(low4, hibits));
    }

    static void MovBasePtr(IsaParser& parser)
    {
        auto [dst, local] = ByteReaderM(parser.reader).ReadU4().ReadU4().Get();
        parser.MovBasePtr(dst, local);
    }

    static void BFX(IsaParser& parser)
    {
        auto [dst, src, byte1, byte2 ] =
            ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU8().ReadU8().Get();

        uint8_t b1  = static_cast<uint8_t>(byte1);
        auto res64  = b1 & 0b10000000;
        auto arg64  = b1 & 0b01000000;
        auto offset = b1 & 0b00111111;

        uint8_t b2  = static_cast<uint8_t>(byte2);
        auto sx     = b2 & 0b10000000;
        auto size   = b2 & 0b01111111;

        parser.BFX(dst, src, width64Or32(res64), width64Or32(arg64), sx, offset, size);
    }

    template <Width::Value width> static void FloatToInt(IsaParser& parser)
    {
        auto [dst, src] = ByteReaderM(parser.reader).ReadU4().ReadU4().Get();
        parser.FloatToInt(width, dst, src);
    }

    template <Width::Value width> static void IntToFloat(IsaParser& parser)
    {
        auto [dst, src] = ByteReaderM(parser.reader).ReadU4().ReadU4().Get();
        parser.IntToFloat(width, dst, src);
    }

    template <Common::Value op, Width::Value width> static void BinarySpecialized(IsaParser& parser)
    {
        auto [lhs, rhs] = ByteReaderM(parser.reader).ReadU4().ReadU4().Get();
        parser.Binary(op, width, lhs, lhs, rhs);
    }

    template <Width::Value width> static void BinaryGeneric(IsaParser& parser)
    {
        auto [op, dst, lhs, rhs] = ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU4().ReadU4().Get();
        parser.Binary(op, width, dst, lhs, rhs);
    }

    template <Width::Value width> static void BinaryImm(IsaParser& parser)
    {
        auto [op, dst, lhs, low4, hibits] =
            ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU4().ReadU4().ReadSLEB().Get();
        parser.BinaryImm(op, width, dst, lhs, static_cast<uint64_t>(MergeLowHi(low4, hibits)));
    }

    static void Convert(IsaParser& parser)
    {
        auto [toType, fromType, to, from] = ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU4().ReadU4().Get();
        parser.Convert(Format::ConvertType(toType), Format::ConvertType(fromType), to, from);
    }

    static void NewArr(IsaParser& parser)
    {
        auto [dst, len, id] = ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU16().Get();
        parser.NewArr(dst, len, id);
    }

    template <Width::Value width> static void FMovImm(IsaParser& parser)
    {
        if constexpr (width == Width::W32) {
            auto [unused, dst, imm] = ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU32().Get();
            float fimm              = imm;
            parser.FMovImm(width, dst, fimm);
        } else {
            auto [unused, dst, imm] = ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU64().Get();
            double fimm             = imm;
            parser.FMovImm(width, dst, fimm);
        }
    }

    static void GcPoint(IsaParser& parser) { parser.GcPoint(); }

    static void LoadRawMemory(IsaParser& parser)
    {
        auto [dst, base, ldk, low4, hibits] =
            ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU4().ReadU4().ReadSLEB().Get();
        parser.LoadRawMemory(dst, base, MergeLowHi(low4, hibits), ldk);
    }

    static void StoreRawMemory(IsaParser& parser)
    {
        auto [src, base, stk, low4, hibits] =
            ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU4().ReadU4().ReadSLEB().Get();
        parser.StoreRawMemory(src, base, MergeLowHi(low4, hibits), stk);
    }

    static void LoadStatic(IsaParser& parser)
    {
        auto [r, id] = ByteReaderM(parser.reader).ReadU4Skip4().ReadU16().Get();
        parser.LoadStatic(r, id);
    }

    static void LoadStackRec(IsaParser& parser)
    {
        auto [r, skip, ts] = ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU16().Get();
        parser.LoadStackRec(r, ts);
    }

    static void StoreStatic(IsaParser& parser)
    {
        auto [r, id] = ByteReaderM(parser.reader).ReadU4Skip4().ReadU16().Get();
        parser.StoreStatic(r, id);
    }

    static void LoadField(IsaParser& parser)
    {
        auto [rb, rd, id] = ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU16().Get();
        parser.LoadField(rb, rd, id);
    }

    static void StoreField(IsaParser& parser)
    {
        auto [rb, rs, id] = ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU16().Get();
        parser.StoreField(rb, rs, id);
    }

    static void PrepareRecord(IsaParser& parser)
    {
        auto [id] = ByteReaderM(parser.reader).ReadU16().Get();
        parser.PrepareRecord(id);
    }

    static void ZeroRefs(IsaParser& parser)
    {
        auto [id] = ByteReaderM(parser.reader).ReadU16().Get();
        parser.PrepareRecord(id);
    }

    template <Width::Value width> static void Scc(IsaParser& parser)
    {
        auto [cc, dst, lhs, rhs] = ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU4().ReadU4().Get();
        parser.Scc(width, cc, dst, lhs, rhs);
    }

    template <Width::Value width> static void SccImm(IsaParser& parser)
    {
        auto [cc, dst, lhs, low4, hibits] =
            ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU4().ReadU4().ReadSLEB().Get();
        parser.SccImm(width, cc, dst, lhs, MergeLowHi(low4, hibits));
    }

    static void InstanceOf(IsaParser& parser)
    {
        auto [dst, obj, id] = ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU16().Get();
        parser.InstanceOf(dst, obj, id);
    }

    static void LoadTypeInfoObj(IsaParser& parser)
    {
        auto [dst, obj] = ByteReaderM(parser.reader).ReadU4().ReadU4().Get();
        parser.LoadTypeInfoObj(dst, obj);
    }

    static void RegSymGroup(IsaParser& parser)
    {
        static constexpr bool GENERIC     = true;
        static constexpr bool NOT_GENERIC = false;

        auto [opc_, dst, id]  = ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU16().Get();
        class RegSymGroup opc = opc_;
        switch (opc) {
            case Cbc::RegSymGroup::LoadTypeInfoSig: parser.LoadTypeInfoSig(dst, id); break;
            case Cbc::RegSymGroup::NewObj:          parser.NewObj(dst, id); break;
            case Cbc::RegSymGroup::CallDirect:      parser.CallDirect(dst, id); break;
            case Cbc::RegSymGroup::CallVirt:        parser.CallVirtual(dst, id); break;
            case Cbc::RegSymGroup::CallInterf:      parser.CallInterf(dst, id); break;
            case Cbc::RegSymGroup::Spawn:           parser.Spawn(dst, id); break;
            case Cbc::RegSymGroup::SpawnFuture:     parser.SpawnFuture(dst, id); break;
            case Cbc::RegSymGroup::CallClosure:     parser.CallClosure(dst, id, NOT_GENERIC); break;
            case Cbc::RegSymGroup::NewClosure:      parser.NewClosure(dst, id); break;

            case Cbc::RegSymGroup::CallClosureGeneric:  parser.CallClosure(dst, id, GENERIC); break;
            case Cbc::RegSymGroup::LoadTypeInfoGeneric: parser.LoadTypeInfoGeneric(dst, id); break;
            case Cbc::RegSymGroup::CallInterfGeneric:   parser.CallInterfGeneric(dst, id); break;

            default: {
                FATAL("Should not reach here");
            }
        }
    }

    static void RegGroup(IsaParser& parser)
    {
        auto [opc_, reg]   = ByteReaderM(parser.reader).ReadU4().ReadU4().Get();
        class RegGroup opc = opc_;
        switch (opc) {
            case Cbc::RegGroup::Ret32:     parser.Ret(Width::W32, reg); break;
            case Cbc::RegGroup::Ret64:     parser.Ret(Width::W64, reg); break;
            case Cbc::RegGroup::FRet32:    parser.FRet(Width::W32, reg); break;
            case Cbc::RegGroup::FRet64:    parser.FRet(Width::W64, reg); break;
            case Cbc::RegGroup::DivCheck:  parser.DivCheck(reg); break;
            case Cbc::RegGroup::Catch:     parser.Catch(reg); break;
            case Cbc::RegGroup::Throw:     parser.Throw(reg); break;
            case Cbc::RegGroup::RetRef:    parser.RetRef(reg); break;
            case Cbc::RegGroup::NullCheck: parser.NullCheck(reg); break;
            default:                      {
                FATAL("Should not reach here");
            }
        }
    }

    static void InitObj(IsaParser& parser)
    {
        auto [ts] = ByteReaderM(parser.reader).ReadU16().Get();
        parser.InitObj(ts);
    }

    static void InitString(IsaParser& parser)
    {
        auto [offset, ts] = ByteReaderM(parser.reader).ReadU32().ReadU16().Get();
        parser.InitString(ts, offset);
    }

    static void ArrayLength(IsaParser& parser)
    {
        auto [dst, ra] = ByteReaderM(parser.reader).ReadU4().ReadU4().Get();
        parser.ArrayLength(dst, ra);
    }

    static void ArrayIndexCheck(IsaParser& parser)
    {
        auto [rl, ri] = ByteReaderM(parser.reader).ReadU4().ReadU4().Get();
        parser.ArrayIndexCheck(rl, ri);
    }

    template <Width::Value width> static void FloatOp(IsaParser& parser)
    {
        auto [op, dst, lhs, rhs] = ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU4().ReadU4().Get();

        Format::FloatOperations fop = Format::FloatOperations::From(op);
        switch (fop) {
            case Format::FloatOperations::FADD: // fallthrough
            case Format::FloatOperations::FSUB: // fallthrough
            case Format::FloatOperations::FMUL: // fallthrough
            case Format::FloatOperations::FDIV: parser.FBinary(fop, width, dst, lhs, rhs); break;

            case Format::FloatOperations::FMOV: parser.FMov(width, dst, rhs); break;

            case Format::FloatOperations::FNEG: // fallthrough
            case Format::FloatOperations::FABS: // fallthrough
            case Format::FloatOperations::FSQRT: parser.FUnary(fop, width, dst, rhs); break;

            case Format::FloatOperations::I2F: parser.IntToFloat(width, dst, rhs); break;
            case Format::FloatOperations::F2I: parser.FloatToInt(width, dst, rhs); break;
        }
    }

    static void LoadUntyped(IsaParser& parser)
    {
        auto [dst, ldk, us] = ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU16().Get();
        parser.LoadUntyped(dst, ldk, us);
    }

    static void StoreUntyped(IsaParser& parser)
    {
        auto [src, stk, us] = ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU16().Get();
        parser.StoreUntyped(src, stk, us);
    }

    static void StoreUntypedImm(IsaParser& parser)
    {
        auto [us, imm] = ByteReaderM(parser.reader).ReadU16().ReadSLEB().Get();
        parser.StoreUntypedImm(imm, us);
    }

    static void LoadTyped(IsaParser& parser)
    {
        auto [dst, ts, field] = ByteReaderM(parser.reader).ReadU4Skip4().ReadU16().ReadU16().Get();
        parser.LoadTyped(dst, ts, field);
    }

    static void StoreTyped(IsaParser& parser)
    {
        auto [src, ts, field] = ByteReaderM(parser.reader).ReadU4Skip4().ReadU16().ReadU16().Get();
        parser.StoreTyped(src, ts, field);
    }

    static void StoreTypedImm(IsaParser& parser)
    {
        auto [ts, field, imm] = ByteReaderM(parser.reader).ReadU16().ReadU16().ReadSLEB().Get();
        parser.StoreTypedImm(imm, ts, field);
    }

    static void LoadArray(IsaParser& parser)
    {
        auto [dst, ldk, arr, idx] = ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU4().ReadU4().Get();
        parser.LoadArray(dst, ldk, arr, idx);
    }

    static void StoreArray(IsaParser& parser)
    {
        auto [src, stk, arr, idx] = ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU4().ReadU4().Get();
        parser.StoreArray(src, stk, arr, idx);
    }

    static void TypeArg(IsaParser& parser)
    {
        auto [ti, dst, idx] = ByteReaderM(parser.reader).ReadU4().ReadU4().ReadSLEB().Get();
        parser.TypeArg(ti, idx, dst);
    }

    static void BoxRec(IsaParser& parser)
    {
        auto [src, dst, tk] = ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU16().Get();
        parser.Box(src, dst, tk);
    }

    static void UnboxRec(IsaParser& parser)
    {
        auto [dst, src, tk] = ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU16().Get();
        parser.Unbox(dst, src, tk);
    }

    static void Box(IsaParser& parser)
    {
        auto [src, dst, tk] = ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU8().Get();
        parser.Box(src, dst, tk);
    }

    static void BoxT(IsaParser& parser)
    {
        auto [_, dst, src] = ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU16().Get();
        parser.BoxT(src, dst);
    }

    static void Unbox(IsaParser& parser)
    {
        auto [dst, src, tk] = ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU8().Get();
        parser.Unbox(dst, src, tk);
    }

    static void UnboxT(IsaParser& parser)
    {
        auto [_, src, dst] = ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU16().Get();
        parser.UnboxT(dst, src);
    }

    static void Offset(IsaParser& parser)
    {
        auto [dst, ti, field] = ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU16().Get();
        parser.Offset(dst, ti, field, false);
    }

    static void AddOffset(IsaParser& parser)
    {
        auto [dst, ti, field] = ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU16().Get();
        parser.Offset(dst, ti, field, true);
    }

    static void TagGeneric(IsaParser& parser)
    {
        auto [dst, src, tiReg, _, typeId] =
            ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU4().ReadU4().ReadU16().Get();
        parser.TagGeneric(dst, src, tiReg, typeId);
    }

    static void PayloadGeneric(IsaParser& parser)
    {
        auto [dst, src, underlyingTiReg, optionTiReg, typeId] =
            ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU4().ReadU4().ReadU16().Get();
        parser.PayloadGeneric(dst, src, underlyingTiReg, optionTiReg, typeId);
    }

    static void NewNoneGeneric(IsaParser& parser)
    {
        auto [dst, underlyingTiReg, optionTiReg, _, typeId] =
            ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU4().ReadU4().ReadU16().Get();
        parser.NewNoneGeneric(dst, underlyingTiReg, optionTiReg, typeId);
    }

    static void NewSomeGeneric(IsaParser& parser)
    {
        auto [dst, src, underlyingTiReg, optionTiReg, typeId] =
            ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU4().ReadU4().ReadU16().Get();
        parser.NewSomeGeneric(dst, src, underlyingTiReg, optionTiReg, typeId);
    }

    static void MemHeadReg(IsaParser& parser)
    {
        auto ms = parser.OpenMemSpace();
        auto [base, isRef] = ByteReaderM(parser.reader).ReadU4().ReadU4().Get();
        parser.MemHeadReg(*ms, base, isRef);
        ParseMemExpr(parser, *ms);
    }

    static void MemHeadField(IsaParser& parser)
    {
        auto ms = parser.OpenMemSpace();
        auto [base, skip, field] = ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU16().Get();
        parser.MemHeadField(*ms, base, field);
        ParseMemExpr(parser, *ms);
    }

    static void MemHeadStatic(IsaParser& parser)
    {
        auto ms = parser.OpenMemSpace();
        auto [field] = ByteReaderM(parser.reader).ReadU16().Get();
        parser.MemHeadStatic(*ms, field);
        ParseMemExpr(parser, *ms);
    }

    static void MemHeadHandle(IsaParser& parser)
    {
        auto ms = parser.OpenMemSpace();
        auto [base, derived] = ByteReaderM(parser.reader).ReadU4().ReadU4().Get();
        parser.MemHeadHandle(*ms, base, derived);
        ParseMemExpr(parser, *ms);
    }

    static void MemHeadTyped(IsaParser& parser)
    {
        auto ms = parser.OpenMemSpace();
        auto [ts] = ByteReaderM(parser.reader).ReadU16().Get();
        parser.MemHeadTyped(*ms, ts);
        ParseMemExpr(parser, *ms);
    }

    static bool MemBodyFieldGeneric(IsaParser& parser, IsaParser::MemSpace& ms)
    {
        auto [f, ti] = ByteReaderM(parser.reader).ReadU16().ReadU4Skip4().Get();
        parser.MemBodyFieldGeneric(ms, f, ti);
        return false;
    }

    static bool MemBodyIndexGeneric(IsaParser& parser, IsaParser::MemSpace& ms)
    {
        auto [elemType, reg, ti] = ByteReaderM(parser.reader).ReadU16().ReadU4().ReadU4().Get();
        parser.MemBodyIndexGeneric(ms, reg, elemType, ti);
        return false;
    }

    static bool MemBodyConstIndexGeneric(IsaParser& parser, IsaParser::MemSpace& ms)
    {
        auto [idx, elemType, ti] = ByteReaderM(parser.reader).ReadSLEB().ReadU16().ReadU4Skip4().Get();
        parser.MemBodyConstIndexGeneric(ms, idx, elemType, ti);
        return false;
    }

    static bool MemBodyOffset(IsaParser& parser, IsaParser::MemSpace& ms)
    {
        auto [reg] = ByteReaderM(parser.reader).ReadU4Skip4().Get();
        parser.MemBodyOffset(ms, reg);
        return false;
    }

    static bool MemBodyField1(IsaParser& parser, IsaParser::MemSpace& ms)
    {
        auto [f1] = ByteReaderM(parser.reader).ReadU16().Get();
        parser.MemBodyField1(ms, f1);
        return false;
    }

    static bool MemBodyField2(IsaParser& parser, IsaParser::MemSpace& ms)
    {
        auto [f1, f2] = ByteReaderM(parser.reader).ReadU16().ReadU16().Get();
        parser.MemBodyField2(ms, f1, f2);
        return false;
    }

    static bool MemBodyField3(IsaParser& parser, IsaParser::MemSpace& ms)
    {
        auto [f1, f2, f3] = ByteReaderM(parser.reader).ReadU16().ReadU16().ReadU16().Get();
        parser.MemBodyField3(ms, f1, f2, f3);
        return false;
    }

    static bool MemBodyField4(IsaParser& parser, IsaParser::MemSpace& ms)
    {
        auto [f1, f2, f3, f4] = ByteReaderM(parser.reader).ReadU16().ReadU16().ReadU16().ReadU16().Get();
        parser.MemBodyField4(ms, f1, f2, f3, f4);
        return false;
    }

    static bool MemBodyIndex(IsaParser& parser, IsaParser::MemSpace& ms)
    {
        auto [reg, checked, elemType] = ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU16().Get();
        parser.MemBodyIndex(ms, reg, elemType, checked);
        return false;
    }

    static bool MemBodyConstIndex(IsaParser& parser, IsaParser::MemSpace& ms)
    {
        auto [idx, elemType] = ByteReaderM(parser.reader).ReadSLEB().ReadU16().Get();
        parser.MemBodyConstIndex(ms, idx, elemType);
        return false;
    }

    static bool MemTailLoadGeneric(IsaParser& parser, IsaParser::MemSpace& ms)
    {
        auto [dst, ti] = ByteReaderM(parser.reader).ReadU4().ReadU4().Get();
        parser.MemTailLoadGeneric(ms, dst, ti);
        return true;
    }

    static bool MemTailStoreGeneric(IsaParser& parser, IsaParser::MemSpace& ms)
    {
        auto [src, ti] = ByteReaderM(parser.reader).ReadU4().ReadU4().Get();
        parser.MemTailStoreGeneric(ms, src, ti);
        return true;
    }

    static bool MemTailLoad(IsaParser& parser, IsaParser::MemSpace& ms)
    {
        auto [reg, _size] = ByteReaderM(parser.reader).ReadU4().ReadU4().Get();
        std::vector<uint16_t> refs;
        uint8_t size = _size;
        for (int i = 0; i < size; i++) {
            refs.emplace_back(parser.reader.Read16());
        }
        parser.MemTailLoad(ms, reg, refs);
        return true;
    }

    static bool MemTailStore(IsaParser& parser, IsaParser::MemSpace& ms)
    {
        auto [reg, _size] = ByteReaderM(parser.reader).ReadU4().ReadU4().Get();
        std::vector<uint16_t> refs;
        uint8_t size = _size;
        for (int i = 0; i < size; i++) {
            refs.emplace_back(parser.reader.Read16());
        }
        parser.MemTailStore(ms, reg, refs);
        return true;
    }

    static bool MemTailStoreImm(IsaParser& parser, IsaParser::MemSpace& ms)
    {
        uint64_t imm = parser.reader.Read64();
        parser.MemTailStoreImm(ms, imm);
        return true;
    }

    static bool MemTailCopyReg(IsaParser& parser, IsaParser::MemSpace& ms)
    {
        auto [reg, skip, recType] = ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU16().Get();
        parser.MemTailCopyReg(ms, reg, recType);
        return true;
    }

    static bool MemTailCopyInterior(IsaParser& parser, IsaParser::MemSpace& ms)
    {
        auto [reg, _size] = ByteReaderM(parser.reader).ReadU4().ReadU4().Get();
        std::vector<uint16_t> refs;
        uint8_t size = _size;
        for (int i = 0; i < size; i++) {
            refs.emplace_back(parser.reader.Read16());
        }
        parser.MemTailCopyInterior(ms, reg, refs);
        return true;
    }

    static bool MemTailCopyInteriorArr(IsaParser& parser, IsaParser::MemSpace& ms)
    {
        auto [reg, idx, _size, skip] = ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU4().ReadU4().Get();
        std::vector<uint16_t> refs;
        uint8_t size = _size;
        for (int i = 0; i < size; i++) {
            refs.emplace_back(parser.reader.Read16());
        }
        parser.MemTailCopyInteriorArr(ms, reg, idx, refs);
        return true;
    }

    static bool MemTailCopyStatic(IsaParser& parser, IsaParser::MemSpace& ms)
    {
        std::vector<uint16_t> refs;
        uint8_t size = parser.reader.Read8();
        for (int i = 0; i < size; i++) {
            refs.emplace_back(parser.reader.Read16());
        }
        parser.MemTailCopyStatic(ms, refs);
        return true;
    }

    static bool MemTailCopyTyped(IsaParser& parser, IsaParser::MemSpace& ms)
    {
        std::vector<uint16_t> refs;
        uint8_t size = parser.reader.Read8();
        uint16_t ts = parser.reader.Read16();
        for (int i = 0; i < size; i++) {
            refs.emplace_back(parser.reader.Read16());
        }
        parser.MemTailCopyTyped(ms, ts, refs);
        return true;
    }

    static bool MemTailCopyHandle(IsaParser& parser, IsaParser::MemSpace& ms)
    {
        auto [base, offs] = ByteReaderM(parser.reader).ReadU4().ReadU4().Get();
        parser.MemTailCopyHandle(ms, base, offs);
        return true;
    }

    template <Width::Value width, CC::Value value>
    static constexpr ParseFunction BranchSpecialized[] = {
        &BranchSpecializedDefault<width, value>,
        &BranchSpecializedWide<width, value>,
    };

    template <Width::Value width>
    static constexpr ParseFunction BranchGeneric[] = {
        &BranchGenericDefault<width>,
        &BranchGenericWide<width>,
    };

    template <Width::Value width>
    static constexpr ParseFunction BranchImm[] = {
        &BranchImmDefault<width>,
        &BranchImmWide<width>,
    };

    static constexpr ParseFunction Jump[] = {
        &JumpDefault,
        &JumpWide,
    };

    static void Unreachable(IsaParser& parser) {}

    static bool UnreachableMem(IsaParser& parser, IsaParser::MemSpace& ms) { return true; }
};

void IsaParser::ParseOne() { IsaParserImpl::ParseOne(*this); }

void IsaParser::End() {}

void IsaParser::ParseAll()
{
    while (!reader.IsEndReached()) {
        ParseOne();
    }
    End();
}

IsaParser::IsaParser(Decoder::FatByteReader reader) : reader(reader) {}

IsaParser::IsaParser(uint8_t* start, uint8_t* end) : reader(start, start, end) {}

IsaParser::IsaParser(Cbc::MethodCode& code) : IsaParser(code.CodePtr(), code.CodePtr() + code.CodeSize()) {}

} // namespace Cbc
