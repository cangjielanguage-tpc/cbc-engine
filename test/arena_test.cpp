#include <gtest/gtest.h>

#include "engine/arena.h"

TEST(Arena, SmallAllocs)
{
    Engine::Arena arena;
    auto mem1 = arena.Allocate(1, 1);
    auto mem2 = arena.Allocate(1, 1);
    auto mem3 = arena.Allocate(1, 1);
    ASSERT_NE(mem1, mem2);
    ASSERT_NE(mem1, mem3);
    ASSERT_NE(mem3, mem2);
}

TEST(Arena, ImplAware1)
{
    Engine::Arena arena;
    auto mem1 = reinterpret_cast<uintptr_t>(arena.Allocate(1, 1));
    auto mem2 = reinterpret_cast<uintptr_t>(arena.Allocate(1, 1));
    auto mem3 = reinterpret_cast<uintptr_t>(arena.Allocate(1, 1));
    ASSERT_EQ(mem1 + alignof(std::max_align_t), mem2);
    ASSERT_EQ(mem2 + alignof(std::max_align_t), mem3);
}

TEST(Arena, ImplAware2)
{
    Engine::Arena arena;
    auto mem1   = reinterpret_cast<uintptr_t>(arena.Allocate(1, 1));
    auto mem2   = reinterpret_cast<uintptr_t>(arena.Allocate(1, 1));
    auto bigMem = reinterpret_cast<char*>(arena.Allocate(1024, 1));
    auto mem3   = reinterpret_cast<uintptr_t>(arena.Allocate(1, 1));
    ASSERT_EQ(mem1 + alignof(std::max_align_t), mem2);
    ASSERT_EQ(mem2 + alignof(std::max_align_t), mem3);
    bigMem[1023] = 0;
}
