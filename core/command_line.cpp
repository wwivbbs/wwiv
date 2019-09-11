/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*               Copyright (C)2014-2019, WWIV Software Services           */
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
#include <sstream>
#include <string>
#include <vector>

#include "core/command_line.h"
#include "core/file.h"
#include "core/filesystem.h"
#include "core/os.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/version.h"

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
using namespace wwiv::os;

namespace wwiv::core {

CommandLineArgument::CommandLineArgument(const std::string& name, char key,
                                         const std::string& help_text,
                                         const std::string& default_value,
                                         const std::string& environment_variable)
    : name(name), key(static_cast<char>(std::toupper(key))), help_text_(help_text),
      default_value_(default_value), environment_variable_(environment_variable) {}

std::string CommandLineArgument::help_text() const { return help_text_; }

std::string CommandLineArgument::default_value() const {
  const auto env = environment_variable(environment_variable_);
  return env.empty() ? default_value_ : env;
}

CommandLineCommand::CommandLineCommand(const std::string& name, const std::string& help_text)
    : name_(name), help_text_(help_text) {}

static std::string CreateProgramName(const std::string& arg) {
  wwiv::fs::path p{arg};
  return p.filename().string();
}

// TODO(rushfan): Make the static command for the root commandline here and pass it as the invoker.
CommandLine::CommandLine(const std::vector<std::string>& args, const std::string& dot_argument)
    : CommandLineCommand("", ""), program_name_(CreateProgramName(args[0])) {
  set_raw_args(args);
  set_dot_argument(dot_argument);
}

static std::vector<std::string> make_args(int argc, char** argv) {
  std::vector<std::string> v;
  for (int i = 0; i < argc; i++) {
    v.push_back(argv[i]);
  }
  return v;
}

CommandLine::CommandLine(int argc, char** argv, const std::string& dot_argument)
    : CommandLine(make_args(argc, argv), dot_argument) {}

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

  if (raw_args_.size() <= 1 && !no_args_allowed()) {
    clog << "No command line arguments specified." << endl;
    cout << GetUsage();
    cout << GetHelp();
    return false;
  }

  bbsdir_ = sarg("bbsdir");
  bindir_ = sarg("bindir");
  configdir_ = sarg("configdir");
  logdir_ = sarg("logdir");
  verbose_ = iarg("v");
  return true;
}

bool CommandLine::ParseImpl() {
  return CommandLineCommand::Parse(1) >= size_int(CommandLineCommand::raw_args_);
}

int CommandLine::Execute() { return CommandLineCommand::Execute(); }

bool CommandLineCommand::add_argument(const CommandLineArgument& cmd) {
  // Add cmd to the list of allowable arguments, and also set
  // a default empty value.
  args_allowed_.emplace(cmd.name, cmd);
  args_.emplace(cmd.name, CommandLineValue(cmd.default_value(), true));
  return true;
}

bool CommandLineCommand::SetNewDefault(const std::string& key, const std::string& value) {
  return SetCommandLineArgument(key, value, true);
}

bool CommandLineCommand::HandleCommandLineArgument(const std::string& key,
                                                   const std::string& value) {
  return SetCommandLineArgument(key, value, false);
}

bool CommandLineCommand::SetCommandLineArgument(const std::string& key, const std::string& value,
                                                bool default_value) {
  args_.erase(key); // emplace doesn't seem to replace.
  if (!contains(args_allowed_, key)) {
    VLOG(1) << "No arg: " << key << " to use for dot argument.";
    return false;
  }
  if (args_allowed_.at(key).is_boolean) {
    if (value == "N" || value == "0" || value == "n" || iequals(value.c_str(), "false")) {
      args_.emplace(key, CommandLineValue("false", default_value));
    } else {
      args_.emplace(key, CommandLineValue("true", default_value));
    }
  } else {
    args_.emplace(key, CommandLineValue(value, default_value));
  }
  return true;
}

bool CommandLineCommand::contains_arg(const std::string& name) const noexcept {
  return contains(args_, name);
}

const CommandLineValue CommandLineCommand::arg(const std::string& name) const {
  if (!contains(args_, name)) {
    VLOG(1) << "Unknown argument name: " << name << endl;
    return CommandLineValue("", true);
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
  return {};
}

int CommandLineCommand::Parse(int start_pos) {
  for (size_t i = start_pos; i < raw_args_.size(); i++) {
    const string& s{raw_args_[i]};
    if (starts_with(s, "--")) {
      const auto delims = SplitString(s, "=");
      const auto key = delims[0].substr(2);
      const auto value = (delims.size() > 1) ? delims[1] : "";
      if (!contains(args_allowed_, key)) {
        if (unknown_args_allowed()) {
          continue;
        }
        throw unknown_argument_error(StrCat("Command: ", name(), "; key=", key));
      }
      HandleCommandLineArgument(key, value);
    } else if (starts_with(s, "/") || starts_with(s, "-")) {
      auto letter = static_cast<char>(std::toupper(s[1]));
      const auto key = ArgNameForKey(letter);
      if (key.empty()) {
        if (unknown_args_allowed()) {
          continue;
        }
        throw unknown_argument_error(StrCat("Command: ", name(), "; letter=", letter));
      }
      const auto value = s.substr(2);
      HandleCommandLineArgument(key, value);
    } else if (starts_with(s, ".")) {
      // Were handling dot arguments.
      if (!dot_argument_.empty()) {
        // LOG's use of command_line never defines dot arguments.
        VLOG(3) << "Ignoring dot argument since no mapping is defined by the application.";
        const auto value = s.substr(1);
        HandleCommandLineArgument(dot_argument_, value);
      }
    } else {
      if (contains(commands_allowed_, s)) {
        // If s is a subcommand, parse it, incrementing our pointer.
        command_ = commands_allowed_.at(s).get();
        i = command_->Parse(++i);
      } else {
        // Add all residue to list of remaining args.
        // These usually are the positional arguments.
        for (; i < raw_args_.size(); i++) {
          remaining_.emplace_back(raw_args_[i]);
        }
      }
    }
  }
  return raw_args_.size();
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
  if (help_requested()) {
    // Help was selected.
    cout << GetUsage();
    cout << GetHelp();
    return 0;
  }

  if (command_ != nullptr) {
    // We have sub command, so execute that.
    if (command_->help_requested()) {
      clog << command_->GetUsage();
      clog << command_->GetHelp();
      return 0;
    }
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
    cout << GetUsage() << GetHelp();
    return 0;
  }

  // Nothing was able to be executed.
  clog << "Nothing to do for command: " << name_ << endl;
  cout << GetUsage() << GetHelp();
  return 1;
}

std::string CommandLineCommand::GetHelp() const {
  std::ostringstream ss;
  string program_name = (name_.empty()) ? "program" : name_;
  ss << program_name << " arguments:" << std::endl;
  for (const auto& a : args_allowed_) {
    const auto& c = a.second;
    if (c.key != 0) {
      ss << "-" << c.key << " ";
    } else {
      ss << "   ";
    }
    string text = c.name;
    if (!c.is_boolean) {
      text = StrCat(c.name, "=value");
    }
    ss << "--" << left << setw(25) << text << " " << c.help_text() << endl;
  }
  if (!commands_allowed_.empty()) {
    ss << endl;
    ss << "commands:" << std::endl;
    for (const auto& a : commands_allowed_) {
      const string allowed_name = a.second->name();
      ss << "   "
         << "  " << setw(25) << left << allowed_name << " " << a.second->help_text() << endl;
    }
  }
  return ss.str();
}

bool CommandLine::AddStandardArgs() {
  if (!CommandLineCommand::AddStandardArgs()) {
    return false;
  }
  add_argument({"bindir", "Main BBS binary directory.", File::current_directory().string(), "WWIV_BIN_DIR"});
  add_argument(
      {"bbsdir", "Root BBS directory (i.e. C:\bbs)", File::current_directory().string(), "WWIV_DIR"});
  add_argument({"configdir", "Main BBS Directory containing CONFIG.DAT",
                File::current_directory().string(), "WWIV_CONFIG_DIR"});
  add_argument({"logdir", "Directory where log files are written.",
                File::current_directory().string(), "WWIV_LOG_DIR"});

  add_argument(
      BooleanCommandLineArgument{"log_startup", "Should the start/stop/args be logged.", false});

  // Ignore these. used by logger
  add_argument({"v", "verbose log", "0"});
  return true;
}

std::string CommandLine::GetHelp() const {
  std::ostringstream ss;
  ss << program_name_ << " [" << wwiv_version << beta_version << "]" << endl << endl;
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
    : std::runtime_error(StrCat("unknown_argument_error: ", message)) {}

} // namespace wwiv
