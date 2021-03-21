/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2021, WWIV Software Services             */
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
/**************************************************************************/
#include "wwivutil/help.h"

#include "core/command_line.h"
#include "core/file.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/format.h"
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using wwiv::core::BooleanCommandLineArgument;
using namespace wwiv::sdk;
using namespace wwiv::strings;

namespace wwiv::wwivutil {

HelpCommand::HelpCommand()
  : UtilCommand("help", "Displays help on wwivutil and commands.") {
}

[[nodiscard]] std::string HelpCommand::GetUsage() const {
  std::ostringstream ss;
  ss << "Usage: " << std::endl << std::endl;
  ss << "  help : Displays WWIVutil help." << std::endl << std::endl;
  return ss.str();
}

// ReSharper disable once CppMemberFunctionMayBeStatic
std::string HelpCommand::LongHelpForArg(const core::CommandLineArgument& c, int max_len) {
  const auto md = markdown();
  std::ostringstream ss;
  if (md) {
    ss << "| ";
  }
  if (c.key_ != 0) {
    ss << "-" << c.key_ << " ";
  } else {
    ss << "   ";
  }
  ss << "--";
  if (!md) {
    ss << std::left << std::setw(max_len);
  }
  ss << c.name_ << "  ";
  if (md) {
    ss << " | ";
  }
  if (c.is_boolean) {
    ss << "[boolean] ";
  }
  ss << c.help_text() << std::endl;
  return ss.str();
}

std::string HelpCommand::HelpForCommand(const CommandLineCommand& c,
    const std::string& cmdpath) {
  if (barg("full") || markdown()) {
    return FullHelpForCommand(c, cmdpath);
  }
  return ShortHelpForCommand(c, cmdpath);
}


std::string HelpCommand::FullHelpForCommand(const CommandLineCommand& uc,
                                            const std::string& cmdpath) {
  std::ostringstream ss;
  const auto md = markdown();
  if (md) {
    ss << "## ";
  }
  ss << cmdpath << std::endl << std::endl;
  ss << uc.help_text() << std::endl << std::endl;
  if (!uc.args_allowed().empty()) {
    if (md) {
      ss << std::endl;
      ss << "| Arg     | Description |" << std::endl;
      ss << "| ------- | ----------- |" << std::endl;
    } else {
      ss << "Args:" << std::endl;
    }
    for (const auto& [_, c] : uc.args_allowed()) {
      ss << LongHelpForArg(c, 15);
    }
    if (md) {
      ss << "\r\n<br/>\r\n";
    }
  }
  ss << std::endl;
  ss << std::endl;
  if (!uc.commands_allowed().empty()) {
    for (const auto& [_, c] : uc.commands_allowed()) {
      auto full_command_name = fmt::format("{} {}", cmdpath, c->name());
      ss << FullHelpForCommand(*c, full_command_name);
      ss << std::endl;
    }
  }
  return ss.str();
}

// ReSharper disable once CppMemberFunctionMayBeStatic
std::string HelpCommand::ShortHelpForCommand(const CommandLineCommand& c,
                                             const std::string& cmdpath) {
  std::ostringstream ss;
  ss << cmdpath << std::endl;
  ss << c.GetUsage() << std::endl;
  ss << c.GetHelp() << std::endl;
  return ss.str();
}

bool HelpCommand::ShowHelpForPath(std::vector<std::string> path, const core::CommandLineCommand* c) {
  const auto e = std::end(path);
  if (path.empty()) {
    return false;
  }
  auto cmdpath = fmt::format("wwivutil {}", path.front());
  for (auto b = std::begin(path) + 1; b != e; ++b) {
    if (const auto it = c->commands_allowed().find(ToStringLowerCase(*b));
        it != std::end(c->commands_allowed())) {
      cmdpath += " ";
      cmdpath += it->first;
      c = it->second.get();
    }
  }

  std::cout << HelpForCommand(*c, cmdpath);
  std::cout << std::endl << std::endl;
  return true;
}

int HelpCommand::Execute() {
  // note that our docs is generated by running the following command:
  // wwivutil help  --format=markdown > wwivutil.md
  if (markdown()) {
    std::cout << R"(
# WWIVutil Help

WWIVutil is WWIV's Swiss Army Knife utilty program.  It handles many different
types of commands for WWIV.  It's also replaces the 4.x FIX command.

Here's documentation for the various commands.


## Table of Contents

[TOC]


)";
  } else {
    std::cout << "WWIVutil Help" << std::endl;
    std::cout << std::endl;
  }

  if (!remaining().empty()) {
    // We have a command:
    const auto& cmdname = remaining().front();
    for (const CommandLineCommand* c : config()->subcommands()) {
      if (iequals(cmdname, c->name())) {
        // Should we walk?
        if (ShowHelpForPath(remaining(), c)) {
          return 0;
        }
      }
    }
  }

  // no command name found, displaying help for everything.
  for (const auto* c : config()->subcommands()) {
    const auto cmdpath = fmt::format("wwivutil {}", c->name());
    std::cout << HelpForCommand(*c, cmdpath);
    std::cout << std::endl << std::endl;
  }
  return 0;
}

bool HelpCommand::markdown() const {
  return iequals(sarg("format"), "markdown");
}

bool HelpCommand::AddSubCommands() {
  add_argument(BooleanCommandLineArgument("full", "Display long format for help.", false));
  // TODO: implement format.
  add_argument({"format", "The format of the help to use (text | markdown)", "text"});
  return true;
}

} // namespace wwiv::wwivutil
