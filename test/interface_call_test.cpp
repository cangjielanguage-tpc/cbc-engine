#include <gtest/gtest.h>

#include <array>
#include <fstream>
#include <vector>

#include "cbc/emitter/emitter.h"
#include "cbc/formater_rt.h"
#include "cbc/isa_rt.h"
#include "engine/terms.h"
#include "interpreter/code.h"
#include "interpreter/interpretation_loop.h"
#include "interpreter/platform_traits.h"
#include "mock/interpreter.h"
#include "resolution/resolution.h"
#include "testutils.h"

namespace {

using Cbc::IReg;
using Interpretation::StaticCallTypeInfoArgs;
using RTSupport::TypeInfo;

class InterfaceCallTest : public testing::Test {
protected:
    LimitedHeap<16384> heap;

    void SetUp() override { InitializeMockInterpreter(); }

    void TearDown() override { InterfaceDispatchMock() = {}; }
};

Engine::Term Signature(Engine::Session& session, std::vector<Engine::TermKind> params)
{
    std::vector<Engine::Term> terms;
    for (auto kind : params) {
        terms.push_back(Engine::Term::Predefined(kind));
    }
    terms.push_back(Engine::Term::Predefined(Engine::TermKind::I64));
    return Engine::TermManager::NewTermWithId(session, Engine::TagTermId(Engine::TermKind::FUNCTIONAL), false, terms);
}

TEST_F(InterfaceCallTest, HiddenArgumentLocations)
{
    using K = Engine::TermKind;
    Engine::Loader loader;
    Engine::Session session(loader.Build());

    for (bool sret : { false, true }) {
        unsigned shift = HAS_SRET_SHIFT && sret;
        auto args      = Interpretation::LocateStaticCallTypeInfoArgs(Signature(session, {}), sret, true);
        EXPECT_EQ(args.outerTi, 1 + shift);
        EXPECT_EQ(args.thisTi, 2 + shift);

        // Independent FP registers do not move the hidden integer parameters.
        args = Interpretation::LocateStaticCallTypeInfoArgs(Signature(session, { K::F64, K::I64, K::F32 }), sret, true);
        EXPECT_EQ(args.outerTi, 2 + shift);
        EXPECT_EQ(args.thisTi, 3 + shift);

        // Outer TI is in the last integer register and this TI is the first stack argument.
        std::vector<K> params(IREG_PARAM_PASSING_AMOUNT - 1 - shift, K::I64);
        args = Interpretation::LocateStaticCallTypeInfoArgs(Signature(session, params), sret, true);
        EXPECT_EQ(args.outerTi, IREG_PARAM_PASSING_AMOUNT);
        EXPECT_EQ(args.thisTi, IReg::VIRT_COUNT);

        // Both TIs spill, after integer and floating-point overflow arguments.
        params.assign(IREG_PARAM_PASSING_AMOUNT + 2 - shift, K::I64);
        params.insert(params.end(), FREG_ABI_AMOUNT + 1, K::F64);
        args = Interpretation::LocateStaticCallTypeInfoArgs(Signature(session, params), sret, true);
        EXPECT_EQ(args.outerTi, IReg::VIRT_COUNT + 3);
        EXPECT_EQ(args.thisTi, IReg::VIRT_COUNT + 4);
    }

    auto args = Interpretation::LocateStaticCallTypeInfoArgs(Signature(session, {}), false, false);
    EXPECT_EQ(args.outerTi, StaticCallTypeInfoArgs::NONE);
    EXPECT_EQ(args.thisTi, IReg::IR1);
}

TEST_F(InterfaceCallTest, StaticVirtualFlags)
{
    using F  = Image::MethodRefFlag;
    auto nil = Engine::Term::Predefined(Engine::TermKind::NIL);
    Resolution::InterfaceCall::Content method { { nil, nullptr }, {}, { nullptr, nil }, 0, {} };
    EXPECT_FALSE(method.IsStaticVirtual());
    method.flags = method.flags.Or(F::HAS_THIS_TI).Or(F::HAS_OUTER_TI).Or(F::SRET);
    EXPECT_TRUE(method.IsStaticVirtual());
    for (auto receiver : { F::REF_RECEIVER, F::REC_RECEIVER, F::MUT }) {
        auto instance  = method;
        instance.flags = instance.flags.Or(receiver);
        EXPECT_FALSE(instance.IsStaticVirtual());
    }
}

TEST_F(InterfaceCallTest, ResolvesDeclaredSignatureInReferenceContext)
{
    if (!std::ifstream(std::string(TEST_RESOURCE_DIR) + "/cbc-asm.jar")) {
        GTEST_SKIP() << "Assembler is not present";
    }
    const std::string fileName = "static-interface-reference.asm";
    Engine::Loader loader;
    ASSERT_TRUE(loader.Load(OpenAsm(fileName), fileName));
    auto& engine = loader.Build();
    Engine::Session session(engine);
    auto main = engine.FindMain(session, fileName);
    ASSERT_TRUE(main.has_value());
    Resolution::Resolver resolver(session, *main);

    for (unsigned index : { 0, 1 }) {
        auto call = resolver.Query(Resolution::Index<Resolution::InterfaceCall>(index));
        ASSERT_TRUE(call.has_value());
        auto method = *call;
        EXPECT_TRUE(method->IsStaticVirtual());
        EXPECT_EQ(method->methodNum, 0);
        auto args = Interpretation::LocateStaticCallTypeInfoArgs(method->signature.term, true, true);
        // Even Operation<Float64>.calculate takes the erased generic argument in an IReg.
        EXPECT_EQ(args.outerTi, 2 + HAS_SRET_SHIFT);
        EXPECT_EQ(args.thisTi, 3 + HAS_SRET_SHIFT);
    }
}

TEST_F(InterfaceCallTest, EncodingAndDisassembly)
{
    Cbc::Emitter::Emitter emitter;
    emitter.InterfaceCall(7, TypeInfo(uintptr_t(1234)), true, { IReg::IR6, IReg::VIRT_COUNT });
    emitter.InterfaceCallGeneric(9, IReg::VIRT_COUNT + 1, false, IReg::VIRT_COUNT + 2);
    emitter.InterfaceCall(11, TypeInfo(uintptr_t(4321)), false);
    emitter.InterfaceCallGeneric(12, IReg::IR3, true);
    auto code = emitter.Build(heap);
    Decoder::ByteReader reader(code.bytecode, code.bytecode, code.bytecode + code.bytecodeSize);

    auto concrete = Cbc::RT::InterfaceCall::Decode(reader);
    EXPECT_EQ(concrete.opc, Cbc::RT::Opcode::INTERFACE_CALL);
    EXPECT_EQ(concrete.vnum, 7);
    EXPECT_EQ(concrete.ti, 1234u);
    EXPECT_EQ(concrete.sret, 1);
    EXPECT_EQ(concrete.outerTiArg, IReg::IR6);
    EXPECT_EQ(concrete.thisTiArg, IReg::VIRT_COUNT);

    auto generic = Cbc::RT::InterfaceCallGeneric::Decode(reader);
    EXPECT_EQ(generic.opc, Cbc::RT::Opcode::INTERFACE_CALL_GENERIC);
    EXPECT_EQ(generic.vnum, 9);
    EXPECT_EQ(generic.argn, IReg::VIRT_COUNT + 1);
    EXPECT_EQ(generic.sret, 0);
    EXPECT_EQ(generic.thisTiArg, IReg::VIRT_COUNT + 2);

    concrete = Cbc::RT::InterfaceCall::Decode(reader);
    EXPECT_EQ(concrete.outerTiArg, StaticCallTypeInfoArgs::NONE);
    EXPECT_EQ(concrete.thisTiArg, StaticCallTypeInfoArgs::NONE);
    generic = Cbc::RT::InterfaceCallGeneric::Decode(reader);
    EXPECT_EQ(generic.thisTiArg, StaticCallTypeInfoArgs::NONE);
    EXPECT_EQ(reader.Cursor(), code.bytecode + code.bytecodeSize);

    Stream::StringBuffer stream;
    Cbc::RT::Log(code, stream);
    auto text = stream.ToString();
    EXPECT_NE(text.find("outer=0x6 this=0xe"), std::string::npos);
    EXPECT_NE(text.find("this=0x10"), std::string::npos);
}

TEST_F(InterfaceCallTest, StaticDispatchPreservesArguments)
{
    // Deliberately invalid object addresses: static dispatch must never dereference these TIs.
    TypeInfo receiver(uintptr_t(0x1111)), reference(uintptr_t(0x2222)), outer(uintptr_t(0x3333));
    const StaticCallTypeInfoArgs locations[] = {
        { IReg::IR3, IReg::IR4 },
        { IREG_PARAM_PASSING_AMOUNT, IReg::VIRT_COUNT },
        { IReg::VIRT_COUNT + 3, IReg::VIRT_COUNT + 4 },
    };
    for (bool generic : { false, true }) {
        for (bool sret : { false, true }) {
            for (auto args : locations) {
                SCOPED_TRACE(testing::Message() << generic << "/" << sret << "/" << args.outerTi);
                heap.Reset();
                Cbc::Emitter::Emitter emitter;
                if (generic) {
                    emitter.InterfaceCallGeneric(7, args.outerTi, sret, args.thisTi);
                } else {
                    emitter.InterfaceCall(7, reference, sret, args);
                }
                auto code = emitter.Build(heap);
                Decoder::ByteReader reader(code.bytecode, code.bytecode, code.bytecode + code.bytecodeSize);
                Interpretation::Ectype ectype;
                std::array<uint64_t, 8> slots;
                slots.fill(0x5555);
                for (unsigned i = 1; i < IReg::COUNT; ++i) {
                    ectype.Put(IReg::From(i), U64(0x100 + i));
                }
                for (unsigned i = 0; i < Cbc::FReg::COUNT; ++i) {
                    ectype.Put(Cbc::FReg::From(i), F64(i + 0.5));
                }
                ectype.Put(IReg::IR_ACC, U64(reference.UInt()));
                if (args.thisTi < IReg::VIRT_COUNT) {
                    ectype.Put(IReg::From(args.thisTi), U64(receiver.UInt()));
                } else {
                    slots[args.thisTi - IReg::VIRT_COUNT] = receiver.UInt();
                }
                auto before        = ectype;
                auto previousSlots = slots;

                auto& mock   = InterfaceDispatchMock();
                mock         = {};
                mock.enabled = true;
                mock.outerTi = outer;
                mock.thunk   = { reinterpret_cast<void*>(0x4444), reinterpret_cast<void*>(0x6666) };
                auto thunk   = Interpretation::InterpretationLoop(
                    &ectype,
                    { reinterpret_cast<uintptr_t>(slots.data()) },
                    RTSupport::ThreadHandle(nullptr),
                    code.literals,
                    reader
                );
                EXPECT_EQ(thunk.function, mock.thunk.function);
                EXPECT_EQ(thunk.arg, mock.thunk.arg);
                EXPECT_EQ(reader.Cursor(), code.bytecode + code.bytecodeSize);
                ASSERT_EQ(mock.outerTiCalls.size(), 1u);
                ASSERT_EQ(mock.dispatchCalls.size(), 1u);
                for (auto call : { mock.outerTiCalls[0], mock.dispatchCalls[0] }) {
                    EXPECT_EQ(call.receiver.UInt(), receiver.UInt());
                    EXPECT_EQ(call.reference.UInt(), reference.UInt());
                    EXPECT_EQ(call.methodNum, 7);
                }
                for (unsigned i = 1; i < IReg::COUNT; ++i) {
                    auto expected = i == args.outerTi ? outer.UInt() : before.GetPrimitive(IReg::From(i)).u64;
                    EXPECT_EQ(ectype.GetPrimitive(IReg::From(i)).u64, expected);
                }
                for (unsigned i = 0; i < slots.size(); ++i) {
                    auto expected = i + IReg::VIRT_COUNT == args.outerTi ? outer.UInt() : previousSlots[i];
                    EXPECT_EQ(slots[i], expected);
                }
                for (unsigned i = 0; i < Cbc::FReg::COUNT; ++i) {
                    EXPECT_EQ(ectype.GetPrimitive(Cbc::FReg::From(i)).f64, i + 0.5);
                }
            }
        }
    }
}

TEST_F(InterfaceCallTest, InstanceDispatchStillReadsObjectHeader)
{
    TypeInfo objectHeader(uintptr_t(0x1111)), reference(uintptr_t(0x2222)), outer(uintptr_t(0x3333));
    for (bool generic : { false, true }) {
        for (bool sret : { false, true }) {
            heap.Reset();
            Cbc::Emitter::Emitter emitter;
            if (generic) {
                emitter.InterfaceCallGeneric(4, IReg::IR4, sret);
            } else {
                emitter.InterfaceCall(4, reference, sret);
            }
            auto code = emitter.Build(heap);
            Decoder::ByteReader reader(code.bytecode, code.bytecode, code.bytecode + code.bytecodeSize);
            Interpretation::Ectype ectype;
            auto receiver = HAS_SRET_SHIFT && sret ? IReg::IR2 : IReg::IR1;
            ectype.Put(receiver, Interpretation::Value::Reference { reinterpret_cast<uintptr_t>(&objectHeader) });
            ectype.Put(IReg::IR_ACC, U64(reference.UInt()));
            auto& mock   = InterfaceDispatchMock();
            mock         = {};
            mock.enabled = true;
            mock.outerTi = outer;
            Interpretation::InterpretationLoop(&ectype, { 0 }, RTSupport::ThreadHandle(nullptr), code.literals, reader);
            ASSERT_EQ(mock.dispatchCalls.size(), 1u);
            EXPECT_EQ(mock.dispatchCalls[0].receiver.UInt(), objectHeader.UInt());
            EXPECT_EQ(mock.dispatchCalls[0].reference.UInt(), reference.UInt());
            EXPECT_EQ(mock.dispatchCalls[0].methodNum, 4);
            EXPECT_EQ(mock.outerTiCalls.size(), generic ? 1u : 0u);
            if (generic) {
                EXPECT_EQ(ectype.GetPrimitive(IReg::IR4).u64, outer.UInt());
            }
        }
    }
}

} // namespace
