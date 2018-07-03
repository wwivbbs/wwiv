#include "gtest/gtest.h"
#include "core/log.h"

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  wwiv::core::LoggerConfig logger_config{};
  logger_config.register_file_destinations = false;
  logger_config.log_startup = false;
  wwiv::core::Logger::Init(argc, argv, logger_config);
  return RUN_ALL_TESTS();
}
