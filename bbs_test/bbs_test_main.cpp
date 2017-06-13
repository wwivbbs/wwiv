#include "gtest/gtest.h"
#include "core/log.h"

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  wwiv::core::Logger::Init(argc, argv);
  wwiv::core::Logger::DisableFileLoging();
  return RUN_ALL_TESTS();
}
