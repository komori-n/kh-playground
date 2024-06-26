#include "../redundant_move_table.hpp"

#include <gtest/gtest.h>
#include "test_lib.hpp"

using komori::tt::RedundantMoveTable;

TEST(RedundantMoveTableTest, Clear) {
  RedundantMoveTable table;
  table.Insert(0x334, HAND_ZERO, make_move(SQ_11, SQ_12, B_PAWN));
  EXPECT_TRUE(table.Contains(0x334, HAND_ZERO, make_move(SQ_11, SQ_12, B_PAWN)));

  // When
  table.Clear();
  // Then
  EXPECT_FALSE(table.Contains(0x334, HAND_ZERO, make_move(SQ_11, SQ_12, B_PAWN)));
}

TEST(RedundantMoveTableTest, Insert) {
  RedundantMoveTable table;
  table.Insert(0x334, HAND_ZERO, make_move(SQ_11, SQ_12, B_PAWN));
  EXPECT_TRUE(table.Contains(0x334, HAND_ZERO, make_move(SQ_11, SQ_12, B_PAWN)));
}

TEST(RedundantMoveTableTest, Contains) {
  RedundantMoveTable table;
  const Hand hand = MakeHand<PAWN, LANCE, LANCE>();
  table.Insert(0x334, hand, make_move(SQ_11, SQ_12, B_PAWN));

  // equal lookup
  EXPECT_TRUE(table.Contains(0x334, hand, make_move(SQ_11, SQ_12, B_PAWN)));
  // inferior lookup
  EXPECT_FALSE(table.Contains(0x334, MakeHand<PAWN>(), make_move(SQ_11, SQ_12, B_PAWN)));
  // superior lookup
  EXPECT_TRUE(table.Contains(0x334, MakeHand<PAWN, KNIGHT, LANCE, LANCE>(), make_move(SQ_11, SQ_12, B_PAWN)));
}
