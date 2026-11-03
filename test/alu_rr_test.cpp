#include "cbc_test_harness.h"

enum class AluOp { Add, Sub, Mul, SDiv, SRem, UDiv, URem, And, Or, Xor, Lsl, Lsr, Asr, Pow, Neg };

// Mirrors the Common binary ops of Arith<W32>/Arith<W64> in
// src/interpreter/internal/operations.h: unsigned wrapping arithmetic, shift
// amounts masked to the width, and the SDIV/SREM INT_MIN/-1 special cases,
// which return the dividend/zero instead of raising. A zero divisor never
// reaches the reference (the value tables keep it out; the interpreter FATALs
// on the HALT path it triggers).
template <typename S, typename U> uint64_t AluRefW(AluOp op, uint64_t l, uint64_t r)
{
    constexpr U shiftMask = sizeof(U) * 8 - 1;
    U ul = static_cast<U>(l);
    U ur = static_cast<U>(r);
    S sl = static_cast<S>(l);
    S sr = static_cast<S>(r);
    switch (op) {
        case AluOp::Add: return static_cast<uint64_t>(static_cast<U>(ul + ur));
        case AluOp::Sub: return static_cast<uint64_t>(static_cast<U>(ul - ur));
        case AluOp::Mul: return static_cast<uint64_t>(static_cast<U>(ul * ur));
        case AluOp::And: return static_cast<uint64_t>(static_cast<U>(ul & ur));
        case AluOp::Or: return static_cast<uint64_t>(static_cast<U>(ul | ur));
        case AluOp::Xor: return static_cast<uint64_t>(static_cast<U>(ul ^ ur));
        case AluOp::Lsl: return static_cast<uint64_t>(static_cast<U>(ul << (ur & shiftMask)));
        case AluOp::Lsr: return static_cast<uint64_t>(static_cast<U>(ul >> (ur & shiftMask)));
        case AluOp::Asr: return static_cast<uint64_t>(static_cast<U>(static_cast<S>(sl >> (ur & shiftMask))));
        case AluOp::UDiv: return static_cast<uint64_t>(static_cast<U>(ul / ur));
        case AluOp::URem: return static_cast<uint64_t>(static_cast<U>(ul % ur));
        case AluOp::SDiv:
            if (sl == std::numeric_limits<S>::min() && sr == -1) {
                return static_cast<uint64_t>(ul);
            }
            return static_cast<uint64_t>(static_cast<U>(static_cast<S>(sl / sr)));
        case AluOp::SRem:
            if (sl == std::numeric_limits<S>::min() && sr == -1) {
                return 0;
            }
            return static_cast<uint64_t>(static_cast<U>(static_cast<S>(sl % sr)));
        case AluOp::Pow: {
            // PowWrapping multiplies in 64 bits even at W32 (zero-extended
            // operands); only the final result is truncated back.
            uint64_t base   = ul;
            uint64_t exp    = ur;
            uint64_t result = 1;
            while (exp > 0) {
                if (exp & 1) {
                    result *= base;
                }
                base *= base;
                exp >>= 1;
            }
            return static_cast<uint64_t>(static_cast<U>(result));
        }
        case AluOp::Neg: return static_cast<uint64_t>(static_cast<U>(U { 0 } - ur));
    }
    return 0;
}

static uint64_t AluRef(AluOp op, int width, uint64_t l, uint64_t r)
{
    switch (width) {
        case 32: return AluRefW<int32_t, uint32_t>(op, l, r);
        default: return AluRefW<int64_t, uint64_t>(op, l, r);
    }
}

struct AluRrParams {
    std::string name;
    AluOp op;
    int width;
    bool immForm;
    bool unary;
    std::vector<CheckedArithPair> pairs;
};

static std::ostream& operator<<(std::ostream& os, const AluRrParams& p)
{
    return os << p.name << "." << p.width << (p.immForm ? "-imm" : "-rr");
}

// The value tables keep zero (and a zero low 32-bit half at W32) out of the
// rhs so that div/rem never hit the interpreter HALT path.
static std::vector<uint64_t> AluLhsValues(int width)
{
    if (width == 32) {
        return { 0, 1, 2, 3, 0x7fffffff, 0x80000000, 0xfffffffe, 0xffffffff };
    }
    return { 0, 1, 2, 3, 0x7fffffffffffffff, 0x8000000000000000, 0xfffffffffffffffe, 0xffffffffffffffff };
}

static std::vector<uint64_t> AluRhsValues(int width)
{
    if (width == 32) {
        return { 1, 2, 3, 0x7fffffff, 0x80000000, 0xffffffff, 0x10000, 0x23456789 };
    }
    return { 1, 2, 3, 0x7fffffffffffffff, 0x8000000000000000, 0xffffffffffffffff, 0x100000000, 0xabcdef012345678 };
}

// The imm file set must stay in sync with ALU_POWI_IMMS in
// test/asm-generator/asm-generator.py.
static const std::vector<uint64_t> AluPowiImms = { 0x0, 0x1, 0x2, 0x3, 0x7ff, 0x800 };

static std::vector<AluRrParams> AluRrParamsList()
{
    std::vector<AluRrParams> list;
    auto add = [&](const char* name, AluOp op, int width, bool immForm, bool unary, std::vector<CheckedArithPair> pairs) {
        list.push_back({ name, op, width, immForm, unary, std::move(pairs) });
    };
    for (int width : { 32, 64 }) {
        auto lhs = AluLhsValues(width);
        auto rhs = AluRhsValues(width);
        add("add", AluOp::Add, width, false, false, Cross(lhs, rhs));
        add("sub", AluOp::Sub, width, false, false, Cross(lhs, rhs));
        add("mul", AluOp::Mul, width, false, false, Cross(lhs, rhs));
        add("div", AluOp::SDiv, width, false, false, Cross(lhs, rhs));
        add("rem", AluOp::SRem, width, false, false, Cross(lhs, rhs));
        add("udiv", AluOp::UDiv, width, false, false, Cross(lhs, rhs));
        add("urem", AluOp::URem, width, false, false, Cross(lhs, rhs));
        add("and", AluOp::And, width, false, false, Cross(lhs, rhs));
        add("or", AluOp::Or, width, false, false, Cross(lhs, rhs));
        add("xor", AluOp::Xor, width, false, false, Cross(lhs, rhs));
        add("lsl", AluOp::Lsl, width, false, false, Cross(lhs, rhs));
        add("lsr", AluOp::Lsr, width, false, false, Cross(lhs, rhs));
        add("asr", AluOp::Asr, width, false, false, Cross(lhs, rhs));
        add("pow", AluOp::Pow, width, false, false, Cross(lhs, rhs));
        add("neg", AluOp::Neg, width, false, true, Cross(lhs, { 0 }));
        add("powi", AluOp::Pow, width, true, false, Cross(lhs, AluPowiImms));
    }
    return list;
}

class CbcSpecializedAluRr : public ::testing::TestWithParam<AluRrParams> {
    void SetUp() override { DoSetUp(); }
};

TEST_P(CbcSpecializedAluRr, test)
{
    if (!CheckForAssembler()) {
        GTEST_SKIP() << "Assembler is not present";
    }
    AluRrParams params = GetParam();

    std::string path;
    Interpretation::ExecBytecodeInfo* info = nullptr;
    for (auto pair : params.pairs) {
        std::string casePath;
        std::string method;
        if (params.immForm) {
            casePath = "./generated-asm/alu-rr/powi-" + std::to_string(params.width) + "/" + hex(pair.rhs) + ".asm";
            method = "test_" + hex(pair.rhs);
        } else {
            casePath = "./generated-asm/alu-rr/" + params.name + "-" + std::to_string(params.width) + "-rr/rr.asm";
            method = "test_" + params.name + "_" + std::to_string(params.width);
        }
        SCOPED_TRACE(casePath + " lhs=" + hex(pair.lhs) + " rhs=" + hex(pair.rhs));
        if (casePath != path) {
            info = OpenAndRewrite(method, casePath);
            path = casePath;
        }
        auto ref = params.unary ? AluRef(params.op, params.width, 0, pair.lhs)
                                : AluRef(params.op, params.width, pair.lhs, pair.rhs);
        auto res = Interpret(info->code, U64(pair.lhs), U64(pair.rhs));
        // Narrow results are stored as 32-bit primitives; the union upper
        // half is not guaranteed to be zeroed by the interpreter.
        if (params.width == 64) {
            EXPECT_EQ(res.u64, ref);
        } else {
            EXPECT_EQ(res.u32, static_cast<uint32_t>(ref));
        }
    }
}

INSTANTIATE_TEST_SUITE_P(CbcTest, CbcSpecializedAluRr, ::testing::ValuesIn(AluRrParamsList()));

TEST_ASM(CbcTest, CbcAluRrMovi32)
{
    {
        auto code = OpenAndRewrite("test_add", "./generated-asm/alu-rr/movi-32/add.asm")->code;
        for (uint64_t v : { 0u, 1u, 0x7ffffff8u, 0xffffffffu }) {
            SCOPED_TRACE("add v=" + hex(v));
            EXPECT_EQ(Interpret(code, U64(v), U64(0)).u32, static_cast<uint32_t>(v + 7));
        }
    }
    {
        auto code = OpenAndRewrite("test_neg", "./generated-asm/alu-rr/movi-32/neg.asm")->code;
        for (uint64_t v : { 0u, 1u, 0x80000000u, 0xffffffffu }) {
            SCOPED_TRACE("neg v=" + hex(v));
            EXPECT_EQ(Interpret(code, U64(v), U64(0)).u32, static_cast<uint32_t>(0 - v));
        }
    }
}
