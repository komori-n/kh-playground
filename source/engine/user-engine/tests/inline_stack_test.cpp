#include <gtest/gtest.h>

#include "../inline_stack.hpp"

using komori::InlineStack;

TEST(InlineStackTest, Constructor) {
  InlineStack<std::uint32_t, 10> stack1;
  EXPECT_TRUE(stack1.empty());

  InlineStack<std::uint32_t, 10> stack2{3, 3, 4};
  EXPECT_EQ(stack2.size(), 3);
}

TEST(InlineStackTest, Push) {
  InlineStack<std::uint32_t, 10> stack;

  EXPECT_EQ(stack.Push(33), 0);
  EXPECT_EQ(stack.Push(4), 1);
}

TEST(InlineStackTest, Emplace) {
  InlineStack<std::pair<std::uint32_t, std::uint32_t>, 10> stack;

  EXPECT_EQ(stack.Emplace(33, 4), 0);
  EXPECT_EQ(stack.Emplace(26, 4), 1);
}

TEST(InlineStackTest, Pop) {
  InlineStack<std::uint32_t, 10> stack{3, 3, 4};

  stack.Pop();
  EXPECT_EQ(stack.size(), 2);
}

TEST(InlineStackTest, IteratorArithmetic) {
  InlineStack<std::uint32_t, 10> stack{3, 3, 4};
  auto begin = stack.begin();

  auto it1 = begin;
  EXPECT_EQ(++it1, begin + 1);
  EXPECT_EQ(it1, begin + 1);

  auto it2 = begin;
  EXPECT_EQ(it2++, begin);
  EXPECT_EQ(it2, begin + 1);

  auto it3 = begin + 1;
  EXPECT_EQ(--it3, begin);
  EXPECT_EQ(it3, begin);

  auto it4 = begin + 1;
  EXPECT_EQ(it4--, begin + 1);
  EXPECT_EQ(it4, begin);

  EXPECT_EQ(stack.end() - stack.begin(), 3);
  EXPECT_EQ(stack.begin() + 3, stack.end());
  EXPECT_EQ(3 + stack.begin(), stack.end());
  EXPECT_EQ(stack.end() - 3, stack.begin());

  auto it5 = begin;
  EXPECT_EQ(it5 += 2, begin + 2);
  EXPECT_EQ(it5, begin + 2);

  auto it6 = begin + 2;
  EXPECT_EQ(it6 -= 2, begin);
  EXPECT_EQ(it6, begin);
}

TEST(InlineStackTest, IteratorComparison) {
  InlineStack<std::uint32_t, 10> stack{3, 3, 4};

  auto it1 = stack.begin();
  auto it2 = stack.begin() + 1;

  EXPECT_TRUE(it1 == it1);
  EXPECT_FALSE(it1 == it2);

  EXPECT_FALSE(it1 != it1);
  EXPECT_TRUE(it1 != it2);

  EXPECT_TRUE(it1 < it2);
  EXPECT_FALSE(it1 < it1);

  EXPECT_TRUE(it1 <= it1);
  EXPECT_FALSE(it2 <= it1);

  EXPECT_TRUE(it2 > it1);
  EXPECT_FALSE(it1 > it1);

  EXPECT_TRUE(it1 >= it1);
  EXPECT_FALSE(it1 >= it2);
}

TEST(InlineStackTest, IteratorAccess) {
  InlineStack<std::pair<std::uint32_t, std::uint32_t>, 10> stack{{33, 4}, {26, 4}};

  auto it = stack.begin();
  EXPECT_EQ((*it).first, 33);
  EXPECT_EQ(it->second, 4);
  EXPECT_EQ(it[1].first, 26);
}

TEST(InlineStackTest, Access) {
  InlineStack<std::uint32_t, 10> stack{3, 3, 4};

  EXPECT_EQ(stack.front(), 3);
  EXPECT_EQ(stack[1], 3);
  EXPECT_EQ(stack.back(), 4);
}

TEST(IntlineStackTest, ConstAccess) {
  InlineStack<std::uint32_t, 10> stack{3, 3, 4};

  const auto& const_stack = stack;
  EXPECT_EQ(const_stack.front(), 3);
  EXPECT_EQ(const_stack[1], 3);
  EXPECT_EQ(const_stack.back(), 4);
}

TEST(InlineStackTest, Size) {
  InlineStack<std::uint32_t, 10> stack{3, 3, 4};

  EXPECT_EQ(stack.max_size(), 10);
  EXPECT_EQ(stack.size(), 3);
  EXPECT_FALSE(stack.empty());
}

TEST(InlineStackTest, Clear) {
  InlineStack<std::uint32_t, 10> stack{3, 3, 4};

  stack.clear();
  EXPECT_TRUE(stack.empty());
}

TEST(InlineStackTest, NonDefaultConstructible) {
  struct NonDefaultConstructible {
    explicit NonDefaultConstructible(int) {}
  };

  InlineStack<NonDefaultConstructible, 10> stack;
  stack.Emplace(33);
  stack.Emplace(4);
}
