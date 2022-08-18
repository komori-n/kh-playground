#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "../../../thread.h"
#include "../cc.hpp"
#include "../initial_estimation.hpp"

using komori::kInfinitePnDn;
using komori::MateLen;

TEST(IndexTable, Push) {
  komori::detail::IndexTable idx;

  EXPECT_EQ(idx.Push(2), 0);
  EXPECT_EQ(idx.Push(6), 1);
  EXPECT_EQ(idx.Push(4), 2);
}

TEST(IndexTable, Pop) {
  komori::detail::IndexTable idx;

  idx.Push(2);
  idx.Push(6);
  idx.Push(4);

  EXPECT_EQ(idx.size(), 3);
  idx.Pop();
  EXPECT_EQ(idx.size(), 2);
}

TEST(IndexTable, operator) {
  komori::detail::IndexTable idx;

  idx.Push(2);
  idx.Push(6);
  idx.Push(4);

  EXPECT_EQ(idx[0], 2);
  EXPECT_EQ(idx[1], 6);
  EXPECT_EQ(idx[2], 4);
}

TEST(IndexTable, iterators) {
  komori::detail::IndexTable idx;

  idx.Push(2);
  idx.Push(6);
  idx.Push(4);
  EXPECT_EQ(*idx.begin(), 2);
  EXPECT_EQ(idx.begin() + 3, idx.end());
  EXPECT_EQ(*idx.begin(), idx.front());
}

TEST(IndexTable, size) {
  komori::detail::IndexTable idx;
  EXPECT_TRUE(idx.empty());
  EXPECT_EQ(idx.size(), 0);

  idx.Push(2);
  idx.Push(6);
  idx.Push(4);
  EXPECT_FALSE(idx.empty());
  EXPECT_EQ(idx.size(), 3);
}

namespace {
class ChildrenCacheTest : public ::testing::Test {
 protected:
  void SetUp() override { tt_.Resize(1); }

  void Init(const std::string& sfen, bool or_node) {
    p_.set(sfen, &si_, Threads[0]);
    n_ = std::make_unique<komori::Node>(p_, or_node);
  }

  StateInfo si_;
  Position p_;
  std::unique_ptr<komori::Node> n_;
  komori::tt::TranspositionTable tt_;
};
}  // namespace

TEST_F(ChildrenCacheTest, NoLegalMoves) {
  Init("4k4/9/9/9/9/9/9/9/9 b 2r2b4g4s4n4l18p 1", true);
  komori::ChildrenCache cc{tt_, *n_, MateLen::Make(33, 4), true};

  const auto res = cc.CurrentResult(*n_);
  EXPECT_EQ(res.pn, kInfinitePnDn);
  EXPECT_EQ(res.dn, 0);
}

TEST_F(ChildrenCacheTest, ObviousNomate) {
  Init("lnsgkgsnl/1r2G2b1/ppppppppp/9/9/9/PPPPPPPPP/9/LNS1KGSNL w rb 1", false);
  komori::ChildrenCache cc{tt_, *n_, MateLen::Make(33, 4), true};

  const auto res = cc.CurrentResult(*n_);
  EXPECT_EQ(res.pn, kInfinitePnDn);
  EXPECT_EQ(res.dn, 0);
}

TEST_F(ChildrenCacheTest, ObviousMate) {
  Init("7kG/7p1/9/7N1/9/9/9/9/9 w G2r2b2g4s3n4l17p 1", false);
  komori::ChildrenCache cc{tt_, *n_, MateLen::Make(33, 4), true};

  const auto res = cc.CurrentResult(*n_);
  EXPECT_EQ(res.pn, 0);
  EXPECT_EQ(res.dn, kInfinitePnDn);
}

TEST_F(ChildrenCacheTest, DelayExpansion) {
  Init("6R1k/7lp/9/9/9/9/9/9/9 w r2b4g4s4n3l17p 1", false);
  komori::ChildrenCache cc{tt_, *n_, MateLen::Make(33, 4), true};

  const auto [pn, dn] = komori::InitialPnDn(*n_, make_move_drop(ROOK, SQ_21, BLACK));
  const auto res = cc.CurrentResult(*n_);
  EXPECT_EQ(res.pn, pn + 1);
  EXPECT_EQ(res.dn, dn);
}

TEST_F(ChildrenCacheTest, ObviousRepetition) {
  Init("7lk/7p1/9/8L/8p/9/9/9/9 w 2r2b4g4s4n2l16p 1", false);
  n_->DoMove(make_move_drop(LANCE, SQ_13, WHITE));
  n_->DoMove(make_move(SQ_14, SQ_13, B_LANCE));
  n_->DoMove(make_move_drop(GOLD, SQ_12, WHITE));
  n_->DoMove(make_move(SQ_13, SQ_12, B_LANCE));
  n_->DoMove(make_move(SQ_11, SQ_12, W_KING));
  n_->DoMove(make_move_drop(GOLD, SQ_11, BLACK));
  n_->DoMove(make_move(SQ_12, SQ_11, W_KING));
  n_->DoMove(make_move_drop(LANCE, SQ_15, BLACK));
  n_->DoMove(make_move_drop(LANCE, SQ_13, WHITE));
  n_->DoMove(make_move(SQ_15, SQ_13, B_LANCE));
  n_->DoMove(make_move_drop(GOLD, SQ_12, WHITE));
  n_->DoMove(make_move(SQ_13, SQ_12, B_LANCE));
  n_->DoMove(make_move(SQ_11, SQ_12, W_KING));
  n_->DoMove(make_move_drop(GOLD, SQ_11, BLACK));
  n_->DoMove(make_move(SQ_12, SQ_11, W_KING));
  komori::ChildrenCache cc{tt_, *n_, MateLen::Make(33, 4), true};

  const auto res = cc.CurrentResult(*n_);
  EXPECT_EQ(res.pn, kInfinitePnDn);
  EXPECT_EQ(res.dn, 0);
}

TEST_F(ChildrenCacheTest, InitialSort) {
  Init("7k1/6pP1/7LP/8L/9/9/9/9/9 w 2r2b4g4s4n2l15p 1", false);
  komori::ChildrenCache cc{tt_, *n_, MateLen::Make(33, 4), true};

  const auto [pn, dn] = komori::InitialPnDn(*n_, make_move(SQ_21, SQ_31, W_KING));
  const auto res = cc.CurrentResult(*n_);
  EXPECT_EQ(res.pn, pn);
  EXPECT_EQ(res.dn, dn);
}

TEST_F(ChildrenCacheTest, MaxChildren) {
  Init("6pkp/7PR/7L1/9/9/9/9/9/9 w r2b4g4s4n3l15p 1", false);
  komori::ChildrenCache cc{tt_, *n_, MateLen::Make(33, 4), true, komori::BitSet64{}};

  const auto [pn1, dn1] = komori::InitialPnDn(*n_, make_move(SQ_21, SQ_12, W_KING));
  const auto [pn2, dn2] = komori::InitialPnDn(*n_, make_move(SQ_21, SQ_32, W_KING));
  const auto res = cc.CurrentResult(*n_);
  EXPECT_EQ(res.pn, std::max(pn1, pn2));
  EXPECT_EQ(res.dn, std::min(dn1, dn2));
}
