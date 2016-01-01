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

#include <cctype>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <sstream>
#include <vector>

#include "core/command_line.h"
#include "core/file.h"
#include "core/strings.h"
#include "core/stl.h"

using std::clog;
using std::cout;
using std::endl;
using std::left;
using std::map;
using std::setw;
using std::string;
using std::vector;
using namespace wwiv::strings;
using namespace wwiv::stl;

namespace wwiv {
namespace core {

CommandLineArgument::CommandLineArgument(
  const std::string& name, char key,
  const std::string& help_text, const std::string& default_value)
  : name(name), key(static_cast<char>(std::toupper(key))), 
    help_text(help_text), default_value(default_value) {}

CommandLineCommand::CommandLineCommand(
    const std::string& name, const std::string& help_text)
  : name_(name), help_text_(help_text) {}

static std::string CreateProgramName(const std::string arg) {
  string::size_type last_slash = arg.find_last_of(File::separatorChar);
  if (last_slash == string::npos) {
    return arg;
  }
  string program_name = arg.substr(last_slash + 1);
  return program_name;
}

// TODO(rushfan): Make the static command for the root commandlinehere and pass it as the invoker.
CommandLine::CommandLine(int argc, char** argv, const std::string dot_argument)
  : CommandLineCommand("", ""), program_name_(CreateProgramName(argv[0])) {
  set_argc(argc);
  set_argv(argv);
  set_dot_argument(dot_argument);
}

bool CommandLine::Parse() {
  try {
    if (!ParseImpl()) {
      clog << "Unable to parse command line." << endl;
      return false;
    }
  } catch (const unknown_argument_error& e) {
    clog << "Unable to parse command line." << endl;
    clog << e.what() << endl;
    return false;
  }

  if (argc_ <= 1) {
    clog << "No command line arguments specified." << endl;
    cout << GetHelp();
    return false;
  }
  return true;
}

bool CommandLine::ParseImpl() {
  return CommandLineCommand::Parse(1) >= CommandLineCommand::argc_;
}

int CommandLine::Execute() {
  return CommandLineCommand::Execute();
}

bool CommandLineCommand::add_argument(const CommandLineArgument& cmd) {
  // Add cmd to the list of allowable arguments, and also set 
  // a default empty value.
  args_allowed_.emplace(cmd.name, cmd);
  args_.emplace(cmd.name, CommandLineValue(cmd.default_value, true));

  return true;
}

bool CommandLineCommand::HandleCommandLineArgument(
    const std::string& key, const std::string& value) {
  args_.erase(key);  // emplace doesn't seem to replace.
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

const CommandLineValue CommandLineCommand::arg(const std::string name) const {
  if (!contains(args_, name)) {
    std::clog << "Unknown argument name: " << name << endl;
  }
  return args_.at(name);
}

bool CommandLineCommand::AddStandardArgs() {
  add_argument(BooleanCommandLineArgument("help", '?', "Displays Help", false));
  return true;
}

std::string CommandLineCommand::ArgNameForKey(char key) {
  for (const auto& a : args_allowed_) {
    if (key == a.second.key) {
      return a.first;
    }
  }
  return "";
}

int CommandLineCommand::Parse(int start_pos) {
  for (int i = start_pos; i < argc_; i++) {
    const string s(argv_[i]);
    if (starts_with(s, "--")) {
      const vector<string> delims = SplitString(s, "=");
      const string key = delims[0].substr(2);
      const string value = (delims.size() > 1) ? delims[1] : "";
      if (!contains(args_allowed_, key)) {
        throw unknown_argument_error(StrCat("Command: ", name(), "; key=", key));
      }
      HandleCommandLineArgument(key, value);
    } else if (starts_with(s, "/") || starts_with(s, "-")) {
      char letter = static_cast<char>(std::toupper(s[1]));
      const string key = ArgNameForKey(letter);
      if (key.empty()) {
        throw unknown_argument_error(StrCat("Command: ", name(), "; letter=", letter));
      }
      const string value = s.substr(2);
      HandleCommandLineArgument(key, value);
    } else if (starts_with(s, ".")) {
      const string value = s.substr(1);
      HandleCommandLineArgument(dot_argument_, value);
    } else {
      if (contains(commands_allowed_, s)) {
        // If s is a subcommand, parse it, incrementing our pointer.
        command_ = commands_allowed_.at(s).get();
        i = command_->Parse(++i);
      } else {
        // Add all residue to list of remaining args.
        // These usually are the positional arguments.
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

int CommandLineCommand::Execute() {
  if (arg("help").as_bool()) {
    // Help was selected.
    cout << GetHelp();
    return 0;
  }

  if (command_ != nullptr) {
    // We have sub command, so execute that.
    return command_->Execute();
  }

  if (name_.empty() || !remaining_.empty()) {
    // If name is empty, we're at the root command and
    // no valid command was specified.
    clog << "Invalid command specified: ";
    for (const auto& a : remaining_) {
      clog << a << " ";
    }
    clog << endl;
    cout << GetHelp();
    return 0;
  }

  // Nothing was able to be executed.
  clog << "Nothing to do for command: " << name_ << endl;
  cout << GetHelp();
  return 1;
}


std::string CommandLineCommand::GetHelp() const {
  std::ostringstream ss;
  string program_name = (name_.empty()) ? "program" : name_;
  ss << program_name << " arguments:" << std::endl;
  for (const auto& a : args_allowed_) {
    ss << "--" << left << setw(20) << a.second.name << " " << a.second.help_text << endl;
  }
  if (!commands_allowed_.empty()) {
    ss << endl;
    ss << "commands:" << std::endl;
    for (const auto& a : commands_allowed_) {
      const string allowed_name = a.second->name();
      ss << setw(20) << left << allowed_name << " " << a.second->help_text() << endl;
    }
  }
  return ss.str();
}

bool CommandLine::AddStandardArgs() {
  if (!CommandLineCommand::AddStandardArgs()) {
    return false;
  }
  add_argument({"bbsdir", "Main BBS Directory containing CONFIG.DAT", File::current_directory()});
  return true;
}

std::string CommandLine::GetHelp() const {
  std::ostringstream ss;
  ss << "Usage:" << endl;
  ss << program_name_ << " [args]";
  if (!commands_allowed_.empty()) {
    ss << " <command> [command args]";
  }
  ss << endl << endl;
  ss << CommandLineCommand::GetHelp();
  return ss.str();
}

unknown_argument_error::unknown_argument_error(const std::string& message)
    : std::runtime_error(StrCat("unknown_argument_error: ", message)) {
}


}  // namespace core
}  // namespace wwiv

