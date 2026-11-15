#include <gtest/gtest.h>

#include "engine/engine.h"
#include "engine/terms.h"

namespace {

class TermMatcherTest : public testing::Test {
protected:
    Engine::Loader loader;
    Engine::Session session { loader.Build() };
    Engine::TermMatcher matcher;

    Engine::Term t0  = Engine::Term::ClassTypeVariable(0);
    Engine::Term t1  = Engine::Term::ClassTypeVariable(1);
    Engine::Term i64 = Engine::Term::Predefined(Engine::TermKind::I64);

    Engine::Term Pair(Engine::Term first, Engine::Term second)
    {
        Engine::Term args[] = { first, second };
        return Engine::TermManager::NewTermWithId(
            session, Engine::TagTermId(Engine::TermKind::TUPLE), false, Utils::Span<Engine::Term const>(args, 2)
        );
    }
};

TEST_F(TermMatcherTest, BindsVariableToItself)
{
    ASSERT_TRUE(matcher.IsPrefix(t0, t0));
    EXPECT_FALSE(matcher.HasErrors());
    ASSERT_EQ(matcher.vars.Size(), 1u);
    EXPECT_EQ(matcher.vars[0], t0);
}

TEST_F(TermMatcherTest, BindsVariableToConcreteType)
{
    ASSERT_TRUE(matcher.IsPrefix(t0, i64));
    EXPECT_FALSE(matcher.HasErrors());
    ASSERT_EQ(matcher.vars.Size(), 1u);
    EXPECT_EQ(matcher.vars[0], i64);
}

TEST_F(TermMatcherTest, BindsVariablesInsideIdenticalCompound)
{
    auto pair = Pair(t0, t1);
    ASSERT_TRUE(matcher.IsPrefix(pair, pair));
    EXPECT_FALSE(matcher.HasErrors());
    ASSERT_EQ(matcher.vars.Size(), 2u);
    EXPECT_EQ(matcher.vars[0], t0);
    EXPECT_EQ(matcher.vars[1], t1);
}

TEST_F(TermMatcherTest, BindsVariablesInsideSharedSubtree)
{
    auto shared = Pair(t1, t1);
    ASSERT_TRUE(matcher.IsPrefix(Pair(t0, shared), Pair(i64, shared)));
    EXPECT_FALSE(matcher.HasErrors());
    ASSERT_EQ(matcher.vars.Size(), 2u);
    EXPECT_EQ(matcher.vars[0], i64);
    EXPECT_EQ(matcher.vars[1], t1);
}

TEST_F(TermMatcherTest, BindsSymbolicAndConcreteArguments)
{
    ASSERT_TRUE(matcher.IsPrefix(Pair(t0, t1), Pair(t0, i64)));
    EXPECT_FALSE(matcher.HasErrors());
    ASSERT_EQ(matcher.vars.Size(), 2u);
    EXPECT_EQ(matcher.vars[0], t0);
    EXPECT_EQ(matcher.vars[1], i64);
}

TEST_F(TermMatcherTest, AcceptsConsistentRepeatedVariable)
{
    for (auto argument : { t0, i64 }) {
        matcher.Clear();
        ASSERT_TRUE(matcher.IsPrefix(Pair(t0, t0), Pair(argument, argument)));
        EXPECT_FALSE(matcher.HasErrors());
        ASSERT_EQ(matcher.vars.Size(), 1u);
        EXPECT_EQ(matcher.vars[0], argument);
    }
}

TEST_F(TermMatcherTest, RejectsConflictingRepeatedVariable)
{
    for (auto target : { Pair(t0, i64), Pair(i64, t0) }) {
        matcher.Clear();
        ASSERT_TRUE(matcher.IsPrefix(Pair(t0, t0), target));
        EXPECT_TRUE(matcher.HasErrors());
    }
}

TEST_F(TermMatcherTest, RejectsDifferentTermStructure)
{
    EXPECT_FALSE(matcher.IsPrefix(Pair(t0, t1), i64));
    EXPECT_FALSE(matcher.IsPrefix(i64, t0));
}

TEST_F(TermMatcherTest, EqualityStillComparesSharedAndDistinctTerms)
{
    auto pair = Pair(t0, t1);
    EXPECT_EQ(pair, pair);
    EXPECT_EQ(pair, Pair(t0, t1));
    EXPECT_NE(pair, Pair(t0, i64));
    EXPECT_NE(pair, Pair(t1, t0));
    EXPECT_NE(pair, t0);
}

} // namespace
