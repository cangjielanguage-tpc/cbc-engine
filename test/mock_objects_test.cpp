#include "cbc_test_harness.h"

// The convert-ext value table covers inf/NaN float sources; probe the FP-seeded
// InterpretThrows entry (todo-mock 3.6) with a denormal, which the checked
// range check must let through without a throw.
TEST_ASM(CbcTest, CheckedConvertDenormalSource)
{
    auto code = OpenAndRewrite("test_I32_F32", "./generated-asm/convert-ext/checked/I32_F32.asm")->code;
    EXPECT_EQ(
        InterpretThrows(code, U64(0), U64(0), F32(std::numeric_limits<float>::denorm_min()), F64(0)),
        Interpretation::Type::Successful
    );
}

// The FP-seeded InterpretThrows overload must still stage the integer seeds:
// nullcheck's IR1 reference decides between a clean run and NoneValueException.
TEST_ASM(CbcTest, InterpretThrowsFPIntSeeds)
{
    auto code = OpenAndRewrite("test_nullcheck", "./generated-asm/checks-misc/nullcheck.asm")->code;
    uint64_t obj = 0;
    auto ref     = reinterpret_cast<uint64_t>(&obj);
    EXPECT_EQ(InterpretThrows(code, U64(ref), U64(0), F64(0), F64(0)), Interpretation::Type::Successful);
    EXPECT_EQ(InterpretThrows(code, U64(0), U64(0), F64(0), F64(0)), Interpretation::Type::NoneValueException);
}
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

TEST_ASM(CbcTest, MockNewOption)
{
    // new.none.g/new.some.g lower to NEWOBJ_ACC_G - the engine's ACC-form
    // allocation, whose result the real thunk stores in IR_ACC (Init*Generic
    // reads its write base and the final Mov(dst, IR_ACC) reads the result
    // from there) - so MockNewObj must place the object into IR_ACC for the
    // flows to work at all. The mock's IsReference is constant true:
    // none.g leaves the zeroed payload word (None for both enum kinds) and
    // some.g stores the raw param behind the header; tag.g/payload.g read the
    // very same words back.
    auto path = "./generated-asm/mock-objects/new-option.asm";
    for (auto kind : {std::string {"opt0"}, std::string {"opt1"}}) {
        {
            SCOPED_TRACE("test_none_" + kind);
            auto code = OpenAndRewrite("test_none_" + kind, path)->code;
            auto obj  = Interpret(code, U64(0), U64(0)).u64;
            ExpectMockObjectHeader(obj);
            EXPECT_EQ(HeaderWord(obj, RTSupport::MetaInfo::ObjectHeaderSize()), 0ull);
        }
        {
            // tag.g over the fresh None object: Option1 { None, Some } reads
            // the null payload as 0, Option0 { Some, None } as 1.
            auto tag = OpenAndRewrite("test_none_tag_" + kind, path)->code;
            EXPECT_EQ(Interpret(tag, U64(0), U64(0)).u64, kind == "opt0" ? 1 : 0);
        }
        {
            // The fresh Some object: the raw payload parameter is stored
            // behind the header and reads back raw.
            auto some = OpenAndRewrite("test_some_" + kind, path)->code;
            for (uint64_t payload : {0x1234ull, 0ull, 0xffffffffffffffffull}) {
                SCOPED_TRACE("test_some_" + kind + " payload=" + hex(payload));
                auto obj = Interpret(some, U64(payload), U64(0)).u64;
                ExpectMockObjectHeader(obj);
                EXPECT_EQ(HeaderWord(obj, RTSupport::MetaInfo::ObjectHeaderSize()), payload);
            }
        }
        {
            // payload.g round-trips the stored payload word.
            auto payloadG = OpenAndRewrite("test_some_payload_" + kind, path)->code;
            for (uint64_t payload : {0x5a5aull, 0ull, 0xffffffffffffffffull}) {
                SCOPED_TRACE("test_some_payload_" + kind + " payload=" + hex(payload));
                EXPECT_EQ(Interpret(payloadG, U64(payload), U64(0)).u64, payload);
            }
        }
    }
}

TEST_ASM(CbcTest, MockAccResultRegister)
{
    // IR_ACC observability (todo-mock 3.4, mock level): the acc forms have no
    // DSL spelling, so the none/some flows stand in for them - their
    // NEWOBJ_ACC_G allocation lands in IR_ACC and the lowering's final
    // Mov(dst, IR_ACC) leaves it there through the ret. The fresh allocations
    // are deterministic (the mock heap rewinds per run), so the
    // InterpretAccRes runs must return the very pointer the dst-reading
    // Interpret runs returned.
    auto path = "./generated-asm/mock-objects/new-option.asm";
    for (auto name : {"test_none_opt0", "test_none_opt1"}) {
        SCOPED_TRACE(name);
        auto code = OpenAndRewrite(name, path)->code;
        EXPECT_EQ(InterpretAccRes(code, U64(0), U64(0)).u64, Interpret(code, U64(0), U64(0)).u64);
        EXPECT_NE(InterpretAccRes(code, U64(0), U64(0)).u64, 0);
    }
    for (auto name : {"test_some_opt0", "test_some_opt1"}) {
        SCOPED_TRACE(name);
        auto code = OpenAndRewrite(name, path)->code;
        for (uint64_t payload : {0x1234ull, 0ull}) {
            SCOPED_TRACE("payload=" + hex(payload));
            EXPECT_EQ(InterpretAccRes(code, U64(payload), U64(0)).u64, Interpret(code, U64(payload), U64(0)).u64);
        }
    }
}

// ---------------------------------------------------------------------------
// mock-lea: lea family / offset / movbp / lea.static / payload.g over the mock base pointers
// ---------------------------------------------------------------------------

// The stubbed field-offset/base-pointer queries (todo-mock 3.3) shape these
// identities: GetFieldOffset folds to ordinal * 8 for the non-overlapping
// synthetic TIs and to 0 for the overlapping (enum-like) ones, plus the object
// header with adjustByHeader. For the slot-aligned I64 fields of the mock-lea
// images that coincides with the ld.field/st.field offsets - rec fields stay
// cell-relative, class fields carry the header - so the lea'd addresses must
// byte-match the field accesses. The base pointers are two distinct mock-owned
// 512-byte areas (zeroed by InitializeMockInterpreter, exposed via
// MockLocalBaseArea/MockGlobalBaseArea); their addresses are embedded as
// MovImm immediates at rewrite. The generic loads/stores address through the
// derived pointer alone (the mock's Read/WriteObjectInstance ignore the base
// register), so the base-area words are seeded/inspected at the exact derived
// address, while the registers themselves are checked against the exposed
// areas.

TEST_ASM(CbcTest, MockLea)
{
    auto path = "./generated-asm/mock-lea/lea.asm";
    alignas(16) uint64_t rec[4] = { 0 };
    alignas(16) uint64_t obj[4] = { 0 };
    auto recAddr = reinterpret_cast<uint64_t>(rec);
    auto objAddr = reinterpret_cast<uint64_t>(obj);

    for (auto [method, base, offset] :
         {std::tuple {"test_lea_rec_f0", recAddr, size_t {0}},
          std::tuple {"test_lea_rec_f1", recAddr, size_t {8}},
          std::tuple {"test_lea_obj_x", objAddr, size_t {RTSupport::MetaInfo::ObjectHeaderSize()}},
          std::tuple {"test_lea_obj_y", objAddr, size_t {RTSupport::MetaInfo::ObjectHeaderSize() + 8}}}) {
        SCOPED_TRACE(std::string(method));
        auto code = OpenAndRewrite(method, path)->code;
        EXPECT_EQ(Interpret(code, U64(base), U64(0)).u64, base + offset);
    }
    for (auto [method, base] :
         {std::pair {"test_lea_rec_rt", recAddr}, std::pair {"test_lea_obj_rt", objAddr}}) {
        auto code = OpenAndRewrite(method, path)->code;
        for (uint64_t value : {0x1234ull, 0ull, 0xffffffffffffffffull}) {
            SCOPED_TRACE(std::string(method) + " value=" + hex(value));
            EXPECT_EQ(Interpret(code, U64(base), U64(value)).u64, 1);
        }
    }
}

TEST_ASM(CbcTest, MockLeaGeneric)
{
    // lea.g rewrites to LEA_GENERIC over the loaded container TI plus the
    // header addend for class containers; the addresses must match the
    // non-generic lea and the ld.field offsets.
    auto path = "./generated-asm/mock-lea/lea-g.asm";
    alignas(16) uint64_t rec[4] = { 0 };
    alignas(16) uint64_t obj[4] = { 0 };
    auto recAddr = reinterpret_cast<uint64_t>(rec);
    auto objAddr = reinterpret_cast<uint64_t>(obj);

    for (auto [method, base, offset] :
         {std::tuple {"test_leag_rec_f0", recAddr, size_t {0}},
          std::tuple {"test_leag_rec_f1", recAddr, size_t {8}},
          std::tuple {"test_leag_obj_x", objAddr, size_t {RTSupport::MetaInfo::ObjectHeaderSize()}},
          std::tuple {"test_leag_obj_y", objAddr, size_t {RTSupport::MetaInfo::ObjectHeaderSize() + 8}}}) {
        SCOPED_TRACE(std::string(method));
        auto code = OpenAndRewrite(method, path)->code;
        EXPECT_EQ(Interpret(code, U64(base), U64(0)).u64, base + offset);
    }
    for (auto [method, base] :
         {std::pair {"test_leag_rec_rt", recAddr}, std::pair {"test_leag_obj_rt", objAddr}}) {
        auto code = OpenAndRewrite(method, path)->code;
        for (uint64_t value : {0x1234ull, 0ull, 0xffffffffffffffffull}) {
            SCOPED_TRACE(std::string(method) + " value=" + hex(value));
            EXPECT_EQ(Interpret(code, U64(base), U64(value)).u64, 1);
        }
    }
}

TEST_ASM(CbcTest, MockFieldOffset)
{
    // offset runs at run time (OFFSET) over the loaded container TI; the
    // returned deltas are the exact ld.field offsets. add.offset composes the
    // base with the delta in place and raw-round-trips through it.
    auto path = "./generated-asm/mock-lea/offset.asm";
    for (auto [method, expected] :
         {std::pair {"test_offset_rec_f0", uint64_t {0}},
          std::pair {"test_offset_rec_f1", uint64_t {8}},
          std::pair {"test_offset_obj_x", uint64_t {RTSupport::MetaInfo::ObjectHeaderSize()}},
          std::pair {"test_offset_obj_y", uint64_t {RTSupport::MetaInfo::ObjectHeaderSize() + 8}}}) {
        SCOPED_TRACE(std::string(method));
        auto code = OpenAndRewrite(method, path)->code;
        EXPECT_EQ(Interpret(code, U64(0), U64(0)).u64, expected);
    }

    alignas(16) uint64_t rec[4] = { 0 };
    alignas(16) uint64_t obj[4] = { 0 };
    auto recAddr = reinterpret_cast<uint64_t>(rec);
    auto objAddr = reinterpret_cast<uint64_t>(obj);
    for (auto [method, base, offset] :
         {std::tuple {"test_add_offset_rec_f1", recAddr, size_t {8}},
          std::tuple {"test_add_offset_obj_x", objAddr, size_t {RTSupport::MetaInfo::ObjectHeaderSize()}}}) {
        SCOPED_TRACE(std::string(method));
        auto code = OpenAndRewrite(method, path)->code;
        EXPECT_EQ(Interpret(code, U64(base), U64(0)).u64, base + offset);
    }
    for (auto [method, base] :
         {std::pair {"test_add_offset_rec_rt", recAddr}, std::pair {"test_add_offset_obj_rt", objAddr}}) {
        auto code = OpenAndRewrite(method, path)->code;
        for (uint64_t value : {0x1234ull, 0ull, 0xffffffffffffffffull}) {
            SCOPED_TRACE(std::string(method) + " value=" + hex(value));
            EXPECT_EQ(Interpret(code, U64(base), U64(value)).u64, 1);
        }
    }
}

TEST_ASM(CbcTest, MockMovBasePtr)
{
    auto path = "./generated-asm/mock-lea/movbp.asm";
    {
        auto code = OpenAndRewrite("test_movbp_local", path)->code;
        EXPECT_EQ(Interpret(code, U64(0), U64(0)).u64, reinterpret_cast<uint64_t>(MockLocalBaseArea()));
    }
    {
        auto code = OpenAndRewrite("test_movbp_global", path)->code;
        EXPECT_EQ(Interpret(code, U64(0), U64(0)).u64, reinterpret_cast<uint64_t>(MockGlobalBaseArea()));
    }
    {
        EXPECT_NE(MockLocalBaseArea(), MockGlobalBaseArea());
        auto code = OpenAndRewrite("test_movbp_distinct", path)->code;
        EXPECT_EQ(Interpret(code, U64(0), U64(0)).u64, 1);
    }

    // The base areas are the actual storage: a pre-seeded word at area + 0x10
    // reads back through movbp + ld.g, and the st.g writes land in the
    // mock-owned buffers.
    constexpr auto seedOffset = 0x10;
    for (auto [readMethod, writeMethod, area] :
         {std::tuple {"test_movbp_local_read", "test_movbp_local_write", reinterpret_cast<uint8_t*>(MockLocalBaseArea())},
          std::tuple {"test_movbp_global_read", "test_movbp_global_write", reinterpret_cast<uint8_t*>(MockGlobalBaseArea())}}) {
        auto readCode  = OpenAndRewrite(readMethod, path)->code;
        auto writeCode = OpenAndRewrite(writeMethod, path)->code;
        auto areaAddr  = reinterpret_cast<uint64_t>(area) + seedOffset;
        *reinterpret_cast<uint64_t*>(area + seedOffset) = 0x5a5a;

        SCOPED_TRACE(std::string(readMethod) + " preseed");
        EXPECT_EQ(Interpret(readCode, U64(areaAddr), U64(0)).u64, 0x5a5a);
        for (uint64_t value : {0x1234ull, 0ull, 0xffffffffffffffffull}) {
            SCOPED_TRACE(std::string(writeMethod) + " value=" + hex(value));
            EXPECT_EQ(Interpret(writeCode, U64(areaAddr), U64(value)).u64, value);
            EXPECT_EQ(*reinterpret_cast<uint64_t*>(area + seedOffset), value);
        }
    }
}

TEST_ASM(CbcTest, MockLeaStatic)
{
    auto path = "./generated-asm/mock-lea/lea-static.asm";
    {
        // The base-ref register of lea.static is the mock global base area,
        // baked in as a MovImm immediate at rewrite.
        auto code = OpenAndRewrite("test_lea_static_base", path)->code;
        EXPECT_EQ(Interpret(code, U64(0), U64(0)).u64, reinterpret_cast<uint64_t>(MockGlobalBaseArea()));
    }
    {
        // The lea'd address and ld.static name the same static slot: a raw
        // store through the address reads back through ld.static.
        auto code = OpenAndRewrite("test_lea_static_rt", path)->code;
        for (uint64_t value : {0x1234ull, 0ull, 0xffffffffffffffffull}) {
            SCOPED_TRACE("test_lea_static_rt value=" + hex(value));
            EXPECT_EQ(Interpret(code, U64(value), U64(0)).u64, 1);
        }
    }
    {
        // st.static/ld.static round-trip both static slots.
        auto code = OpenAndRewrite("test_static_pair", path)->code;
        for (auto [v0, v1] :
             {std::pair {0x1234ull, 0x5678ull}, std::pair {0ull, 0xffffffffffffffffull}}) {
            SCOPED_TRACE("test_static_pair v0=" + hex(v0) + " v1=" + hex(v1));
            EXPECT_EQ(Interpret(code, U64(v0), U64(v1)).u64, 1);
        }
    }
}

TEST_ASM(CbcTest, MockPayloadGeneric)
{
    // payload.g rewrites to LEA_GENERIC of the overlapping option TI (every
    // ordinal folds to 0) plus the header addend, so its address is the
    // payload word behind the header: the st.raw store at +8 and the st.g
    // store through lea.box(obj) both surface through payload.g.
    auto path = "./generated-asm/mock-lea/payload.asm";
    for (auto method :
         {"test_payload_raw_opt1", "test_payload_gen_opt1", "test_payload_raw_opt0", "test_payload_gen_opt0"}) {
        auto code = OpenAndRewrite(method, path)->code;
        for (uint64_t payload : {0x5a5aull, 0ull, 0xffffffffffffffffull}) {
            SCOPED_TRACE(std::string(method) + " payload=" + hex(payload));
            EXPECT_EQ(Interpret(code, U64(payload), U64(0)).u64, payload);
        }
    }
}

// ---------------------------------------------------------------------------
// mock-throw: throw/catch minimal flow over the mocked HandleException
// ---------------------------------------------------------------------------

// The mocked HandleException (todo-mock 3.5) models the handler-found leg of
// the real trampoline flow: THROW saves the pc right after itself and returns
// the thunk carrying the exception object; the mock stages it as the pending
// exception and publishes it into IR_ACC through GetPendingException, so the
// CATCH pad right after the throw reads it (the mock has no exception regions
// - the pad must be the next instruction). The mock heap rewinds per run, so
// the allocations are deterministic across the run pairs below.

TEST_ASM(CbcTest, MockThrowCatch)
{
    auto path = "./generated-asm/mock-throw/catch.asm";
    {
        // The catch path is taken and everything live across the throw
        // survives it: the flag is the product of the payload round-trip
        // (seeded before the throw, read after the catch) and the
        // caught==thrown identity.
        auto code = OpenAndRewrite("test_catch", path)->code;
        for (uint64_t seed : {0x1234ull, 0ull, 0xffffffffffffffffull}) {
            SCOPED_TRACE("test_catch seed=" + hex(seed));
            EXPECT_EQ(Interpret(code, U64(seed), U64(0)).u64, 1);
        }
    }
    {
        // The caught object observable: returned through ret.ref and still
        // parked in IR_ACC (CATCH reads IR_ACC and nothing moves it before the
        // ret) - the mock heap rewinds per run, so both reads return the same
        // pointer. Its header carries the synthetic TI and the payload the
        // pre-throw seed.
        auto code = OpenAndRewrite("test_caught_ptr", path)->code;
        for (uint64_t seed : {0x1234ull, 0ull}) {
            SCOPED_TRACE("test_caught_ptr seed=" + hex(seed));
            auto caught = Interpret(code, U64(seed), U64(0)).u64;
            EXPECT_EQ(InterpretAccRes(code, U64(seed), U64(0)).u64, caught);
            ExpectMockObjectHeader(caught);
            EXPECT_EQ(HeaderWord(caught, RTSupport::MetaInfo::ObjectHeaderSize()), seed);
        }
    }
    {
        // An exception object fabricated outside the mock heap (a test-seeded
        // buffer arriving as the parameter) is caught unchanged.
        auto code = OpenAndRewrite("test_throw_param", path)->code;
        TestTypeInfo ti { 2 * RTSupport::MetaInfo::ObjectHeaderSize(), 8, {} };
        alignas(16) uint64_t exc[2] = { 0 };
        exc[0] = reinterpret_cast<uint64_t>(&ti);
        exc[1] = 0x5a5a;
        EXPECT_EQ(Interpret(code, U64(reinterpret_cast<uint64_t>(exc)), U64(0)).u64, 1);
    }
}

// ---------------------------------------------------------------------------
// mock-checks: iof/assign.g/copy and the ref-shaped atomics over the todo-mock
// 3.7 stubs
// ---------------------------------------------------------------------------

// The IsInstanceOf stub is TI-pointer identity with the object header (null is
// not an instance), WriteStructField memcpys the flat payload (synthetic TI
// size - ObjectHeaderSize) between the absolute data addresses, UnwrapDerivedPtr
// is identity on x86_64, and the ref-shaped atomic hooks are seq-cst 8-byte
// accesses of the absolute slot (obj + field offset). The registry TIs carry no
// supertype information and are keyed by the interned term, so the identities
// below hold within one image only - all comparisons stay in-asm or within a
// single Interpret run; the cross-run checks rely on the deterministic mock-heap
// allocation (the heap rewinds per run) rather than on TI pointers.

TEST_ASM(CbcTest, MockIof)
{
    auto path = "./generated-asm/mock-checks/iof.asm";
    // iof/iof.g over a fresh newobj allocation: 1 for the self term, 0 for the
    // different-term object (the header TI mismatches the queried one).
    for (auto [method, expected] :
         {std::pair {"test_iof_self", 1ull},
          std::pair {"test_iof_other", 0ull},
          std::pair {"test_iof_g_self", 1ull},
          std::pair {"test_iof_g_other", 0ull}}) {
        SCOPED_TRACE(std::string(method));
        auto code = OpenAndRewrite(method, path)->code;
        EXPECT_EQ(Interpret(code, U64(0), U64(0)).u64, expected);
    }
    // The IOF lowerings read the reference without a null check, so a raw null
    // (@live.ref register seeded 0) reaches IsInstanceOf and is rejected.
    for (auto method : {"test_iof_null", "test_iof_g_null"}) {
        SCOPED_TRACE(std::string(method));
        auto code = OpenAndRewrite(method, path)->code;
        EXPECT_EQ(Interpret(code, U64(0), U64(0)).u64, 0);
    }
}

TEST_ASM(CbcTest, MockAssignGeneric)
{
    // assign.g lowers to ASSIGN_GENERIC: WriteStructField memcpys the payload
    // word (synthetic TI size - header) from src+header to dst+header, so the
    // destination header must stay untouched, the payload must move exactly,
    // and the source must stay intact.
    auto path = "./generated-asm/mock-checks/assign-g.asm";
    {
        // The destination header and payload arrive poisoned; the copy must
        // leave the poison header, land the seed payload, and keep the source
        // (its header still the run's registry TI, payload the seed).
        auto code = OpenAndRewrite("test_assign_g", path)->code;
        for (uint64_t seed : {0x1234ull, 0ull, 0xffffffffffffffffull}) {
            SCOPED_TRACE("test_assign_g seed=" + hex(seed));
            EXPECT_EQ(Interpret(code, U64(seed), U64(0)).u64, 1);
        }
    }
    {
        // C++-side inspection: dst is allocated before src, so src sits at
        // dst + allocation unit (the synthetic TI size); both headers carry
        // the pointer-identical registry TI of the run.
        auto code = OpenAndRewrite("test_assign_g_dst", path)->code;
        for (uint64_t seed : {0x5a5aull, 0ull}) {
            SCOPED_TRACE("test_assign_g_dst seed=" + hex(seed));
            auto dst = Interpret(code, U64(seed), U64(0)).u64;
            ExpectMockObjectHeader(dst);
            EXPECT_EQ(HeaderWord(dst, RTSupport::MetaInfo::ObjectHeaderSize()), seed);
            auto src = dst + 2 * RTSupport::MetaInfo::ObjectHeaderSize();
            EXPECT_EQ(HeaderWord(src, 0), HeaderWord(dst, 0));
            ExpectMockObjectHeader(src);
            EXPECT_EQ(HeaderWord(src, RTSupport::MetaInfo::ObjectHeaderSize()), seed);
        }
    }
}

TEST_ASM(CbcTest, MockCopyDerived)
{
    // copy/copy.g lower to COPY_DERIVED/COPY_DERIVED_GENERIC: the derived
    // pointers unwrap to the object addresses (identity on x86_64, bases
    // non-null = the HEAP leg) and the copy spans the full synthetic TI size
    // (header + payload) - the clobbered source header overwrites the
    // destination one (the family-2 behavior the mock exhibits), the payload
    // follows, the guard object behind stays intact.
    auto path = "./generated-asm/mock-checks/copy.asm";
    for (auto name : {"test_copy", "test_copy_g"}) {
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

TEST_ASM(CbcTest, MockAtomicRefs)
{
    // atomic.load/store/cas/swap over the REF-typed field r0 lower to
    // ATOMIC_LOAD/LD_REF, ATOMIC_STORE/ST_REF, CAS_REF and ATOMIC_SWAP_REF.
    // The exchanged values are distinct mock-heap objects; the pointer values
    // are asserted in-asm (REQ identities) and C++-side against the sibling
    // run's allocation.
    auto path = "./generated-asm/mock-checks/atomic-refs.asm";
    auto valA = Interpret(OpenAndRewrite("test_atomic_val_ptr", path)->code, U64(0), U64(0)).u64;
    ExpectMockObjectHeader(valA);
    {
        // The store/load round-trip returns the very valA pointer, and the
        // slot behind the header physically holds it.
        SCOPED_TRACE("test_atomic_st_ld");
        EXPECT_EQ(Interpret(OpenAndRewrite("test_atomic_st_ld", path)->code, U64(0), U64(0)).u64, 1);
        auto loaded = Interpret(OpenAndRewrite("test_atomic_st_ld_ptr", path)->code, U64(0), U64(0)).u64;
        EXPECT_EQ(loaded, valA);
        ExpectMockObjectHeader(loaded);
        auto container = Interpret(OpenAndRewrite("test_atomic_st_ptr", path)->code, U64(0), U64(0)).u64;
        EXPECT_NE(container, valA);
        ExpectMockObjectHeader(container);
        EXPECT_EQ(HeaderWord(container, RTSupport::MetaInfo::ObjectHeaderSize()), valA);
    }
    {
        // The swap returns the previous ref (valA) and leaves valB in the slot
        // (checked by the in-asm load identity).
        SCOPED_TRACE("test_atomic_swap");
        EXPECT_EQ(Interpret(OpenAndRewrite("test_atomic_swap", path)->code, U64(0), U64(0)).u64, 1);
        auto old = Interpret(OpenAndRewrite("test_atomic_swap_old", path)->code, U64(0), U64(0)).u64;
        EXPECT_EQ(old, valA);
        ExpectMockObjectHeader(old);
    }
    {
        // CAS success stores the new ref and reports 1; failure keeps the old
        // one and reports 0 (both verified in-asm against the fresh loads).
        for (auto method : {"test_atomic_cas_ok", "test_atomic_cas_fail"}) {
            SCOPED_TRACE(std::string(method));
            EXPECT_EQ(Interpret(OpenAndRewrite(method, path)->code, U64(0), U64(0)).u64, 1);
        }
    }
}
