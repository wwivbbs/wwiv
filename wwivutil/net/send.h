/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                  Copyright (C)2020, WWIV Software Services             */
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
#ifndef __INCLUDED_WWIVUTIL_SEND_H__
#define __INCLUDED_WWIVUTIL_SEND_H__

#include "wwivutil/command.h"
#include <string>

namespace wwiv {
namespace wwivutil {
namespace net {

class SubSendCommand final: public UtilCommand {
public:
  // (from autosend.doc): AUTOSEND <sub type> <node> <.net> [num msgs]
  SubSendCommand()
    : UtilCommand("send", "<sub type> <node> <.net> [num msgs]") {}
  int Execute() override;
  [[nodiscard]] std::string GetUsage() const override;
  bool AddSubCommands() override;
};

}  // namespace net
}  // namespace wwivutil
}  // namespace wwiv

#endif  // __INCLUDED_WWIVUTIL_SEND_H__
