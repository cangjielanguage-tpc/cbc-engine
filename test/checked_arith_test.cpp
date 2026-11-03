#include "cbc_test_harness.h"

enum class CheckedOp { Add, Sub, Mul, Div, UAdd, USub, UMul, Pow, Lsl, Lsr, Asr, SatDiv, SatMod, SatPow, SatULsr };

struct CheckedArithResult {
    uint64_t value;
    Interpretation::Type type;
};

// Mirrors the checked binary ops in src/interpreter/internal/operations.h.
template <typename S, typename U> CheckedArithResult CheckedRefW(CheckedOp op, uint64_t l, uint64_t r)
{
    constexpr int64_t bitWidth = std::numeric_limits<U>::digits;
    S sl = static_cast<S>(l);
    S sr = static_cast<S>(r);
    U ul = static_cast<U>(l);
    U ur = static_cast<U>(r);
    S sres;
    U ures;
    switch (op) {
        case CheckedOp::Add:
            if (__builtin_add_overflow(sl, sr, &sres)) return { 0, Interpretation::Type::AddOverflowException };
            return { CheckedMakeS(sres), Interpretation::Type::Successful };
        case CheckedOp::Sub:
            if (__builtin_sub_overflow(sl, sr, &sres)) return { 0, Interpretation::Type::SubOverflowException };
            return { CheckedMakeS(sres), Interpretation::Type::Successful };
        case CheckedOp::Mul:
            if (__builtin_mul_overflow(sl, sr, &sres)) return { 0, Interpretation::Type::MulOverflowException };
            return { CheckedMakeS(sres), Interpretation::Type::Successful };
        case CheckedOp::Div:
            if (sr == 0) return { 0, Interpretation::Type::ArithmeticException };
            if (sl == std::numeric_limits<S>::min() && sr == -1) return { 0, Interpretation::Type::DivOverflowException };
            return { CheckedMakeS(static_cast<S>(sl / sr)), Interpretation::Type::Successful };
        case CheckedOp::UAdd:
            if (__builtin_add_overflow(ul, ur, &ures)) return { 0, Interpretation::Type::AddOverflowException };
            return { CheckedMakeU(ures), Interpretation::Type::Successful };
        case CheckedOp::USub:
            if (__builtin_sub_overflow(ul, ur, &ures)) return { 0, Interpretation::Type::SubOverflowException };
            return { CheckedMakeU(ures), Interpretation::Type::Successful };
        case CheckedOp::UMul:
            if (__builtin_mul_overflow(ul, ur, &ures)) return { 0, Interpretation::Type::MulOverflowException };
            return { CheckedMakeU(ures), Interpretation::Type::Successful };
        case CheckedOp::Pow: {
            S base     = sl;
            U exponent = ur;
            if (exponent == 0) {
                return { CheckedMakeS(static_cast<S>(1)), Interpretation::Type::Successful };
            }
            S result = 1;
            while (exponent != 0) {
                if (exponent & 1) {
                    if (__builtin_mul_overflow(result, base, &result)) return { 0, Interpretation::Type::PowOverflowException };
                }
                exponent >>= 1;
                if (exponent != 0) {
                    if (__builtin_mul_overflow(base, base, &base)) return { 0, Interpretation::Type::PowOverflowException };
                }
            }
            return { CheckedMakeS(result), Interpretation::Type::Successful };
        }
        case CheckedOp::Lsl:
            if (sr >= static_cast<S>(bitWidth)) return { 0, Interpretation::Type::LshOverflowException };
            if (sr < 0) return { 0, Interpretation::Type::LshNegativeShift };
            return { CheckedMakeU(static_cast<U>(ul << sr)), Interpretation::Type::Successful };
        case CheckedOp::Lsr:
            if (sr >= static_cast<S>(bitWidth)) return { 0, Interpretation::Type::RshOverflowException };
            if (sr < 0) return { 0, Interpretation::Type::RshNegativeShift };
            return { CheckedMakeU(static_cast<U>(ul >> sr)), Interpretation::Type::Successful };
        case CheckedOp::Asr:
            if (sr >= static_cast<S>(bitWidth)) return { 0, Interpretation::Type::AshOverflowException };
            if (sr < 0) return { 0, Interpretation::Type::AshNegativeShift };
            return { CheckedMakeS(static_cast<S>(sl >> sr)), Interpretation::Type::Successful };
        // The saturating imm forms (sdivi/smodi/spowi/sulsri) run through the
        // same SBINIMM* opcode families as the other saturating imm ops; the
        // interpreter delivers the immediate as a plain u64 primitive and
        // dispatches to the same SatArith/SatArithU helpers as the rr forms
        // (interpreter.h BinaryImm), so the semantics below mirror the engine
        // code exactly. Saturating ops clamp instead of raising (they never
        // produce an exception), and a zero Div/Mod rhs asserts in the engine
        // (operations.h:142/151), so the callers keep it out of the tables.
        case CheckedOp::SatDiv:
            // Saturating div is NOT clamping div: only smin / -1 saturates to
            // smax (Saturating::SDIV, operations.h:139-147), everything else
            // is plain truncating division (smin/1 = smin is left as is).
            if (sr == -1 && sl == std::numeric_limits<S>::min()) {
                return { CheckedMakeS(std::numeric_limits<S>::max()), Interpretation::Type::Successful };
            }
            return { CheckedMakeS(static_cast<S>(sl / sr)), Interpretation::Type::Successful };
        case CheckedOp::SatMod:
            // Saturating::SMOD (operations.h:148-156): smin % -1 → 0, else
            // plain truncated modulo.
            if (sr == -1 && sl == std::numeric_limits<S>::min()) {
                return { CheckedMakeS(static_cast<S>(0)), Interpretation::Type::Successful };
            }
            return { CheckedMakeS(static_cast<S>(sl % sr)), Interpretation::Type::Successful };
        case CheckedOp::SatPow: {
            // Saturating::SPOW (operations.h:157-201): signed base, unsigned
            // exponent; bases with |base| <= 1 are special-cased (0^0 = 1),
            // the rest multiplies a wide accumulator while it stays in range
            // and saturates by the sign of the escaped value, flipped by the
            // remaining exponent's parity for a negative base.
            using swide = __int128;
            constexpr S smin = std::numeric_limits<S>::min();
            constexpr S smax = std::numeric_limits<S>::max();
            S base = sl;
            U exp  = ur;
            if (base == 0) return { CheckedMakeS(static_cast<S>(exp == 0 ? 1 : 0)), Interpretation::Type::Successful };
            if (base == 1) return { CheckedMakeS(static_cast<S>(1)), Interpretation::Type::Successful };
            if (base == -1) {
                return { CheckedMakeS(static_cast<S>((exp & 1) != 0 ? -1 : 1)), Interpretation::Type::Successful };
            }
            swide acc = 1;
            while (exp != 0 && smin <= acc && acc <= smax) {
                acc *= base;
                exp--;
            }
            if (acc < smin || acc > smax) {
                bool negative = (acc < 0) != (base < 0 && (exp & 1) != 0);
                return { CheckedMakeS(negative ? smin : smax), Interpretation::Type::Successful };
            }
            return { CheckedMakeS(static_cast<S>(acc)), Interpretation::Type::Successful };
        }
        case CheckedOp::SatULsr:
            // Saturating::SULSR (operations.h:283-292): the shift is read
            // unsigned, cast to int64 and clamped to [0, bitWidth-1] — a
            // full-width shift pattern wraps to a negative int64 and clamps
            // to 0, a huge one clamps to bitWidth-1.
            {
                int64_t shift = static_cast<int64_t>(ur);
                if (shift < 0) shift = 0;
                if (shift > bitWidth - 1) shift = bitWidth - 1;
                return { CheckedMakeU(static_cast<U>(ul >> shift)), Interpretation::Type::Successful };
            }
    }
    return { 0, Interpretation::Type::Successful };
}

static CheckedArithResult CheckedRef(CheckedOp op, int width, uint64_t l, uint64_t r)
{
    switch (width) {
        case 8: return CheckedRefW<int8_t, uint8_t>(op, l, r);
        case 16: return CheckedRefW<int16_t, uint16_t>(op, l, r);
        case 32: return CheckedRefW<int32_t, uint32_t>(op, l, r);
        default: return CheckedRefW<int64_t, uint64_t>(op, l, r);
    }
}

struct CheckedArithParams {
    std::string name;
    CheckedOp op;
    int width;
    bool immForm;
    std::vector<CheckedArithPair> pairs;
};

static std::ostream& operator<<(std::ostream& os, const CheckedArithParams& p)
{
    return os << p.name << "." << p.width << (p.immForm ? "" : "-rr");
}

template <typename... Rest>
static std::vector<CheckedArithPair> Join(std::vector<CheckedArithPair> first, Rest... rest)
{
    (first.insert(first.end(), rest.begin(), rest.end()), ...);
    return first;
}

// The imm-form rhs values must match the generated file sets in
// test/resources/generated-asm/checked-arith/ (asm-generator.py).
static std::vector<CheckedArithParams> CheckedArithParamsList()
{
    std::vector<CheckedArithParams> list;
    auto add = [&](const char* name, CheckedOp op, int width, bool immForm, std::vector<CheckedArithPair> pairs) {
        list.push_back({ name, op, width, immForm, std::move(pairs) });
    };
    for (int width : { 8, 16, 32, 64 }) {
        uint64_t smax = CheckedSMax(width);
        uint64_t smin = CheckedSMin(width);
        uint64_t umax = CheckedUMax(width);
        uint64_t neg1 = umax;
        uint64_t neg2 = umax - 1;
        uint64_t widthU = static_cast<uint64_t>(width);
        uint64_t wideNeg = width == 16 ? 0x8000 : width == 32 ? 0x80000000 : ~uint64_t { 0 };

        add("caddi", CheckedOp::Add, width, true, Join(
            Cross({ 0, 1, 2, smin }, { 1, 2, 0x10 }),
            Cross({ 1, 2, smax }, { smax })));
        add("cadd", CheckedOp::Add, width, false, {
            { 0, 1 }, { 1, neg1 }, { smax, 0 }, { smin, 1 }, { neg2, neg2 },
            { smax, 1 }, { smax, smax }, { smin, neg1 }, { smin, smin } });
        add("csubi", CheckedOp::Sub, width, true, Join(
            Cross({ 0, 1, 2, 0x10 }, { 1, 2, 0x10 }),
            Cross({ smin }, { 1, 0x10, smax })));
        add("csub", CheckedOp::Sub, width, false, {
            { 0, 1 }, { smin, 0 }, { neg1, neg1 }, { smax, 1 }, { neg2, 1 },
            { smin, 1 }, { 0, smin }, { smax, neg1 } });
        add("cmuli", CheckedOp::Mul, width, true, Join(
            Cross({ 0, 1, 2, 5 }, { 2, 3 }),
            Cross({ 2, smax }, { smax })));
        add("cmul", CheckedOp::Mul, width, false, {
            { 0, 1 }, { 1, 1 }, { neg2, 3 }, { smax, 0 }, { 3, neg2 },
            { smax, smax }, { smin, smin }, { smax, 2 }, { smin, neg1 } });
        // The imm rhs file sets mirror checked_imm_rhs("cdivi", w) in
        // asm-generator.py; only divisors reaching the CDIV boundaries are
        // exercised (src/interpreter/internal/operations.h:458-468: rhs 0 →
        // ArithmeticException, smin/-1 → DivOverflowException, else plain
        // truncating division). lhs tables are per-imm:
        //   1     — identity, incl. the sign extremes (smin, -1);
        //   2     — positive divisor, truncation toward zero (-1/2 = 0);
        //   -2    — negative divisor, truncation (5/-2 = -2, smin/-2 = -smin);
        //   smax  — positive boundary divisor (smin/smax = -1); also the wide
        //           CBINI*W encoding at W16/32/64;
        //   -1    — negation divisor; ok lhs avoid smin (smin+1 → smax), the
        //           of lhs is smin → DivOverflowException. The -1 pattern
        //           therefore has files in both the ok/ and of/ dirs.
        // The rhs bit patterns spell the instruction immediates unsigned
        // (0xff... is -1); the -2 pattern is umax-1.
        add("cdivi", CheckedOp::Div, width, true, Join(
            Cross({ 0, 1, smin, neg1 }, { 1 }),
            Cross({ 0, 5, neg1, smin }, { 2 }),
            Cross({ 0, 5, smax, smin }, { neg2 }),
            Cross({ 0, 1, smax, smin }, { smax }),
            Cross({ 0, 5, smax, smin + 1 }, { neg1 }),
            Cross({ 0, 1, smin }, { 0 }),
            Cross({ smin }, { neg1 })));
        add("cuaddi", CheckedOp::UAdd, width, true, Join(
            Cross({ 0, 1, 2, 100 }, { 1, 2, 0x10 }),
            Cross({ 1, umax }, { umax })));
        add("cuadd", CheckedOp::UAdd, width, false, {
            { 0, 1 }, { 1, 2 }, { umax, 0 }, { 100, 27 },
            { umax, 1 }, { umax, umax } });
        add("cusubi", CheckedOp::USub, width, true, Join(
            Cross({ 2, 5, 100 }, { 1, 2 }),
            Cross({ 0 }, { 1, 2 })));
        add("cusub", CheckedOp::USub, width, false, {
            { 1, 1 }, { 2, 1 }, { umax, 1 }, { umax, umax },
            { 0, 1 }, { 1, 2 }, { 5, umax } });
        add("cumuli", CheckedOp::UMul, width, true, Join(
            Cross({ 0, 1, 2, 5 }, { 2, 3 }),
            Cross({ 2, umax }, { umax })));
        add("cumul", CheckedOp::UMul, width, false, {
            { 0, 1 }, { 1, 1 }, { 3, 5 }, { umax, 0 }, { umax, 1 },
            { umax, 2 }, { umax, umax } });
        auto clsli = Join(
            Cross({ 0, 1, 2, 5 }, { 0, 1, 3 }),
            Cross({ 1, 2 }, { widthU }));
        auto clsri = Join(
            Cross({ 0, 1, 2, 5 }, { 0, 1, 3 }),
            Cross({ 1, umax }, { widthU }));
        auto casri = Join(
            Cross({ 0, 1, smin }, { 0, 1, 3 }),
            Cross({ 1, smin }, { widthU }));
        if (width != 8) {
            clsli = Join(std::move(clsli), Cross({ 1, 2 }, { wideNeg }));
            clsri = Join(std::move(clsri), Cross({ 1 }, { wideNeg }));
            casri = Join(std::move(casri), Cross({ 1 }, { wideNeg }));
        }
        add("clsli", CheckedOp::Lsl, width, true, std::move(clsli));
        add("clsri", CheckedOp::Lsr, width, true, std::move(clsri));
        add("casri", CheckedOp::Asr, width, true, std::move(casri));
        add("clsl", CheckedOp::Lsl, width, false, {
            { 1, 0 }, { 1, 3 }, { 0, 5 }, { 3, 2 }, { umax, 3 },
            { 1, widthU }, { 1, neg1 } });
        add("clsr", CheckedOp::Lsr, width, false, {
            { 1, 0 }, { umax, 4 }, { 0x80, 3 }, { 0, 5 },
            { 1, widthU }, { 1, neg1 } });
        add("casr", CheckedOp::Asr, width, false, {
            { neg1, 3 }, { smin, 1 }, { 8, 2 }, { 0, 5 },
            { 1, widthU }, { 1, neg1 } });
        // cpow/cpowi mirror Checked::CPOW in
        // src/interpreter/internal/operations.h:484-509: signed base, unsigned
        // exponent; exponent 0 → 1 (so 0^0 = 1); binary exponentiation with
        // __builtin_mul_overflow on the width's signed type (NOT in 64 bits
        // with a truncation — per-width types via WidthTraits); the first
        // overflowing multiply raises PowOverflowException. For non-overflow
        // results this agrees bit-for-bit with the wrapping
        // static_cast<uint32_t>(PowWrapping(l.u32, r.u32)) of Common::POW
        // (operations.h:383), but the checked path throws instead of wrapping.
        // powHi is the positive boundary exponent (= smax): squaring base
        // overflows immediately. The imm rhs file sets mirror
        // checked_imm_rhs("cpowi", w) / CPOWI_OF_RHS in asm-generator.py.
        uint64_t powHi = width == 8    ? 0x7FULL
                         : width == 16 ? 0x7FFFULL
                         : width == 32 ? 0x7FFFFFFFULL
                                       : 0x7FFFFFFFFFFFFFFFULL;
        add("cpowi", CheckedOp::Pow, width, true, Join(
            Cross({ 0, 1, 2, 5, neg2 }, { 2, 3 }),
            Cross({ 2, 3 }, { widthU, powHi })));
        add("cpow", CheckedOp::Pow, width, false, {
            { 0, 5 }, { 1, 100 }, { neg1, 3 }, { neg1, 4 }, { 2, 3 }, { neg2, 3 },
            { 2, widthU }, { 3, widthU }, { neg2, widthU } });
        // sdivi/smodi/spowi/sulsri are the saturating imm forms (upstream
        // 854529c2). They run through the SBINIMM{8,16,32,64} opcode families
        // (probed: `op.<w> IRd, IRl, imm` at widths 8/16/32/64, negative
        // spellings included; imms beyond the signed 12-bit range select the
        // wide SBINIMM*W encodings, W8 has no wide-imm case) and dispatch to
        // the same SatArith/SatArithU helpers as the rr forms covered by the
        // sat-arith suite — CheckedRefW mirrors Saturating::SDIV/SMOD/SPOW/
        // SULSR (src/interpreter/internal/operations.h:139-201, 283-292).
        // Saturating ops clamp instead of raising, so every case is
        // Successful and only the ok/ flavor dirs exist; the imm rhs tables
        // exclude 0x0 for Div/Mod (the interpreter asserts, operations.h:
        // 142/151) and mirror checked_imm_rhs in asm-generator.py; only
        // boundary-reaching lhs values are crossed per imm:
        //   1     — identity, incl. the sign extremes (smin/1 = smin, which
        //           shows saturating div is not clamping div);
        //   2     — positive divisor, truncation toward zero (-1/2 = 0);
        //   -2    — negative divisor, truncation (5/-2 = -2, smin/-2 = 2^(w-2));
        //   smax  — positive boundary divisor/exponent; also the wide
        //           SBINIMM*W encoding at W16/32/64;
        //   -1    — negation divisor: smin/-1 saturates to smax (the only
        //           saturating div case), smin+1/-1 = smax exactly; smin%-1
        //           → 0;
        //   0/1   — SPOW's |base| <= 1 special cases (0^0 = 1);
        //   2/3   — squaring/cubing with negative bases: smax^2/smin^2 → smax,
        //           smin^3 → smin (both saturation signs);
        //   w     — 2^w saturates; (-1)^w = 1;
        //   umax  — huge unsigned exponent/shift (odd for (-1)^umax = -1);
        //           at W64 the shift bit pattern casts to a negative int64
        //           and clamps to 0; at W8 the imm stays within [-128, 255].
        add("sdivi", CheckedOp::SatDiv, width, true, Join(
            Cross({ 0, 1, smin, neg1 }, { 1 }),
            Cross({ 0, 5, neg1, smin }, { 2 }),
            Cross({ 0, 5, smax, smin }, { neg2 }),
            Cross({ 0, 1, smax, smin }, { smax }),
            Cross({ 0, 5, smax, smin, smin + 1 }, { neg1 })));
        add("smodi", CheckedOp::SatMod, width, true, Join(
            Cross({ 0, 5, smin, neg1 }, { 1 }),
            Cross({ 0, 5, neg1, smin }, { 2 }),
            Cross({ 0, 5, neg1, smin }, { neg2 }),
            Cross({ 0, 5, smax, smin }, { smax }),
            Cross({ 0, 5, smax, smin }, { neg1 })));
        add("spowi", CheckedOp::SatPow, width, true, Join(
            Cross({ 0, 1, 2, neg1, smin }, { 0, 1 }),
            Cross({ 2, neg2, smax, smin }, { 2, 3 }),
            Cross({ 2, neg1, neg2 }, { widthU }),
            Cross({ 2, neg1, smin }, { umax })));
        add("sulsri", CheckedOp::SatULsr, width, true, Join(
            Cross({ 0, 1, 5, smax, smin, neg1 }, { 0 }),
            Cross({ 1, 5, smax, smin, neg1 }, { 1 }),
            Cross({ 5, smin, neg1 }, { 3 }),
            Cross({ smax, smin, neg1 }, { widthU - 1 }),
            Cross({ 1, smin, neg1 }, { widthU }),
            Cross({ 1, neg1 }, { umax })));
    }
    return list;
}

class CbcSpecializedCheckedArith : public ::testing::TestWithParam<CheckedArithParams> {
    void SetUp() override { DoSetUp(); }
};

TEST_P(CbcSpecializedCheckedArith, test)
{
    if (!CheckForAssembler()) {
        GTEST_SKIP() << "Assembler is not present";
    }
    CheckedArithParams params = GetParam();

    std::string path;
    Interpretation::ExecBytecodeInfo* info = nullptr;
    for (auto pair : params.pairs) {
        auto ref = CheckedRef(params.op, params.width, pair.lhs, pair.rhs);
        auto flavor = ref.type == Interpretation::Type::Successful ? "ok" : "of";
        std::string casePath;
        std::string method;
        if (params.immForm) {
            casePath = "./generated-asm/checked-arith/" + params.name + "-" + std::to_string(params.width) + "-" +
                       flavor + "/" + hex(pair.rhs) + ".asm";
            method = "test_" + hex(pair.rhs);
        } else {
            casePath = "./generated-asm/checked-arith/" + params.name + "-" + std::to_string(params.width) + "-rr/rr.asm";
            method = "test_" + params.name + "_" + std::to_string(params.width);
        }
        SCOPED_TRACE(casePath + " lhs=" + hex(pair.lhs));
        if (casePath != path) {
            info   = OpenAndRewrite(method, casePath);
            path   = casePath;
        }
        auto code = info->code;
        if (ref.type == Interpretation::Type::Successful) {
            EXPECT_EQ(InterpretThrows(code, U64(pair.lhs), U64(pair.rhs)), Interpretation::Type::Successful);
            auto res = Interpret(code, U64(pair.lhs), U64(pair.rhs));
            // Narrow results are stored as 32-bit primitives; the union upper
            // half is not guaranteed to be zeroed by the interpreter.
            if (params.width == 64) {
                EXPECT_EQ(res.u64, ref.value);
            } else {
                EXPECT_EQ(res.u32, static_cast<uint32_t>(ref.value));
            }
        } else {
            EXPECT_EQ(InterpretThrows(code, U64(pair.lhs), U64(pair.rhs)), ref.type);
        }
    }
}

INSTANTIATE_TEST_SUITE_P(CbcTest, CbcSpecializedCheckedArith, ::testing::ValuesIn(CheckedArithParamsList()));
