#include "gtest/gtest.h"
#include "wwivbasic/value.h"

using namespace wwivbasic;
using Type = Value::Type;

TEST(ValueTest, WholeResultsAreIntegers) {
  EXPECT_EQ(Type::INTEGER, (Value(1.5) + Value(1.5)).value_type());
  EXPECT_EQ(3, (Value(1.5) + Value(1.5)).toInt());
  EXPECT_EQ(Type::INTEGER, (Value(6) / Value(2)).value_type());
  EXPECT_EQ(Type::REAL, (Value(7) / Value(2)).value_type());
  EXPECT_DOUBLE_EQ(3.5, (Value(7) / Value(2)).toDouble());
}

TEST(ValueTest, MixedArithmetic) {
  EXPECT_DOUBLE_EQ(2.5, (Value(10) / Value(4)).toDouble());
  EXPECT_EQ(3, (Value(2) * Value(1.5)).toInt());
  EXPECT_DOUBLE_EQ(0.5, (Value(2.5) - Value(2)).toDouble());
  EXPECT_EQ(Type::REAL, (Value(1) + Value("0.5")).value_type());
}

TEST(ValueTest, IntegerOverflowBecomesReal) {
  const auto v = Value(100000) * Value(100000);
  EXPECT_EQ(Type::REAL, v.value_type());
  EXPECT_EQ("1e+10", v.toString());
}

TEST(ValueTest, RealToString) {
  EXPECT_EQ("1.5", Value(1.5).toString());
  EXPECT_EQ("0.333333", Value(1.0 / 3).toString());
  EXPECT_EQ("3", Value(3.0).toString());
  EXPECT_EQ("-0.25", Value(-0.25).toString());
}

TEST(ValueTest, RealToInt) {
  EXPECT_EQ(1, Value(1.9).toInt());
  EXPECT_EQ(-1, Value(-1.9).toInt());
  EXPECT_EQ(2147483647, Value(1e20).toInt());
}

TEST(ValueTest, Comparisons) {
  EXPECT_TRUE(Value(1) < Value(1.5));
  EXPECT_FALSE(Value(1) == Value(1.5));
  EXPECT_TRUE(Value(2) == Value(2.0));
  EXPECT_TRUE(Value(2.5) > Value(2));
}

TEST(ValueTest, ModTruncatesReals) {
  EXPECT_EQ(1, (Value(7.9) % Value(2)).toInt());
}

TEST(ValueTest, DivisionByZero) {
  EXPECT_THROW(Value(1.5) / Value(0), BasicError);
  EXPECT_THROW(Value(1) / Value(0.0), BasicError);
}
