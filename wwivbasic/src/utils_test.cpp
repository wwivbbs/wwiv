#include "gtest/gtest.h"
#include "utils.h"


TEST(UtilsTest, SplitPackageFromId) {
  {
    auto [p, id] = split_package_from_id("this.is.a.test");
    EXPECT_EQ(p, "this.is.a");
    EXPECT_EQ(id, "test");
  }
}


TEST(UtilsTest, FindPackageAndId) {
  {
    std::set<std::string> x{"this", "this.is", "this.is.a" };
    auto [p, id] = find_package_and_id(x, "this.is.a.test");
    EXPECT_EQ(p, "this.is.a");
    EXPECT_EQ(id, "test");
  }

  {
    std::set<std::string> x{"this", "this.is" };
    auto [p, id] = find_package_and_id(x, "this.is.a.test");
    EXPECT_EQ(p, "this.is");
    EXPECT_EQ(id, "a.test");
  }

}