/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*               Copyright (C)2014-2016 WWIV Software Services            */
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

class NoopCommandLineCommand: public CommandLineCommand {
public:
  NoopCommandLineCommand(const std::string& name)
      : CommandLineCommand(name, name) {}
  int Execute() override final { return 0; }
  std::string GetHelp() const override final { return ""; }
};

TEST_F(CommandLineTest, Basic) {
  CommandLine cmdline({"", "--foo", "--bar=baz"}, "");
  cmdline.add_argument({"foo", "help for foo", "asdf"});
  cmdline.add_argument({"bar", "help for bar"});

  ASSERT_TRUE(cmdline.Parse());
  EXPECT_EQ("baz", cmdline.arg("bar").as_string());
}

TEST_F(CommandLineTest, Command) {
  CommandLine cmdline({"", "--foo=bar", "print", "--all", "--some=false", "myfile.txt"}, "");
  cmdline.add_argument({"foo", ' ', "", "asdf"});
  auto print = std::make_unique<NoopCommandLineCommand>("print");
  print->add_argument(BooleanCommandLineArgument("all", ' ', "", false));
  print->add_argument(BooleanCommandLineArgument("some", ' ', "", true));
  cmdline.add(std::move(print));

  ASSERT_TRUE(cmdline.Parse());
  EXPECT_EQ("bar", cmdline.arg("foo").as_string());

  ASSERT_TRUE(cmdline.subcommand_selected());
  EXPECT_EQ("print", cmdline.command()->name());
  std::clog << cmdline.command()->ToString() << std::endl;

  EXPECT_TRUE(cmdline.command()->arg("all").as_bool());
  EXPECT_FALSE(cmdline.command()->arg("some").as_bool());
}

TEST_F(CommandLineTest, Several_Commands) {
  CommandLine cmdline({"", "--foo=bar", "print",}, "");
  cmdline.add_argument({"foo", ' ', "", "asdf"});
  cmdline.add(std::make_unique<NoopCommandLineCommand>("print"));

  ASSERT_TRUE(cmdline.Parse());
  EXPECT_EQ("bar", cmdline.arg("foo").as_string());

  ASSERT_TRUE(cmdline.subcommand_selected());
  EXPECT_EQ("print", cmdline.command()->name());
  std::clog << cmdline.command()->ToString() << std::endl;
}

TEST_F(CommandLineTest, SlashArg) {
  CommandLine cmdline({"foo.exe", "/n500", ".1"}, "network_number");
  cmdline.add_argument({"node", 'n', "node to dial", "0"});
  cmdline.add_argument({"network_number", "network number to use.", "0"});

  ASSERT_TRUE(cmdline.Parse());
  EXPECT_EQ(500, cmdline.arg("node").as_int());
}

TEST_F(CommandLineTest, DotArg) {
  CommandLine cmdline({"foo.exe", "/n500", ".123"}, "network_number");
  cmdline.add_argument({"node", 'n', "node to dial", "0"});
  cmdline.add_argument({"network_number", "network number to use.", "0"});

  ASSERT_TRUE(cmdline.Parse());
  EXPECT_EQ(123, cmdline.arg("network_number").as_int());
}

TEST_F(CommandLineTest, Help) {
  CommandLine cmdline({"foo.exe", "/?"}, "network_number");
  cmdline.add_argument(BooleanCommandLineArgument("help", '?', "display help", false));

  ASSERT_TRUE(cmdline.Parse());
  EXPECT_TRUE(cmdline.arg("help").as_bool());
}

