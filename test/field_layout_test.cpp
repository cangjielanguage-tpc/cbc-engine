#include <gtest/gtest.h>

#include <cstdint>
#include <optional>

#include "engine/engine.h"
#include "engine/field_layout.h"
#include "engine/terms.h"
#include "engine/typeinfo_manager.h"
#include "mock/interpreter.h"
#include "utils/vector.h"

namespace {

class FixedTypeInfoManager final : public Engine::TypeInfoManager {
public:
    explicit FixedTypeInfoManager(RTSupport::TypeInfo tupleTypeInfo) : tupleTypeInfo(tupleTypeInfo) {}

    Engine::GlobalTerm AcquireTerm(Engine::Session&, RTSupport::TypeInfo) override
    {
        return Engine::Term::Predefined(Engine::TermKind::NIL);
    }

    std::optional<RTSupport::TypeInfo> AcquireTypeInfo(Engine::Session&, Engine::GlobalTerm term) override
    {
        if (term.GetKind() == Engine::TermKind::TUPLE) {
            return tupleTypeInfo;
        }
        return std::nullopt;
    }

private:
    RTSupport::TypeInfo tupleTypeInfo;
};

Engine::Term MakeTuple(Engine::Session& session)
{
    auto& terms    = Engine::TermManager::Of(session);
    auto reference = terms.NewAotTerm(session, "test.Reference", {}, true);
    Utils::Vector<Engine::Term> subterms;
    subterms.push_back(Engine::Term::Predefined(Engine::TermKind::I32));
    subterms.push_back(reference);
    return terms.NewTermWithId(
        session,
        Engine::TagTermId(Engine::TermKind::TUPLE),
        false,
        subterms
    );
}

TEST(FieldLayoutTest, TupleUsesTypeInfoAlignment)
{
    Engine::Loader loader;
    Engine::Session session(loader.Build());
    TestTypeInfo tupleTypeInfo(16, 8, { 8 });
    FixedTypeInfoManager typeInfoManager { RTSupport::TypeInfo(&tupleTypeInfo) };
    auto layouts = Engine::FieldLayoutManager::New(session, typeInfoManager);
    auto tuple   = MakeTuple(session);

    EXPECT_EQ(layouts->GetFlatSize(tuple), 16u);
    EXPECT_EQ(layouts->GetFlatAlignment(tuple), 8u);

    auto layout = layouts->GetLayout(tuple);
    ASSERT_TRUE(layout.has_value());
    auto content = layout->operator->();
    ASSERT_TRUE(content->desc.size.has_value());
    EXPECT_EQ(content->desc.size.value(), 16u);
    EXPECT_EQ(content->desc.alignment, 8u);
    ASSERT_EQ(content->fields.size(), 2u);
    EXPECT_EQ(content->fields[0].offset, 0u);
    EXPECT_EQ(content->fields[1].offset, 8u);
}

TEST(FieldLayoutTest, TupleReferenceOffsetsIncludeContainingDisplacement)
{
    Engine::Loader loader;
    Engine::Session session(loader.Build());
    TestTypeInfo tupleTypeInfo(16, 8, { 8 });
    FixedTypeInfoManager typeInfoManager { RTSupport::TypeInfo(&tupleTypeInfo) };
    auto layouts = Engine::FieldLayoutManager::New(session, typeInfoManager);
    auto tuple   = MakeTuple(session);

    Utils::Vector<uint32_t> referenceOffsets;
    layouts->FillRefOffsets(tuple, referenceOffsets, 8);

    ASSERT_EQ(referenceOffsets.size(), 1u);
    EXPECT_EQ(referenceOffsets[0], 16u);
}

} // namespace
