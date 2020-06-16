#include "core/command_line.h"
#include "core/file.h"
#include "core/log.h"
#include "core/os.h"
#include "core_test/file_helper.h"
#include "gtest/gtest.h"
#include <cstdio>
#include <string>

using std::string;
using namespace wwiv::core;

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);

  LoggerConfig log_config{};
  log_config.log_startup = false;
  Logger::Init(argc, argv, log_config);

  tzset();
  FileHelper::set_wwiv_test_tempdir_from_commandline(argc, argv);
  return RUN_ALL_TESTS();
}
