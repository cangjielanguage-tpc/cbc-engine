#include "cbc_test_harness.h"

// ---------------------------------------------------------------------------
// atomics: atomic loads/stores, fetch ops, swap and CAS on a fake object ref
// ---------------------------------------------------------------------------

enum class AtomicsGroup { Ld, St, Fetch, Swap, Cas };

enum class AtomicsFetchOp { Add, Sub, And, Or, Xor };

static const char* AtomicsFetchOpName(AtomicsFetchOp op)
{
    switch (op) {
        case AtomicsFetchOp::Add: return "add";
        case AtomicsFetchOp::Sub: return "sub";
        case AtomicsFetchOp::And: return "and";
        case AtomicsFetchOp::Or: return "or";
        case AtomicsFetchOp::Xor: return "xor";
    }
    return "";
}

// The kind table must stay in sync with generate_atomics in
// test/asm-generator/asm-generator.py: kind name -> field offset in the
// atm:Cell record (declaration-order layout with natural alignment: f8s@0,
// f8u@1, f16s@2, f16u@4, f32@8, f64@16) and the value mask of the width.
struct AtomicsKind {
    const char* name;
    uint32_t offset;
    uint64_t mask;
    int width;
};

static const AtomicsKind AtomicsKindTable[] = {
    { "8s", 0, 0xFF, 8 },
    { "8u", 1, 0xFF, 8 },
    { "16s", 2, 0xFFFF, 16 },
    { "16u", 4, 0xFFFF, 16 },
    { "32", 8, 0xFFFFFFFF, 32 },
    { "64", 16, 0xFFFFFFFFFFFFFFFF, 64 },
};

static std::vector<uint64_t> AtomicsValues(int width)
{
    switch (width) {
        case 8: return { 0x00, 0x01, 0x7F, 0x80, 0xFF };
        case 16: return { 0x0000, 0x0001, 0x7FFF, 0x8000, 0xFFFF };
        case 32: return { 0x00000000, 0x00000001, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF };
        default:
            return { 0x0000000000000000, 0x0000000000000001, 0x7FFFFFFFFFFFFFFF, 0x8000000000000000,
                     0xFFFFFFFFFFFFFFFF };
    }
}

// Mirrors AtomicFetch<uintN_t>/AtomicSwapPrim<uintN_t>/AtomicStorePrim<uintN_t>
// of src/interpreter/interpreter.h: the rhs is truncated to the unsigned width
// and the arithmetic wraps (the RT fetch cases always use uintN_t, so narrow
// results are zero-extended).
static uint64_t AtomicsFetchRef(AtomicsFetchOp op, const AtomicsKind& kind, uint64_t v0, uint64_t r)
{
    uint64_t rhs = r & kind.mask;
    switch (op) {
        case AtomicsFetchOp::Add: return (v0 + rhs) & kind.mask;
        case AtomicsFetchOp::Sub: return (v0 - rhs) & kind.mask;
        case AtomicsFetchOp::And: return v0 & rhs;
        case AtomicsFetchOp::Or: return (v0 | rhs) & kind.mask;
        case AtomicsFetchOp::Xor: return (v0 ^ rhs) & kind.mask;
    }
    return 0;
}

struct AtomicsParams {
    std::string name;
    AtomicsGroup group;
    AtomicsFetchOp fetchOp;
    AtomicsKind kind;
    bool casOk;
};

static std::ostream& operator<<(std::ostream& os, const AtomicsParams& p) { return os << p.name; }

// The file/method names must stay in sync with generate_atomics in
// test/asm-generator/asm-generator.py.
static std::vector<AtomicsParams> AtomicsParamsList()
{
    std::vector<AtomicsParams> list;
    auto kindName = [](const AtomicsKind& kind) { return std::string(kind.name); };
    for (auto kind : AtomicsKindTable) {
        list.push_back({ "ld-" + kindName(kind), AtomicsGroup::Ld, AtomicsFetchOp::Add, kind, false });
        list.push_back({ "st-" + kindName(kind), AtomicsGroup::St, AtomicsFetchOp::Add, kind, false });
        list.push_back({ "swap-" + kindName(kind), AtomicsGroup::Swap, AtomicsFetchOp::Add, kind, false });
        for (auto op : { AtomicsFetchOp::Add, AtomicsFetchOp::Sub, AtomicsFetchOp::And, AtomicsFetchOp::Or,
                         AtomicsFetchOp::Xor }) {
            list.push_back(
                { std::string("fetch-") + AtomicsFetchOpName(op) + "-" + kindName(kind), AtomicsGroup::Fetch, op, kind,
                  false }
            );
        }
        list.push_back({ "cas-" + kindName(kind) + "-ok", AtomicsGroup::Cas, AtomicsFetchOp::Add, kind, true });
        list.push_back({ "cas-" + kindName(kind) + "-fail", AtomicsGroup::Cas, AtomicsFetchOp::Add, kind, false });
    }
    return list;
}

static void AtomicsSeedBuffer(uint64_t* buffer, const AtomicsKind& kind, uint64_t value)
{
    auto masked = value & kind.mask;
    std::memcpy(reinterpret_cast<uint8_t*>(buffer) + kind.offset, &masked, kind.width / 8);
}

static uint64_t AtomicsBufferValue(const uint64_t* buffer, const AtomicsKind& kind)
{
    uint64_t stored = 0;
    std::memcpy(&stored, reinterpret_cast<const uint8_t*>(buffer) + kind.offset, kind.width / 8);
    return stored;
}

class CbcSpecializedAtomics : public ::testing::TestWithParam<AtomicsParams> {
    void SetUp() override { DoSetUp(); }
};

TEST_P(CbcSpecializedAtomics, test)
{
    if (!CheckForAssembler()) {
        GTEST_SKIP() << "Assembler is not present";
    }
    AtomicsParams params = GetParam();

    std::string kind = params.kind.name;
    std::string casePath;
    std::string method;
    switch (params.group) {
        case AtomicsGroup::Ld:
            casePath = "./generated-asm/atomics/ld/" + kind + ".asm";
            method   = "test_ld_" + kind;
            break;
        case AtomicsGroup::St:
            casePath = "./generated-asm/atomics/st/" + kind + ".asm";
            method   = "test_st_" + kind;
            break;
        case AtomicsGroup::Fetch:
            casePath = "./generated-asm/atomics/fetch/" + std::string(AtomicsFetchOpName(params.fetchOp)) + "-" + kind +
                       ".asm";
            method = "test_fetch_" + std::string(AtomicsFetchOpName(params.fetchOp)) + "_" + kind;
            break;
        case AtomicsGroup::Swap:
            casePath = "./generated-asm/atomics/swap/" + kind + ".asm";
            method   = "test_swap_" + kind;
            break;
        case AtomicsGroup::Cas:
            casePath = "./generated-asm/atomics/cas/" + kind + (params.casOk ? "-ok" : "-fail") + ".asm";
            method   = "test_cas_" + kind + (params.casOk ? "_ok" : "_fail");
            break;
    }
    auto code = OpenAndRewrite(method, casePath)->code;

    // The fake object is the buffer itself; the atomic field offsets (all below
    // the 12-bit AtomicOp window) address its bytes.
    alignas(8) uint64_t buffer[4] = { 0 };
    auto base = reinterpret_cast<uint64_t>(buffer);

    switch (params.group) {
        case AtomicsGroup::Ld:
            for (uint64_t v : AtomicsValues(params.kind.width)) {
                SCOPED_TRACE(casePath + " v=" + hex(v));
                AtomicsSeedBuffer(buffer, params.kind, v);
                auto res = Interpret(code, U64(base), U64(0));
                ExpectMemValue(res, v, params.kind.width == 64);
            }
            break;
        case AtomicsGroup::St:
            for (uint64_t v : AtomicsValues(params.kind.width)) {
                SCOPED_TRACE(casePath + " v=" + hex(v));
                AtomicsSeedBuffer(buffer, params.kind, ~v);
                auto res = Interpret(code, U64(base), U64(v));
                ExpectMemValue(res, v & params.kind.mask, params.kind.width == 64);
                EXPECT_EQ(AtomicsBufferValue(buffer, params.kind), v & params.kind.mask);
            }
            break;
        case AtomicsGroup::Fetch:
            for (uint64_t v0 : AtomicsValues(params.kind.width)) {
                for (uint64_t r : AtomicsValues(params.kind.width)) {
                    SCOPED_TRACE(casePath + " v0=" + hex(v0) + " r=" + hex(r));
                    AtomicsSeedBuffer(buffer, params.kind, v0);
                    auto res = Interpret(code, U64(base), U64(r));
                    ExpectMemValue(res, v0, params.kind.width == 64);
                    EXPECT_EQ(AtomicsBufferValue(buffer, params.kind), AtomicsFetchRef(params.fetchOp, params.kind, v0, r));
                }
            }
            break;
        case AtomicsGroup::Swap:
            for (uint64_t v0 : AtomicsValues(params.kind.width)) {
                for (uint64_t r : AtomicsValues(params.kind.width)) {
                    SCOPED_TRACE(casePath + " v0=" + hex(v0) + " r=" + hex(r));
                    AtomicsSeedBuffer(buffer, params.kind, v0);
                    auto res = Interpret(code, U64(base), U64(r));
                    ExpectMemValue(res, v0, params.kind.width == 64);
                    EXPECT_EQ(AtomicsBufferValue(buffer, params.kind), r & params.kind.mask);
                }
            }
            break;
        case AtomicsGroup::Cas:
            for (uint64_t v0 : AtomicsValues(params.kind.width)) {
                for (uint64_t r : AtomicsValues(params.kind.width)) {
                    // A failing CAS must pass an expected value that differs
                    // from the cell content after the width truncation.
                    uint64_t expected = params.casOk ? v0 : v0 ^ 1;
                    SCOPED_TRACE(casePath + " v0=" + hex(v0) + " expected=" + hex(expected) + " r=" + hex(r));
                    AtomicsSeedBuffer(buffer, params.kind, v0);
                    auto res = Interpret(code, U64(base), U64(expected), U64(r));
                    EXPECT_EQ(res.u32, params.casOk ? 1u : 0u);
                    EXPECT_EQ(AtomicsBufferValue(buffer, params.kind),
                              params.casOk ? r & params.kind.mask : v0 & params.kind.mask);
                }
            }
            break;
    }
}

INSTANTIATE_TEST_SUITE_P(CbcTest, CbcSpecializedAtomics, ::testing::ValuesIn(AtomicsParamsList()));
