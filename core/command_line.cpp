/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*               Copyright (C)2014-2020, WWIV Software Services           */
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

#include "core/command_line.h"
#include "core/file.h"
#include "core/os.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/version.h"
#include <cctype>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

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

CommandLineArgument::CommandLineArgument(std::string name, char key,
                                         std::string help_text,
                                         std::string default_value,
                                         std::string environment_variable)
  : name_(std::move(name)), key_(static_cast<char>(std::toupper(key))), help_text_(
        std::move(help_text)),
    default_value_(std::move(default_value)), environment_variable_(std::move(environment_variable)) {
}

std::string CommandLineArgument::help_text() const { return help_text_; }

std::string CommandLineArgument::default_value() const {
  const auto env = environment_variable(environment_variable_);
  return env.empty() ? default_value_ : env;
}

CommandLineCommand::CommandLineCommand(std::string name, std::string help_text)
  : name_(std::move(name)), help_text_(std::move(help_text)) {
}

static std::string CreateProgramName(const std::string& arg) {
  const std::filesystem::path p{arg};
  return p.filename().string();
}

CommandLine::CommandLine(const std::vector<std::string>& args, const std::string& dot_argument)
  : CommandLineCommand("", ""), 
    program_name_(CreateProgramName(args[0])),
    program_path_(std::filesystem::path(args[0])) {
  set_raw_args(args);
  set_dot_argument(dot_argument);
}

static std::vector<std::string> make_args(int argc, char** argv) {
  std::vector<std::string> v;
  for (auto i = 0; i < argc; i++) {
    v.emplace_back(argv[i]);
  }
  return v;
}

CommandLine::CommandLine(int argc, char** argv, const std::string& dot_argument)
  : CommandLine(make_args(argc, argv), dot_argument) {
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
  return CommandLineCommand::Parse(1) >= ssize(raw_args_);
}

int CommandLine::Execute() { return CommandLineCommand::Execute(); }

bool CommandLineCommand::add_argument(const CommandLineArgument& cmd) {
  // Add cmd to the list of allowable arguments, and also set
  // a default empty value.
  args_allowed_.emplace(cmd.name_, cmd);
  args_.emplace(cmd.name_, CommandLineValue(cmd.default_value(), true));
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
  args_.erase(key); // "emplace" doesn't replace, so erase it first.
  if (!contains(args_allowed_, key)) {
    VLOG(1) << "No arg: " << key << " to use for dot argument.";
    return false;
  }
  if (at(args_allowed_, key).is_boolean) {
    if (value == "N" || value == "0" || value == "n" || iequals(value, "false")) {
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

CommandLineValue CommandLineCommand::arg(const std::string& name) const {
  if (!contains(args_, name)) {
    VLOG(1) << "Unknown argument name: " << name << endl;
    return CommandLineValue("", true);
  }
  return at(args_, name);
}

bool CommandLineCommand::AddStandardArgs() {
  add_argument(BooleanCommandLineArgument("help", '?', "Displays Help", false));
  return true;
}

std::string CommandLineCommand::ArgNameForKey(char key) {
  for (const auto& a : args_allowed_) {
    if (key == a.second.key_) {
      return a.first;
    }
  }
  return {};
}

static bool is_shortarg_start(char c) {
#ifdef _WIN32
  return (c == '/' || c == '-');
#else
  return (c == '-');
#endif
}

int CommandLineCommand::Parse(int start_pos) {
  for (auto i = start_pos; i < wwiv::stl::ssize(raw_args_); i++) {
    const string& s{raw_args_[i]};
    if (s.empty()) {
      continue;
    }
    if (s == "--") {
      // Everything after this should be positional args.
      for (++i; i < wwiv::stl::ssize(raw_args_); i++) {
        remaining_.emplace_back(raw_args_[i]);
      }
      continue;
    }
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
    } else if (is_shortarg_start(s.front())) {
      const auto letter = static_cast<char>(std::toupper(s[1]));
      const auto key = ArgNameForKey(letter);
      if (key.empty()) {
        if (unknown_args_allowed()) {
          continue;
        }
        throw unknown_argument_error(StrCat("Command: ", name(), "; letter=", letter));
      }
      const auto value = s.substr(2);
      HandleCommandLineArgument(key, value);
    } else if (s.front() == '.') {
      // Were handling dot arguments.
      if (!dot_argument_.empty()) {
        // LOG's use of command_line never defines dot arguments.
        VLOG(3) << "Ignoring dot argument since no mapping is defined by the application.";
        const auto value = s.substr(1);
        HandleCommandLineArgument(dot_argument_, value);
      }
    } else {
      if (contains(commands_allowed_, s)) {
        // If s is a sub-command, parse it, incrementing our pointer.
        command_ = at(commands_allowed_, s).get();
        i = command_->Parse(++i);
      } else {
        // Add all residue to list of remaining args.
        // These usually are the positional arguments.
        for (; i < wwiv::stl::ssize(raw_args_); i++) {
          remaining_.emplace_back(raw_args_[i]);
        }
      }
    }
  }
  return wwiv::stl::size_int(raw_args_);
}

std::string CommandLineCommand::ToString() const {
  std::ostringstream ss;
  ss << "name: " << name_ << "; args: ";
  for (const auto& [key, value] : args_) {
    ss << "{" << key << ": \"" << value.as_string() << "\"}";
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
  const auto program_name = (name_.empty()) ? "program" : name_;
  ss << program_name << " arguments:" << std::endl;
  for (const auto& [_, c] : args_allowed_) {
    if (c.key_ != 0) {
      ss << "-" << c.key_ << " ";
    } else {
      ss << "   ";
    }
    auto text = c.name_;
    if (!c.is_boolean) {
      text = StrCat(c.name_, "=value");
    }
    ss << "--" << left << setw(25) << text << " " << c.help_text() << endl;
  }
  if (!commands_allowed_.empty()) {
    ss << endl;
    ss << "commands:" << std::endl;
    for (const auto& a : commands_allowed_) {
      const auto allowed_name = a.second->name();
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
  add_argument({"bindir", "Main BBS binary directory.", File::current_directory().string(),
                "WWIV_BIN_DIR"});
  add_argument(
  {"bbsdir", "Root BBS directory (i.e. C:\\bbs)", File::current_directory().string(),
   "WWIV_DIR"});
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
  ss << program_name_ << " [" << full_version() << "]" << endl << endl;
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


void SetNewStringDefault(CommandLine& cmdline, const IniFile& ini, const std::string& key) {
  if (cmdline.contains_arg(key) && cmdline.arg(key).is_default()) {
    const auto f = ini.value<std::string>(key, cmdline.sarg(key));
    cmdline.SetNewDefault(key, f);
  }
}

void SetNewBooleanDefault(CommandLine& cmdline, const IniFile& ini, const std::string& key) {
  if (cmdline.contains_arg(key) && cmdline.arg(key).is_default()) {
    const auto f = ini.value<bool>(key, cmdline.barg(key));
    cmdline.SetNewDefault(key, f ? "Y" : "N");
  }
}

void SetNewIntDefault(CommandLine& cmdline, const IniFile& ini, const std::string& key) {
  if (cmdline.contains_arg(key) && cmdline.arg(key).is_default()) {
    const auto f = ini.value<int>(key, cmdline.iarg(key));
    cmdline.SetNewDefault(key, std::to_string(f));
  }
}

void SetNewIntDefault(CommandLine& cmdline, const IniFile& ini, const std::string& key,
                             const std::function<void(int)>& f) {
  if (cmdline.contains_arg(key) && cmdline.arg(key).is_default()) {
    const auto v = ini.value<int>(key, cmdline.iarg(key));
    cmdline.SetNewDefault(key, std::to_string(v));
    f(v);
  }
}

} // namespace
