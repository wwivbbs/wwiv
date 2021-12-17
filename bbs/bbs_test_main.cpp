#include "gtest/gtest.h"

#include "core/log.h"

using namespace wwiv::core;

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);

  LoggerConfig logger_config{};
  logger_config.register_file_destinations = false;
  logger_config.log_startup = false;
  Logger::Init(argc, argv, logger_config);
  return RUN_ALL_TESTS();
}
