#include <gtest/gtest.h>

#include "utils/span.h"
#include "utils/vector.h"
#include <string>

TEST(Span, DefaultIsEmpty)
{
    Utils::Span<int const> s;
    ASSERT_EQ(nullptr, s.Data());
    ASSERT_EQ(0u, s.Size());
    ASSERT_TRUE(s.Empty());
}

TEST(Span, FromPointerAndSize)
{
    int arr[] = {1, 2, 3};
    Utils::Span<int> s(arr, 3);
    ASSERT_EQ(arr, s.Data());
    ASSERT_EQ(3u, s.Size());
    ASSERT_FALSE(s.Empty());
    ASSERT_EQ(1, s[0]);
    ASSERT_EQ(3, s[2]);
    ASSERT_EQ(1, s.Front());
    ASSERT_EQ(3, s.Back());
    s[1] = 42;
    ASSERT_EQ(42, arr[1]);
    s.Back() = 33;
    ASSERT_EQ(33, arr[2]);
}

TEST(Span, ZeroSizeWithNonNullPointer)
{
    int arr[] = {1, 2, 3};
    Utils::Span<int> s(arr, 0);
    ASSERT_TRUE(s.Empty());
    ASSERT_EQ(arr, s.Data());
    ASSERT_EQ(s.begin(), s.end());
}

TEST(Span, AtOutOfBoundsAborts)
{
    int arr[] = {1, 2, 3};
    Utils::Span<int> s(arr, 3);
    ASSERT_DEATH(s.At(3), ".*");
    Utils::Span<int const> c(arr, 3);
    ASSERT_DEATH(c.At(100), ".*");
}

TEST(Span, FromVectorDeducesConstSpan)
{
    Utils::Vector<int> v {1, 2, 3};
    Utils::Span s = v;
    static_assert(std::is_same_v<decltype(s), Utils::Span<int const>>);
    ASSERT_EQ(v.Data(), s.Data());
    ASSERT_EQ(3u, s.Size());
    ASSERT_EQ(1, s[0]);
    ASSERT_EQ(3, s.Back());

    // The span is a view: mutations of the vector are visible through it.
    v[1] = 42;
    ASSERT_EQ(42, s[1]);
}

TEST(Span, ExplicitConstSpanFromVector)
{
    Utils::Vector<int> v {7};
    Utils::Span<int const> s(v);
    ASSERT_EQ(v.Data(), s.Data());
    ASSERT_EQ(1u, s.Size());
    ASSERT_EQ(7, s[0]);
}

TEST(Span, MutableSpanConvertsToConstSpan)
{
    int arr[] = {1, 2};
    Utils::Span<int> m(arr, 2);

    Utils::Span<int const> c = m;   // implicit conversion operator
    ASSERT_EQ(arr, c.Data());
    ASSERT_EQ(2u, c.Size());

    Utils::Span<int const> c2(m);   // converting copy-ctor from Span<U>
    ASSERT_EQ(arr, c2.Data());
}

TEST(Span, Iteration)
{
    int arr[] = {1, 2, 3};
    Utils::Span<int> s(arr, 3);

    int sum = 0;
    for (int x : s) {
        sum += x;
    }
    ASSERT_EQ(6, sum);

    for (auto it = s.begin(); it != s.end(); ++it) {
        *it += 1;
    }
    ASSERT_EQ(2, arr[0]);
    ASSERT_EQ(4, arr[2]);

    Utils::Span<int const> c(arr, 3);
    sum = 0;
    for (int x : c) {
        sum += x;
    }
    ASSERT_EQ(9, sum);
}

TEST(Span, SpanOfStrings)
{
    std::string strs[] = {"a", "bb"};
    Utils::Span<std::string> s(strs, 2);
    ASSERT_EQ("a", s.Front());
    ASSERT_EQ("bb", s.Back());
    s[0] = "z";
    ASSERT_EQ("z", strs[0]);
}
