/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2022, WWIV Software Services             */
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
#include "wwivutil/fido/fido.h"

#include "core/command_line.h"
#include "sdk/config.h"
#include "wwivutil/fido/dump_fido_packet.h"
#include "wwivutil/fido/dump_fido_subscribers.h"
#include "wwivutil/fido/dump_nodelist.h"

#include <memory>

using wwiv::core::BooleanCommandLineArgument;
using namespace wwiv::sdk;

namespace wwiv::wwivutil::fido {

bool FidoCommand::AddSubCommands() {
  add(std::make_unique<DumpFidoPacketCommand>());
  add(std::make_unique<DumpNodelistCommand>());
  add(std::make_unique<DumpFidoSubscribersCommand>());
  return true;
}


}  // namespace

