#include "cbc_test_harness.h"

enum class FrameSlotKind { Int, Ref, Fp };

struct FrameSlotPair {
    uint64_t in;
    uint64_t expected;
};

struct FrameSlotCase {
    std::string name;
    FrameSlotKind kind;
    std::vector<FrameSlotPair> values;
};

static std::ostream& operator<<(std::ostream& os, const FrameSlotCase& c) { return os << c.name; }

// The case set must stay in sync with generate_frame_slots in
// test/asm-generator/asm-generator.py: one rt/<name>.asm file per case.
// The narrow loads read back a full st.uslot.64 cell, st16/st32 check the
// narrow stores against matching-width loads (expected values include the
// truncation of the full-width argument), and fp/ref round-trip bit-exact.
static std::vector<FrameSlotPair> FloatSlotValues(bool f32)
{
    auto nan = f32 ? std::numeric_limits<float>::quiet_NaN() : std::numeric_limits<double>::quiet_NaN();
    auto inf = f32 ? std::numeric_limits<float>::infinity() : std::numeric_limits<double>::infinity();
    std::vector<FrameSlotPair> values;
    for (double v : { 0.0, -0.0, 1.5, -2.25, static_cast<double>(inf), -static_cast<double>(inf),
                      static_cast<double>(nan) }) {
        if (f32) {
            auto bits = F32(static_cast<float>(v)).u32;
            values.push_back({ bits, bits });
        } else {
            auto bits = F64(v).u64;
            values.push_back({ bits, bits });
        }
    }
    return values;
}

static std::vector<FrameSlotCase> FrameSlotCases()
{
    return {
        FrameSlotCase { "u8", FrameSlotKind::Int, { { 0xFF, 0xFF }, { 0x80, 0x80 }, { 0xA5, 0xA5 }, { 0x1FF, 0xFF } } },
        FrameSlotCase { "s8",
                        FrameSlotKind::Int,
                        { { 0xFF, 0xFFFFFFFFFFFFFFFF },
                          { 0x80, 0xFFFFFFFFFFFFFF80 },
                          { 0x7F, 0x7F },
                          { 0x1F1, 0xFFFFFFFFFFFFFFF1 } } },
        FrameSlotCase { "u16", FrameSlotKind::Int, { { 0xFFFF, 0xFFFF }, { 0x8000, 0x8000 }, { 0x12345, 0x2345 } } },
        FrameSlotCase { "s16",
                        FrameSlotKind::Int,
                        { { 0xFFFF, 0xFFFFFFFFFFFFFFFF }, { 0x8000, 0xFFFFFFFFFFFF8000 }, { 0x7FFF, 0x7FFF } } },
        FrameSlotCase { "32",
                        FrameSlotKind::Int,
                        { { 0xFFFFFFFF, 0xFFFFFFFF }, { 0x80000000, 0x80000000 }, { 0x123456789, 0x23456789 } } },
        FrameSlotCase { "64",
                        FrameSlotKind::Int,
                        { { 0x123456789ABCDEF0, 0x123456789ABCDEF0 },
                          { 0x8000000000000000, 0x8000000000000000 },
                          { 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF },
                          { 0x1, 0x1 } } },
        FrameSlotCase { "st16",
                        FrameSlotKind::Int,
                        { { 0x123456789ABCDEF0, 0xDEF0 },
                          { 0x800000000000FFFF, 0xFFFF },
                          { 0x1000000000008000, 0x8000 } } },
        FrameSlotCase { "st32",
                        FrameSlotKind::Int,
                        { { 0x123456789ABCDEF0, 0x9ABCDEF0 },
                          { 0xFFFFFFFF12345678, 0x12345678 },
                          { 0x100000000, 0 } } },
        FrameSlotCase { "ref", FrameSlotKind::Ref, {} },
        FrameSlotCase { "f32", FrameSlotKind::Fp, FloatSlotValues(true) },
        FrameSlotCase { "f64", FrameSlotKind::Fp, FloatSlotValues(false) },
    };
}

class CbcSpecializedFrameSlots : public ::testing::TestWithParam<FrameSlotCase> {
    void SetUp() override { DoSetUp(); }
};

TEST_P(CbcSpecializedFrameSlots, test)
{
    if (!CheckForAssembler()) {
        GTEST_SKIP() << "Assembler is not present";
    }
    FrameSlotCase params = GetParam();

    auto casePath = "./generated-asm/frame-slots/rt/" + params.name + ".asm";
    auto info     = OpenAndRewrite("test_" + params.name, casePath);
    constexpr auto stackSlotCount = 100;
    uint64_t frameSlots[stackSlotCount] = { 0 };
    // Slot $0 sits one STACK_SLOT_SIZE below the frame start.
    ASSERT_EQ(info->staticFrameSize, sizeof(uint64_t));
    Interpretation::Frame frame { reinterpret_cast<uintptr_t>(frameSlots + stackSlotCount) };

    switch (params.kind) {
        case FrameSlotKind::Ref: {
            uint64_t obj   = 0;
            auto addr      = reinterpret_cast<uint64_t>(&obj);
            SCOPED_TRACE(casePath);
            EXPECT_EQ(Interpret(info->code, frame, U64(addr), U64(0)).u64, addr);
            break;
        }
        case FrameSlotKind::Fp:
            for (auto [in, expected] : params.values) {
                SCOPED_TRACE(casePath + " in=" + hex(in));
                if (params.name == "f32") {
                    float f;
                    std::memcpy(&f, &in, sizeof(f));
                    EXPECT_EQ(InterpretFPRes(info->code, frame, F32(f), F32(0)).u32, static_cast<uint32_t>(expected));
                } else {
                    double d;
                    std::memcpy(&d, &in, sizeof(d));
                    EXPECT_EQ(InterpretFPRes(info->code, frame, F64(d), F64(0)).u64, expected);
                }
            }
            break;
        default:
            for (auto [in, expected] : params.values) {
                SCOPED_TRACE(casePath + " in=" + hex(in));
                auto res = Interpret(info->code, frame, U64(in), U64(0));
                // Narrow results are stored as 32-bit primitives; the union upper
                // half is not guaranteed to be zeroed by the interpreter.
                if (params.name == "64") {
                    EXPECT_EQ(res.u64, expected);
                } else {
                    EXPECT_EQ(res.u32, static_cast<uint32_t>(expected));
                }
            }
    }
}

INSTANTIATE_TEST_SUITE_P(CbcTest, CbcSpecializedFrameSlots, ::testing::ValuesIn(FrameSlotCases()));

TEST_ASM(CbcTest, FrameSlotsLong)
{
    auto info = OpenAndRewrite("test_long", "./generated-asm/frame-slots/long.asm");
    // Slot $0 lands 2080 bytes below the frame start, outside the signed
    // 12-bit offset window, so the pair must take the LONG encodings; the
    // $255 pair (-40) stays inside it.
    ASSERT_EQ(info->staticFrameSize, 260u * sizeof(uint64_t));
    constexpr auto stackSlotCount = 400;
    uint64_t frameSlots[stackSlotCount] = { 0 };
    Interpretation::Frame frame { reinterpret_cast<uintptr_t>(frameSlots + stackSlotCount) };
    ASSERT_EQ(Interpret(info->code, frame, U64(0x1234), U64(0)).u64, 0x2468);

    auto finfo = OpenAndRewrite("test_long_f", "./generated-asm/frame-slots/long.asm");
    ASSERT_EQ(finfo->staticFrameSize, 260u * sizeof(uint64_t));
    ASSERT_EQ(InterpretFPRes(finfo->code, frame, F64(42.5), F64(0)).f64, 42.5);
}

// ---------------------------------------------------------------------------
// frame-slots: st.uslot.imm (STORE_FRAME_IMM_1/2/4/8) and st.uslot.8 (ST_8)
// ---------------------------------------------------------------------------

// The case set must stay in sync with IMM_SLOT_CASES in
// test/asm-generator/asm-generator.py: one rt/<file>.asm method
// test_<file>_<value>_<read> per entry. The rewriter lowers every
// st.uslot.imm through ST_64 (isa_rewriter.cpp:2047 StoreUntypedImm), and
// StoreFrameImm (src/cbc/emitter/emitter.cpp:1040) picks the RT opcode by
// the immediate magnitude: [-2^7, 2^7-1] -> STORE_FRAME_IMM_1, then IMM_2 at
// 16 bits, IMM_4 at 32 bits, IMM_8 otherwise; the boundary values below would
// truncate on a wrong choice (the RT decode sign-extends the N-bit payload,
// src/cbc/isa_rt.h:568 StoreFrameImm). The interpreter side is
// MemoryLocation::StoreImm<uint64_t> (src/interpreter/internal/operations.h:832:
// the full 8 slot bytes are replaced with the immediate).
struct FrameImmCase {
    std::string_view file;
    std::string_view method;
    uint64_t imm;      // the stored immediate, as its 64-bit pattern
    uint64_t expected; // the read-back value
    bool narrow;       // narrow (8/16/32) read-back compared via res.u32
};

static std::ostream& operator<<(std::ostream& os, const FrameImmCase& c) { return os << c.method; }

TEST_ASM(CbcTest, FrameSlotsImm)
{
    static const FrameImmCase cases[] = {
        // imm1: [-2^7, 2^7-1] -> STORE_FRAME_IMM_1
        { "imm1", "test_imm1_m1_64", 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, false },
        { "imm1", "test_imm1_m1_32", 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFF, true },
        { "imm1", "test_imm1_m1_s8", 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFF, true },
        { "imm1", "test_imm1_m1_u8", 0xFFFFFFFFFFFFFFFF, 0x000000FF, true },
        { "imm1", "test_imm1_p127_64", 0x7F, 0x7F, false },
        // imm2: 128 leaves the 8-bit class; [-2^15, 2^15-1] -> STORE_FRAME_IMM_2
        { "imm2", "test_imm2_p128_64", 0x80, 0x80, false },
        { "imm2", "test_imm2_m4660_64", 0xFFFFFFFFFFFFEDCC, 0xFFFFFFFFFFFFEDCC, false },
        { "imm2", "test_imm2_m4660_32", 0xFFFFFFFFFFFFEDCC, 0xFFFFEDCC, true },
        { "imm2", "test_imm2_m4660_s16", 0xFFFFFFFFFFFFEDCC, 0xFFFFEDCC, true },
        { "imm2", "test_imm2_m4660_u16", 0xFFFFFFFFFFFFEDCC, 0x0000EDCC, true },
        { "imm2", "test_imm2_m4660_s8", 0xFFFFFFFFFFFFEDCC, 0xFFFFFFCC, true },
        { "imm2", "test_imm2_m4660_u8", 0xFFFFFFFFFFFFEDCC, 0x000000CC, true },
        // imm4: 32768 leaves the 16-bit class; [-2^31, 2^31-1] -> STORE_FRAME_IMM_4
        { "imm4", "test_imm4_p32768_64", 0x8000, 0x8000, false },
        { "imm4", "test_imm4_m305419896_64", 0xFFFFFFFFEDCBA988, 0xFFFFFFFFEDCBA988, false },
        { "imm4", "test_imm4_m305419896_32", 0xFFFFFFFFEDCBA988, 0xEDCBA988, true },
        { "imm4", "test_imm4_m305419896_s16", 0xFFFFFFFFEDCBA988, 0xFFFFA988, true },
        { "imm4", "test_imm4_m305419896_u16", 0xFFFFFFFFEDCBA988, 0x0000A988, true },
        { "imm4", "test_imm4_m305419896_s8", 0xFFFFFFFFEDCBA988, 0xFFFFFF88, true },
        { "imm4", "test_imm4_m305419896_u8", 0xFFFFFFFFEDCBA988, 0x00000088, true },
        // imm8: 2147483648 leaves the 32-bit class -> STORE_FRAME_IMM_8
        { "imm8", "test_imm8_p2147483648_64", 0x80000000, 0x80000000, false },
        { "imm8", "test_imm8_pattern_64", 0x123456789ABCDEF0, 0x123456789ABCDEF0, false },
        { "imm8", "test_imm8_pattern_32", 0x123456789ABCDEF0, 0x9ABCDEF0, true },
        { "imm8", "test_imm8_pattern_s8", 0x123456789ABCDEF0, 0xFFFFFFF0, true },
        { "imm8", "test_imm8_pattern_u8", 0x123456789ABCDEF0, 0x000000F0, true },
        { "imm8", "test_imm8_smin_64", 0x8000000000000000, 0x8000000000000000, false },
    };

    for (const auto& c : cases) {
        SCOPED_TRACE(c.method);
        auto path = "./generated-asm/frame-slots/rt/" + std::string(c.file) + ".asm";
        auto info = OpenAndRewrite(std::string(c.method), path);
        ASSERT_EQ(info->staticFrameSize, sizeof(uint64_t));
        constexpr auto stackSlotCount = 100;
        uint64_t frameSlots[stackSlotCount] = { 0 };
        Interpretation::Frame frame { reinterpret_cast<uintptr_t>(frameSlots + stackSlotCount) };

        auto res = Interpret(info->code, frame, U64(0), U64(0));
        if (c.narrow) {
            EXPECT_EQ(res.u32, static_cast<uint32_t>(c.expected));
        } else {
            EXPECT_EQ(res.u64, c.expected);
        }
        // The immediate store replaces all 8 slot bytes (ST_64), independent
        // of which narrower read-back the method returns through.
        EXPECT_EQ(frameSlots[stackSlotCount - 1], c.imm);
    }
}

// st.uslot.8 truncates the source to the low byte (ST_8,
// src/interpreter/internal/operations.h:827 MemoryLocation::Store<uint8_t>:
// `(uint8_t)src.u64`) while the untouched upper slot bytes survive; the
// read-backs expose the extensions: ld.uslot.64 sees them verbatim,
// ld.uslot.s8 sign-extends the byte (operations.h:837 Load<int8_t>) and
// ld.uslot.u8 zero-extends it (Load<uint8_t>). There is no ld.uslot.8
// spelling (do not attempt). The case set must stay in sync with
// FRAME_SLOT_ST8_READS in test/asm-generator/asm-generator.py.
enum class FrameSlotRead { Bits64, S8, U8 };

TEST_ASM(CbcTest, FrameSlotsByteStore)
{
    constexpr auto stackSlotCount = 100;
    uint64_t frameSlots[stackSlotCount] = { 0 };
    Interpretation::Frame frame { reinterpret_cast<uintptr_t>(frameSlots + stackSlotCount) };

    // The slot starts as all-ones, so every st.uslot.8 must replace exactly
    // its low byte and leave 0xFF...00 in the upper 7 bytes.
    for (uint64_t in : { uint64_t { 0x2A }, uint64_t { 0xFF }, uint64_t { 0x80 }, uint64_t { 0x123456789ABCDEF0 },
                         uint64_t { 0x100 } }) {
        auto byte = static_cast<uint8_t>(in);
        uint64_t slot       = 0xFFFFFFFFFFFFFF00 | byte;
        uint64_t signExtend = static_cast<uint64_t>(static_cast<int8_t>(byte));

        for (auto [method, kind] :
             { std::pair { "test_st8_64", FrameSlotRead::Bits64 }, std::pair { "test_st8_s8", FrameSlotRead::S8 },
               std::pair { "test_st8_u8", FrameSlotRead::U8 } }) {
            SCOPED_TRACE(std::string(method) + " in=" + hex(in));
            auto info = OpenAndRewrite(method, "./generated-asm/frame-slots/rt/st8.asm");
            ASSERT_EQ(info->staticFrameSize, sizeof(uint64_t));
            frameSlots[stackSlotCount - 1] = 0xFFFFFFFFFFFFFFFF;
            auto res = Interpret(info->code, frame, U64(in), U64(0));
            switch (kind) {
                case FrameSlotRead::Bits64:
                    // The untouched 0xFF upper bytes read back verbatim.
                    EXPECT_EQ(res.u64, slot);
                    break;
                case FrameSlotRead::S8:
                    // Only the stored byte participates: sign-extended on load.
                    EXPECT_EQ(res.u32, static_cast<uint32_t>(signExtend));
                    break;
                case FrameSlotRead::U8:
                    EXPECT_EQ(res.u32, byte);
                    break;
            }
            EXPECT_EQ(frameSlots[stackSlotCount - 1], slot);
        }
    }
}

// ---------------------------------------------------------------------------
// frame-slots: typed narrow fields (I8/U8/I16/U16) and F32 (st.typed/ld.typed(.f))
// ---------------------------------------------------------------------------

// The rewriter lowers ld.typed/st.typed through LOAD_FRAME/STORE_FRAME with
// the access kinds chosen by the field's declared kind (isa_rewriter.cpp:913
// LdTyped / :1002 StTyped -> isa_rewriter.cpp:54 Ldk / :79 Stk): I8 -> LD_S8,
// U8 -> LD_U8, I16 -> LD_S16, U16 -> LD_U16, F32 -> LD_F32 on the load side;
// the store side collapses the signedness (I8/U8 -> ST_8, I16/U16 -> ST_16,
// F32 -> ST_F32), so unlike the uslot pair (st.uslot.8 + ld.uslot.s8/.u8)
// there is exactly one store spelling per width and the signedness lives in
// the field declaration. The interpreter truncates on store
// (operations.h:827 MemoryLocation::Store<P>: static_cast<P>(GetPrimitive(
// src.IR()).u64)) and sign/zero-extends on load (operations.h:837 Load<P>:
// Put(IR, {u64 = static_cast<uint64_t>(P value)})); F32 goes through
// operations.h:787 Store<float> / operations.h:802 Load<float>.
//
// The case set must stay in sync with TYPED_NARROW_KINDS in
// test/asm-generator/asm-generator.py: rt/typed_narrow.asm holds one
// test_typed_narrow_<kind> method per entry.
TEST_ASM(CbcTest, TypedSlotNarrowRoundTrip)
{
    constexpr auto stackSlotCount = 100;
    uint64_t frameSlots[stackSlotCount] = { 0 };
    Interpretation::Frame frame { reinterpret_cast<uintptr_t>(frameSlots + stackSlotCount) };

    // The reference applies the exact static_cast chain of the engine:
    // st.typed truncates through Store<P> (operations.h:827) and ld.typed
    // widens through Load<P> (operations.h:837), i.e. the result is
    // static_cast<uint64_t>(static_cast<P>(in)) - compared through res.u32
    // per the narrow-result convention.
    const struct {
        std::string_view method;
        uint32_t (*extend)(uint64_t);
    } kinds[] = {
        { "test_typed_narrow_i8", [](uint64_t v) { return static_cast<uint32_t>(static_cast<int8_t>(v)); } },
        { "test_typed_narrow_u8", [](uint64_t v) { return static_cast<uint32_t>(static_cast<uint8_t>(v)); } },
        { "test_typed_narrow_i16", [](uint64_t v) { return static_cast<uint32_t>(static_cast<int16_t>(v)); } },
        { "test_typed_narrow_u16", [](uint64_t v) { return static_cast<uint32_t>(static_cast<uint16_t>(v)); } },
    };
    for (const uint64_t in : { uint64_t { 0x123456789ABCDEF0 }, uint64_t { 0xFF }, uint64_t { 0x80 },
                               uint64_t { 0x7F }, uint64_t { 0x8000 }, uint64_t { 0xFFFF }, uint64_t { 0x1FF } }) {
        for (const auto& kind : kinds) {
            SCOPED_TRACE(std::string(kind.method) + " in=" + hex(in));
            auto info = OpenAndRewrite(std::string(kind.method), "./generated-asm/frame-slots/rt/typed_narrow.asm");
            ASSERT_EQ(info->staticFrameSize, 2 * sizeof(uint64_t));
            auto res = Interpret(info->code, frame, U64(in), U64(0));
            EXPECT_EQ(res.u32, kind.extend(in));
        }
    }
}

// st.typed replaces exactly the field bytes (Store<P>, operations.h:827), so
// one all-store pass over the RecN record pins the field layout AND the
// narrow-store byte isolation: i8@0, u8@1, i16@2, u16@4, f32@8 (the
// resolver packs the declared fields at their natural alignment,
// field_layout.cpp:82/86/92 flat sizes and :302 SizeAlignmentAccumulator::
// AddField; the typed slot is AlignUp(flat size 12, STACK_SLOT_SIZE) = 16
// bytes, isa_rewriter.cpp:2326). The load-only methods then re-read the
// stored fields with the junk neighbors in place, pinning the extensions
// (operations.h:837 Load<P>).
TEST_ASM(CbcTest, TypedSlotNarrowAllStore)
{
    auto info = OpenAndRewrite("test_typed_narrow_all_store", "./generated-asm/frame-slots/rt/typed_narrow.asm");
    ASSERT_EQ(info->staticFrameSize, 2 * sizeof(uint64_t));
    constexpr auto stackSlotCount = 100;
    uint64_t frameSlots[stackSlotCount] = { 0 };
    Interpretation::Frame frame { reinterpret_cast<uintptr_t>(frameSlots + stackSlotCount) };

    // All-ones junk: every narrow store must replace exactly its own bytes.
    frameSlots[stackSlotCount - 2] = 0xFFFFFFFFFFFFFFFF;
    frameSlots[stackSlotCount - 1] = 0xFFFFFFFFFFFFFFFF;
    Interpret(info->code, frame, U64(0), U64(0));
    // i8 <- 0xA5, u8 <- (0x5A1C & 0xFF) = 0x1C, i16 <- 0x8163, u16 <- 0x7211,
    // f32 <- bits of 1.5f (0x3FC00000); the upper bytes of both qwords stay
    // 0xFF (ST_8/ST_16/ST_F32 are field-width stores).
    EXPECT_EQ(frameSlots[stackSlotCount - 2], 0xFFFF721181631CA5ull);
    EXPECT_EQ(frameSlots[stackSlotCount - 1], 0xFFFFFFFF3FC00000ull);

    // The load-only methods see the same slot memory: each ld.typed reads
    // exactly its field bytes and extends them (sign for i8/i16, zero for
    // u8/u16).
    const struct {
        std::string_view method;
        uint32_t (*extend)(uint64_t);
    } kinds[] = {
        { "test_typed_narrow_load_i8", [](uint64_t v) { return static_cast<uint32_t>(static_cast<int8_t>(v)); } },
        { "test_typed_narrow_load_u8", [](uint64_t v) { return static_cast<uint32_t>(static_cast<uint8_t>(v)); } },
        { "test_typed_narrow_load_i16", [](uint64_t v) { return static_cast<uint32_t>(static_cast<int16_t>(v)); } },
        { "test_typed_narrow_load_u16", [](uint64_t v) { return static_cast<uint32_t>(static_cast<uint16_t>(v)); } },
    };
    const uint64_t stored[] = { 0xA5, 0x5A1C, 0x8163, 0x7211 };
    for (size_t i = 0; i < std::size(kinds); i++) {
        SCOPED_TRACE(std::string(kinds[i].method));
        auto ld = OpenAndRewrite(std::string(kinds[i].method), "./generated-asm/frame-slots/rt/typed_narrow.asm");
        ASSERT_EQ(ld->staticFrameSize, 2 * sizeof(uint64_t));
        auto res = Interpret(ld->code, frame, U64(0), U64(0));
        EXPECT_EQ(res.u32, kinds[i].extend(stored[i]));
    }
}

// st.typed.f/ld.typed.f go through Store<float>/Load<float>
// (operations.h:787/802): the 4 field bytes are replaced/extended verbatim,
// bit-exact. RecF { f32 } fills 4 bytes -> the typed slot occupies
// AlignUp(4, STACK_SLOT_SIZE) = 8 bytes (isa_rewriter.cpp:2326), so
// staticFrameSize is one qword. rt/typed_f32.asm must stay in sync with the
// method names used here (test_typed_f32 / test_typed_f32_load,
// asm-generator.py typed_f32_method/typed_f32_load_method).
TEST_ASM(CbcTest, TypedSlotF32)
{
    auto rt = OpenAndRewrite("test_typed_f32", "./generated-asm/frame-slots/rt/typed_f32.asm");
    ASSERT_EQ(rt->staticFrameSize, sizeof(uint64_t));
    constexpr auto stackSlotCount = 100;
    uint64_t frameSlots[stackSlotCount] = { 0 };
    Interpretation::Frame frame { reinterpret_cast<uintptr_t>(frameSlots + stackSlotCount) };

    // All-ones junk above the field bytes: st.typed.f must replace exactly
    // the low 4 bytes of the slot.
    frameSlots[stackSlotCount - 1] = 0xFFFFFFFFFFFFFFFF;
    for (auto [in, expected] : FloatSlotValues(true)) {
        SCOPED_TRACE("typed_f32 in=" + hex(in));
        float f;
        std::memcpy(&f, &in, sizeof(f));
        auto res = InterpretFPRes(rt->code, frame, F32(f), F32(0));
        EXPECT_EQ(res.u32, static_cast<uint32_t>(expected));
        EXPECT_EQ(static_cast<uint32_t>(frameSlots[stackSlotCount - 1]), static_cast<uint32_t>(in));
        EXPECT_EQ(frameSlots[stackSlotCount - 1] >> 32, 0xFFFFFFFFu);
    }

    // The load-only method re-reads the field bytes with the junk upper half
    // in place (Load<float>, operations.h:802) - the bits of the last
    // round-trip value come back unchanged.
    auto ld = OpenAndRewrite("test_typed_f32_load", "./generated-asm/frame-slots/rt/typed_f32.asm");
    ASSERT_EQ(ld->staticFrameSize, sizeof(uint64_t));
    auto res = InterpretFPRes(ld->code, frame, F32(0), F32(0));
    EXPECT_EQ(res.u32, static_cast<uint32_t>(FloatSlotValues(true).back().in));
}
// ---------------------------------------------------------------------------
// frame-slots: typed slot copies (copy.typed(.g), copy.to.typed(.g),
// copy.from.typed(.g))
// ---------------------------------------------------------------------------

// The DSL spellings (NewAsmParser.scala:1351-1357) lower through the assembler
// (Assembler.scala:1154 copyTypeSlot - no register operands, hence no liveness
// bookkeeping; :1204 copyTypeSlot(g) books the TI register with usePrim;
// :1161/:1168 copyFrom/ToTypeSlot useRec the address register) to the RT
// opcodes COPY_TYPESLOT, COPY_TYPESLOT_GENERIC, COPY_TO_TYPESLOT(GENERIC),
// COPY_FROM_TYPESLOT(GENERIC) (isa_rt.h:194-199).
//
// At rewrite time the non-.g forms resolve the TI of both involved slots and
// assert they are equal (isa_rewriter.cpp:1086-1092); the .g forms never
// resolve the TI - it travels in a register and the RT handler reads it with
// GetPrimitive (interpretation_loop.cpp:1016/:1024/:1032). The registry
// TYPE-kind TI (load.type.info default:Rec@rec) is 2 * ObjectHeaderSize = 16
// bytes (test/mock/interpreter.cpp:402, TYPE case at :425), matching the Rec
// slot footprint, so the register-TI copies are full-slot copies as well.
//
// At run time CopyTypeSlot memcpy's GetTypeSize(ti) bytes between
// frame.start + dstOffset and frame.start + srcOffset
// (interpreter.h:745-752); CopyToTypeSlot/CopyFromTypeSlot go through
// CopyDerivedToLocal/CopyLocalToDerived (interpreter.h:753-764), which take
// the LOCAL branch (plain memcpy) when the base register is IRZ -
// GetStructLocationKind(base=0) (runtime.cpp:260-275). Every offset is
// negative (two 16-byte slots below frame start), so each of these sites
// depended on the sign-extending double cast
// `frame.start + static_cast<uintptr_t>(static_cast<int64_t>(offset))`
// (commit 72521659): the debug (UBSan halt_on_error) leg of this suite is the
// acceptance proof for that fix.
//
// Geometry: both slots are declared `default:Rec@rec` (flat size 16,
// isa_rewriter.cpp:2318) -> slot footprint AlignUp(16, STACK_SLOT_SIZE) = 16
// (isa_rewriter.cpp:2326), staticFrameSize 32; slot $0 = typedOffset 0 - 32 =
// qwords [-4]/[-3], slot $1 = qwords [-2]/[-1] relative to frame start.
// typed_copy.asm must stay in sync with the method names used here
// (asm-generator.py typed_copy_* methods).
TEST_ASM(CbcTest, TypedSlotCopy)
{
    constexpr auto stackSlotCount = 100;
    uint64_t frameSlots[stackSlotCount] = { 0 };
    Interpretation::Frame frame { reinterpret_cast<uintptr_t>(frameSlots + stackSlotCount) };

    // $0's f field and both $1 qwords are junk the methods never store: after
    // the copy the whole 16-byte slot $1 must equal slot $0, i.e. the
    // never-stored f traveled into $1's upper qword and the junk left $1.
    constexpr uint64_t seedF     = 0xDEADBEEFCAFEF00D;
    constexpr uint64_t junkLow   = 0x5152535455565758;
    constexpr uint64_t junkHigh  = 0x6162636465666768;

    auto info = OpenAndRewrite("test_typed_copy", "./generated-asm/frame-slots/typed_copy.asm");
    ASSERT_EQ(info->staticFrameSize, 4 * sizeof(uint64_t));
    for (uint64_t p1 : { uint64_t { 0x2A2B }, uint64_t { 0x123456789ABCDEF0 },
                         uint64_t { 0xF0F1F2F3F4F5F6F7 } }) {
        SCOPED_TRACE("typed_copy in=" + hex(p1));
        frameSlots[stackSlotCount - 4] = p1;
        frameSlots[stackSlotCount - 3] = seedF;
        frameSlots[stackSlotCount - 2] = junkLow;
        frameSlots[stackSlotCount - 1] = junkHigh;
        // ld.typed $1.x + ld.typed $0.x: the copy must land in $1 (dst, src
        // operand order) and leave the source $0 intact -> sum 2 * p1.
        auto res = Interpret(info->code, frame, U64(p1), U64(0));
        EXPECT_EQ(res.u64, 2 * p1);
        EXPECT_EQ(frameSlots[stackSlotCount - 4], p1);
        EXPECT_EQ(frameSlots[stackSlotCount - 3], seedF);
        EXPECT_EQ(frameSlots[stackSlotCount - 2], p1);
        EXPECT_EQ(frameSlots[stackSlotCount - 1], seedF);
    }

    // RecC { c I64, d F32 } has flat size 12 -> the 16-byte slot carries a
    // 4-byte tail (d's upper half + padding) that st.typed/ld.typed cannot
    // address. Only a full-slot memcpy moves it: after copy.typed $1, $0 the
    // tail junk must have traveled to $1 verbatim, while a field-wise copy
    // would have left the $1-seeded junk in place.
    auto pad = OpenAndRewrite("test_typed_copy_pad", "./generated-asm/frame-slots/typed_copy.asm");
    ASSERT_EQ(pad->staticFrameSize, 4 * sizeof(uint64_t));
    for (uint64_t p1 : { uint64_t { 0x2A2B2C2D2E2F3031 }, uint64_t { 0xF0F0F0F0F0F0F0F0 } }) {
        SCOPED_TRACE("typed_copy_pad in=" + hex(p1));
        // $0 tail: d = bits 0xAABBCCDD at bytes [8, 12), padding 0x21222324 at
        // bytes [12, 16) - one never-stored qword.
        frameSlots[stackSlotCount - 4] = p1;
        frameSlots[stackSlotCount - 3] = 0x21222324AABBCCDD;
        frameSlots[stackSlotCount - 2] = junkLow;
        frameSlots[stackSlotCount - 1] = junkHigh;
        auto res = Interpret(pad->code, frame, U64(p1), U64(0));
        EXPECT_EQ(res.u64, p1);
        EXPECT_EQ(frameSlots[stackSlotCount - 2], p1);
        EXPECT_EQ(frameSlots[stackSlotCount - 1], 0x21222324AABBCCDD);
    }
}

// copy.typed.g $1, $0, IR3 with IR3 = load.type.info default:Rec@rec: the TI
// never goes through the rewriter's resolver (isa_rewriter.cpp:1106-1109), the
// RT handler reads it from the register (interpretation_loop.cpp:1016-1022)
// and the interpreter memcpy's GetTypeSize(ti) = 16 bytes
// (interpreter.h:745-752). Same junk-travel assertions as TypedSlotCopy.
TEST_ASM(CbcTest, TypedSlotCopyGeneric)
{
    constexpr auto stackSlotCount = 100;
    uint64_t frameSlots[stackSlotCount] = { 0 };
    Interpretation::Frame frame { reinterpret_cast<uintptr_t>(frameSlots + stackSlotCount) };

    constexpr uint64_t seedF    = 0xDEADBEEFCAFEF00D;
    constexpr uint64_t junkLow  = 0x5152535455565758;
    constexpr uint64_t junkHigh = 0x6162636465666768;

    auto info = OpenAndRewrite("test_typed_copy_g", "./generated-asm/frame-slots/typed_copy.asm");
    ASSERT_EQ(info->staticFrameSize, 4 * sizeof(uint64_t));
    for (uint64_t p1 : { uint64_t { 0x2A2B }, uint64_t { 0x123456789ABCDEF0 },
                         uint64_t { 0xF0F1F2F3F4F5F6F7 } }) {
        SCOPED_TRACE("typed_copy_g in=" + hex(p1));
        frameSlots[stackSlotCount - 4] = p1;
        frameSlots[stackSlotCount - 3] = seedF;
        frameSlots[stackSlotCount - 2] = junkLow;
        frameSlots[stackSlotCount - 1] = junkHigh;
        auto res = Interpret(info->code, frame, U64(p1), U64(0));
        EXPECT_EQ(res.u64, 2 * p1);
        EXPECT_EQ(frameSlots[stackSlotCount - 2], p1);
        EXPECT_EQ(frameSlots[stackSlotCount - 1], seedF);
    }
}

// copy.to.typed $1, IRZ, IR2 / copy.from.typed IRZ, IR2, $0 (and their .g
// register-TI variants) with IR2 = ld.stack.rec of the peer slot: the base is
// IRZ, so GetStructLocationKind takes the LOCAL branch
// (runtime.cpp:260-275) and the copy is a plain memcpy through
// CopyToTypeSlot/CopyFromTypeSlot (interpreter.h:753-764). copy.to.typed
// copies [IR2] -> slot $1; copy.from.typed copies slot $0 -> [IR2] - both
// produce $1 == $0 (sum of the x read-backs = 2 * p1), with the never-stored
// f traveling either way.
TEST_ASM(CbcTest, TypedSlotCopyDerived)
{
    constexpr auto stackSlotCount = 100;
    uint64_t frameSlots[stackSlotCount] = { 0 };
    Interpretation::Frame frame { reinterpret_cast<uintptr_t>(frameSlots + stackSlotCount) };

    constexpr uint64_t seedF    = 0xDEADBEEFCAFEF00D;
    constexpr uint64_t junkLow  = 0x5152535455565758;
    constexpr uint64_t junkHigh = 0x6162636465666768;

    const std::pair<const char*, const char*> methods[] = {
        { "test_typed_copy_to", "./generated-asm/frame-slots/typed_copy.asm" },
        { "test_typed_copy_from", "./generated-asm/frame-slots/typed_copy.asm" },
        { "test_typed_copy_to_g", "./generated-asm/frame-slots/typed_copy.asm" },
        { "test_typed_copy_from_g", "./generated-asm/frame-slots/typed_copy.asm" },
    };
    for (const auto& [name, path] : methods) {
        auto info = OpenAndRewrite(name, path);
        ASSERT_EQ(info->staticFrameSize, 4 * sizeof(uint64_t));
        for (uint64_t p1 : { uint64_t { 0x2A2B }, uint64_t { 0x123456789ABCDEF0 },
                             uint64_t { 0xF0F1F2F3F4F5F6F7 } }) {
            SCOPED_TRACE(std::string(name) + " in=" + hex(p1));
            frameSlots[stackSlotCount - 4] = p1;
            frameSlots[stackSlotCount - 3] = seedF;
            frameSlots[stackSlotCount - 2] = junkLow;
            frameSlots[stackSlotCount - 1] = junkHigh;
            auto res = Interpret(info->code, frame, U64(p1), U64(0));
            EXPECT_EQ(res.u64, 2 * p1);
            EXPECT_EQ(frameSlots[stackSlotCount - 2], p1);
            EXPECT_EQ(frameSlots[stackSlotCount - 1], seedF);
        }
    }
}
