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
#include "sdk/fido/fido_packets.h"

using std::cout;
using std::endl;
using std::string;
using wwiv::core::CommandLineCommand;
using namespace wwiv::sdk::fido;
using namespace wwiv::strings;

namespace wwiv {
namespace wwivutil {
namespace fido {

static std::string FidoToWWIVText(const std::string& ft) {
  std::string wt;
  for (auto& c : ft) {
    if (c == 13) {
      wt.push_back(13);
      wt.push_back(10);
    } else if (c == 10) {
      // NOP
    } else {
      wt.push_back(c);
    }
  }
  return wt;
}

int dump_file(const std::string& filename) {
  File f(filename);
  if (!f.Open(File::modeBinary | File::modeReadOnly)) {
    LOG(ERROR) << "Unable to open file: " << filename;
    return 1;
  }

  bool done = false;
  packet_header_2p_t header = {};
  int num_header_read = f.Read(&header, sizeof(packet_header_2p_t));
  if (num_header_read < sizeof(packet_header_2p_t)) {
    LOG(ERROR) << "Read less than packet header";
    return 1;
  }
  while (!done) {
    FidoPackedMessage msg;
    ReadPacketResponse response = read_packed_message(f, msg);
    if (response == ReadPacketResponse::END_OF_FILE) {
      return 0;
    } else if (response == ReadPacketResponse::ERROR) {
      return 1;
    }

    cout << "msg_type:" << msg.nh.message_type << std::endl;
    cout << "cost:    " << msg.nh.cost << std::endl;
    cout << "to:      " << msg.vh.to_user_name << "(" << msg.nh.dest_net << "/" << msg.nh.dest_node << ")" << std::endl;
    cout << "from:    " << msg.vh.from_user_name << "(" << msg.nh.orig_net << "/" << msg.nh.orig_node << ")" << std::endl;
    cout << "subject: " << msg.vh.subject << std::endl;
    cout << "date:    " << msg.vh.date_time << std::endl;
    cout << "text: " << std::endl << std::endl << FidoToWWIVText(msg.vh.text) << std::endl;
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
