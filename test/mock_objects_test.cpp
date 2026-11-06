#include "cbc_test_harness.h"

// ---------------------------------------------------------------------------
// mock-objects: newobj/newarr/index/load.type.info over the synthetic TypeInfos
// ---------------------------------------------------------------------------

// The synthetic-TI registry (test/mock/interpreter.cpp) resolves every
// bytecode-embedded term to an immortal TestTypeInfo: object-like
// {size = 2 * ObjectHeaderSize, align = sizeof(void*), no reference offsets}
// for non-primitive terms, natural sizes for primitives. MockNewObj/MockNewArr
// store that TI pointer in the object header, so the in-asm identities hold:
// the rewrite-time TI is pointer-identical to the run-time one. The registry is
// keyed by the interned term, so TIs produced by different OpenAndRewrite
// engines must not be compared - identities are asserted in-asm instead. The
// mock heap is a static buffer only rewound by Reset, so pointers returned by
// one Interpret run stay valid seeds for the next one.

// Checks the header shape every mock-heap allocation carries:
// [TestTypeInfo*][payload...] with the object-like synthetic TI.
static void ExpectMockObjectHeader(uint64_t obj)
{
    ASSERT_NE(obj, 0);
    EXPECT_EQ(obj % alignof(std::max_align_t), 0);
    auto* ti = *reinterpret_cast<TestTypeInfo**>(obj);
    ASSERT_NE(ti, nullptr);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(ti) % sizeof(void*), 0);
    EXPECT_EQ(ti->size, 2 * RTSupport::MetaInfo::ObjectHeaderSize());
    EXPECT_EQ(ti->alignment, sizeof(void*));
    EXPECT_TRUE(ti->referenceOffsets.Empty());
}

static uint64_t HeaderWord(uint64_t obj, uintptr_t offset)
{
    uint64_t word = 0;
    std::memcpy(&word, reinterpret_cast<void*>(obj + offset), sizeof(word));
    return word;
}

TEST_ASM(CbcTest, MockNewObj)
{
    auto path = "./generated-asm/mock-objects/newobj.asm";
    {
        SCOPED_TRACE(std::string(path) + " test_ptr");
        auto code = OpenAndRewrite("test_ptr", path)->code;
        ExpectMockObjectHeader(Interpret(code, U64(0), U64(0)).u64);
    }
    {
        // A prim stored through the class field path (x I64@8) lands in the
        // payload slot behind the TI header.
        auto code = OpenAndRewrite("test_field", path)->code;
        for (uint64_t value : {0x1234ull, 0ull, 0xffffffffffffffffull}) {
            SCOPED_TRACE("test_field value=" + hex(value));
            auto obj = Interpret(code, U64(0), U64(value)).u64;
            ExpectMockObjectHeader(obj);
            EXPECT_EQ(HeaderWord(obj, RTSupport::MetaInfo::ObjectHeaderSize()), value);
        }
    }
    {
        SCOPED_TRACE(std::string(path) + " test_distinct");
        auto code = OpenAndRewrite("test_distinct", path)->code;
        EXPECT_EQ(Interpret(code, U64(0), U64(0)).u64, 1);
    }
    {
        // The header TI of a newobj allocation is pointer-identical to the
        // rewrite-time TI of the same term (load.type.info result).
        SCOPED_TRACE(std::string(path) + " test_ti");
        auto code = OpenAndRewrite("test_ti", path)->code;
        EXPECT_EQ(Interpret(code, U64(0), U64(0)).u64, 1);
    }
    {
        // newobj.g takes the TI from a register and still stores it in the
        // header.
        SCOPED_TRACE(std::string(path) + " test_g");
        auto code = OpenAndRewrite("test_g", path)->code;
        EXPECT_EQ(Interpret(code, U64(0), U64(0)).u64, 1);
    }
}

TEST_ASM(CbcTest, MockNewArr)
{
    auto path = "./generated-asm/mock-objects/newarr.asm";
    // The layout is [TestTypeInfo*][u64 length][elements...]; the elem stride
    // of the index-computed addresses is the elem TI's aligned size (rec 16,
    // I64 8). The batches re-allocate before use because every Interpret run
    // rewinds the heap cursor.
    {
        auto code   = OpenAndRewrite("test_ptr", path)->code;
        auto worker = OpenAndRewrite("test_index", "./generated-asm/mock-objects/index.asm")->code;
        auto arr    = Interpret(code, U64(0), U64(7)).u64;
        ExpectMockObjectHeader(arr);
        EXPECT_EQ(HeaderWord(arr, RTSupport::MetaInfo::ObjectHeaderSize()), 7);
        for (uint64_t idx : {0, 1, 5}) {
            SCOPED_TRACE("rec elem idx=" + hex(idx));
            auto res = Interpret(worker, U64(arr), U64(idx), U64(0x1234)).u64;
            EXPECT_EQ(res, 0x1234);
            EXPECT_EQ(HeaderWord(arr, RTSupport::MetaInfo::ArrayBodyOffset() + idx * 16), 0x1234);
        }
    }
    {
        auto code   = OpenAndRewrite("test_ptr_prim", path)->code;
        auto worker = OpenAndRewrite("test_index_prim", "./generated-asm/mock-objects/index.asm")->code;
        auto arr    = Interpret(code, U64(0), U64(7)).u64;
        ExpectMockObjectHeader(arr);
        EXPECT_EQ(HeaderWord(arr, RTSupport::MetaInfo::ObjectHeaderSize()), 7);
        for (uint64_t idx : {0, 1, 5}) {
            SCOPED_TRACE("i64 elem idx=" + hex(idx));
            auto res = Interpret(worker, U64(arr), U64(idx), U64(0x1234)).u64;
            EXPECT_EQ(res, 0x1234);
            EXPECT_EQ(HeaderWord(arr, RTSupport::MetaInfo::ArrayBodyOffset() + idx * 8), 0x1234);
        }
    }
    {
        // Empty arrays still carry the header and the zeroed length word.
        auto code = OpenAndRewrite("test_ptr", path)->code;
        auto arr  = Interpret(code, U64(0), U64(0)).u64;
        ExpectMockObjectHeader(arr);
        EXPECT_EQ(HeaderWord(arr, RTSupport::MetaInfo::ObjectHeaderSize()), 0);
    }
    for (auto [method, lenPath] : {std::pair {"test_len", path},
                                   std::pair {"test_len_prim", path},
                                   std::pair {"test_len_g", path}}) {
        auto code = OpenAndRewrite(method, lenPath)->code;
        for (uint64_t len : {0, 1, 7, 64}) {
            SCOPED_TRACE(std::string(method) + " len=" + hex(len));
            EXPECT_EQ(Interpret(code, U64(0), U64(len)).u64, len);
        }
    }
    {
        // The header TI of a newarr allocation is pointer-identical to the
        // rewrite-time TI of the array term (load.type.info result).
        SCOPED_TRACE(std::string(path) + " test_arr_ti");
        auto code = OpenAndRewrite("test_arr_ti", path)->code;
        EXPECT_EQ(Interpret(code, U64(0), U64(7)).u64, 1);
    }
}

TEST_ASM(CbcTest, MockIndex)
{
    // index addressing over test-fabricated arrays: [TestTypeInfo*][u64 len]
    // [elements...] with the elem TI's aligned size as the stride; the raw
    // store/load round-trips assert the address arithmetic, the direct slot
    // reads assert where the value landed.
    auto path = "./generated-asm/mock-objects/index.asm";
    for (auto [method, elemSize] : {std::pair {"test_index", size_t {16}}, std::pair {"test_index_prim", size_t {8}}}) {
        auto code = OpenAndRewrite(method, path)->code;
        TestTypeInfo elemTi { elemSize, 8, {} };
        alignas(16) uint64_t fake[20] = { 0 };
        fake[0] = reinterpret_cast<uint64_t>(&elemTi);
        fake[1] = 8;
        auto base = reinterpret_cast<uint64_t>(fake);
        for (uint64_t idx : {0, 1, 5}) {
            SCOPED_TRACE(std::string(method) + " idx=" + hex(idx));
            auto res = Interpret(code, U64(base), U64(idx), U64(0x1234)).u64;
            EXPECT_EQ(res, 0x1234);
            EXPECT_EQ(HeaderWord(base, RTSupport::MetaInfo::ArrayBodyOffset() + idx * elemSize), 0x1234);
        }
    }
}

TEST_ASM(CbcTest, MockLoadTypeInfo)
{
    // load.type.info embeds the synthetic TI as a MovImm immediate: two runs
    // return the identical immortal pointer and the shapes mirror the registry
    // (object-like for rec/class/array terms, natural sizes for primitives).
    // The header identities of MockNewObj/MockNewArr prove the same TIs reach
    // the object headers.
    auto path = "./generated-asm/mock-objects/load-type-info.asm";
    for (auto [method, size, alignment] :
         {std::tuple {"test_lti_rec", size_t {16}, uint8_t {8}},
          std::tuple {"test_lti_i64", size_t {8}, uint8_t {8}},
          std::tuple {"test_lti_u8", size_t {1}, uint8_t {1}},
          std::tuple {"test_lti_f64", size_t {8}, uint8_t {8}},
          std::tuple {"test_lti_bool", size_t {1}, uint8_t {1}},
          std::tuple {"test_lti_arr", size_t {16}, uint8_t {8}}}) {
        SCOPED_TRACE(std::string(method));
        auto code    = OpenAndRewrite(method, path)->code;
        auto first   = Interpret(code, U64(0), U64(0)).u64;
        auto second  = Interpret(code, U64(0), U64(0)).u64;
        EXPECT_EQ(first, second);
        ASSERT_NE(first, 0);
        EXPECT_EQ(first % sizeof(void*), 0);
        auto* ti = reinterpret_cast<TestTypeInfo*>(first);
        EXPECT_EQ(ti->size, size);
        EXPECT_EQ(ti->alignment, alignment);
        EXPECT_TRUE(ti->referenceOffsets.Empty());
    }
}

TEST_ASM(CbcTest, MockCopyLocal)
{
    // copy.local(.g)/copy.to.local(.g)/copy.from.local(.g) round-trip 16-byte
    // rec cells between adjacent newobj-allocated mock-heap objects: the copy
    // size is the synthetic TI's size, so the clobbered source header word is
    // copied over the destination's untouched one, the payload word follows,
    // and the guard object behind the destination stays intact. The .g forms
    // take the same registry TI from a load.type.info register; the derived
    // local forms run with a zero base (the LOCAL memcpy path - a heap base
    // would need the still-FATALing Read/WriteStructField of todo-mock 3.7).
    auto path = "./generated-asm/mock-objects/copy.asm";
    for (auto name :
         {"test_copy_local",
          "test_copy_local_g",
          "test_copy_to_local",
          "test_copy_to_local_g",
          "test_copy_from_local",
          "test_copy_from_local_g"}) {
        auto code = OpenAndRewrite(name, path)->code;
        for (auto [hdr, pay] :
             {std::pair {0x1234ull, 0x5678ull},
              std::pair {0ull, 0xdeadbeefull},
              std::pair {0xffffffffffffffffull, 0ull}}) {
            SCOPED_TRACE(std::string(name) + " hdr=" + hex(hdr) + " pay=" + hex(pay));
            EXPECT_EQ(Interpret(code, U64(hdr), U64(pay)).u64, 1);
        }
    }
}

TEST_ASM(CbcTest, MockTagGeneric)
{
    // tag.g rewrites to BRANCH_IS_REF + a ref load of the payload word behind
    // the header + SCC against IRZ; the mock's IsReference is constant true, so
    // the reference branch always runs and the tag is the payload non-nullness
    // per the option's enum kind: Option1 { None, Some } reads non-null as Some
    // (1), Option0 { Some, None } reads null as None (1).
    auto path = "./generated-asm/mock-objects/tag.asm";
    for (auto [method, tagNonZero] :
         {std::pair {"test_tag_opt1", 1ull}, std::pair {"test_tag_opt0", 0ull}}) {
        auto code = OpenAndRewrite(method, path)->code;
        for (uint64_t payload : {0ull, 0x5a5aull, 0xffffffffffffffffull}) {
            SCOPED_TRACE(std::string(method) + " payload=" + hex(payload));
            EXPECT_EQ(Interpret(code, U64(payload), U64(0)).u64, payload != 0 ? tagNonZero : 1 - tagNonZero);
        }
    }
}

TEST_ASM(CbcTest, MockInterfaceCall)
{
    // call.interf resolves the interface's method table at rewrite time and
    // reads the receiver TI from the mock-heap object header at run time; the
    // InterfaceDispatchMock supplies the I2ICall thunk targeting the impl
    // method of the same image and records the dispatch. The callee multiplies
    // its payload+1 by an in-asm identity check (receiver header TI ==
    // load.type.info of the same term), so the returned value proves both the
    // dispatch and the argument/result plumbing. The non-generic spelling never
    // queries the outer TI, and the .g form has no DSL spelling.
    auto path = "./generated-asm/mock-objects/interf.asm";
    auto code = OpenAndRewrite("main", path)->code;
    // The same engine built the caller (GetEngineInstance points at it); the
    // thunk target is its impl_foo handle.
    Engine::Session session(Engine::GetEngineInstance());
    auto calleeId = Engine::GetEngineInstance().FindMethod(session, path, "mem:Impl", "impl_foo");
    ASSERT_TRUE(calleeId.has_value());
    auto& fuhManager = Interpretation::FunctionHandleManager::Of(session);
    auto fuh = std::get<Interpretation::DynamicFunctionHandle*>(fuhManager.AcquireTagged(session, calleeId.value()));

    auto& mock = InterfaceDispatchMock();
    mock.enabled = true;
    mock.thunk   = Interpretation::Thunk {.op = Interpretation::ThunkOp::I2ICall, .argUInt = reinterpret_cast<uint64_t>(fuh)};

    for (uint64_t x : {0x1234ull, 0ull, 0xabcdefull}) {
        SCOPED_TRACE("x=" + hex(x));
        mock.dispatchCalls.Clear();
        EXPECT_EQ(Interpret(code, U64(x), U64(0)).u64, x + 1);
        ASSERT_EQ(mock.dispatchCalls.Size(), 1);
        EXPECT_EQ(mock.dispatchCalls[0].methodNum, 0);
        auto* receiver = reinterpret_cast<TestTypeInfo*>(mock.dispatchCalls[0].receiver.UInt());
        ASSERT_NE(receiver, nullptr);
        EXPECT_EQ(receiver->size, 2 * RTSupport::MetaInfo::ObjectHeaderSize());
        EXPECT_EQ(receiver->alignment, sizeof(void*));
        auto* interf = reinterpret_cast<TestTypeInfo*>(mock.dispatchCalls[0].reference.UInt());
        ASSERT_NE(interf, nullptr);
        EXPECT_NE(interf, receiver);
        EXPECT_EQ(mock.outerTiCalls.Size(), 0);
    }
}
