#include <gtest/gtest.h>

#include <sstream>

#include "../mate_len.hpp"

using komori::MateLen;
using komori::MateLen16;

namespace {
template <typename MateLen>
class MateLenTest : public ::testing::Test {};

using TestTypes = ::testing::Types<MateLen16, MateLen>;
}  // namespace

TYPED_TEST_SUITE(MateLenTest, TestTypes);

TYPED_TEST(MateLenTest, OperatorEqual) {
  const TypeParam m1{334};
  const TypeParam m2{264};

  EXPECT_TRUE(m1 == m1);
  EXPECT_FALSE(m1 == m2);
}

TYPED_TEST(MateLenTest, OperatorLess) {
  const TypeParam m1{264};
  const TypeParam m2{334};
  EXPECT_TRUE(m1 < m2);
  EXPECT_FALSE(m2 < m1);
  EXPECT_FALSE(m1 < m1);
}

TYPED_TEST(MateLenTest, OperatorPlus) {
  const TypeParam m1{264};
  EXPECT_EQ(m1 + 70, TypeParam{334});
  EXPECT_EQ(70 + m1, TypeParam{334});
}

TYPED_TEST(MateLenTest, OperatorMinus) {
  const TypeParam m1{334};
  EXPECT_EQ(m1 - 70, TypeParam{264});

  const TypeParam m2{TypeParam::DepthMax()};
  EXPECT_EQ(m2 - 30, m2);
}

TYPED_TEST(MateLenTest, OutputOperator) {
  const TypeParam m1{334};
  const TypeParam m2{TypeParam::Min()};

  std::ostringstream oss1;
  oss1 << m1;
  EXPECT_EQ(oss1.str(), "334");

  std::ostringstream oss2;
  oss2 << m2;
  EXPECT_EQ(oss2.str(), "-10");
}

TYPED_TEST(MateLenTest, ConvertOtherType) {
  using OtherType = std::conditional_t<std::is_same_v<TypeParam, MateLen>, MateLen16, MateLen>;

  const TypeParam m1{334};
  const OtherType m2{m1};

  EXPECT_EQ(m2.Len(), 334);
}

TEST(MateLen, Constants) {
  EXPECT_EQ(MateLen::Min().Len(), 0);
  EXPECT_EQ(MateLen::DepthMax().Len(), komori::kDepthMax);
  EXPECT_EQ((MateLen::Min() + 11).Len(), 1);
  EXPECT_EQ(MateLen::Max().Len(), komori::kDepthMax + 1);

  EXPECT_EQ(MateLen16::Min().Len(), 0);
  EXPECT_EQ(MateLen16::DepthMax().Len(), komori::kDepthMax);
  EXPECT_EQ((MateLen16::Min() + 11).Len(), 1);
  EXPECT_EQ(MateLen16::Max().Len(), komori::kDepthMax + 1);
}
