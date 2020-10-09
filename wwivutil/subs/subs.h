/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5                              */
/*             Copyright (C)2015-2020, WWIV Software Services             */
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
#ifndef __INCLUDED_WWIVUTIL_SUBS_H__
#define __INCLUDED_WWIVUTIL_SUBS_H__

#include "sdk/subxtr.h"
#include "sdk/msgapi/message_api.h"
#include "wwivutil/command.h"
#include <memory>

namespace wwiv::wwivutil {

//class BaseMessagesSubCommand : public UtilCommand {
//public:
//  BaseMessagesSubCommand(const std::string& name, const std::string& descr)
//      : UtilCommand(name, descr) {}
//  virtual ~BaseMessagesSubCommand();
//  bool CreateMessageApiMap(const std::string& basename);
//  const std::string& basename() const noexcept { return basename_; }
//  const wwiv::sdk::subboard_t& sub() const noexcept { return sub_; }
//  // N.B. if this doesn't exist this message will crash.
//  wwiv::sdk::msgapi::MessageApi& api() noexcept {
//    return *apis_.at(sub_.storage_type).get();
//  }
//
//protected:
//  std::map<int, std::unique_ptr<wwiv::sdk::msgapi::MessageApi>> apis_;
//  std::unique_ptr<wwiv::sdk::Subs> subs_;
//  wwiv::sdk::subboard_t sub_;
//
//private:
//  std::string basename_;
//};

class SubsCommand: public UtilCommand {
public:
  SubsCommand(): UtilCommand("subs", "WWIV subboard commands.") {}
  virtual ~SubsCommand() {}
  bool AddSubCommands() override final;
};


}  // namespace wwiv::wwivutil


#endif  // __INCLUDED_WWIVUTIL_SUBS_H__
