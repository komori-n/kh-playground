#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "../typedefs.hpp"
#include "test_lib.hpp"

using komori::ClampPnDn;
using komori::Delta;
using komori::kInfinitePnDn;
using komori::OrdinalNumber;
using komori::Phi;
using komori::SaturatedAdd;
using komori::SaturatedMultiply;
using komori::SaturatedSubtract;
using komori::ToString;

namespace {
template <typename T>
class SaturationTest : public ::testing::Test {};
using SaturationTestTypes = ::testing::Types<std::uint8_t,
                                             std::uint16_t,
                                             std::uint32_t,
                                             std::uint64_t,
                                             std::int8_t,
                                             std::int16_t,
                                             std::int32_t,
                                             std::int64_t>;
}  // namespace

TYPED_TEST_SUITE(SaturationTest, SaturationTestTypes);

TYPED_TEST(SaturationTest, SaturatedAdd) {
  constexpr TypeParam kMin = std::numeric_limits<TypeParam>::min();
  constexpr TypeParam kMax = std::numeric_limits<TypeParam>::max();

  EXPECT_EQ(SaturatedAdd<TypeParam>(33, 4), 33 + 4);
  EXPECT_EQ(SaturatedAdd<TypeParam>(kMax, 1), kMax);

  if constexpr (std::is_signed_v<TypeParam>) {
    EXPECT_EQ(SaturatedAdd<TypeParam>(-33, -4), -33 - 4);
    EXPECT_EQ(SaturatedAdd<TypeParam>(kMin, kMax), kMin + kMax);
    EXPECT_EQ(SaturatedAdd<TypeParam>(kMax, kMin), kMax + kMin);
    EXPECT_EQ(SaturatedAdd<TypeParam>(kMin, -1), kMin);
    EXPECT_EQ(SaturatedAdd<TypeParam>(kMin, 1), kMin + 1);
  }
}

TYPED_TEST(SaturationTest, SaturatedSubtract) {
  constexpr TypeParam kMin = std::numeric_limits<TypeParam>::min();
  constexpr TypeParam kMax = std::numeric_limits<TypeParam>::max();

  EXPECT_EQ(SaturatedSubtract<TypeParam>(33, 4), 33 - 4);
  EXPECT_EQ(SaturatedSubtract<TypeParam>(kMin, 1), kMin);

  if constexpr (std::is_signed_v<TypeParam>) {
    EXPECT_EQ(SaturatedSubtract<TypeParam>(-33, -4), -33 + 4);
    EXPECT_EQ(SaturatedSubtract<TypeParam>(kMin, kMax), kMin);
    EXPECT_EQ(SaturatedSubtract<TypeParam>(kMax, kMin), kMax);
    EXPECT_EQ(SaturatedSubtract<TypeParam>(kMin, 1), kMin);
    EXPECT_EQ(SaturatedSubtract<TypeParam>(kMin, -1), kMin + 1);
  }
}

TYPED_TEST(SaturationTest, SaturatedMultiply) {
  constexpr TypeParam kMin = std::numeric_limits<TypeParam>::min();
  constexpr TypeParam kMax = std::numeric_limits<TypeParam>::max();

  // 調子に乗って (33, 4) を渡すと int8_t のときにオーバーフローするので注意（一敗）
  EXPECT_EQ(SaturatedMultiply<TypeParam>(3, 4), 3 * 4);
  EXPECT_EQ(SaturatedMultiply<TypeParam>(0, 4), 0);
  EXPECT_EQ(SaturatedMultiply<TypeParam>(kMax / 2, 3), kMax);

  if constexpr (std::is_signed_v<TypeParam>) {
    EXPECT_EQ(SaturatedMultiply<TypeParam>(-3, -4), (-3) * (-4));
    EXPECT_EQ(SaturatedMultiply<TypeParam>(3, -4), 3 * (-4));
    EXPECT_EQ(SaturatedMultiply<TypeParam>(-3, 4), (-3) * 4);
    EXPECT_EQ(SaturatedMultiply<TypeParam>(kMin / 2, 3), kMin);
    EXPECT_EQ(SaturatedMultiply<TypeParam>(3, kMin / 2), kMin);
    EXPECT_EQ(SaturatedMultiply<TypeParam>(kMin / 2, -3), kMax);
  }
}

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
