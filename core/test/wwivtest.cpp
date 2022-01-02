/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2021-2022, WWIV Software Services             */
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
#include "core/test/wwivtest.h"

#include "gtest/gtest.h"
#include "core/cp437.h"
#include "core/test/file_helper.h"
#include "core/log.h"
#include <ctime>
#include <string>

namespace wwiv::core::test {

/**
 * Initializes much of the WWIV Environment for a GTest main method.
 *
 * Specifically:
 *
 * - Sets codepage to UTF8
 * - Initialies WWIV's global logger
 * - Initialies GoogleTest
 * - Sets WWIV's test tempdir and testdata directories from the commandline.
 */
void InitTestForMain(int argc, char** argv) {
  set_wwiv_codepage(wwiv_codepage_t::utf8);
  tzset();
  testing::InitGoogleTest(&argc, argv);
  LoggerConfig logger_config{};
  logger_config.register_file_destinations = false;
  logger_config.log_startup = false;
  Logger::Init(argc, argv, logger_config);
  FileHelper::set_wwiv_test_tempdir_from_commandline(argc, argv);
}


void TestDataTest::SetUp() {
  if (!wwiv::core::test::FileHelper::HasTestData()) {
    GTEST_SKIP() << "Skipping all tests that use real files since FileHelper::TestData() is empty.";
  }
}

} // namespace wwiv::core::test

