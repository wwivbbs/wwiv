/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2021, WWIV Software Services             */
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
/**************************************************************************/
#include "gtest/gtest.h"
#include "core/cp437.h"
#include "core/file_helper.h"
#include "core/log.h"

using namespace wwiv::core;

int main(int argc, char* argv[]) {
  try {
    set_wwiv_codepage(wwiv_codepage_t::utf8);
    testing::InitGoogleTest(&argc, argv);
    LoggerConfig logger_config{};
    logger_config.register_file_destinations = false;
    logger_config.log_startup = false;
    Logger::Init(argc, argv, logger_config);
    FileHelper::set_wwiv_test_tempdir_from_commandline(argc, argv);
    return RUN_ALL_TESTS();
  } catch (const std::exception& e) {
    std::cerr << e.what();
    return 1;
  }
} 
