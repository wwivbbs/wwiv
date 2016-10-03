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
#include "wwivutil/fido/dump_fido_packet.h"

#include <iostream>
#include <string>
#include <vector>
#include "core/command_line.h"
#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "networkb/net_util.h"
#include "networkb/packets.h"
#include "sdk/net.h"

using std::cout;
using std::endl;
using std::string;
using wwiv::core::CommandLineCommand;
using namespace wwiv::net;
using namespace wwiv::strings;

namespace wwiv {
namespace wwivutil {
namespace fido {

static string daten_to_humantime(uint32_t daten) {
  time_t t = static_cast<time_t>(daten);
  string human_date = string(asctime(localtime(&t)));
  StringTrimEnd(&human_date);
  return human_date;
}

int dump_file(const std::string& filename) {
  File f(filename);
  if (!f.Open(File::modeBinary | File::modeReadOnly)) {
    LOG(ERROR) << "Unable to open file: " << filename;
    return 1;
  }

  bool done = false;
  while (!done) {
    Packet packet;
    ReadPacketResponse response = read_packet(f, packet);
    if (response == ReadPacketResponse::END_OF_FILE) {
      return 0;
    } else if (response == ReadPacketResponse::ERROR) {
      return 1;
    }

    //    cout << "destination: " << packet.nh.touser << "@" << packet.nh.tosys << endl;
    cout << "==============================================================================" << endl;
  }
  return 0;
}

std::string DumpFidoPacketCommand::GetUsage() const {
  std::ostringstream ss;
  ss << "Usage:   dump <filename>" << endl;
  ss << "Example: dump 02054366.PKT" << endl;
  return ss.str();
}

int DumpFidoPacketCommand::Execute() {
  if (remaining().empty()) {
    cout << GetUsage() << GetHelp() << endl;
    return 2;
  }
  const string filename(remaining().front());
  return dump_file(filename);
}

bool DumpFidoPacketCommand::AddSubCommands() {
  return true;
}

}
}
}
