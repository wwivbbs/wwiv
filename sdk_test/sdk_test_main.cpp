#include <cstdio>
#include <string>
#include "core/os.h"
#include "gtest/gtest.h"

using std::string;

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);

  const string tmpdir = wwiv::os::environment_variable("WWIV_TEST_TEMPDIR");
  if (tmpdir.empty()) {
    FAIL() << "WWIV_TEST_TEMPDIR must be set for this test suite.";
  }

  return RUN_ALL_TESTS();
}
