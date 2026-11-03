#include "cbc_test_harness.h"

enum class SatOp { Add, Sub, Mul, Div, Mod, Pow, Lsl, Asr, UAdd, USub, UMul, UDiv, UMod, ULsl, ULsr };

// Mirrors the saturating binary ops SatArith/SatArithU in
// src/interpreter/internal/operations.h. A zero rhs for Div/Mod is not
// modeled (the interpreter asserts on it), the callers keep it out.
template <typename S, typename U> uint64_t SatRefW(SatOp op, uint64_t l, uint64_t r)
{
    constexpr int64_t bitWidth  = std::numeric_limits<U>::digits;
    constexpr S smaxValue       = std::numeric_limits<S>::max();
    constexpr S sminValue       = std::numeric_limits<S>::min();
    constexpr U umaxValue       = std::numeric_limits<U>::max();
    using swide                 = __int128;
    using uwide                 = unsigned __int128;

    auto makeS = [&](swide v) {
        return CheckedMakeS(v < sminValue ? sminValue : (v > smaxValue ? smaxValue : static_cast<S>(v)));
    };
    auto makeU = [&](uwide v) { return CheckedMakeU(v > umaxValue ? umaxValue : static_cast<U>(v)); };

    S sl = static_cast<S>(l);
    U ul = static_cast<U>(l);
    S sr = static_cast<S>(r);
    U ur = static_cast<U>(r);
    switch (op) {
        case SatOp::Add: return makeS(static_cast<swide>(sl) + static_cast<swide>(sr));
        case SatOp::Sub: return makeS(static_cast<swide>(sl) - static_cast<swide>(sr));
        case SatOp::Mul: return makeS(static_cast<swide>(sl) * static_cast<swide>(sr));
        case SatOp::Div:
            if (sr == -1 && sl == sminValue) return makeS(smaxValue);
            return makeS(sl / sr);
        case SatOp::Mod:
            if (sr == -1 && sl == sminValue) return makeS(static_cast<S>(0));
            return makeS(sl % sr);
        case SatOp::Pow: {
            S base = sl;
            U exp  = ur;
            if (base == 0) return makeS(exp == 0 ? 1 : 0);
            if (base == 1) return makeS(1);
            if (base == -1) return makeS((exp & 1) != 0 ? -1 : 1);
            swide acc = 1;
            while (exp != 0 && sminValue <= acc && acc <= smaxValue) {
                acc *= base;
                exp--;
            }
            if (acc < sminValue || acc > smaxValue) {
                bool negative = (acc < 0) != (base < 0 && (exp & 1) != 0);
                return makeS(negative ? sminValue : smaxValue);
            }
            return makeS(acc);
        }
        case SatOp::Lsl: {
            // Mirrors Saturating::SLSL (src/interpreter/internal/operations.h:202-216)
            // verbatim: the shift is clamped to [0, bitWidth-1], the
            // sign-extended base is left-shifted in the unsigned wide type
            // (post-af23200b) and the bit pattern is reinterpreted as signed
            // before the saturating clamp.
            int64_t shift = static_cast<int64_t>(sr);
            if (shift < 0) shift = 0;
            if (shift > bitWidth - 1) shift = bitWidth - 1;
            uwide ures = static_cast<uwide>(sl) << shift;
            return makeS(static_cast<swide>(ures));
        }
        case SatOp::Asr: {
            int64_t shift = static_cast<int64_t>(sr);
            if (shift < 0) shift = 0;
            if (shift > bitWidth - 1) shift = bitWidth - 1;
            return makeS(static_cast<swide>(sl >> shift));
        }
        case SatOp::UAdd: return makeU(static_cast<uwide>(ul) + static_cast<uwide>(ur));
        case SatOp::USub: return makeU(ul >= ur ? static_cast<uwide>(ul - ur) : static_cast<uwide>(0));
        case SatOp::UMul: return makeU(static_cast<uwide>(ul) * static_cast<uwide>(ur));
        case SatOp::UDiv: return makeU(ul / ur);
        case SatOp::UMod: return makeU(ul % ur);
        case SatOp::ULsl: {
            int64_t shift = static_cast<int64_t>(ur);
            if (shift < 0) shift = 0;
            if (shift > bitWidth - 1) shift = bitWidth - 1;
            return makeU(static_cast<uwide>(ul) << shift);
        }
        case SatOp::ULsr: {
            int64_t shift = static_cast<int64_t>(ur);
            if (shift < 0) shift = 0;
            if (shift > bitWidth - 1) shift = bitWidth - 1;
            return makeU(ul >> shift);
        }
    }
    return 0;
}

static uint64_t SatRef(SatOp op, int width, uint64_t l, uint64_t r)
{
    switch (width) {
        case 8: return SatRefW<int8_t, uint8_t>(op, l, r);
        case 16: return SatRefW<int16_t, uint16_t>(op, l, r);
        case 32: return SatRefW<int32_t, uint32_t>(op, l, r);
        default: return SatRefW<int64_t, uint64_t>(op, l, r);
    }
}

struct SatArithPair {
    uint64_t lhs;
    uint64_t rhs;
};

struct SatArithParams {
    std::string name;
    SatOp op;
    int width;
    bool immForm;
    bool unary;
    std::vector<SatArithPair> pairs;
};

static std::ostream& operator<<(std::ostream& os, const SatArithParams& p)
{
    return os << p.name << "." << p.width << (p.immForm ? "-imm" : "-rr");
}

static std::vector<SatArithPair> SatCross(const std::vector<uint64_t>& lhs, const std::vector<uint64_t>& rhs)
{
    std::vector<SatArithPair> pairs;
    for (auto l : lhs) {
        for (auto r : rhs) {
            pairs.push_back({ l, r });
        }
    }
    return pairs;
}

static std::vector<SatArithPair> DropZeroRhs(std::vector<SatArithPair> pairs)
{
    pairs.erase(std::remove_if(pairs.begin(), pairs.end(), [](SatArithPair p) { return p.rhs == 0; }), pairs.end());
    return pairs;
}

// The imm-form rhs values must match the generated file sets in
// test/resources/generated-asm/sat-arith/ (asm-generator.py).
static std::vector<uint64_t> SatImmRhs(int width)
{
    switch (width) {
        case 8: return { 0x0, 0x1, 0x2, 0x7F, 0x80, 0xFF };
        case 16: return { 0x0, 0x1, 0x2, 0x7FF, 0x1000, 0x7FFF, 0x8000, 0xFFFF };
        case 32: return { 0x0, 0x1, 0x2, 0x7FF, 0x10000, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF };
        default: return { 0x0, 0x1, 0x2, 0x7FF, 0x100000000, 0x7FFFFFFFFFFFFFFF, 0x8000000000000000,
                          0xFFFFFFFFFFFFFFFF };
    }
}

static std::vector<SatArithParams> SatArithParamsList()
{
    std::vector<SatArithParams> list;
    auto add = [&](const char* name, SatOp op, int width, bool immForm, bool unary, std::vector<SatArithPair> pairs) {
        list.push_back({ name, op, width, immForm, unary, std::move(pairs) });
    };
    for (int width : { 8, 16, 32, 64 }) {
        uint64_t smax = CheckedSMax(width);
        uint64_t smin = CheckedSMin(width);
        uint64_t umax = CheckedUMax(width);
        std::vector<uint64_t> lhs = { 0, 1, 2, smax - 1, smax, smin, smin + 1, umax, umax - 1, (umax >> 1) + 1 };
        std::vector<uint64_t> immRhs = SatImmRhs(width);
        // LSL bases: the interpreter's SLSL (src/interpreter/internal/operations.h:202-216)
        // left-shifts the sign-extended base in the unsigned wide type and
        // re-wraps the bit pattern as signed before the saturating clamp
        // (post-af23200b), so negative bases are well-defined and covered;
        // SatRefW's SatOp::Lsl mirrors that. The shifted rhs (SatImmRhs/lhs)
        // tables are shared with the other saturating ops - keep them as is.
        std::vector<uint64_t> lslLhs = { 0, 1, 2, smax - 1, smax, ~uint64_t { 4 }, smin, smin + 1 };

        add("saddi", SatOp::Add, width, true, false, SatCross(lhs, immRhs));
        add("ssubi", SatOp::Sub, width, true, false, SatCross(lhs, immRhs));
        add("smuli", SatOp::Mul, width, true, false, SatCross(lhs, immRhs));
        add("slsli", SatOp::Lsl, width, true, false, SatCross(lslLhs, immRhs));
        add("sasri", SatOp::Asr, width, true, false, SatCross(lhs, immRhs));
        add("suaddi", SatOp::UAdd, width, true, false, SatCross(lhs, immRhs));
        add("susubi", SatOp::USub, width, true, false, SatCross(lhs, immRhs));
        add("sumuli", SatOp::UMul, width, true, false, SatCross(lhs, immRhs));

        add("sadd", SatOp::Add, width, false, false, SatCross(lhs, lhs));
        add("ssub", SatOp::Sub, width, false, false, SatCross(lhs, lhs));
        add("smul", SatOp::Mul, width, false, false, SatCross(lhs, lhs));
        add("sdiv", SatOp::Div, width, false, false, DropZeroRhs(SatCross(lhs, lhs)));
        add("smod", SatOp::Mod, width, false, false, DropZeroRhs(SatCross(lhs, lhs)));
        add("spow", SatOp::Pow, width, false, false, SatCross(lhs, lhs));
        add("slsl", SatOp::Lsl, width, false, false, SatCross(lslLhs, lhs));
        add("sasr", SatOp::Asr, width, false, false, SatCross(lhs, lhs));
        // sneg/suneg lower to ssub/susub against IRZ (zero register).
        add("sneg", SatOp::Sub, width, false, true, SatCross(lhs, { 0 }));
        add("suadd", SatOp::UAdd, width, false, false, SatCross(lhs, lhs));
        add("susub", SatOp::USub, width, false, false, SatCross(lhs, lhs));
        add("sumul", SatOp::UMul, width, false, false, SatCross(lhs, lhs));
        add("sudiv", SatOp::UDiv, width, false, false, DropZeroRhs(SatCross(lhs, lhs)));
        add("sumod", SatOp::UMod, width, false, false, DropZeroRhs(SatCross(lhs, lhs)));
        add("suneg", SatOp::USub, width, false, true, SatCross(lhs, { 0 }));
        // sulsl/sulsr exist only in the rr form at every width (upstream
        // NewAsmParser.scala:1091-1100); SatRefW's SatOp::ULsl/ULsr cases
        // mirror Saturating::SULSL/SULSR (src/interpreter/internal/operations.h:271-293)
        // width-generically, so the narrow widths reuse the same tables —
        // the shift rhs comes from lhs, whose bit patterns exceed bitWidth-1
        // and exercise the shift clamp.
        add("sulsl", SatOp::ULsl, width, false, false, SatCross(lhs, lhs));
        add("sulsr", SatOp::ULsr, width, false, false, SatCross(lhs, lhs));
    }
    return list;
}

class CbcSpecializedSatArith : public ::testing::TestWithParam<SatArithParams> {
    void SetUp() override { DoSetUp(); }
};

TEST_P(CbcSpecializedSatArith, test)
{
    if (!CheckForAssembler()) {
        GTEST_SKIP() << "Assembler is not present";
    }
    SatArithParams params = GetParam();

    std::string path;
    Interpretation::ExecBytecodeInfo* info = nullptr;
    for (auto pair : params.pairs) {
        std::string casePath;
        std::string method;
        if (params.immForm) {
            casePath = "./generated-asm/sat-arith/" + params.name + "-" + std::to_string(params.width) + "/" +
                       hex(pair.rhs) + ".asm";
            method = "test_" + hex(pair.rhs);
        } else {
            casePath = "./generated-asm/sat-arith/" + params.name + "-" + std::to_string(params.width) + "-rr/rr.asm";
            method = "test_" + params.name + "_" + std::to_string(params.width);
        }
        SCOPED_TRACE(casePath + " lhs=" + hex(pair.lhs) + " rhs=" + hex(pair.rhs));
        if (casePath != path) {
            info = OpenAndRewrite(method, casePath);
            path = casePath;
        }
        auto code = info->code;
        // Unary ops (sneg/suneg) take the value in IR1 and subtract it
        // from the IRZ zero register.
        auto ref = params.unary ? SatRef(params.op, params.width, 0, pair.lhs)
                                : SatRef(params.op, params.width, pair.lhs, pair.rhs);
        auto res = Interpret(code, U64(pair.lhs), U64(pair.rhs));
        // Narrow results are stored as 32-bit primitives; the union upper
        // half is not guaranteed to be zeroed by the interpreter.
        if (params.width == 64) {
            EXPECT_EQ(res.u64, ref);
        } else {
            EXPECT_EQ(res.u32, static_cast<uint32_t>(ref));
        }
    }
}

INSTANTIATE_TEST_SUITE_P(CbcTest, CbcSpecializedSatArith, ::testing::ValuesIn(SatArithParamsList()));
