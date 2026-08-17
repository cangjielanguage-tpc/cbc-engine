#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "cbc/emitter/emitter.h"
#include "cbc/isa_rt.h"
#include "engine/engine.h"
#include "engine/field_layout.h"
#include "engine/terms.h"
#include "resolution/resolution.h"
#include "testutils.h"

namespace {

TEST(BStringTest, HasPointerLayoutButIsNotAGcReference)
{
    Engine::Loader loader;
    Engine::Session session(loader.Build());
    auto layouts = Engine::FieldLayoutManager::New(session);
    auto bstring = Engine::Term::Predefined(Engine::TermKind::BSTRING);

    auto size = layouts->GetFlatSize(bstring);
    ASSERT_TRUE(size.has_value());
    EXPECT_EQ(size.value(), sizeof(void*));
    EXPECT_EQ(layouts->GetFlatAlignment(bstring), alignof(void*));
    EXPECT_FALSE(bstring.IsReference());

    std::vector<uint32_t> referenceOffsets;
    layouts->FillRefOffsets(bstring, referenceOffsets, 32);
    EXPECT_TRUE(referenceOffsets.empty());
}

TEST(BStringTest, CStringBuiltinBoxUsesWideEncoding)
{
    LimitedHeap<1024> heap;
    Cbc::Emitter::Emitter emitter;
    auto rawTypeInfo      = reinterpret_cast<void*>(uintptr_t { 0x12345678 });
    auto previousTypeInfo = Interpretation::builtinTypeInfos[Interpretation::BUILTIN_CSTRING];
    Interpretation::builtinTypeInfos[Interpretation::BUILTIN_CSTRING] = RTSupport::TypeInfo(rawTypeInfo);

    emitter.NewBox(Interpretation::BUILTIN_CSTRING);
    Interpretation::builtinTypeInfos[Interpretation::BUILTIN_CSTRING] = previousTypeInfo;

    auto code = emitter.Build(heap);

    ASSERT_EQ(code.bytecodeSize, 9u);
    Decoder::ByteReader reader(code.bytecode, code.bytecode, code.bytecode + code.bytecodeSize);
    auto instruction = Cbc::RT::B9i64::Decode(reader);
    EXPECT_EQ(instruction.opc, Cbc::RT::Opcode::NEWBOX2);
    EXPECT_EQ(instruction.imm64.ptr, rawTypeInfo);
}

TEST(BStringTest, UsesScalarIntegerRegisterClass)
{
    Engine::Loader loader;
    Engine::Session session(loader.Build());
    auto method =
        Engine::Identifier<Symlevel::MethodDefinition>(Symlevel::Offset<Symlevel::MethodDefinition>(0), IO::FileId(0));
    Resolution::Resolver resolver(session, method);
    auto bstring = Engine::Term::Predefined(Engine::TermKind::BSTRING);

    EXPECT_EQ(resolver.Wrap(bstring).GetKind(), Resolution::CbcTypeKind::U64);
    EXPECT_FALSE(bstring.IsFloat());
}

} // namespace
