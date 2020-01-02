/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                 Copyright (C)2018-2020, WWIV Software Services         */
/*                                                                        */
/*    Licensed  under the  Apache License, Version  2.0 (the "License");  */
/*    you may not use this  file  except in compliance with the License.  */
/*    You may obtain a copy of the License at                             */
/*                                                                        */
/*                http://www.apache.org/licenses/LICENSE-2.0              */
/*                                                                        */
/*    Unless  required  by  applicable  law  or agreed to  in  writing,   */
/*    software  distributed  under  the  License  is  distributed on an   */
/*    "AS IS"  BASIS, WITHOUT  WARRANTIES  OR  CONDITIONS OF ANY  KIND,   */
/*    either  express  or implied.  See  the  License for  the specific   */
/*    language governing permissions and limitations under the License.   */
/*                                                                        */
/**************************************************************************/

#include "core/command_line.h"
#include "core/file.h"
#include "core/log.h"
#include "core/os.h"
#include "core_test/file_helper.h"
#include "gtest/gtest.h"
#include <string>

using std::string;
using namespace wwiv::core;

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  LoggerConfig log_config{};
  log_config.log_startup = false;
  Logger::Init(argc, argv, log_config);
  CommandLine cmdline(argc, argv, "net");
  cmdline.AddStandardArgs();
  cmdline.add_argument(
      {"wwiv_test_tempdir", "Use instead of WWIV_TEST_TEMPDIR environment variable.", ""});
  cmdline.set_no_args_allowed(true);
  if (!cmdline.Parse()) {
    LOG(ERROR) << "Failed to parse cmdline.";
  }

  tzset();

  auto tmpdir = cmdline.arg("wwiv_test_tempdir").as_string();
  if (tmpdir.empty()) {
    tmpdir = wwiv::os::environment_variable("WWIV_TEST_TEMPDIR");
  }
  if (!tmpdir.empty()) {
    if (!File::Exists(tmpdir)) {
      File::mkdirs(tmpdir);
    }
    File::set_current_directory(tmpdir);
    FileHelper::set_wwiv_test_tempdir(tmpdir);
  }

  return RUN_ALL_TESTS();
}
