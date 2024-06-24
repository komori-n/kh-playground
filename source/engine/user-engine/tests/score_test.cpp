#include <gtest/gtest.h>

#include "../score.hpp"

using komori::kNullKey;
using komori::MateLen;
using komori::Score;
using komori::ScoreCalculationMethod;
using komori::ScoreMaker;
using komori::SearchResult;
using komori::UnknownData;

TEST(ScoreTest, MakeProven) {
  const ScoreMaker maker{ScoreCalculationMethod::kNone};
  const auto s1 = maker.MakeProven(334, true);
  EXPECT_EQ(s1.ToString(), "mate 334");

  const auto s2 = maker.MakeProven(334, false);
  EXPECT_EQ(s2.ToString(), "mate -334");
}

TEST(ScoreTest, MakeUnknown_None) {
  const ScoreMaker maker{ScoreCalculationMethod::kNone};
  const SearchResult result = SearchResult::MakeFirstVisit(33, 4, MateLen::DepthMax(), 264);

  const auto s1 = maker.Make(result, true);
  EXPECT_EQ(s1.ToString(), "cp 0");

  const auto s2 = maker.Make(result, false);
  EXPECT_EQ(s2.ToString(), "cp 0");
}

TEST(ScoreTest, MakeUnknown_Dn) {
  const ScoreMaker maker{ScoreCalculationMethod::kDn};
  const SearchResult result = SearchResult::MakeFirstVisit(33, 4, MateLen::DepthMax(), 264);

  const auto s1 = maker.Make(result, true);
  EXPECT_EQ(s1.ToString(), "cp 4");

  const auto s2 = maker.Make(result, false);
  EXPECT_EQ(s2.ToString(), "cp -4");
}

TEST(ScoreTest, MakeUnknown_MinusPn) {
  const ScoreMaker maker{ScoreCalculationMethod::kMinusPn};
  const SearchResult result = SearchResult::MakeFirstVisit(33, 4, MateLen::DepthMax(), 264);

  const auto s1 = maker.Make(result, true);
  EXPECT_EQ(s1.ToString(), "cp -33");

  const auto s2 = maker.Make(result, false);
  EXPECT_EQ(s2.ToString(), "cp 33");
}

TEST(ScoreTest, MakeUnknown_Ponanza) {
  const ScoreMaker maker{ScoreCalculationMethod::kPonanza};
  const SearchResult result = SearchResult::MakeFirstVisit(33, 4, MateLen::DepthMax(), 264);

  const auto s1 = maker.Make(result, true);
  EXPECT_EQ(s1.ToString(), "cp -1266");

  const auto s2 = maker.Make(result, false);
  EXPECT_EQ(s2.ToString(), "cp 1266");
}

TEST(ScoreTest, MakeUnknown_Proven) {
  const ScoreMaker no_maker{ScoreCalculationMethod::kNone};
  const ScoreMaker dn_maker{ScoreCalculationMethod::kDn};
  const ScoreMaker pn_maker{ScoreCalculationMethod::kMinusPn};
  const ScoreMaker pona_maker{ScoreCalculationMethod::kPonanza};
  const SearchResult result = SearchResult::MakeFinal<true>(HAND_ZERO, MateLen{264}, 1);

  const auto s1 = no_maker.Make(result, true);
  EXPECT_EQ(s1.ToString(), "mate 264");
  EXPECT_EQ(s1, dn_maker.Make(result, true));
  EXPECT_EQ(s1, pn_maker.Make(result, true));
  EXPECT_EQ(s1, pona_maker.Make(result, true));

  const auto s2 = no_maker.Make(result, false);
  EXPECT_EQ(s2.ToString(), "mate -264");
  EXPECT_EQ(s2, dn_maker.Make(result, false));
  EXPECT_EQ(s2, pn_maker.Make(result, false));
  EXPECT_EQ(s2, pona_maker.Make(result, false));
}

TEST(ScoreTest, MakeUnknown_Disproven) {
  const ScoreMaker no_maker{ScoreCalculationMethod::kNone};
  const ScoreMaker dn_maker{ScoreCalculationMethod::kDn};
  const ScoreMaker pn_maker{ScoreCalculationMethod::kMinusPn};
  const ScoreMaker pona_maker{ScoreCalculationMethod::kPonanza};
  const SearchResult result = SearchResult::MakeFinal<false>(HAND_ZERO, MateLen{264}, 1);

  const auto s1 = no_maker.Make(result, true);
  EXPECT_EQ(s1.ToString(), "mate -264");
  EXPECT_EQ(s1, dn_maker.Make(result, true));
  EXPECT_EQ(s1, pn_maker.Make(result, true));
  EXPECT_EQ(s1, pona_maker.Make(result, true));

  const auto s2 = no_maker.Make(result, false);
  EXPECT_EQ(s2.ToString(), "mate 264");
  EXPECT_EQ(s2, dn_maker.Make(result, false));
  EXPECT_EQ(s2, pn_maker.Make(result, false));
  EXPECT_EQ(s2, pona_maker.Make(result, false));
}

TEST(ScoreTest, MakeUnknown_Repetition) {
  const ScoreMaker no_maker{ScoreCalculationMethod::kNone};
  const ScoreMaker dn_maker{ScoreCalculationMethod::kDn};
  const ScoreMaker pn_maker{ScoreCalculationMethod::kMinusPn};
  const ScoreMaker pona_maker{ScoreCalculationMethod::kPonanza};
  const SearchResult result = SearchResult::MakeRepetition(HAND_ZERO, MateLen{264}, 1, 334);

  const auto s1 = no_maker.Make(result, true);
  EXPECT_EQ(s1.ToString(), "mate -264");
  EXPECT_EQ(s1, dn_maker.Make(result, true));
  EXPECT_EQ(s1, pn_maker.Make(result, true));
  EXPECT_EQ(s1, pona_maker.Make(result, true));

  const auto s2 = no_maker.Make(result, false);
  EXPECT_EQ(s2.ToString(), "mate 264");
  EXPECT_EQ(s2, dn_maker.Make(result, false));
  EXPECT_EQ(s2, pn_maker.Make(result, false));
  EXPECT_EQ(s2, pona_maker.Make(result, false));
}

TEST(ScoreTest, IsFinal) {
  const ScoreMaker maker{ScoreCalculationMethod::kNone};
  const SearchResult r1 = SearchResult::MakeFirstVisit(33, 4, MateLen::DepthMax(), 264);
  const auto s1 = maker.Make(r1, true);
  EXPECT_FALSE(s1.IsFinal());

  const SearchResult r2 = SearchResult::MakeFinal<true>(HAND_ZERO, MateLen{264}, 1);
  const auto s2 = maker.Make(r2, true);
  EXPECT_TRUE(s2.IsFinal());

  const SearchResult r3 = SearchResult::MakeFinal<false>(HAND_ZERO, MateLen{264}, 1);
  const auto s3 = maker.Make(r3, true);
  EXPECT_TRUE(s3.IsFinal());
}

TEST(ScoreTest, AddOneIfFinal) {
  const ScoreMaker maker{ScoreCalculationMethod::kDn};
  const SearchResult r1 = SearchResult::MakeFirstVisit(33, 4, MateLen::DepthMax(), 264);
  auto s1 = maker.Make(r1, true);
  s1.AddOneIfFinal();
  EXPECT_EQ(s1.ToString(), "cp 4");

  const SearchResult r2 = SearchResult::MakeFinal<true>(HAND_ZERO, MateLen{263}, 1);
  auto s2 = maker.Make(r2, true);
  s2.AddOneIfFinal();
  EXPECT_EQ(s2.ToString(), "mate 264");

  const SearchResult r3 = SearchResult::MakeFinal<false>(HAND_ZERO, MateLen{333}, 1);
  auto s3 = maker.Make(r3, true);
  s3.AddOneIfFinal();
  EXPECT_EQ(s3.ToString(), "mate -334");
}
