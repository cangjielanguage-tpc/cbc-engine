#include <gtest/gtest.h>

#include "utils/vector.h"
#include <numeric>
#include <string>

TEST(Vector, DefaultIsEmpty)
{
    Utils::Vector<int> v;
    ASSERT_TRUE(v.Empty());
    ASSERT_EQ(v.Size(), 0u);
    ASSERT_EQ(v.Capacity(), 0u);
}

TEST(Vector, SizeCtorValueInitializesElements)
{
    Utils::Vector<int> v(5);
    ASSERT_EQ(v.Size(), 5u);
    for (auto x : v) {
        ASSERT_EQ(x, 0);
    }
}

TEST(Vector, PushBackAndAccess)
{
    Utils::Vector<int> v;
    for (int i = 0; i < 100; i++) {
        v.PushBack(i);
    }
    ASSERT_EQ(v.Size(), 100u);
    for (int i = 0; i < 100; i++) {
        ASSERT_EQ(v[i], i);
        ASSERT_EQ(v.At(i), i);
    }
}

TEST(Vector, EmplaceBackReturnsReference)
{
    struct P {
        int x;
        int y;
    };
    Utils::Vector<P> v;
    v.PushBack(P {1, 2});
    ASSERT_EQ(v.Back().x, 1);
    ASSERT_EQ(v.Back().y, 2);
}

TEST(Vector, CopySemantics)
{
    Utils::Vector<int> a(3);
    std::iota(a.begin(), a.end(), 1);
    Utils::Vector<int> b(a);
    ASSERT_EQ(b.Size(), 3u);
    ASSERT_NE(b.Data(), a.Data());
    for (size_t i = 0; i < a.Size(); i++) {
        ASSERT_EQ(a[i], b[i]);
    }
}

TEST(Vector, SelfAssignmentKeepsContent)
{
    Utils::Vector<int> v(3);
    std::iota(v.begin(), v.end(), 1);
    auto* alias = &v;
    v = *alias;
    ASSERT_EQ(v.Size(), 3u);
    ASSERT_EQ(v[0], 1);
    ASSERT_EQ(v[2], 3);
}

TEST(Vector, CopyAssignment)
{
    Utils::Vector<int> a(3);
    std::iota(a.begin(), a.end(), 1);
    Utils::Vector<int> b;
    b.PushBack(100);
    b = a;
    ASSERT_EQ(b.Size(), 3u);
    ASSERT_EQ(b[0], 1);
    ASSERT_EQ(b[2], 3);
}

TEST(Vector, MoveSemantics)
{
    Utils::Vector<int> a(3);
    std::iota(a.begin(), a.end(), 1);
    int* data = a.Data();
    Utils::Vector<int> b(std::move(a));
    ASSERT_EQ(b.Data(), data);
    ASSERT_EQ(b.Size(), 3u);
    ASSERT_TRUE(a.Empty());
    ASSERT_EQ(a.Data(), nullptr);

    Utils::Vector<int> c;
    c.PushBack(7);
    c = std::move(b);
    ASSERT_EQ(c.Size(), 3u);
    ASSERT_EQ(c[0], 1);
    ASSERT_TRUE(b.Empty());
}

TEST(Vector, InitializerList)
{
    Utils::Vector<int> v {1, 2, 3};
    ASSERT_EQ(v.Size(), 3u);
    ASSERT_EQ(v[1], 2);
}

TEST(Vector, Resize)
{
    Utils::Vector<int> v;
    v.Resize(4);
    ASSERT_EQ(v.Size(), 4u);
    v.Resize(2);
    ASSERT_EQ(v.Size(), 2u);
    v.Resize(5, 9);
    ASSERT_EQ(v.Size(), 5u);
    ASSERT_EQ(v[0], 0);
    ASSERT_EQ(v[4], 9);
}

TEST(Vector, PopBackAndClear)
{
    Utils::Vector<int> v {1, 2, 3};
    v.PopBack();
    ASSERT_EQ(v.Size(), 2u);
    ASSERT_EQ(v.Back(), 2);
    v.Clear();
    ASSERT_TRUE(v.Empty());
}

TEST(Vector, NonTrivialElementType)
{
    static int alive = 0;
    struct Counter {
        int* slot;
        explicit Counter(int v) : slot(new int(v)) { alive++; }
        Counter(Counter const& other) : slot(new int(*other.slot)) { alive++; }
        Counter(Counter&& other) : slot(other.slot)
        {
            other.slot = nullptr;
            alive++;
        }
        Counter& operator=(Counter const&) = delete;
        ~Counter()
        {
            delete slot;
            alive--;
        }
    };

    ASSERT_EQ(alive, 0);
    {
        Utils::Vector<Counter> v;
        v.EmplaceBack(1);
        v.EmplaceBack(2);
        v.PushBack(Counter(3));
        ASSERT_EQ(alive, 3);
        ASSERT_EQ(*v[1].slot, 2);

        v.Reserve(100);
        ASSERT_EQ(alive, 3);
        ASSERT_EQ(*v[2].slot, 3);

        v.PopBack();
        ASSERT_EQ(alive, 2);
    }
    ASSERT_EQ(alive, 0);
}

TEST(Vector, NonTrivialMoveDuringGrowth)
{
    // Elements must survive reallocation (move + destroy) when capacity grows.
    Utils::Vector<std::string> v;
    for (int i = 0; i < 64; i++) {
        v.PushBack("element" + std::to_string(i));
    }
    ASSERT_EQ(v.Size(), 64u);
    for (int i = 0; i < 64; i++) {
        ASSERT_EQ(v[i], "element" + std::to_string(i));
    }
}

TEST(Vector, ReserveLargeCapacityDoesNotWrapAllocationSize)
{
    Utils::Vector<int> v;
    ASSERT_DEATH(v.Reserve(SIZE_MAX), ".*");
}
