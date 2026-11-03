#include "cbc_test_harness.h"

enum class CmpCC { EQ, NE, LT, GE, ULT, UGE, REQ, RNE, TESTZ, TESTNZ };

static const char* CmpCCName(CmpCC cc)
{
    switch (cc) {
        case CmpCC::EQ: return "EQ";
        case CmpCC::NE: return "NE";
        case CmpCC::LT: return "LT";
        case CmpCC::GE: return "GE";
        case CmpCC::ULT: return "ULT";
        case CmpCC::UGE: return "UGE";
        case CmpCC::REQ: return "REQ";
        case CmpCC::RNE: return "RNE";
        case CmpCC::TESTZ: return "TESTZ";
        case CmpCC::TESTNZ: return "TESTNZ";
    }
    return "";
}

// Mirrors the Compare<CC::*> specializations in
// src/interpreter/internal/operations.h: EQ/NE/ULT/UGE at operations.h:637-653
// (W32) and 665-681 (W64); the signed LT/GE compare via static_cast to
// int32_t/int64_t at operations.h:641-649 and 669-677; TESTZ/TESTNZ at
// operations.h:655-663 and 683-691. The negative scci/bcci immediates reach
// these compares sign-extended from the 12-bit field (DecodeImmediate,
// operations.h:747-750) or from the literal pool (operations.h:752-755).
// REQ/RNE read the register cells as raw 64-bit references, so they compare
// the full arguments at any width (the DSL only exposes them at W64).
static bool IntCmpRef(CmpCC cc, int width, uint64_t l, uint64_t r)
{
    if (cc == CmpCC::REQ) {
        return l == r;
    }
    if (cc == CmpCC::RNE) {
        return l != r;
    }
    if (width == 32) {
        auto lu = static_cast<uint32_t>(l);
        auto ru = static_cast<uint32_t>(r);
        auto ls = static_cast<int32_t>(lu);
        auto rs = static_cast<int32_t>(ru);
        switch (cc) {
            case CmpCC::EQ: return lu == ru;
            case CmpCC::NE: return lu != ru;
            case CmpCC::LT: return ls < rs;
            case CmpCC::GE: return ls >= rs;
            case CmpCC::ULT: return lu < ru;
            case CmpCC::UGE: return lu >= ru;
            case CmpCC::TESTZ: return (lu & ru) == 0;
            case CmpCC::TESTNZ: return (lu & ru) != 0;
            default: return false;
        }
    }
    switch (cc) {
        case CmpCC::EQ: return l == r;
        case CmpCC::NE: return l != r;
        case CmpCC::LT: return static_cast<int64_t>(l) < static_cast<int64_t>(r);
        case CmpCC::GE: return static_cast<int64_t>(l) >= static_cast<int64_t>(r);
        case CmpCC::ULT: return l < r;
        case CmpCC::UGE: return l >= r;
        case CmpCC::TESTZ: return (l & r) == 0;
        case CmpCC::TESTNZ: return (l & r) != 0;
        default: return false;
    }
}

enum class FloatCmpCC { FEQ, FNE, FGE, FNGE, FLT, FNLT };

static const char* FloatCmpCCName(FloatCmpCC cc)
{
    switch (cc) {
        case FloatCmpCC::FEQ: return "FEQ";
        case FloatCmpCC::FNE: return "FNE";
        case FloatCmpCC::FGE: return "FGE";
        case FloatCmpCC::FNGE: return "FNGE";
        case FloatCmpCC::FLT: return "FLT";
        case FloatCmpCC::FNLT: return "FNLT";
    }
    return "";
}

// Mirrors Compare<CC::F*>: the negated forms are defined as !(comparison),
// so unlike FEQ/FNE they are true on NaN operands.
template <typename T> static bool FloatCmpRef(FloatCmpCC cc, T l, T r)
{
    switch (cc) {
        case FloatCmpCC::FEQ: return l == r;
        case FloatCmpCC::FNE: return l != r;
        case FloatCmpCC::FGE: return l >= r;
        case FloatCmpCC::FNGE: return !(l >= r);
        case FloatCmpCC::FLT: return l < r;
        case FloatCmpCC::FNLT: return !(l < r);
    }
    return false;
}

struct CmpBranchParams {
    std::string form;
    CmpCC cc;
    int width;
    bool immForm;
    std::vector<CheckedArithPair> pairs;
};

static std::ostream& operator<<(std::ostream& os, const CmpBranchParams& p)
{
    return os << p.form << "-" << p.width << "-" << CmpCCName(p.cc) << (p.immForm ? "-imm" : "-rr");
}

// The value tables must stay in sync with generate_cmp_branch in
// test/asm-generator/asm-generator.py.
static std::vector<uint64_t> CmpBoundaryValues(int width)
{
    if (width == 32) {
        return { 0, 1, 2, 0x7fffffff, 0x80000000, 0xffffffff };
    }
    return { 0, 1, 2, 0x7fffffffffffffff, 0x8000000000000000, 0xffffffffffffffff };
}

// The imm patterns (file names carry the unsigned bit pattern, the
// instruction spells it signed). Negative immediates are generated for both
// scci and bcci: the bcci isa_parser entry decodes the immediate through the
// uint64_t Decoder::ReadSLEB, and the scci entry widens it through an
// explicit cast since 8049baee, so the UBSan build accepts negative values
// for both (src/cbc/isa_parser.cpp).
static std::vector<uint64_t> CmpImmPatterns(int width)
{
    if (width == 32) {
        return { 0x0, 0x1, 0x2, 0x7ff, 0xffffffff, 0xfffff800 };
    }
    return { 0x0, 0x1, 0x2, 0x7ff, 0xffffffffffffffff, 0xfffffffffffff800 };
}

// bcci shares the negative patterns with scci now; the entry points differ
// only in shape (uint64_t Decoder::ReadSLEB vs the explicitly cast
// MergeLowHi merge, src/cbc/isa_parser.cpp).
static std::vector<uint64_t> CmpBccImmPatterns(int width)
{
    if (width == 32) {
        return { 0x0, 0x1, 0x2, 0x7ff, 0xffffffff, 0xfffff800 };
    }
    return { 0x0, 0x1, 0x2, 0x7ff, 0xffffffffffffffff, 0xfffffffffffff800 };
}

// Immediates outside the signed 12-bit range are literal-encoded (SCCI*L/
// BCCL*I, Emitter::SCCImm/BccImmFixup); generated for the EQ CC only. The
// negative literal patterns are width-natural (the W32 spellings are the low
// 32 bits of the W64 patterns): smin matches the smin lhs bound, so the
// EQ-taken path is covered too; -0x10000000 mirrors the positive 0x10000000.
static const std::vector<uint64_t> CmpLiteralImms = { 0x800, 0x10000000 };

static std::vector<uint64_t> CmpNegLiteralImms(int width)
{
    if (width == 32) {
        return { 0x80000000, 0xf0000000 };
    }
    return { 0x8000000000000000, 0xfffffffff0000000ull };
}

// The far (2050-nop padded) bcci form: the branch offset moves into the
// literal table (BCCI*L) while the immediate stays in-instruction and decodes
// the same way as in the near form (DecodeImmediate<ImmKind::VALUE>,
// src/interpreter/internal/operations.h:747-750). One negative immediate per
// CC: -1 has a matching lhs bound, so both the taken and the skipped paths are
// exercised.
static std::vector<uint64_t> CmpFarBccImmPatterns(int width)
{
    if (width == 32) {
        return { 0xffffffff };
    }
    return { 0xffffffffffffffff };
}

// Fake reference operands for the REQ/RNE cases.
static const std::vector<uint64_t> CmpRefValues = { 0x1000, 0x2000 };

static std::vector<CmpBranchParams> CmpBranchParamsList()
{
    std::vector<CmpBranchParams> list;
    auto add = [&](const char* form, CmpCC cc, int width, bool immForm, std::vector<CheckedArithPair> pairs) {
        list.push_back({ form, cc, width, immForm, std::move(pairs) });
    };
    for (int width : { 32, 64 }) {
        auto bounds = CmpBoundaryValues(width);
        auto cross  = Cross(bounds, bounds);
        auto imms   = CmpImmPatterns(width);
        auto bccImms = CmpBccImmPatterns(width);
        auto farBccImms = CmpFarBccImmPatterns(width);
        for (CmpCC cc :
             { CmpCC::EQ, CmpCC::NE, CmpCC::LT, CmpCC::GE, CmpCC::ULT, CmpCC::UGE, CmpCC::TESTZ, CmpCC::TESTNZ }) {
            add("scc", cc, width, false, cross);
            add("scci", cc, width, true, Cross(bounds, imms));
            add("bcc", cc, width, false, cross);
            add("bcci", cc, width, true, Cross(bounds, bccImms));
            add("far-bcci", cc, width, true, Cross(bounds, farBccImms));
            if (cc == CmpCC::EQ) {
                auto lits = CmpLiteralImms;
                auto negs = CmpNegLiteralImms(width);
                lits.insert(lits.end(), negs.begin(), negs.end());
                add("scci", cc, width, true, Cross(bounds, lits));
                add("bcci", cc, width, true, Cross(bounds, lits));
            }
        }
        if (width == 64) {
            auto refs = Cross(CmpRefValues, CmpRefValues);
            for (CmpCC cc : { CmpCC::REQ, CmpCC::RNE }) {
                add("scc", cc, width, false, refs);
                add("bcc", cc, width, false, refs);
            }
        }
    }
    return list;
}

class CbcSpecializedCmpBranch : public ::testing::TestWithParam<CmpBranchParams> {
    void SetUp() override { DoSetUp(); }
};

TEST_P(CbcSpecializedCmpBranch, test)
{
    if (!CheckForAssembler()) {
        GTEST_SKIP() << "Assembler is not present";
    }
    CmpBranchParams params = GetParam();

    std::string path;
    Interpretation::ExecBytecodeInfo* info = nullptr;
    for (auto pair : params.pairs) {
        std::string casePath;
        std::string method;
        if (params.immForm) {
            casePath = "./generated-asm/cmp-branch/" + params.form + "-" + std::to_string(params.width) + "/" +
                       CmpCCName(params.cc) + "/" + hex(pair.rhs) + ".asm";
            method = "test_" + hex(pair.rhs);
        } else {
            casePath = "./generated-asm/cmp-branch/" + params.form + "-" + std::to_string(params.width) + "/" +
                       CmpCCName(params.cc) + ".asm";
            method = "test_" + std::string(CmpCCName(params.cc));
        }
        SCOPED_TRACE(casePath + " lhs=" + hex(pair.lhs) + " rhs=" + hex(pair.rhs));
        if (casePath != path) {
            info = OpenAndRewrite(method, casePath);
            path = casePath;
        }
        auto res = Interpret(info->code, U64(pair.lhs), U64(pair.rhs));
        auto ref = IntCmpRef(params.cc, params.width, pair.lhs, pair.rhs);
        // Narrow results are stored as 32-bit primitives; the union upper
        // half is not guaranteed to be zeroed by the interpreter.
        if (params.width == 64) {
            EXPECT_EQ(res.u64, ref ? 1u : 0u);
        } else {
            EXPECT_EQ(res.u32, ref ? 1u : 0u);
        }
    }
}

INSTANTIATE_TEST_SUITE_P(CbcTest, CbcSpecializedCmpBranch, ::testing::ValuesIn(CmpBranchParamsList()));

struct FloatCmpBranchParams {
    std::string form;
    FloatCmpCC cc;
    int width;
};

static std::ostream& operator<<(std::ostream& os, const FloatCmpBranchParams& p)
{
    return os << p.form << "-" << p.width << "-" << FloatCmpCCName(p.cc);
}

// All values are exactly representable in float, so the F32 width compares
// the same numbers; NaN and the infinities survive the conversion.
static std::vector<std::pair<double, double>> FloatCmpValues()
{
    auto nan = std::numeric_limits<double>::quiet_NaN();
    auto inf = std::numeric_limits<double>::infinity();
    return {
        { 0.0, 0.0 },  { 0.0, -0.0 }, { -0.0, 0.0 }, { -0.0, -0.0 }, { 1.5, 1.5 },  { 1.5, 2.5 },
        { 2.5, 1.5 },  { -1.5, 1.5 }, { 1.5, -1.5 }, { nan, 1.0 },   { 1.0, nan },  { nan, nan },
        { inf, inf },  { inf, -inf }, { -inf, inf }, { -inf, -inf }, { inf, 1.0 },  { 1.0, inf },
        { -inf, 1.0 }, { 1.0, -inf },
    };
}

static std::vector<FloatCmpBranchParams> FloatCmpBranchParamsList()
{
    std::vector<FloatCmpBranchParams> list;
    for (int width : { 32, 64 }) {
        for (FloatCmpCC cc : { FloatCmpCC::FEQ, FloatCmpCC::FNE, FloatCmpCC::FGE, FloatCmpCC::FNGE, FloatCmpCC::FLT,
                               FloatCmpCC::FNLT }) {
            list.push_back({ "fscc", cc, width });
            list.push_back({ "fbcc", cc, width });
        }
    }
    return list;
}

class CbcSpecializedFloatCmpBranch : public ::testing::TestWithParam<FloatCmpBranchParams> {
    void SetUp() override { DoSetUp(); }
};

TEST_P(CbcSpecializedFloatCmpBranch, test)
{
    if (!CheckForAssembler()) {
        GTEST_SKIP() << "Assembler is not present";
    }
    FloatCmpBranchParams params = GetParam();

    auto casePath = "./generated-asm/cmp-branch/" + params.form + "-" + std::to_string(params.width) + "/" +
                    FloatCmpCCName(params.cc) + ".asm";
    auto code = OpenAndRewrite("test_" + std::string(FloatCmpCCName(params.cc)), casePath)->code;
    for (auto [l, r] : FloatCmpValues()) {
        SCOPED_TRACE(casePath + " l=" + hex(F64(l).u64) + " r=" + hex(F64(r).u64));
        if (params.width == 32) {
            auto fl = static_cast<float>(l);
            auto fr = static_cast<float>(r);
            auto res = Interpret(code, U64(0), U64(0), F32(fl), F32(fr));
            EXPECT_EQ(res.u32, FloatCmpRef(params.cc, fl, fr) ? 1u : 0u);
        } else {
            auto res = Interpret(code, U64(0), U64(0), F64(l), F64(r));
            EXPECT_EQ(res.u64, FloatCmpRef(params.cc, l, r) ? 1u : 0u);
        }
    }
}

INSTANTIATE_TEST_SUITE_P(CbcTest, CbcSpecializedFloatCmpBranch, ::testing::ValuesIn(FloatCmpBranchParamsList()));

TEST_ASM(CbcTest, CmpBranchJmpForms)
{
    {
        auto code = OpenAndRewrite("test_forward", "./generated-asm/cmp-branch/jmp/forward.asm")->code;
        ASSERT_EQ(Interpret(code, U64(5), U64(0)).u64, 1);
    }
    {
        auto code = OpenAndRewrite("test_loop", "./generated-asm/cmp-branch/jmp/loop.asm")->code;
        for (uint64_t n : { 1, 2, 3, 5, 10 }) {
            SCOPED_TRACE("loop n=" + hex(n));
            ASSERT_EQ(Interpret(code, U64(n), U64(0)).u64, n * (n + 1) / 2);
        }
    }
    {
        auto code = OpenAndRewrite("test_loop_jmp", "./generated-asm/cmp-branch/jmp/loop-jmp.asm")->code;
        for (uint64_t n : { 1, 2, 5, 10 }) {
            SCOPED_TRACE("loop-jmp n=" + hex(n));
            ASSERT_EQ(Interpret(code, U64(n), U64(0)).u64, n * (n + 1) / 2);
        }
    }
}

TEST_ASM(CbcTest, CmpBranchWideForms)
{
    {
        auto code = OpenAndRewrite("test_wbcc32", "./generated-asm/cmp-branch/wide/w-bcc-32.asm")->code;
        ASSERT_EQ(Interpret(code, U32(1), U32(1)).u32, 1);
        ASSERT_EQ(Interpret(code, U32(1), U32(2)).u32, 0);
    }
    {
        auto code = OpenAndRewrite("test_wbcc64", "./generated-asm/cmp-branch/wide/w-bcc-64.asm")->code;
        ASSERT_EQ(Interpret(code, U64(2), U64(1)).u64, 1);
        ASSERT_EQ(Interpret(code, U64(2), U64(2)).u64, 1);
        ASSERT_EQ(Interpret(code, U64(1), U64(2)).u64, 0);
    }
    {
        auto code = OpenAndRewrite("test_wbcci64", "./generated-asm/cmp-branch/wide/w-bcci-64.asm")->code;
        ASSERT_EQ(Interpret(code, U64(5), U64(0)).u64, 1);
        ASSERT_EQ(Interpret(code, U64(0x7ff), U64(0)).u64, 0);
    }
    {
        auto code = OpenAndRewrite("test_wfbcc32", "./generated-asm/cmp-branch/wide/w-fbcc-32.asm")->code;
        ASSERT_EQ(Interpret(code, U64(0), U64(0), F32(1.0f), F32(1.0f)).u32, 1);
        ASSERT_EQ(Interpret(code, U64(0), U64(0), F32(1.0f), F32(2.0f)).u32, 0);
        ASSERT_EQ(Interpret(code, U64(0), U64(0), F32(std::numeric_limits<float>::quiet_NaN()), F32(1.0f)).u32, 0);
    }
    {
        auto code = OpenAndRewrite("test_wfbcc64", "./generated-asm/cmp-branch/wide/w-fbcc-64.asm")->code;
        ASSERT_EQ(Interpret(code, U64(0), U64(0), F64(1.0), F64(2.0)).u64, 1);
        ASSERT_EQ(Interpret(code, U64(0), U64(0), F64(2.0), F64(1.0)).u64, 0);
        ASSERT_EQ(Interpret(code, U64(0), U64(0), F64(std::numeric_limits<double>::quiet_NaN()), F64(1.0)).u64, 1);
    }
    {
        auto code = OpenAndRewrite("test_wjmp", "./generated-asm/cmp-branch/wide/w-jmp.asm")->code;
        ASSERT_EQ(Interpret(code, U64(5), U64(0)).u64, 1);
    }
}

TEST_ASM(CbcTest, CmpBranchFarForms)
{
    {
        auto code = OpenAndRewrite("test_bcc_far_32", "./generated-asm/cmp-branch/far/bcc-32.asm")->code;
        ASSERT_EQ(Interpret(code, U32(1), U32(1)).u32, 2);
        ASSERT_EQ(Interpret(code, U32(1), U32(2)).u32, 0);
    }
    {
        auto code = OpenAndRewrite("test_bcc_far_64", "./generated-asm/cmp-branch/far/bcc-64.asm")->code;
        ASSERT_EQ(Interpret(code, U64(1), U64(1)).u64, 2);
        ASSERT_EQ(Interpret(code, U64(1), U64(2)).u64, 0);
    }
    {
        auto code = OpenAndRewrite("test_bcci_far_32", "./generated-asm/cmp-branch/far/bcci-32.asm")->code;
        ASSERT_EQ(Interpret(code, U32(1), U32(0)).u32, 2);
        ASSERT_EQ(Interpret(code, U32(2), U32(0)).u32, 0);
    }
    {
        auto code = OpenAndRewrite("test_bcci_far", "./generated-asm/cmp-branch/far/bcci-64.asm")->code;
        ASSERT_EQ(Interpret(code, U64(1), U64(0)).u64, 2);
        ASSERT_EQ(Interpret(code, U64(2), U64(0)).u64, 0);
    }
    // Literal negative immediate + literal offset (BCCL*L): the EQ branch is
    // taken exactly on lhs == -0x10000000.
    {
        auto code = OpenAndRewrite("test_bcci_neg_lit_far_32", "./generated-asm/cmp-branch/far/bcci-neg-lit-32.asm")->code;
        ASSERT_EQ(Interpret(code, U32(0xf0000000), U32(0)).u32, 2);
        ASSERT_EQ(Interpret(code, U32(1), U32(0)).u32, 0);
    }
    {
        auto code = OpenAndRewrite("test_bcci_neg_lit_far", "./generated-asm/cmp-branch/far/bcci-neg-lit-64.asm")->code;
        ASSERT_EQ(Interpret(code, U64(0xfffffffff0000000ull), U64(0)).u64, 2);
        ASSERT_EQ(Interpret(code, U64(1), U64(0)).u64, 0);
    }
}
