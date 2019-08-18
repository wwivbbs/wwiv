/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2019, WWIV Software Services             */
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
#include <cstdio>
#include "core/log.h"
#include "gtest/gtest.h"

using wwiv::core::Logger;
using wwiv::core::LoggerConfig;

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  LoggerConfig config{};
  config.log_startup = false;
  Logger::Init(argc, argv, config);
  return RUN_ALL_TESTS();
} 
