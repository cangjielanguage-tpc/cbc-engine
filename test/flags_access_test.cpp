#include "engine/image/flags.h"

#include <gtest/gtest.h>

namespace {

using Image::AccessKind;
using Image::FieldFlags;
using Image::MethodFlags;
using Image::TypeFlags;

TEST(FlagsAccess, FieldFlagsWithSetsAccessKind)
{
    FieldFlags flags;
    ASSERT_TRUE(flags.Is(AccessKind::INVALID));

    auto publicFlags = flags.With(AccessKind::PUBLIC);
    ASSERT_TRUE(publicFlags.Is(AccessKind::PUBLIC));
    ASSERT_EQ(AccessKind::PUBLIC, publicFlags.GetAccessKind());

    auto privateFlags = publicFlags.With(AccessKind::PRIVATE);
    ASSERT_TRUE(privateFlags.Is(AccessKind::PRIVATE));
    ASSERT_FALSE(privateFlags.Is(AccessKind::PUBLIC));

    ASSERT_TRUE(flags.Is(AccessKind::INVALID));
}

TEST(FlagsAccess, FieldFlagsWithKeepsOtherBits)
{
    auto flags  = FieldFlags {}.Or(Image::FieldFlag::FINAL).With(AccessKind::PROTECTED);
    ASSERT_TRUE(flags.Is(AccessKind::PROTECTED));
    ASSERT_TRUE(flags.Is(Image::FieldFlag::FINAL));
}

TEST(FlagsAccess, MethodAndTypeFlagsWithSetAccessKind)
{
    ASSERT_TRUE(MethodFlags {}.With(AccessKind::PRIVATE).Is(AccessKind::PRIVATE));
    ASSERT_TRUE(TypeFlags {}.With(AccessKind::PROTECTED).Is(AccessKind::PROTECTED));
}

} // namespace
