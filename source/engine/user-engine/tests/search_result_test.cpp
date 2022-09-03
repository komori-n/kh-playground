#include <gtest/gtest.h>

#include "../search_result.hpp"
#include "test_lib.hpp"

using komori::FinalData;
using komori::MateLen;
using komori::SearchResult;
using komori::SearchResultComparer;
using komori::UnknownData;

TEST(SearchResultTest, ConstructUnknown) {
  const UnknownData unknown_data{true, 334, MakeHand<PAWN, LANCE>(), 445};
  const auto result =
      SearchResult::MakeUnknown(33, 4, MakeHand<PAWN, PAWN, KNIGHT>(), MateLen::Make(26, 4), 10, unknown_data);

  EXPECT_EQ(result.Pn(), 33);
  EXPECT_EQ(result.Dn(), 4);
  EXPECT_EQ(result.GetHand(), (MakeHand<PAWN, PAWN, KNIGHT>()));
  EXPECT_EQ(result.Len(), MateLen::Make(26, 4));
  EXPECT_EQ(result.Amount(), 10);
  EXPECT_FALSE(result.IsFinal());
  EXPECT_TRUE(result.GetUnknownData().is_first_visit);
  EXPECT_EQ(result.GetUnknownData().parent_board_key, 334);
  EXPECT_EQ(result.GetUnknownData().parent_hand, (MakeHand<PAWN, LANCE>()));
  EXPECT_EQ(result.GetUnknownData().secret, 445);
}

TEST(SearchResultTest, MakeProven) {
  const auto result = SearchResult::MakeFinal<true>(MakeHand<PAWN, SILVER>(), MateLen::Make(33, 4), 20);

  EXPECT_EQ(result.Pn(), 0);
  EXPECT_EQ(result.Dn(), komori::kInfinitePnDn);
  EXPECT_EQ(result.GetHand(), (MakeHand<PAWN, SILVER>()));
  EXPECT_EQ(result.Len(), (MateLen::Make(33, 4)));
  EXPECT_EQ(result.Amount(), 20);
  EXPECT_TRUE(result.IsFinal());
  EXPECT_FALSE(result.GetFinalData().is_repetition);
}

TEST(SearchResultTest, MakeDisproven) {
  const auto result = SearchResult::MakeFinal<false>(MakeHand<GOLD, GOLD>(), MateLen::Make(33, 4), 30);

  EXPECT_EQ(result.Pn(), komori::kInfinitePnDn);
  EXPECT_EQ(result.Dn(), 0);
  EXPECT_EQ(result.GetHand(), (MakeHand<GOLD, GOLD>()));
  EXPECT_EQ(result.Len(), (MateLen::Make(33, 4)));
  EXPECT_EQ(result.Amount(), 30);
  EXPECT_TRUE(result.IsFinal());
  EXPECT_FALSE(result.GetFinalData().is_repetition);
}

TEST(SearchResultTest, MakeRepetition) {
  const auto result = SearchResult::MakeFinal<false, true>(MakeHand<ROOK, BISHOP>(), MateLen::Make(33, 4), 40);

  EXPECT_EQ(result.Pn(), komori::kInfinitePnDn);
  EXPECT_EQ(result.Dn(), 0);
  EXPECT_EQ(result.GetHand(), (MakeHand<ROOK, BISHOP>()));
  EXPECT_EQ(result.Len(), (MateLen::Make(33, 4)));
  EXPECT_EQ(result.Amount(), 40);
  EXPECT_TRUE(result.IsFinal());
  EXPECT_TRUE(result.GetFinalData().is_repetition);
}

TEST(SearchResultTest, InitUnknown) {
  SearchResult result{};
  const UnknownData unknown_data{true, 334, MakeHand<PAWN, LANCE>(), 445};
  result.InitUnknown(33, 4, MakeHand<PAWN, PAWN, KNIGHT>(), MateLen::Make(26, 4), 10, unknown_data);

  EXPECT_EQ(result.Pn(), 33);
  EXPECT_EQ(result.Dn(), 4);
  EXPECT_EQ(result.GetHand(), (MakeHand<PAWN, PAWN, KNIGHT>()));
  EXPECT_EQ(result.Len(), MateLen::Make(26, 4));
  EXPECT_EQ(result.Amount(), 10);
  EXPECT_FALSE(result.IsFinal());
  EXPECT_TRUE(result.GetUnknownData().is_first_visit);
  EXPECT_EQ(result.GetUnknownData().parent_board_key, 334);
  EXPECT_EQ(result.GetUnknownData().parent_hand, (MakeHand<PAWN, LANCE>()));
  EXPECT_EQ(result.GetUnknownData().secret, 445);
}

TEST(SearchResultTest, InitProven) {
  SearchResult result{};
  result.InitFinal<true>(MakeHand<PAWN, SILVER>(), MateLen::Make(33, 4), 20);

  EXPECT_EQ(result.Pn(), 0);
  EXPECT_EQ(result.Dn(), komori::kInfinitePnDn);
  EXPECT_EQ(result.GetHand(), (MakeHand<PAWN, SILVER>()));
  EXPECT_EQ(result.Len(), (MateLen::Make(33, 4)));
  EXPECT_EQ(result.Amount(), 20);
  EXPECT_FALSE(result.GetFinalData().is_repetition);
}

TEST(SearchResultTest, InitDisproven) {
  SearchResult result{};
  result.InitFinal<false>(MakeHand<GOLD, GOLD>(), MateLen::Make(33, 4), 30);

  EXPECT_EQ(result.Pn(), komori::kInfinitePnDn);
  EXPECT_EQ(result.Dn(), 0);
  EXPECT_EQ(result.GetHand(), (MakeHand<GOLD, GOLD>()));
  EXPECT_EQ(result.Len(), (MateLen::Make(33, 4)));
  EXPECT_EQ(result.Amount(), 30);
  EXPECT_FALSE(result.GetFinalData().is_repetition);
}

TEST(SearchResultTest, InitRepetition) {
  SearchResult result{};
  result.InitFinal<false, true>(MakeHand<ROOK, BISHOP>(), MateLen::Make(33, 4), 40);

  EXPECT_EQ(result.Pn(), komori::kInfinitePnDn);
  EXPECT_EQ(result.Dn(), 0);
  EXPECT_EQ(result.GetHand(), (MakeHand<ROOK, BISHOP>()));
  EXPECT_EQ(result.Len(), (MateLen::Make(33, 4)));
  EXPECT_EQ(result.Amount(), 40);
  EXPECT_TRUE(result.GetFinalData().is_repetition);
}

TEST(SearchResultTest, Phi) {
  const auto result = SearchResult::MakeFinal<true>(HAND_ZERO, MateLen::Make(33, 4), 20);

  EXPECT_EQ(result.Phi(true), 0);
  EXPECT_EQ(result.Phi(false), komori::kInfinitePnDn);
}

TEST(SearchResultTest, Delta) {
  const auto result = SearchResult::MakeFinal<true>(HAND_ZERO, MateLen::Make(33, 4), 20);

  EXPECT_EQ(result.Delta(true), komori::kInfinitePnDn);
  EXPECT_EQ(result.Delta(false), 0);
}

TEST(SearchResultComparerTest, OrNode) {
  SearchResultComparer sr_comparer{true};

  const UnknownData unknown_data{true, 334, MakeHand<PAWN, LANCE>(), 445};
  const auto u1 =
      SearchResult::MakeUnknown(33, 4, MakeHand<PAWN, PAWN, KNIGHT>(), MateLen::Make(26, 4), 10, unknown_data);
  const auto u2 =
      SearchResult::MakeUnknown(26, 4, MakeHand<PAWN, PAWN, KNIGHT>(), MateLen::Make(26, 4), 10, unknown_data);
  const auto u3 =
      SearchResult::MakeUnknown(33, 5, MakeHand<PAWN, PAWN, KNIGHT>(), MateLen::Make(26, 4), 10, unknown_data);
  const auto f1 = SearchResult::MakeFinal<false>(MakeHand<PAWN, SILVER>(), MateLen::Make(33, 4), 20);
  const auto f2 = SearchResult::MakeFinal<false, true>(MakeHand<PAWN, SILVER>(), MateLen::Make(33, 4), 20);

  EXPECT_EQ(sr_comparer(u2, u1), SearchResultComparer::Ordering::kLess);
  EXPECT_EQ(sr_comparer(u1, u2), SearchResultComparer::Ordering::kGreater);
  EXPECT_EQ(sr_comparer(u1, u3), SearchResultComparer::Ordering::kLess);
  EXPECT_EQ(sr_comparer(u3, u1), SearchResultComparer::Ordering::kGreater);

  EXPECT_EQ(sr_comparer(f1, f2), SearchResultComparer::Ordering::kLess);
  EXPECT_EQ(sr_comparer(f2, f1), SearchResultComparer::Ordering::kGreater);

  EXPECT_EQ(sr_comparer(u1, u1), SearchResultComparer::Ordering::kEquivalent);
  EXPECT_EQ(sr_comparer(f1, f1), SearchResultComparer::Ordering::kEquivalent);
}