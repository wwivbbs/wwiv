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
#ifndef INCLUDED_CORE_TEST_WWIVTEST_H
#define INCLUDED_CORE_TEST_WWIVTEST_H

#include "gtest/gtest.h"
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
void InitTestForMain(int argc, char** argv);

/**
 * Specialized ::testing::Test class that skips tests that depend on WWIV_TESTDATA being set
 * if it's not specified in the environment or from the commandline.
 * 
 * Example:
 * 
 * class FileTestDataTest : public wwiv::core::test::TestDataTest {};
 * 
 * TEST_F(FileTestDataTest, ...) ...
 */
class TestDataTest : public ::testing::Test {
protected:
  void SetUp() override;
};


}

#endif // INCLUDED_CORE_TEST_WWIVTEST_H