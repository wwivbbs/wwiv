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
#include "sdk/net/net.h"
#include "wwivutil/net/net.h"

#include "core/command_line.h"
#include "core/file.h"
#include "wwivutil/net/dump_bbsdata.h"
#include "wwivutil/net/dump_callout.h"
#include "wwivutil/net/dump_connect.h"
#include "wwivutil/net/dump_contact.h"
#include "wwivutil/net/dump_packet.h"
#include "wwivutil/net/dump_subscribers.h"
#include "wwivutil/net/list.h"
#include "wwivutil/net/req.h"
#include "wwivutil/net/send.h"

#include <memory>

using wwiv::core::BooleanCommandLineArgument;
using namespace wwiv::sdk;

namespace wwiv::wwivutil {

bool NetCommand::AddSubCommands() {
  add(std::make_unique<DumpPacketCommand>());
  add(std::make_unique<DumpCalloutCommand>());
  add(std::make_unique<DumpConnectCommand>());
  add(std::make_unique<DumpContactCommand>());
  add(std::make_unique<DumpBbsDataCommand>());
  add(std::make_unique<wwiv::wwivutil::net::DumpSubscribersCommand>());
  add(std::make_unique<SubReqCommand>());
  add(std::make_unique<NetListCommand>());
  add(std::make_unique<net::SubSendCommand>());
  return true;
}


}  // namespace
