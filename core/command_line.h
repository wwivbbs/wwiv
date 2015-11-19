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
#ifndef __INCLUDED_WWIV_CORE_COMMAND_LINE_H__
#define __INCLUDED_WWIV_CORE_COMMAND_LINE_H__
#pragma once

#include <map>
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
 * cmdline.add({"name", 'n', "Sets Name"});
 * cmdline.add({"location", 'l', "Sets Location"});
 * 
 * CommandLineCommand& bar = cmdline.add_command("bar", "Bar Commands");
 * bar.add({"barname", "Sets Bar Name"});
 * CommandLineCommand& baz = cmdline.add_command("baz", "Baz Commands");
 * baz.add({"bazname", "Sets Baz Name"});
 * cmdLine.Parse();
 *
 * const string name = cmdline.arg("name").as_string();
 * CommandLineCommand cmd = cmdline.command();
 * const string command_name = cmd->name();
 * const string bazname = cmd->arg("bazname").as_string();
 */


namespace wwiv {
namespace core {

struct unknown_argument_error: public std::runtime_error {
  unknown_argument_error(const std::string& message): std::runtime_error(message) {}
};

class CommandLineValue {
public:
  CommandLineValue(): default_(true) {}
  explicit CommandLineValue(const std::string& s)
    : value_(s), default_(false) {}
  CommandLineValue(const std::string& s, bool default_value)
    : value_(s), default_(default_value) {}

  const std::string as_string() const { return value_; }
  const int as_int() const {
    try {
      return std::stoi(value_); 
    } catch (std::exception&) {
      return 0;
    }
  }
  const bool as_bool() const {
    return value_ == "true";
  }
private:
  const std::string value_;
  const bool default_ = true;
};

class CommandLineArgument {
public:
  CommandLineArgument(
    const std::string& name, char key,
    const std::string& help_text, const std::string& default_value)
    : name(name), key(key), help_text(help_text), default_value(default_value) {}
  CommandLineArgument(
    const std::string& name,const std::string& help_text, const std::string& default_value)
    : CommandLineArgument(name, 0, help_text, default_value) {}
  CommandLineArgument(const std::string& name, char key, const std::string& help_text)
    : CommandLineArgument(name, key, help_text, "") {}
  CommandLineArgument(const std::string& name, const std::string& help_text)
    : CommandLineArgument(name, 0, help_text, "") {}
  const std::string name;
  const char key = 0;
  const std::string help_text;
  const std::string default_value;
  bool is_boolean = false;
};

class BooleanCommandLineArgument : public CommandLineArgument {
public:
  BooleanCommandLineArgument(
    const std::string& name, char key,
    const std::string& help_text, bool default_value)
    : CommandLineArgument(name, key, help_text, default_value ? "true" : "false") {
    is_boolean = true; 
  }
};

class CommandLineCommand {
public:
  CommandLineCommand(const std::string& name, const std::string& help_text, int argc, char** argv,
      const std::string dot_argument);
  bool add(const CommandLineArgument& cmd);
  bool add(const CommandLineCommand& cmd) { commands_allowed_.emplace(cmd.name(), cmd); return true; }
  CommandLineCommand& add_command(const std::string& name) {
    return add_command(name, "");
  }
  CommandLineCommand& add_command(const std::string& name, const std::string& help_text) {
    commands_allowed_.emplace(name, CommandLineCommand(name, help_text, argc_, argv_, dot_argument_));
    return commands_allowed_.at(name);
  }

  bool subcommand_selected() const { return command_ != nullptr; }
  const std::string name() const { return name_; }
  const std::string help_text() const { return help_text_; }

  const CommandLineValue arg(const std::string name) const { return args_.at(name); }
  const CommandLineCommand* command() const { return command_; }
  std::vector<std::string> remaining() const { return remaining_; }
  std::string ToString() const;
  virtual std::string GetHelp() const;

protected:
  bool HandleCommandLineArgument(const std::string& key, const std::string& value);
  int Parse(int start_pos);
  int argc_ = 0;

  // Values as allowed to be specified on the commandline.
  std::map<const std::string, CommandLineArgument> args_allowed_;
  std::map<const std::string, CommandLineCommand> commands_allowed_;
  std::vector<std::string> remaining_;

private:
  // Values as entered on the commandline.
  std::map<std::string, CommandLineValue> args_;
  CommandLineCommand* command_ = nullptr;
  const std::string name_;
  const std::string help_text_;
  char** argv_ = nullptr;
  const std::string dot_argument_;
};

class CommandLine : public CommandLineCommand {
public:
  CommandLine(int argc, char** argv, const std::string dot_argument);
  bool Parse() { return CommandLineCommand::Parse(1) >= CommandLineCommand::argc_; }
  virtual std::string GetHelp() const override;
private:
  const std::string program_name_;
};


}  // namespace core
}  // namespace wwiv
#endif  // __INCLUDED_WWIV_CORE_COMMAND_LINE_H__