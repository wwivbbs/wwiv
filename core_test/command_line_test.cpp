/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*               Copyright (C)2014-2015 WWIV Software Services            */
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
#include "gtest/gtest.h"
#include "core/command_line.h"

#include <iostream>
#include <map>
#include <string>
#include <vector>

using std::map;
using std::string;
using std::vector;

using namespace wwiv::core;

class CommandLineTest: public ::testing::Test {};

TEST_F(CommandLineTest, Basic) {
  int argc = 3;
  char* argv[] = {"", "--foo", "--bar=baz"};
  CommandLine cmdline(argc, argv, "");
  cmdline.add(CommandLineArgument("foo", ' ', "", "asdf"));
  cmdline.add(CommandLineArgument("bar", ' ', "", "asdf"));

  ASSERT_TRUE(cmdline.Parse());
  EXPECT_EQ("baz", cmdline.arg("bar").as_string());
}

TEST_F(CommandLineTest, Command) {
  int argc = 6;
  char* argv[] = {"", "--foo=bar", "print", "--all", "--some=false", "myfile.txt"};
  CommandLine cmdline(argc, argv, "");
  cmdline.add({"foo", ' ', "", "asdf"});
  CommandLineCommand& print = cmdline.add_command("print");
  print.add(BooleanCommandLineArgument("all", ' ', "", false));
  print.add(BooleanCommandLineArgument("some", ' ', "", true));

  ASSERT_TRUE(cmdline.Parse());
  EXPECT_EQ("bar", cmdline.arg("foo").as_string());

  ASSERT_TRUE(cmdline.subcommand_selected());
  EXPECT_EQ("print", cmdline.command()->name());
  std::clog << cmdline.command()->ToString() << std::endl;

  EXPECT_TRUE(cmdline.command()->arg("all").as_bool());
  EXPECT_FALSE(cmdline.command()->arg("some").as_bool());
}

TEST_F(CommandLineTest, Several_Commands) {
  int argc = 3;
  char* argv[] = {"", "--foo=bar", "print",};
  CommandLine cmdline(argc, argv, "");
  cmdline.add({"foo", ' ', "", "asdf"});
  cmdline.add_command("print");

  ASSERT_TRUE(cmdline.Parse());
  EXPECT_EQ("bar", cmdline.arg("foo").as_string());

  ASSERT_TRUE(cmdline.subcommand_selected());
  EXPECT_EQ("print", cmdline.command()->name());
  std::clog << cmdline.command()->ToString() << std::endl;
}
