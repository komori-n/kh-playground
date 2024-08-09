#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "../typedefs.hpp"
#include "test_lib.hpp"

using komori::ClampPnDn;
using komori::Delta;
using komori::kInfinitePnDn;
using komori::OrdinalNumber;
using komori::Phi;
using komori::ToString;

TEST(PnDnTest, ClampTest) {
  EXPECT_EQ(ClampPnDn(10, 5, 20), 10);
  EXPECT_EQ(ClampPnDn(4, 5, 20), 5);
  EXPECT_EQ(ClampPnDn(334, 5, 20), 20);
}

TEST(PnDnTest, PhiTest) {
  EXPECT_EQ(Phi(33, 4, true), 33);
  EXPECT_EQ(Phi(33, 4, false), 4);
}

TEST(PnDnTest, DeltaTest) {
  EXPECT_EQ(Delta(33, 4, true), 4);
  EXPECT_EQ(Delta(33, 4, false), 33);
}

TEST(PnDnTest, ToString) {
  EXPECT_EQ(ToString(kInfinitePnDn), "inf");
  EXPECT_EQ(ToString(kInfinitePnDn + 1), "invalid");
  EXPECT_EQ(ToString(334), "334");
}

TEST(MoveRange, ToString) {
  EXPECT_EQ(ToString(std::vector<Move>{}), "");
  EXPECT_EQ(ToString(std::vector<Move>{make_move(SQ_33, SQ_34, W_PAWN), make_move_promote(SQ_11, SQ_99, B_BISHOP),
                                       make_move_drop(ROOK, SQ_44, WHITE)}),
            "3c3d 1a9i+ R*4d");
}

TEST(OrdinalNumberTest, All) {
  EXPECT_EQ(OrdinalNumber(1), "1st");
  EXPECT_EQ(OrdinalNumber(2), "2nd");
  EXPECT_EQ(OrdinalNumber(3), "3rd");
  EXPECT_EQ(OrdinalNumber(4), "4th");
  EXPECT_EQ(OrdinalNumber(5), "5th");
  EXPECT_EQ(OrdinalNumber(10), "10th");
  EXPECT_EQ(OrdinalNumber(11), "11th");
  EXPECT_EQ(OrdinalNumber(12), "12th");
  EXPECT_EQ(OrdinalNumber(13), "13th");
  EXPECT_EQ(OrdinalNumber(14), "14th");
  EXPECT_EQ(OrdinalNumber(20), "20th");
  EXPECT_EQ(OrdinalNumber(21), "21st");
  EXPECT_EQ(OrdinalNumber(22), "22nd");
  EXPECT_EQ(OrdinalNumber(23), "23rd");
  EXPECT_EQ(OrdinalNumber(24), "24th");
  EXPECT_EQ(OrdinalNumber(100), "100th");
  EXPECT_EQ(OrdinalNumber(101), "101st");
  EXPECT_EQ(OrdinalNumber(102), "102nd");
  EXPECT_EQ(OrdinalNumber(103), "103rd");
  EXPECT_EQ(OrdinalNumber(104), "104th");
  EXPECT_EQ(OrdinalNumber(111), "111th");
  EXPECT_EQ(OrdinalNumber(112), "112th");
  EXPECT_EQ(OrdinalNumber(113), "113th");
  EXPECT_EQ(OrdinalNumber(120), "120th");
  EXPECT_EQ(OrdinalNumber(121), "121st");
  EXPECT_EQ(OrdinalNumber(122), "122nd");
  EXPECT_EQ(OrdinalNumber(123), "123rd");
  EXPECT_EQ(OrdinalNumber(124), "124th");
}

TEST(DeferTest, All) {
  using komori::Defer;
  testing::MockFunction<void()> mock1;
  testing::MockFunction<void()> mock2;
  testing::MockFunction<void()> mock3;

  {
    testing::InSequence seq;
    EXPECT_CALL(mock3, Call());
    EXPECT_CALL(mock2, Call());
    EXPECT_CALL(mock1, Call());
  }

  {
    Defer d1{mock1.AsStdFunction()};
    Defer d2{mock2.AsStdFunction()};
    Defer d3{mock3.AsStdFunction()};
  }
}

TEST(DoesHaveMatePossibilityTest, NoOurPiece) {
  TestNode node{"4k4/9/9/9/9/9/9/9/9 b 4G4S18P2r2b4n4l 1", true};
  EXPECT_FALSE(komori::DoesHaveMatePossibility(node->Pos()));

  // 飛び道具がある
  TestNode node2{"4k4/9/9/9/9/9/9/9/9 b 4G4S18PRr2b4n4l 1", true};
  EXPECT_TRUE(komori::DoesHaveMatePossibility(node2->Pos()));
  TestNode node3{"4k4/9/9/9/9/9/9/9/9 b 4G4S18PB2rb4n4l 1", true};
  EXPECT_TRUE(komori::DoesHaveMatePossibility(node3->Pos()));
  TestNode node4{"4k4/9/9/9/9/9/9/9/9 b 4G4S18PN2rb3n4l 1", true};
  EXPECT_TRUE(komori::DoesHaveMatePossibility(node4->Pos()));
  TestNode node5{"4k4/9/9/9/9/9/9/9/9 b 4G4S18PL2rb4n3l 1", true};
  EXPECT_TRUE(komori::DoesHaveMatePossibility(node5->Pos()));

  // 盤上に自分の駒がある
  TestNode node6{"4k4/9/9/9/9/9/9/9/8P b 4G4S17P2r2b4n4l 1", true};
  EXPECT_TRUE(komori::DoesHaveMatePossibility(node6->Pos()));
}

TEST(DoesHaveMatePossibilityTest, BoardPiece) {
  TestNode node{"4k4/9/4P4/PPPP1PPPP/9/9/9/9/9 b 2r2b4g4s4n4l9p 1", true};
  EXPECT_TRUE(komori::DoesHaveMatePossibility(node->Pos()));

  TestNode node2{"4k4/9/9/PPPPPPPPP/9/9/9/9/9 b 2r2b4g4s4n4l9p 1", true};
  EXPECT_FALSE(komori::DoesHaveMatePossibility(node2->Pos()));
}

TEST(DoesHaveMatePossibilityTest, DoublePawnCheck) {
  TestNode node{"4k4/9/9/9/9/9/9/9/8P b P2r2b4g4s4n4l16p 1", true};
  EXPECT_TRUE(komori::DoesHaveMatePossibility(node->Pos()));

  TestNode node2{"4k4/9/9/9/9/9/9/9/4P3P b P2r2b4g4s4n4l15p 1", true};
  EXPECT_FALSE(komori::DoesHaveMatePossibility(node2->Pos()));
}
