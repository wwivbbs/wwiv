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
#include "wwivutil/net/dump_packet.h"

#include "core/command_line.h"
#include "core/datetime.h"
#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "sdk/net/net.h"
#include "sdk/net/packets.h"
#include "wwivutil/util.h"

#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using wwiv::core::CommandLineCommand;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::net;
using namespace wwiv::strings;

namespace wwiv::wwivutil {

int dump_file(const std::string& filename) {
  File f(filename);
  if (!f.Open(File::modeBinary | File::modeReadOnly)) {
    LOG(ERROR) << "Unable to open file: " << filename;
    return 1;
  }

  auto current{0};
  for (;;) {
    auto [packet, response] = read_packet(f, true);
    if (response == ReadPacketResponse::END_OF_FILE) {
      return 0;
    }
    if (response == ReadPacketResponse::ERROR) {
      return 1;
    }

    std::cout << "Header for Packet Index Number: #" << std::setw(5) << std::left << current++ << std::endl;
    std::cout << "=============================================================================="
         << std::endl;

    std::cout << " destination: " << packet.nh.touser << "@" << packet.nh.tosys << std::endl;
    std::cout << "        from: " << packet.nh.fromuser << "@" << packet.nh.fromsys << std::endl;
    std::cout << "        type: (" << main_type_name(packet.nh.main_type);
    if (packet.nh.main_type == main_type_net_info) {
      std::cout << "/" << net_info_minor_type_name(packet.nh.minor_type);
    } else if (packet.nh.main_type > 0) {
      std::cout << "/" << packet.nh.minor_type;
    }
    std::cout << ")" << std::endl;
    if (packet.nh.list_len > 0) {
      std::cout << "    list_len: " << packet.nh.list_len << std::endl;
    }
    std::cout << "       daten: " << daten_to_wwivnet_time(packet.nh.daten) << std::endl;
    std::cout << "      length: " << packet.nh.length << std::endl;
    if (packet.nh.method > 0) {
      std::cout << "compression: de" << packet.nh.method << std::endl;
    }

    if (packet.nh.list_len > 0) {
      // read list of addresses.
      std::cout << "System List: ";
      for (const auto item : packet.list) {
        std::cout << item << " ";
      }
      std::cout << std::endl;
    }
    if (packet.nh.length) {
      std::cout << "=============================================================================="
           << std::endl;
      std::cout << "Raw Packet Text:" << std::endl;
      std::cout << "=============================================================================="
           << std::endl;
      for (const auto ch : packet.text()) {
        dump_char(std::cout, ch);
      }
      std::cout << std::endl << std::endl;
    }
    std::cout << "=============================================================================="
         << std::endl;
  }
}

std::string DumpPacketCommand::GetUsage() const {
  std::ostringstream ss;
  ss << "Usage:   dump <filename>" << std::endl;
  ss << "Example: dump s1.net" << std::endl;
  return ss.str();
}

int DumpPacketCommand::Execute() {
  if (remaining().empty()) {
    std::cout << GetUsage() << GetHelp() << std::endl;
    return 2;
  }
  const std::string filename(remaining().front());
  return dump_file(filename);
}

bool DumpPacketCommand::AddSubCommands() {
  return true;
}


}
