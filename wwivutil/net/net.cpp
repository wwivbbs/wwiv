/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2016 WWIV Software Services              */
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
#include "wwivutil/net/net.h"

#include <cstdio>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include "core/command_line.h"
#include "core/file.h"
#include "core/strings.h"
#include "sdk/config.h"
#include "sdk/net.h"
#include "sdk/networks.h"

#include "wwivutil/net/dump_callout.h"
#include "wwivutil/net/dump_contact.h"
#include "wwivutil/net/dump_packet.h"

using std::cerr;
using std::clog;
using std::cout;
using std::endl;
using std::setw;
using std::string;
using std::unique_ptr;
using std::vector;
using wwiv::core::BooleanCommandLineArgument;
using namespace wwiv::sdk;

namespace wwiv {
namespace wwivutil {

bool NetCommand::AddSubCommands() {
  DumpPacketCommand* dump_packet = new DumpPacketCommand();
  add(dump_packet);

  DumpCalloutCommand* dump_callout = new DumpCalloutCommand();
  add(dump_callout);

  DumpContactCommand* dump_contact = new DumpContactCommand();
  add(dump_contact);
  return true;
}


}  // namespace wwivutil
}  // namespace wwiv
