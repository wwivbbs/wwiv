/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*               Copyright (C)2014-2017, WWIV Software Services           */
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
#ifndef __INCLUDED_WWIV_CORE_COMMAND_LINE_H__
#define __INCLUDED_WWIV_CORE_COMMAND_LINE_H__
#pragma once

#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

/**
 * ComandLine support.
 *
 * 1) Create a tree of allowed values.
 * 2) Parse the actua commandline, reporting any errors.
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

namespace wwiv {
namespace core {

struct unknown_argument_error : public std::runtime_error {
  unknown_argument_error(const std::string& message);
};

class CommandLineValue {
public:
  CommandLineValue() noexcept : default_(true) {}
  explicit CommandLineValue(const std::string& s) noexcept : CommandLineValue(s, false) {}
  CommandLineValue(const std::string& s, bool default_value) noexcept
      : value_(s), default_(default_value) {}

  std::string as_string() const noexcept { return value_; }
  int as_int() const noexcept {
    try {
      return std::stoi(value_);
    } catch (std::logic_error&) {
      return 0;
    }
  }
  bool as_bool() const noexcept { return value_ == "true"; }

  bool is_default() const noexcept { return default_; }

private:
  const std::string value_;
  const bool default_;
};

class CommandLineArgument {
public:
  CommandLineArgument(const std::string& name, char key, const std::string& help_text,
                      const std::string& default_value, const std::string& environment_variable);
  CommandLineArgument(const std::string& name, char key, const std::string& help_text,
                      const std::string& default_value)
      : CommandLineArgument(name, key, help_text, default_value, "") {}
  CommandLineArgument(const std::string& name, const std::string& help_text,
                      const std::string& default_value, const std::string& environment_variable)
      : CommandLineArgument(name, 0, help_text, default_value, environment_variable) {}
  CommandLineArgument(const std::string& name, const std::string& help_text,
                      const std::string& default_value)
      : CommandLineArgument(name, 0, help_text, default_value, "") {}
  CommandLineArgument(const std::string& name, char key, const std::string& help_text)
      : CommandLineArgument(name, key, help_text, "", "") {}
  CommandLineArgument(const std::string& name, const std::string& help_text)
      : CommandLineArgument(name, 0, help_text, "", "") {}

  std::string help_text() const;
  std::string default_value() const;
  const std::string name;
  const char key = 0;
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
public:
  virtual int Execute() = 0;
  virtual std::string GetUsage() const = 0;
  virtual std::string GetHelp() const = 0;
};

/** Generic command implementation for using the CommandLine support in core */
class CommandLineCommand : public Command {
public:
  CommandLineCommand(const std::string& name, const std::string& help_text);
  bool add_argument(const CommandLineArgument& cmd);
  virtual bool add(std::shared_ptr<CommandLineCommand> cmd) {
    cmd->set_raw_args(raw_args_);
    cmd->set_dot_argument(dot_argument_);
    commands_allowed_[cmd->name()] = std::move(cmd);
    return true;
  }
  std::string ArgNameForKey(char key);
  virtual bool AddStandardArgs();

  bool subcommand_selected() const { return command_ != nullptr; }
  std::string name() const { return name_; }
  std::string help_text() const { return help_text_; }

  const CommandLineValue arg(const std::string name) const;
  bool contains_arg(const std::string& name) const noexcept;
  std::string sarg(const std::string name) const { return arg(name).as_string(); }
  int iarg(const std::string name) const { return arg(name).as_int(); }
  bool barg(const std::string name) const { return arg(name).as_bool(); }
  bool help_requested() const { return barg("help"); }
  const CommandLineCommand* command() const { return command_; }
  std::vector<std::string> remaining() const { return remaining_; }
  std::string ToString() const;
  virtual int Execute() override;
  virtual std::string GetHelp() const override;
  virtual std::string GetUsage() const override { return ""; }
  void set_unknown_args_allowed(bool u) { unknown_args_allowed_ = u; }
  bool unknown_args_allowed() const { return unknown_args_allowed_; }

  /**
   * Sets a new default value. NetworkCommandLine will get these from
   * an ini file for each of the network commands and call this.
   * TODO(rushfan): Should we generalize this and let all defaults
   * come from INI files?
   */
  bool SetNewDefault(const std::string& key, const std::string& value);

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
class CommandLine : public CommandLineCommand {
public:
  /**
   * Common constructor.  Note that the dot_argument specifie the
   * command line to replace . with (i.e. "network" means that .0
   * will be replaced by --network=0.  An empty dot_argument means
   * that no replacement will happen.
   */
  CommandLine(const std::vector<std::string>& args, const std::string dot_argument);
  CommandLine(int argc, char** argv, const std::string dot_argument);
  virtual bool Parse();
  virtual int Execute() override final;
  bool AddStandardArgs() override;
  std::string GetHelp() const override final;

  void set_no_args_allowed(bool no_args_allowed) { no_args_allowed_ = no_args_allowed; }
  bool no_args_allowed() const { return no_args_allowed_; }

  std::string program_name() const noexcept { return program_name_; }
  const std::string bbsdir() const noexcept { return bbsdir_; }
  const std::string logdir() const noexcept { return logdir_; }
  const std::string bindir() const noexcept { return bindir_; }

private:
  const std::string program_name_;
  std::string bbsdir_;
  std::string logdir_;
  std::string bindir_;
  bool no_args_allowed_{false};

  bool ParseImpl();
};

} // namespace core
} // namespace wwiv
#endif // __INCLUDED_WWIV_CORE_COMMAND_LINE_H__