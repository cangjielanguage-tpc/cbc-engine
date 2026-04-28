#include <cstdint>

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

    static int64_t MergeLowHi(uint8_t low4, int64_t hi) { return static_cast<int64_t>((hi << 4) | low4); }

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

    static void LoadStatic(IsaParser& parser)
    {
        auto [r, id] = ByteReaderM(parser.reader).ReadU4Skip4().ReadU16().Get();
        parser.LoadStatic(r, id);
    }

    static void StoreStatic(IsaParser& parser)
    {
        auto [r, id] = ByteReaderM(parser.reader).ReadU4Skip4().ReadU16().Get();
        parser.StoreStatic(r, id);
    }

    static void LoadObj(IsaParser& parser)
    {
        auto [rb, rd, id] = ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU16().Get();
        parser.LoadObj(rb, rd, id);
    }

    static void StoreObj(IsaParser& parser)
    {
        auto [rb, rs, id] = ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU16().Get();
        parser.StoreObj(rb, rs, id);
    }

    static void LoadRec(IsaParser& parser)
    {
        auto [rb, rs, id] = ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU16().Get();
        parser.LoadRec(rb, rs, id);
    }

    static void StoreRec(IsaParser& parser)
    {
        auto [rb, rd, id] = ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU16().Get();
        parser.LoadRec(rb, rd, id);
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
        parser.Scc(width, cc, dst, lhs, MergeLowHi(low4, hibits));
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
        auto [opc_, dst, id]  = ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU16().Get();
        class RegSymGroup opc = opc_;
        switch (opc) {
            case Cbc::RegSymGroup::LoadTypeInfoSig: parser.LoadTypeInfoSig(dst, id); break;
            case Cbc::RegSymGroup::LoadTypeInfoFtc: parser.LoadTypeInfoFtc(dst, id); break;
            case Cbc::RegSymGroup::NewObj:          parser.NewObj(dst, id); break;
            case Cbc::RegSymGroup::CallDirect:      parser.CallDirect(dst, id); break;
            case Cbc::RegSymGroup::CallVirt:        parser.CallVirtual(dst, id); break;
            case Cbc::RegSymGroup::CallInterf:      parser.CallInterf(dst, id); break;
        }
    }

    static void RegGroup(IsaParser& parser)
    {
        auto [opc_, reg]   = ByteReaderM(parser.reader).ReadU4().ReadU4().Get();
        class RegGroup opc = opc_;
        switch (opc) {
            case Cbc::RegGroup::Ret32:    parser.Ret(Width::W32, reg); break;
            case Cbc::RegGroup::Ret64:    parser.Ret(Width::W64, reg); break;
            case Cbc::RegGroup::FRet32:   parser.FRet(Width::W32, reg); break;
            case Cbc::RegGroup::FRet64:   parser.FRet(Width::W64, reg); break;
            case Cbc::RegGroup::DivCheck: parser.DivCheck(reg); break;
            case Cbc::RegGroup::Catch:    parser.Catch(reg); break;
            case Cbc::RegGroup::Throw:    parser.Throw(reg); break;
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

    template <Width::Value width> static void FloatBinary(IsaParser& parser)
    {
        auto [op, dst, lhs, rhs] = ByteReaderM(parser.reader).ReadU4().ReadU4().ReadU4().ReadU4().Get();
        parser.FloatBinary(op, width, dst, lhs, rhs);
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
};

void IsaParser::ParseOne() { IsaParserImpl::ParseOne(*this); }

void IsaParser::ParseAll()
{
    while (!reader.IsEndReached()) {
        ParseOne();
    }
}

IsaParser::IsaParser(Decoder::FatByteReader reader) : reader(reader) {}

IsaParser::IsaParser(uint8_t* start, uint8_t* end) : reader(start, start, end) {}

IsaParser::IsaParser(Cbc::MethodCode code) : IsaParser(code.CodePtr(), code.CodePtr() + code.CodeSize()) {}

} // namespace Cbc
