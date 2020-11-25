/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2020, WWIV Software Services             */
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
#include "core/log.h"
#include <clocale>

int main(int argc, char* argv[]) {
  try {
    std::locale::global(std::locale(std::setlocale(LC_ALL, "")));
    
    std::cout << "Current Locale: " << std::locale("").name() << std::endl;
#ifdef _WIN32
    std::setlocale(LC_ALL, ".UTF-8");
#else
    std::cout << "Resetting Locale: " << std::setlocale(LC_ALL, "") << std::endl;
#endif
    std::locale loc2;
    std::cout << "Current Locale after set: " << loc2.name() << std::endl;
    testing::InitGoogleTest(&argc, argv);
    wwiv::core::LoggerConfig logger_config{};
    logger_config.register_file_destinations = false;
    logger_config.log_startup = false;
    wwiv::core::Logger::Init(argc, argv, logger_config);
    return RUN_ALL_TESTS();
  } catch (const std::exception& e) {
    std::cerr << e.what();
    return 1;
  }
} 
