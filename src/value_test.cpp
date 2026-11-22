#include "gtest/gtest.h"
#include "wwivbasic/module.h"
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

TEST(ValueTest, ParseNumberIsDecimalOnly) {
  EXPECT_EQ(12.0, Value::parse_number("12"));
  EXPECT_EQ(-2.5, Value::parse_number(" -2.5"));
  EXPECT_EQ(2500.0, Value::parse_number("+2.5e3"));
  EXPECT_EQ(12.0, Value::parse_number("12abc"));
  EXPECT_EQ(0.5, Value::parse_number(".5"));
  EXPECT_FALSE(Value::parse_number("0x10").has_value() && *Value::parse_number("0x10") != 0);
  EXPECT_FALSE(Value::parse_number("inf"));
  EXPECT_FALSE(Value::parse_number("nan"));
  EXPECT_FALSE(Value::parse_number("-inf"));
  EXPECT_FALSE(Value::parse_number("abc"));
  EXPECT_FALSE(Value::parse_number(""));
  EXPECT_FALSE(Value::parse_number("1e400"));
}

TEST(ValueTest, StringConversionsAgree) {
  // "0x10" is 0 both ways (it reads as the number 0 followed by "x10").
  EXPECT_EQ(0, Value("0x10").toInt());
  EXPECT_EQ(0.0, Value("0x10").toDouble());
  EXPECT_EQ(0, Value("nan").toInt());
  EXPECT_EQ(0.0, Value("inf").toDouble());
  EXPECT_EQ(3, Value("3.9").toInt());
}

TEST(ValueTest, HugeNumericStringIsNotAnInteger) {
  // Used to take the integer path and overflow (undefined behaviour).
  const auto v = Value(5) + Value("1e30");
  EXPECT_EQ(Value::Type::REAL, v.value_type());
  EXPECT_DOUBLE_EQ(1e30, v.toDouble());
  EXPECT_EQ(0, (Value(2) * Value("inf")).toInt());
}

TEST(ValueTest, ModWithExtremeValues) {
  // Used to compute INT_MIN % -1, which crashes with SIGFPE.
  EXPECT_EQ(0, (Value(-3000000000.0) % Value(-1)).toInt());
  // Truncated to 7 MOD 0, like my_basic.
  EXPECT_THROW(Value(7.5) % Value(0.5), BasicError);
}

TEST(ValueTest, NumericStringComparisonsMatchArithmetic) {
  EXPECT_TRUE(Value(1) < Value("1.5"));
  EXPECT_TRUE(Value(2) == Value("2.0"));
  EXPECT_TRUE(Value(1.5) > Value("1"));
}

TEST(ValueTest, NativeWholeDoubleIsInteger) {
  EXPECT_EQ(Value::Type::INTEGER, to_value(4.0).value_type());
  EXPECT_EQ(Value::Type::REAL, to_value(4.5).value_type());
}
