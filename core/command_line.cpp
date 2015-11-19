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

#include <cctype>
#include <map>
#include <string>
#include <sstream>
#include <vector>

#include "core/command_line.h"
#include "core/file.h"
#include "core/strings.h"
#include "core/stl.h"

using std::endl;
using std::map;
using std::string;
using std::vector;
using namespace wwiv::strings;
using namespace wwiv::stl;

namespace wwiv {
namespace core {

CommandLineCommand::CommandLineCommand(const std::string& name, const std::string& help_text, 
    int argc, char** argv, const std::string dot_argument)
  : name_(name), help_text_(help_text), argc_(argc), argv_(argv), dot_argument_(dot_argument) {}

static std::string CreateProgramName(const std::string arg) {
  string::size_type last_slash = arg.find_last_of(File::separatorChar);
  if (last_slash == string::npos) {
    return arg;
  }
  string program_name = arg.substr(last_slash + 1);
  return program_name;
}

CommandLine::CommandLine(int argc, char** argv, const std::string dot_argument)
  : CommandLineCommand("", "", argc, argv, dot_argument), program_name_(CreateProgramName(argv[0])) {
}

bool CommandLineCommand::add(const CommandLineArgument& cmd) { 
  // Add cmd to the list of allowable arguments, and also set 
  // a default empty value.
  args_allowed_.emplace(cmd.name, cmd);
  args_.emplace(cmd.name, CommandLineValue(cmd.default_value, true));

  return true;
}

bool CommandLineCommand::HandleCommandLineArgument(
    const std::string& key, const std::string& value) {
  args_.erase(key);  // emplace doesn't seem to repleace.
  if (args_allowed_.at(key).is_boolean) {
    if (value == "N" || value == "0" || IsEqualsIgnoreCase(value.c_str(), "false")) {
      args_.emplace(key, CommandLineValue("false"));
    } else {
      args_.emplace(key, CommandLineValue("true"));
    }
  } else {
    args_.emplace(key, CommandLineValue(value));
  }
  return true;
}

int CommandLineCommand::Parse(int start_pos) {
  for (int i = start_pos; i < argc_; i++) {
    const string s(argv_[i]);
    if (starts_with(s, "--")) {
      const vector<string> delims = SplitString(s, "=");
      const string key = delims[0].substr(2);
      const string value = (delims.size() > 1) ? delims[1] : "";
      if (!contains(args_allowed_, key)) {
        throw unknown_argument_error(StrCat("unknown_argument_error: ", key));
      }
      HandleCommandLineArgument(key, value);
    } else if (starts_with(s, "/") || starts_with(s, "-")) {
      char letter = static_cast<char>(std::toupper(s[1]));
      if (letter == '?') {
        args_.emplace("help", CommandLineValue(""));
      } else {
        const string key(1, letter);
        const string value = s.substr(2);
        HandleCommandLineArgument(key, value);
      }
    } else if (starts_with(s, ".")) {
      const string value = s.substr(1);
      HandleCommandLineArgument(dot_argument_, value);
    } else {
      if (contains(commands_allowed_, s)) {
        // If s is a subcommand, parse it, incrementing our pointer.
        CommandLineCommand& cmd = commands_allowed_.at(s);
        i = cmd.Parse(++i);
        command_ = &cmd;
      } else {
        // TODO: add all residue to new list.
        for (; i < argc_; i++) {
          remaining_.emplace_back(argv_[i]);
        }
      }
    }
  }
  return argc_;
}

std::string CommandLineCommand::ToString() const {
  std::ostringstream ss;
  ss << "name: " << name_ << "; args: ";
  for (const auto& a : args_) {
    ss << "{" << a.first << ": \"" << a.second.as_string() << "\"}";
  }
  if (!remaining_.empty()) {
    ss << "; remaining: ";
    for (const auto& a : remaining_) {
      ss << a << ", ";
    }
  }
  return ss.str();
}

std::string CommandLineCommand::GetHelp() const {
  std::ostringstream ss;
  string name = (name_.empty()) ? "program" : name_;
  ss << name << " arguments:" << std::endl;
  for (const auto& a : args_allowed_) {
    ss << "--" << StringPrintf("%-20s", a.second.name.c_str()) << " " << a.second.help_text << endl;
  }
  ss << endl;
  ss << "commands:" << std::endl;
  for (const auto& a : commands_allowed_) {
    const string name = a.second.name();
    ss << "--" << StringPrintf("%-20s", name.c_str()) << " " << a.second.help_text() << endl;
  }
  return ss.str();
}

std::string CommandLine::GetHelp() const {
  std::ostringstream ss;
  ss << program_name_ << " [args]";
  if (!commands_allowed_.empty()) {
    ss << " <command> [command args]";
  }
  ss << std::endl;
  ss << CommandLineCommand::GetHelp();
  return ss.str();
}


}  // namespace core
}  // namespace wwiv
