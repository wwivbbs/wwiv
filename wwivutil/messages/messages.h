/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.0x                             */
/*             Copyright (C)2015-2016, WWIV Software Services             */
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
#ifndef __INCLUDED_WWIVUTIL_MESSAGES_H__
#define __INCLUDED_WWIVUTIL_MESSAGES_H__

#include "wwivutil/command.h"

namespace wwiv {
namespace wwivutil {

class MessagesCommand: public UtilCommand {
public:
  MessagesCommand(): UtilCommand("messages", "WWIV message base commands.") {}
  virtual ~MessagesCommand() {}
  bool AddSubCommands() override final;
};

class MessagesDumpHeaderCommand: public UtilCommand {
public:
  MessagesDumpHeaderCommand();
  virtual ~MessagesDumpHeaderCommand() {}
  std::string GetUsage() const override final;
  int Execute() override final;
  bool AddSubCommands() override final;

protected:
  int ExecuteImpl(
    const std::string& basename, const std::string& subs_dir,
    const std::string& msgs_dir,
    const std::vector<net_networks_rec>& net_networks,
    int start, int end, bool all);
};

}  // namespace wwivutil
}  // namespace wwiv


#endif  // __INCLUDED_WWIVUTIL_MESSAGES_H__
