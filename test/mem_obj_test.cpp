#include "cbc_test_harness.h"

// ---------------------------------------------------------------------------
// mem-obj: raw memory, object fields, derived pointers, generic refs, arrays
// ---------------------------------------------------------------------------

enum class RawMemKind { U8, S8, U16, S16, U32, S32, U64, Ref, F32, F64 };

struct RawMemCase {
    std::string name;
    RawMemKind kind;
    std::vector<CheckedArithPair> values;
};

static std::ostream& operator<<(std::ostream& os, const RawMemCase& c) { return os << c.name; }

// The case set must stay in sync with generate_mem_obj in
// test/asm-generator/asm-generator.py: one raw/<name>.asm file per case, each
// a st.raw/ld.raw round trip of the kind pair baked into the name. The narrow
// loads read back a full-width st.raw cell, so the expected values include the
// widening of the stored pattern.
static std::vector<RawMemCase> RawMemCases()
{
    return {
        RawMemCase { "st8_ldu8", RawMemKind::U8, { { 0x1FF, 0xFF }, { 0x80, 0x80 }, { 0x7F, 0x7F }, { 0xA5, 0xA5 } } },
        RawMemCase { "st8_lds8",
                     RawMemKind::S8,
                     { { 0xFF, 0xFFFFFFFFFFFFFFFF },
                       { 0x80, 0xFFFFFFFFFFFFFF80 },
                       { 0x7F, 0x7F },
                       { 0x1F1, 0xFFFFFFFFFFFFFFF1 } } },
        RawMemCase { "st16_ldu16", RawMemKind::U16, { { 0x12345, 0x2345 }, { 0xFFFF, 0xFFFF }, { 0x8000, 0x8000 } } },
        RawMemCase { "st16_lds16",
                     RawMemKind::S16,
                     { { 0xFFFF, 0xFFFFFFFFFFFFFFFF }, { 0x8000, 0xFFFFFFFFFFFF8000 }, { 0x7FFF, 0x7FFF } } },
        RawMemCase { "st32_ld32",
                     RawMemKind::U32,
                     { { 0x123456789, 0x23456789 }, { 0xFFFFFFFF, 0xFFFFFFFF }, { 0x80000000, 0x80000000 } } },
        RawMemCase { "st32_lds32",
                     RawMemKind::S32,
                     { { 0xFFFFFFFF, 0xFFFFFFFFFFFFFFFF },
                       { 0x80000000, 0xFFFFFFFF80000000 },
                       { 0x7FFFFFFF, 0x7FFFFFFF } } },
        RawMemCase { "st64_ld64",
                     RawMemKind::U64,
                     { { 0x123456789ABCDEF0, 0x123456789ABCDEF0 }, { 1, 1 }, { 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF } } },
        RawMemCase { "st64_ld64_multi", RawMemKind::U64, { { 0x123456789ABCDEF0, 0x123456789ABCDEF0 } } },
        RawMemCase { "stref_ldref", RawMemKind::Ref, {} },
        RawMemCase { "stf32_ldf32", RawMemKind::F32, {} },
        RawMemCase { "stf64_ldf64", RawMemKind::F64, {} },
    };
}

static std::vector<float> MemObjFloat32Values()
{
    return { 0.0f, -0.0f, 1.5f, -2.25f, std::numeric_limits<float>::infinity(),
             -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN() };
}

static std::vector<double> MemObjFloat64Values()
{
    return { 0.0, -0.0, 1.5, -2.25, std::numeric_limits<double>::infinity(),
             -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN() };
}

class CbcSpecializedRawMem : public ::testing::TestWithParam<RawMemCase> {
    void SetUp() override { DoSetUp(); }
};

TEST_P(CbcSpecializedRawMem, test)
{
    if (!CheckForAssembler()) {
        GTEST_SKIP() << "Assembler is not present";
    }
    RawMemCase params = GetParam();

    auto casePath = "./generated-asm/mem-obj/raw/" + params.name + ".asm";
    auto code     = OpenAndRewrite("test_" + params.name, casePath)->code;

    // The base is a raw address; the buffer spans the 0x1000 long-offset pair
    // of the multi case (LOAD_LONG_REC/STORE_LONG_REC).
    alignas(8) uint8_t buffer[0x1080] = { 0 };
    auto base  = reinterpret_cast<uint64_t>(buffer);
    uint64_t other = 0;

    switch (params.kind) {
        case RawMemKind::Ref:
            for (uintptr_t value : { reinterpret_cast<uintptr_t>(&other), uintptr_t { 0x1000 } }) {
                SCOPED_TRACE(casePath + " value=" + hex(value));
                EXPECT_EQ(Interpret(code, U64(base), U64(value)).u64, value);
            }
            break;
        case RawMemKind::F32:
            for (float v : MemObjFloat32Values()) {
                SCOPED_TRACE(casePath + " value=" + hex(F32(v).u32));
                testFPOps32(InterpretFPRes(code, U64(base), U64(0), F32(v), F32(0)), v);
            }
            break;
        case RawMemKind::F64:
            for (double v : MemObjFloat64Values()) {
                SCOPED_TRACE(casePath + " value=" + hex(F64(v).u64));
                testFPOps64(InterpretFPRes(code, U64(base), U64(0), F64(v), F64(0)), v);
            }
            break;
        default:
            for (auto [in, expected] : params.values) {
                SCOPED_TRACE(casePath + " in=" + hex(in));
                auto res = Interpret(code, U64(base), U64(in));
                bool fullCell = params.kind == RawMemKind::U64 || params.kind == RawMemKind::S32;
                ExpectMemValue(res, expected, fullCell);
            }
    }
}

INSTANTIATE_TEST_SUITE_P(CbcTest, CbcSpecializedRawMem, ::testing::ValuesIn(RawMemCases()));

enum class FieldCaseKind {
    RecI64, RecI32, RecI16S, RecI16U, RecI8S, RecI8U, RecF32, RecF64, RecRef,
    ObjI64, ObjI32, ObjRef,
    DerivedRecI64, DerivedObjI64, DerivedObjRef, DerivedRecF32
};

struct FieldCase {
    std::string name;
    FieldCaseKind kind;
    uint32_t offset;
    uint32_t size;
    uint64_t storedMask;
    std::vector<CheckedArithPair> values;
};

static std::ostream& operator<<(std::ostream& os, const FieldCase& c) { return os << c.name; }

// Field offsets resolved from the image layouts (Cangjie layout rules): the
// record holds f0 I64@0, f1 I32@8, f2 I16@12, f3 I8@14, f4 U8@15, f5 U16@16,
// f6 F32@20, f7 F64@24, f8 REF@32; the class holds x I64@8, y I32@16, r0 REF@24
// (ObjectHeaderSize is added to class-owner offsets). Must stay in sync with
// generate_mem_obj in test/asm-generator/asm-generator.py.
static std::vector<FieldCase> FieldCases()
{
    std::vector<FieldCase> list;
    auto add = [&](const char* name, FieldCaseKind kind, uint32_t offset, uint32_t size, uint64_t mask,
                   std::vector<CheckedArithPair> values) {
        list.push_back({ name, kind, offset, size, mask, std::move(values) });
    };
    add("rec_64", FieldCaseKind::RecI64, 0, 8, 0xFFFFFFFFFFFFFFFF,
        { { 0x123456789ABCDEF0, 0x123456789ABCDEF0 }, { 1, 1 }, { 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF } });
    add("rec_32", FieldCaseKind::RecI32, 8, 4, 0xFFFFFFFF,
        { { 0x123456789, 0x23456789 }, { 0xFFFFFFFF, 0xFFFFFFFF }, { 0x80000000, 0x80000000 } });
    add("rec_16s", FieldCaseKind::RecI16S, 12, 2, 0xFFFF,
        { { 0xFFFF, 0xFFFFFFFFFFFFFFFF }, { 0x8000, 0xFFFFFFFFFFFF8000 }, { 0x7FFF, 0x7FFF } });
    add("rec_16u", FieldCaseKind::RecI16U, 16, 2, 0xFFFF, { { 0x12345, 0x2345 }, { 0xFFFF, 0xFFFF } });
    add("rec_8s", FieldCaseKind::RecI8S, 14, 1, 0xFF,
        { { 0xFF, 0xFFFFFFFFFFFFFFFF }, { 0x80, 0xFFFFFFFFFFFFFF80 }, { 0x1F1, 0xFFFFFFFFFFFFFFF1 } });
    add("rec_8u", FieldCaseKind::RecI8U, 15, 1, 0xFF, { { 0x1FF, 0xFF }, { 0xA5, 0xA5 } });
    add("rec_f32", FieldCaseKind::RecF32, 20, 4, 0xFFFFFFFF, {});
    add("rec_f64", FieldCaseKind::RecF64, 24, 8, 0xFFFFFFFFFFFFFFFF, {});
    add("rec_ref", FieldCaseKind::RecRef, 32, 8, 0xFFFFFFFFFFFFFFFF, {});
    add("obj_64", FieldCaseKind::ObjI64, 8, 8, 0xFFFFFFFFFFFFFFFF,
        { { 0x123456789ABCDEF0, 0x123456789ABCDEF0 }, { 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF } });
    add("obj_32", FieldCaseKind::ObjI32, 16, 4, 0xFFFFFFFF,
        { { 0x123456789, 0x23456789 }, { 0x80000000, 0x80000000 } });
    add("obj_ref", FieldCaseKind::ObjRef, 24, 8, 0xFFFFFFFFFFFFFFFF, {});
    add("derived_rec_64", FieldCaseKind::DerivedRecI64, 0, 8, 0xFFFFFFFFFFFFFFFF,
        { { 0x123456789ABCDEF0, 0x123456789ABCDEF0 }, { 1, 1 }, { 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF } });
    add("derived_obj_64", FieldCaseKind::DerivedObjI64, 8, 8, 0xFFFFFFFFFFFFFFFF,
        { { 0x123456789ABCDEF0, 0x123456789ABCDEF0 }, { 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF } });
    add("derived_obj_ref", FieldCaseKind::DerivedObjRef, 24, 8, 0xFFFFFFFFFFFFFFFF, {});
    add("derived_rec_f32", FieldCaseKind::DerivedRecF32, 20, 4, 0xFFFFFFFF, {});
    return list;
}

class CbcSpecializedFields : public ::testing::TestWithParam<FieldCase> {
    void SetUp() override { DoSetUp(); }
};

TEST_P(CbcSpecializedFields, test)
{
    if (!CheckForAssembler()) {
        GTEST_SKIP() << "Assembler is not present";
    }
    FieldCase params = GetParam();

    auto casePath = "./generated-asm/mem-obj/fields/" + params.name + ".asm";
    auto code     = OpenAndRewrite("test_" + params.name, casePath)->code;

    alignas(8) uint64_t rec[8] = { 0 };
    alignas(8) uint64_t obj[4] = { 0 };
    uint64_t other = 0;
    auto recAddr   = reinterpret_cast<uint64_t>(rec);
    auto objAddr   = reinterpret_cast<uint64_t>(obj);
    bool recOwner  = params.kind != FieldCaseKind::ObjI64 && params.kind != FieldCaseKind::ObjI32 &&
                  params.kind != FieldCaseKind::ObjRef;
    auto* buffer    = recOwner ? rec : obj;
    auto bufferAddr = recOwner ? recAddr : objAddr;

    switch (params.kind) {
        case FieldCaseKind::RecF32:
            for (float v : MemObjFloat32Values()) {
                SCOPED_TRACE(casePath + " value=" + hex(F32(v).u32));
                testFPOps32(InterpretFPRes(code, U64(bufferAddr), U64(0), F32(v), F32(0)), v);
                uint32_t stored = 0;
                std::memcpy(&stored, reinterpret_cast<uint8_t*>(buffer) + params.offset, sizeof(float));
                EXPECT_EQ(stored, F32(v).u32);
            }
            break;
        case FieldCaseKind::DerivedRecF32: {
            // The f6 field sits at derived + 20; place the value into rec[3].
            auto derived = reinterpret_cast<uint64_t>(rec) + 4;
            for (float v : MemObjFloat32Values()) {
                for (uintptr_t base : { uintptr_t { 0 }, recAddr }) {
                    SCOPED_TRACE(casePath + " base=" + hex(base) + " value=" + hex(F32(v).u32));
                    testFPOps32(InterpretFPRes(code, U64(base), U64(derived), F32(v), F32(0)), v);
                }
            }
            uint32_t stored = 0;
            std::memcpy(&stored, reinterpret_cast<uint8_t*>(rec) + 24, sizeof(float));
            EXPECT_EQ(stored, F32(MemObjFloat32Values().back()).u32);
            break;
        }
        case FieldCaseKind::RecF64:
            for (double v : MemObjFloat64Values()) {
                SCOPED_TRACE(casePath + " value=" + hex(F64(v).u64));
                testFPOps64(InterpretFPRes(code, U64(bufferAddr), U64(0), F64(v), F64(0)), v);
                uint64_t stored = 0;
                std::memcpy(&stored, reinterpret_cast<uint8_t*>(rec) + params.offset, sizeof(double));
                EXPECT_EQ(stored, F64(v).u64);
            }
            break;
        case FieldCaseKind::RecRef:
        case FieldCaseKind::ObjRef:
            for (uintptr_t value : { reinterpret_cast<uintptr_t>(&other), uintptr_t { 0x1000 } }) {
                SCOPED_TRACE(casePath + " value=" + hex(value));
                EXPECT_EQ(Interpret(code, U64(bufferAddr), U64(value)).u64, value);
                uint64_t stored = 0;
                std::memcpy(&stored, reinterpret_cast<uint8_t*>(buffer) + params.offset, sizeof(uintptr_t));
                EXPECT_EQ(stored, value);
            }
            break;
        case FieldCaseKind::DerivedRecI64:
        case FieldCaseKind::DerivedObjI64: {
            // ld.derived reads [derived + offset]; base == 0 takes the LOCAL
            // path of LoadDerived/StoreDerived, a non-zero fake ref the HEAP
            // path. Both must land on the same slot.
            auto slot   = recOwner ? rec + 3 : obj + 1;
            auto derived = reinterpret_cast<uint64_t>(slot) - params.offset;
            for (auto [in, expected] : params.values) {
                for (uintptr_t base : { uintptr_t { 0 }, reinterpret_cast<uintptr_t>(rec) }) {
                    SCOPED_TRACE(casePath + " base=" + hex(base) + " in=" + hex(in));
                    EXPECT_EQ(Interpret(code, U64(base), U64(derived), U64(in)).u64, expected);
                }
            }
            break;
        }
        case FieldCaseKind::DerivedObjRef: {
            auto derived = objAddr;
            for (uintptr_t value : { reinterpret_cast<uintptr_t>(&other), uintptr_t { 0x1000 } }) {
                for (uintptr_t base : { uintptr_t { 0 }, recAddr }) {
                    SCOPED_TRACE(casePath + " base=" + hex(base) + " value=" + hex(value));
                    EXPECT_EQ(Interpret(code, U64(base), U64(derived), U64(value)).u64, value);
                }
            }
            break;
        }
        default:
            for (auto [in, expected] : params.values) {
                SCOPED_TRACE(casePath + " in=" + hex(in));
                auto res = Interpret(code, U64(bufferAddr), U64(in));
                ExpectMemValue(res, expected, params.kind == FieldCaseKind::RecI64);
                uint64_t stored = 0;
                std::memcpy(&stored, reinterpret_cast<uint8_t*>(buffer) + params.offset, params.size);
                EXPECT_EQ(stored, in & params.storedMask);
            }
    }
}

INSTANTIATE_TEST_SUITE_P(CbcTest, CbcSpecializedFields, ::testing::ValuesIn(FieldCases()));

TEST_ASM(CbcTest, MemObjGenericRef)
{
    auto code = OpenAndRewrite("test_gen_ref", "./generated-asm/mem-obj/fields/gen_ref.asm")->code;
    uint64_t slot  = 0;
    uint64_t other = 0;
    auto base      = reinterpret_cast<uintptr_t>(&slot);
    for (uintptr_t value : { reinterpret_cast<uintptr_t>(&other), uintptr_t { 0x1000 } }) {
        SCOPED_TRACE("value=" + hex(value));
        // ir3 carries the derived address written through; the TypeInfo
        // register is a plain constant inside the method.
        auto res = Interpret(code, U64(base), U64(value), U64(base));
        EXPECT_EQ(res.u64, value);
        EXPECT_EQ(slot, value);
    }
}

enum class ArrayCaseKind { U8, S8, U16, S16, I32, I64, Ref, F32, F64, Len, LeaBox };

struct ArrayCase {
    std::string name;
    ArrayCaseKind kind;
    uint32_t elemSize;
    uint64_t storedMask;
    std::vector<CheckedArithPair> values;
};

static std::ostream& operator<<(std::ostream& os, const ArrayCase& c) { return os << c.name; }

// The case set must stay in sync with generate_mem_obj in
// test/asm-generator/asm-generator.py: one arrays/<name>.asm file per case, a
// st.arr/ld.arr round trip of the kind pair (or the len/leabox forms).
static std::vector<ArrayCase> ArrayCases()
{
    return {
        ArrayCase { "u8", ArrayCaseKind::U8, 1, 0xFF, { { 0x1FF, 0xFF }, { 0x80, 0x80 }, { 0x7F, 0x7F }, { 0xA5, 0xA5 } } },
        ArrayCase { "s8",
                    ArrayCaseKind::S8,
                    1,
                    0xFF,
                    { { 0xFF, 0xFFFFFFFFFFFFFFFF }, { 0x80, 0xFFFFFFFFFFFFFF80 }, { 0x7F, 0x7F },
                      { 0x1F1, 0xFFFFFFFFFFFFFFF1 } } },
        ArrayCase { "u16", ArrayCaseKind::U16, 2, 0xFFFF, { { 0x12345, 0x2345 }, { 0xFFFF, 0xFFFF }, { 0x8000, 0x8000 } } },
        ArrayCase { "s16",
                    ArrayCaseKind::S16,
                    2,
                    0xFFFF,
                    { { 0xFFFF, 0xFFFFFFFFFFFFFFFF }, { 0x8000, 0xFFFFFFFFFFFF8000 }, { 0x7FFF, 0x7FFF } } },
        ArrayCase { "32",
                    ArrayCaseKind::I32,
                    4,
                    0xFFFFFFFF,
                    { { 0x123456789, 0x23456789 }, { 0xFFFFFFFF, 0xFFFFFFFF }, { 0x80000000, 0x80000000 } } },
        ArrayCase { "64",
                    ArrayCaseKind::I64,
                    8,
                    0xFFFFFFFFFFFFFFFF,
                    { { 0x123456789ABCDEF0, 0x123456789ABCDEF0 }, { 1, 1 }, { 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF } } },
        ArrayCase { "ref", ArrayCaseKind::Ref, 8, 0xFFFFFFFFFFFFFFFF, {} },
        ArrayCase { "f32", ArrayCaseKind::F32, 4, 0xFFFFFFFF, {} },
        ArrayCase { "f64", ArrayCaseKind::F64, 8, 0xFFFFFFFFFFFFFFFF, {} },
        ArrayCase { "len", ArrayCaseKind::Len, 8, 0, {} },
        ArrayCase { "leabox", ArrayCaseKind::LeaBox, 8, 0, {} },
    };
}

class CbcSpecializedArrays : public ::testing::TestWithParam<ArrayCase> {
    void SetUp() override { DoSetUp(); }
};

TEST_P(CbcSpecializedArrays, test)
{
    if (!CheckForAssembler()) {
        GTEST_SKIP() << "Assembler is not present";
    }
    ArrayCase params = GetParam();

    auto casePath = "./generated-asm/mem-obj/arrays/" + params.name + ".asm";
    auto code     = OpenAndRewrite("test_" + params.name, casePath)->code;

    // The fake array is [TestTypeInfo*][u64 length][elements...]; the runtime
    // constants are ObjectHeaderSize 8 and ArrayBodyOffset 16.
    TestTypeInfo arrayTi { 64, 8, {} };
    alignas(8) uint64_t fakeArray[24] = { 0 };
    fakeArray[0] = reinterpret_cast<uint64_t>(&arrayTi);
    fakeArray[1] = 42;
    uint64_t other = 0;
    constexpr uint64_t idx = 2;
    auto arrayBase = reinterpret_cast<uint64_t>(fakeArray);
    auto elemAddr  = reinterpret_cast<uint8_t*>(fakeArray) + RTSupport::MetaInfo::ArrayBodyOffset() + idx * params.elemSize;

    switch (params.kind) {
        case ArrayCaseKind::Len:
            EXPECT_EQ(Interpret(code, U64(arrayBase), U64(0)).u64, 42);
            break;
        case ArrayCaseKind::LeaBox:
            EXPECT_EQ(Interpret(code, U64(arrayBase), U64(0)).u64, arrayBase + RTSupport::MetaInfo::ObjectHeaderSize());
            break;
        case ArrayCaseKind::Ref:
            for (uintptr_t value : { reinterpret_cast<uintptr_t>(&other), uintptr_t { 0x1000 } }) {
                SCOPED_TRACE(casePath + " value=" + hex(value));
                EXPECT_EQ(Interpret(code, U64(arrayBase), U64(idx), U64(value)).u64, value);
                uint64_t stored = 0;
                std::memcpy(&stored, elemAddr, sizeof(uintptr_t));
                EXPECT_EQ(stored, value);
            }
            break;
        case ArrayCaseKind::F32:
            for (float v : MemObjFloat32Values()) {
                SCOPED_TRACE(casePath + " value=" + hex(F32(v).u32));
                testFPOps32(InterpretFPRes(code, U64(arrayBase), U64(idx), F32(v), F32(0)), v);
                uint32_t stored = 0;
                std::memcpy(&stored, elemAddr, sizeof(float));
                EXPECT_EQ(stored, F32(v).u32);
            }
            break;
        case ArrayCaseKind::F64:
            for (double v : MemObjFloat64Values()) {
                SCOPED_TRACE(casePath + " value=" + hex(F64(v).u64));
                testFPOps64(InterpretFPRes(code, U64(arrayBase), U64(idx), F64(v), F64(0)), v);
                uint64_t stored = 0;
                std::memcpy(&stored, elemAddr, sizeof(double));
                EXPECT_EQ(stored, F64(v).u64);
            }
            break;
        default:
            for (auto [in, expected] : params.values) {
                SCOPED_TRACE(casePath + " in=" + hex(in));
                auto res = Interpret(code, U64(arrayBase), U64(idx), U64(in));
                ExpectMemValue(res, expected, params.kind == ArrayCaseKind::I64);
                uint64_t stored = 0;
                std::memcpy(&stored, elemAddr, params.elemSize);
                EXPECT_EQ(stored, in & params.storedMask);
            }
    }
}

INSTANTIATE_TEST_SUITE_P(CbcTest, CbcSpecializedArrays, ::testing::ValuesIn(ArrayCases()));
