#include <gtest/gtest.h>

#include "../double_count_elimination.hpp"
#include "test_lib.hpp"

using komori::FindKnownAncestor;

namespace {
class FindKnownAncestorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    tt_.Resize(1);
    tt_.NewSearch();
  }

  void SetSearchPath(komori::Node& n, const std::vector<Move>& moves, komori::PnDn pn, komori::PnDn dn) {
    for (const auto& move : moves) {
      const auto query = tt_.BuildChildQuery(n, move);
      const auto parent = n.GetBoardKeyHandPair();
      const auto result = komori::SearchResult::MakeFirstVisit(pn, dn, komori::MateLen{334}, 1);
      query.SetResult(result, parent);
      n.DoMove(move);
    }

    komori::RollBack(n, moves);
  }

  komori::tt::TranspositionTable tt_;
};
}  // namespace

TEST_F(FindKnownAncestorTest, Empty) {
  TestNode n{"4k4/9/9/9/9/9/9/9/9 b G2r2b3g4s4n4l18p 1", true};
  auto opt = FindKnownAncestor(tt_, *n, make_move_drop(GOLD, SQ_52, BLACK));
  EXPECT_EQ(opt, std::nullopt);
}

TEST_F(FindKnownAncestorTest, NonDoubleCount) {
  TestNode n{"4k4/9/9/9/9/9/9/9/9 b G2r2b3g4s4n4l18p 1", true};
  SetSearchPath(*n, {make_move_drop(GOLD, SQ_52, BLACK)}, 100, 100);
  auto opt = FindKnownAncestor(tt_, *n, make_move_drop(GOLD, SQ_52, BLACK));
  EXPECT_EQ(opt, std::nullopt);
}

TEST_F(FindKnownAncestorTest, SimpleDoubleCountOrNode) {
  TestNode n{"9/9/9/7k1/7P1/9/9/9/9 w 2G2r2b2g4s4n4l17p 1", false};
  SetSearchPath(*n,
                {
                    make_move(SQ_24, SQ_23, W_KING),
                    make_move_drop(GOLD, SQ_14, BLACK),
                    make_move(SQ_23, SQ_22, W_KING),
                    make_move(SQ_14, SQ_23, B_GOLD),
                },
                100, 100);
  std::vector<Move> moves{
      make_move(SQ_24, SQ_23, W_KING),
      make_move_drop(GOLD, SQ_24, BLACK),
      make_move(SQ_23, SQ_22, W_KING),
  };
  RollForward(*n, moves);
  auto opt = FindKnownAncestor(tt_, *n, make_move(SQ_24, SQ_23, B_GOLD));

  n->UndoMove();
  n->UndoMove();
  ASSERT_NE(opt, std::nullopt);
  EXPECT_EQ(opt->branch_root_key_hand_pair.board_key, n->BoardKey());
  EXPECT_EQ(opt->branch_root_key_hand_pair.hand, n->OrHand());
  EXPECT_TRUE(opt->branch_root_is_or_node);
}

TEST_F(FindKnownAncestorTest, OrNode_ExceedPnDiffThreshold) {
  TestNode n{"9/9/9/7k1/7P1/9/9/9/9 w 2G2r2b2g4s4n4l17p 1", false};
  SetSearchPath(*n,
                {
                    make_move(SQ_24, SQ_23, W_KING),
                    make_move_drop(GOLD, SQ_14, BLACK),
                    make_move(SQ_23, SQ_22, W_KING),
                    make_move(SQ_14, SQ_23, B_GOLD),
                },
                100, 100);
  SetSearchPath(*n,
                {
                    make_move(SQ_24, SQ_23, W_KING),
                    make_move_drop(GOLD, SQ_14, BLACK),
                },
                1000, 100);
  std::vector<Move> moves{
      make_move(SQ_24, SQ_23, W_KING),
      make_move_drop(GOLD, SQ_24, BLACK),
      make_move(SQ_23, SQ_22, W_KING),
  };
  RollForward(*n, moves);
  auto opt = FindKnownAncestor(tt_, *n, make_move(SQ_24, SQ_23, B_GOLD));

  n->UndoMove();
  n->UndoMove();
  // pn が大きい分には問題ない
  ASSERT_NE(opt, std::nullopt);
  EXPECT_EQ(opt->branch_root_key_hand_pair.board_key, n->BoardKey());
  EXPECT_EQ(opt->branch_root_key_hand_pair.hand, n->OrHand());
  EXPECT_TRUE(opt->branch_root_is_or_node);
}

TEST_F(FindKnownAncestorTest, OrNode_ExceedDnDiffThreshold) {
  TestNode n{"9/9/9/7k1/7P1/9/9/9/9 w 2G2r2b2g4s4n4l17p 1", false};
  SetSearchPath(*n,
                {
                    make_move(SQ_24, SQ_23, W_KING),
                    make_move_drop(GOLD, SQ_14, BLACK),
                    make_move(SQ_23, SQ_22, W_KING),
                    make_move(SQ_14, SQ_23, B_GOLD),
                },
                100, 100);
  SetSearchPath(*n,
                {
                    make_move(SQ_24, SQ_23, W_KING),
                    make_move_drop(GOLD, SQ_14, BLACK),
                },
                100, 1000);
  std::vector<Move> moves{
      make_move(SQ_24, SQ_23, W_KING),
      make_move_drop(GOLD, SQ_24, BLACK),
      make_move(SQ_23, SQ_22, W_KING),
  };
  RollForward(*n, moves);
  auto opt = FindKnownAncestor(tt_, *n, make_move(SQ_24, SQ_23, B_GOLD));

  n->UndoMove();
  n->UndoMove();
  // dn が大きい場合は二重カウントではない
  ASSERT_EQ(opt, std::nullopt);
}

TEST_F(FindKnownAncestorTest, OrNode_ExceedPnDnDiffThreshold) {
  TestNode n{"9/9/9/7k1/7P1/9/9/9/9 w 2G2r2b2g4s4n4l17p 1", false};
  SetSearchPath(*n,
                {
                    make_move(SQ_24, SQ_23, W_KING),
                    make_move_drop(GOLD, SQ_14, BLACK),
                    make_move(SQ_23, SQ_22, W_KING),
                    make_move(SQ_14, SQ_23, B_GOLD),
                },
                100, 100);
  SetSearchPath(*n,
                {
                    make_move(SQ_24, SQ_23, W_KING),
                    make_move_drop(GOLD, SQ_14, BLACK),
                },
                1000, 1000);
  std::vector<Move> moves{
      make_move(SQ_24, SQ_23, W_KING),
      make_move_drop(GOLD, SQ_24, BLACK),
      make_move(SQ_23, SQ_22, W_KING),
  };
  RollForward(*n, moves);
  auto opt = FindKnownAncestor(tt_, *n, make_move(SQ_24, SQ_23, B_GOLD));

  n->UndoMove();
  n->UndoMove();
  // pnとdnがおおきい場合は二重カウントではない
  ASSERT_EQ(opt, std::nullopt);
}

TEST_F(FindKnownAncestorTest, SimpleDoubleCountAndNode) {
  TestNode n{"9/9/9/7k1/7P1/9/9/9/9 w 2G2r2b2g4s4n4l17p 1", false};
  SetSearchPath(*n,
                {
                    make_move(SQ_24, SQ_23, W_KING),
                    make_move_drop(GOLD, SQ_24, BLACK),
                    make_move(SQ_23, SQ_22, W_KING),
                    make_move_drop(GOLD, SQ_23, BLACK),
                    make_move(SQ_22, SQ_21, W_KING),
                },
                100, 100);
  std::vector<Move> moves{
      make_move(SQ_24, SQ_23, W_KING),
      make_move_drop(GOLD, SQ_24, BLACK),
      make_move(SQ_23, SQ_12, W_KING),
      make_move_drop(GOLD, SQ_23, BLACK),
  };
  RollForward(*n, moves);
  auto opt = FindKnownAncestor(tt_, *n, make_move(SQ_12, SQ_21, W_KING));

  n->UndoMove();
  n->UndoMove();
  ASSERT_NE(opt, std::nullopt);
  EXPECT_EQ(opt->branch_root_key_hand_pair.board_key, n->BoardKey());
  EXPECT_EQ(opt->branch_root_key_hand_pair.hand, n->OrHand());
  EXPECT_FALSE(opt->branch_root_is_or_node);
}
