#include "../splitted_hand.hpp"
#include "test_lib.hpp"

#include <gtest/gtest.h>

TEST(HandsTest, SplittedHandMergeByMax) {
  komori::SplittedHand hand = komori::SplittedHand::Zero();
  const Hand rhs = MakeHand<PAWN, LANCE, LANCE, SILVER, GOLD, BISHOP, ROOK>();

  hand.MergeByMax(rhs);
  EXPECT_EQ(hand.ToHand(), rhs);
}

TEST(HandsTest, SplittedHandMergeByMin) {
  komori::SplittedHand hand = komori::SplittedHand::Full();
  const Hand rhs = MakeHand<PAWN, LANCE, LANCE, SILVER, GOLD, BISHOP, ROOK>();

  hand.MergeByMin(rhs);
  EXPECT_EQ(hand.ToHand(), rhs);
}
