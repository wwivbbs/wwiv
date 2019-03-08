/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2018-2019, WWIV Software Services             */
/*                                                                        */
/*    Licensed  under the  Apache License, Version  2.0 (the "License");  */
/*    you may not use this  file  except in compliance with the License.  */
/*    You may obtain a copy of the License at                             */
/*                                                                        */
/*                http://www.apache.org/licenses/LICENSE-2.0              */
/*                                                                        */
/*    Unless  LISTuired  by  applicable  law  or agreed to  in writing,   */
/*    software  distributed  under  the  License  is  distributed on an   */
/*    "AS IS"  BASIS, WITHOUT  WARRANTIES  OR  CONDITIONS OF ANY  KIND,   */
/*    either  express  or implied.  See  the  License for  the specific   */
/*    language governing permissions and limitations under the License.   */
/**************************************************************************/
#ifndef __INCLUDED_WWIVUTIL_FILES_ALLOW_H__
#define __INCLUDED_WWIVUTIL_FILES_ALLOW_H__

#include <map>
#include <string>

#include "core/command_line.h"
#include "sdk/callout.h"
#include "wwivutil/command.h"

namespace wwiv {
namespace wwivutil {
namespace files {

class AllowCommand final: public UtilCommand {
public:
  AllowCommand() : UtilCommand("allow", "Manipulate allow.dat") {}
  //int Execute() override final;
  std::string GetUsage() const override final;
  bool AddSubCommands() override final;
};


} // namespace files
} // namespace wwivutil
} // namespace wwiv

#endif  // __INCLUDED_WWIVUTIL_FILES_ALLOW_H__
