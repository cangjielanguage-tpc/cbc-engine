#include "utils/sharedobj.h"

#include <dlfcn.h>
#include <gtest/gtest.h>

TEST(SharedObjectSearchSym, ResolvesSymbol)
{
    auto object = Utils::SharedObject::OpenCurrentExecutable();
    ASSERT_TRUE(object.IsOpened());

    auto [symbol, error] = object.SearchSym("malloc");
    EXPECT_NE(symbol, nullptr);
    EXPECT_FALSE(error.has_value());
}

TEST(SharedObjectSearchSym, MissingSymbolReturnsError)
{
    auto object = Utils::SharedObject::OpenCurrentExecutable();
    ASSERT_TRUE(object.IsOpened());

    auto [symbol, error] = object.SearchSym("cbc_engine_test_missing_symbol");
    EXPECT_EQ(symbol, nullptr);
    ASSERT_TRUE(error.has_value());
    EXPECT_NE(error->find("cbc_engine_test_missing_symbol"), std::string::npos);
    EXPECT_EQ(dlerror(), nullptr);
}

TEST(SharedObjectSearchSym, SuccessfulLookupClearsStaleError)
{
    auto object = Utils::SharedObject::OpenCurrentExecutable();
    ASSERT_TRUE(object.IsOpened());
    ASSERT_EQ(dlsym(RTLD_DEFAULT, "cbc_engine_test_stale_error"), nullptr);

    auto [symbol, error] = object.SearchSym("malloc");
    EXPECT_NE(symbol, nullptr);
    EXPECT_FALSE(error.has_value());
    EXPECT_EQ(dlerror(), nullptr);
}

TEST(SharedObjectSearchSym, ErrorMessageSurvivesFurtherLookups)
{
    auto object = Utils::SharedObject::OpenCurrentExecutable();
    ASSERT_TRUE(object.IsOpened());

    auto first = object.SearchSym("cbc_engine_test_first_missing_symbol");
    ASSERT_TRUE(first.error.has_value());
    auto firstError = *first.error;

    auto second = object.SearchSym("cbc_engine_test_second_missing_symbol");
    ASSERT_TRUE(second.error.has_value());
    EXPECT_NE(*second.error, firstError);
    EXPECT_EQ(*first.error, firstError);
}

TEST(SharedObjectSearchSym, UnopenedObjectReturnsError)
{
    Utils::SharedObject object;

    auto [symbol, error] = object.SearchSym("malloc");
    EXPECT_EQ(symbol, nullptr);
    ASSERT_TRUE(error.has_value());
    EXPECT_FALSE(error->empty());
}
