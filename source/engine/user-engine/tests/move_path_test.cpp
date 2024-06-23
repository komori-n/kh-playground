#include <gtest/gtest.h>

#include "../move_path.hpp"

using komori::MovePath;

TEST(MovePathTest, All) {
  MovePath path;

  path.AddMove(make_move(SQ_33, SQ_14, B_PAWN), 0);
  path.AddMove(make_move_drop(PAWN, SQ_33, WHITE), 1);

  EXPECT_EQ(path.ToString(), "3c1d P*3c");
}
