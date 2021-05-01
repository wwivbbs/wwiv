/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*               Copyright (C)2014-2021, WWIV Software Services           */
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
#ifndef INCLUDED_WWIV_CORE_COMMAND_LINE_H
#define INCLUDED_WWIV_CORE_COMMAND_LINE_H

#include "core/inifile.h"
#include "core/log.h"
#include <functional>
#include <filesystem>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

/**
 * CommandLine support.
 *
 * 1) Create a tree of allowed values.
 * 2) Parse the actual commandline, reporting any errors.
 * 3) Get the values as needed.
 *
 * 1) Example:
 * program: foo [--name, -n] [--location, -l] command
 * command: bar [--barname]
 * command: baz [--bazname]
 * CommandLine cmdline(argc, argv, "set_style");
 * cmdline.add_argument({"name", 'n', "Sets Name"});
 * cmdline.add_argument({"location", 'l', "Sets Location"});
 *
 * CommandLineCommand& bar = cmdline.add_command("bar", "Bar Commands");
 * bar.add_argument({"barname", "Sets Bar Name"});
 * CommandLineCommand& baz = cmdline.add_command("baz", "Baz Commands");
 * baz.add_argument({"bazname", "Sets Baz Name"});
 * cmdLine.Parse();
 *
 * const string name = cmdline.arg("name").as_string();
 * CommandLineCommand cmd = cmdline.command();
 * const string command_name = cmd->name();
 * const string bazname = cmd->arg("bazname").as_string();
 */

namespace wwiv::core {

struct unknown_argument_error : public std::runtime_error {
  explicit unknown_argument_error(const std::string& message);
};

class CommandLineValue {
public:
  CommandLineValue() noexcept
    : default_(true) {
  }

  explicit CommandLineValue(const std::string& s) noexcept
    : CommandLineValue(s, false) {
  }

  CommandLineValue(std::string value, bool default_value) noexcept
    : value_(std::move(value)), default_(default_value) {
  }

  [[nodiscard]] std::string as_string() const noexcept { return value_; }

  [[nodiscard]] int as_int() const noexcept;

  [[nodiscard]] bool as_bool() const noexcept { return value_ == "true"; }

  [[nodiscard]] bool is_default() const noexcept { return default_; }

private:
  const std::string value_;
  const bool default_;
};

class CommandLineArgument {
public:
  CommandLineArgument(std::string name, char key, std::string help_text,
                      std::string default_value, std::string environment_variable);

  CommandLineArgument(const std::string& name, char key, const std::string& help_text,
                      const std::string& default_value)
    : CommandLineArgument(name, key, help_text, default_value, "") {
  }

  CommandLineArgument(const std::string& name, const std::string& help_text,
                      const std::string& default_value, const std::string& environment_variable)
    : CommandLineArgument(name, 0, help_text, default_value, environment_variable) {
  }

  CommandLineArgument(const std::string& name, const std::string& help_text,
                      const std::string& default_value)
    : CommandLineArgument(name, 0, help_text, default_value, "") {
  }

  CommandLineArgument(const std::string& name, char key, const std::string& help_text)
    : CommandLineArgument(name, key, help_text, "", "") {
  }

  CommandLineArgument(const std::string& name, const std::string& help_text)
    : CommandLineArgument(name, 0, help_text, "", "") {
  }

  [[nodiscard]] std::string help_text() const;
  [[nodiscard]] std::string default_value() const;
  const std::string name_;
  const char key_{0};
  const std::string help_text_;
  const std::string default_value_;
  const std::string environment_variable_;
  bool is_boolean{false};
};

class BooleanCommandLineArgument : public CommandLineArgument {
public:
  BooleanCommandLineArgument(const std::string& name, char key, const std::string& help_text,
                             bool default_value)
    : CommandLineArgument(name, key, help_text, default_value ? "true" : "false") {
    is_boolean = true;
  }

  BooleanCommandLineArgument(const std::string& name, const std::string& help_text,
                             bool default_value)
    : CommandLineArgument(name, 0, help_text, default_value ? "true" : "false") {
    is_boolean = true;
  }

  BooleanCommandLineArgument(const std::string& name, const std::string& help_text)
    : CommandLineArgument(name, 0, help_text, "false") {
    is_boolean = true;
  }

};

class Command {
protected:
  Command() = default;
public:
  virtual ~Command() = default;
  virtual int Execute() = 0;
  [[nodiscard]] virtual std::string GetUsage() const = 0;
  [[nodiscard]] virtual std::string GetHelp() const = 0;
};

/** Generic command implementation for using the CommandLine support in core */
class CommandLineCommand : public Command {
public:
  CommandLineCommand(std::string name, std::string help_text);
  ~CommandLineCommand() override = default;
  bool add_argument(const CommandLineArgument& cmd);

  virtual bool add(std::shared_ptr<CommandLineCommand> cmd) {
    cmd->set_raw_args(raw_args_);
    cmd->set_dot_argument(dot_argument_);
    commands_allowed_[cmd->name()] = std::move(cmd);
    return true;
  }

  [[nodiscard]] std::string ArgNameForKey(char key);
  virtual bool AddStandardArgs();

  [[nodiscard]] bool subcommand_selected() const { return command_ != nullptr; }
  [[nodiscard]] const std::string& name() const { return name_; }
  [[nodiscard]] const std::string& help_text() const { return help_text_; }

  [[nodiscard]] CommandLineValue arg(const std::string& name) const;
  [[nodiscard]] bool contains_arg(const std::string& name) const noexcept;
  [[nodiscard]] std::string sarg(const std::string& name) const { return arg(name).as_string(); }
  [[nodiscard]] int iarg(const std::string& name) const { return arg(name).as_int(); }
  [[nodiscard]] bool barg(const std::string& name) const { return arg(name).as_bool(); }
  [[nodiscard]] bool help_requested() const { return barg("help"); }
  [[nodiscard]] const CommandLineCommand* command() const { return command_; }
  [[nodiscard]] const std::vector<std::string>& remaining() const { return remaining_; }
  [[nodiscard]] std::string ToString() const;
  int Execute() override;
  [[nodiscard]] std::string GetHelpForArg(const CommandLineArgument&, int max_len = 25) const;
  [[nodiscard]] std::string GetHelpForCommand(const CommandLineCommand&, int max_len = 25) const;
  [[nodiscard]] std::string GetHelp() const override;
  [[nodiscard]] std::string GetUsage() const override { return ""; }
  void set_unknown_args_allowed(bool u) { unknown_args_allowed_ = u; }
  [[nodiscard]] bool unknown_args_allowed() const { return unknown_args_allowed_; }

  /**
   * Sets a new default value. NetworkCommandLine will get these from
   * an ini file for each of the network commands and call this.
   * TODO(rushfan): Should we generalize this and let all defaults
   * come from INI files?
   */
  bool SetNewDefault(const std::string& key, const std::string& value);

  /**
   * Public metadata about which command line args and subcommands are allowed. This
   * lets consumers display customized help and possibly command line completion.
   */

  /** Which arguments are allowed for this command */
  [[nodiscard]] const std::map<const std::string, CommandLineArgument>& args_allowed() const {
    return args_allowed_;
  }

  /** Which sub-commands are allowed for this command */
  [[nodiscard]] const std::map<const std::string, std::shared_ptr<CommandLineCommand>>& commands_allowed() const {
    return commands_allowed_;
  }

protected:
  /** Handles a command line argument passed on the commandline */
  bool HandleCommandLineArgument(const std::string& key, const std::string& value);
  bool SetCommandLineArgument(const std::string& key, const std::string& value, bool default_value);
  int Parse(int start_pos);

  void set_raw_args(const std::vector<std::string>& args) { raw_args_ = args; }
  void set_dot_argument(const std::string& dot_argument) { dot_argument_ = dot_argument; }

  std::vector<std::string> raw_args_;
  // Values as allowed to be specified on the commandline.
  std::map<const std::string, CommandLineArgument> args_allowed_;
  std::map<const std::string, std::shared_ptr<CommandLineCommand>> commands_allowed_;
  std::vector<std::string> remaining_;

private:
  // Values as entered on the commandline.
  std::map<std::string, CommandLineValue> args_;
  CommandLineCommand* command_ = nullptr;
  const std::string name_;
  const std::string help_text_;
  std::string dot_argument_;
  bool unknown_args_allowed_{false};
};

/**
 * Class to parse command line arguments and populate the commands.
 */
class CommandLine final : public CommandLineCommand {
public:
  /**
   * Common constructor.  Note that the dot_argument specific the
   * command line to replace . with (i.e. "network" means that .0
   * will be replaced by --network=0.  An empty dot_argument means
   * that no replacement will happen.
   */
  CommandLine(const std::vector<std::string>& args, const std::string& dot_argument);
  CommandLine(int argc, char** argv, const std::string& dot_argument);
  ~CommandLine() override = default;
  CommandLine() = delete;
  CommandLine(const CommandLine&) = delete;
  bool Parse();
  int Execute() override;
  bool AddStandardArgs() override;
  [[nodiscard]] std::string GetHelp() const override;

  void set_no_args_allowed(bool no_args_allowed) { no_args_allowed_ = no_args_allowed; }
  [[nodiscard]] bool no_args_allowed() const { return no_args_allowed_; }

  [[nodiscard]] std::string program_name() const noexcept { return program_name_; }
  [[nodiscard]] std::filesystem::path program_path() const noexcept { return program_path_; }
  [[nodiscard]] std::string bindir() const noexcept { return bindir_; }
  [[nodiscard]] std::string bbsdir() const noexcept { return bbsdir_; }
  [[nodiscard]] std::string configdir() const noexcept { return configdir_; }
  [[nodiscard]] std::string logdir() const noexcept { return logdir_; }
  [[nodiscard]] int verbose() const noexcept { return verbose_; }

private:
  const std::string program_name_;
  const std::filesystem::path program_path_;
  std::string bbsdir_;
  std::string bindir_;
  std::string configdir_;
  std::string logdir_;
  int verbose_{0};
  bool no_args_allowed_{false};

  bool ParseImpl();
};

// Utility methods for setting defaults from INI files.

/**
 * Sets a default value for the command line parameter for to be the value
 * of an INI file key of the same name and type.
 */
void SetNewStringDefault(CommandLine& cmdline, const IniFile& ini, const std::string& key);

/**
 * Sets a default value for the command line parameter for to be the value
 * of an INI file key of the same name and type.
 */
void SetNewBooleanDefault(CommandLine& cmdline, const IniFile& ini, const std::string& key);

/**
 * Sets a default value for the command line parameter for to be the value
 * of an INI file key of the same name and type.
 */
void SetNewIntDefault(CommandLine& cmdline, const IniFile& ini, const std::string& key);

/**
 * Sets a default value for the command line parameter for to be the value
 * of an INI file key of the same name and type.
 *
 * Also invokes function f with the default value pass to it as a parameter.
 */
void SetNewIntDefault(CommandLine& cmdline, const IniFile& ini, const std::string& key,
                      const std::function<void(int)>& f);

} // namespace

#endif
