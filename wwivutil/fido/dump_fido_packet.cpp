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
#include "wwivutil/fido/dump_fido_packet.h"

#include "core/command_line.h"
#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "sdk/fido/fido_packets.h"
#include "sdk/fido/fido_util.h"
#include "sdk/net/ftn_msgdupe.h"
#include "sdk/net/packets.h"
#include "wwivutil/util.h"

#include <iostream>
#include <string>
#include <vector>

using namespace wwiv::core;
using namespace wwiv::sdk::fido;
using namespace wwiv::sdk::net;
using namespace wwiv::strings;

namespace wwiv::wwivutil::fido {

static std::string fido_attrib_to_string(uint16_t a) {
  std::string s;
  if (a & MSGPRIVATE)
    s += "[PRIVATE]";
  if (a & MSGCRASH)
    s += "[CRASH] ";
  if (a & MSGREAD)
    s += "[READ] ";
  if (a & MSGSENT)
    s += "[SENT] ";
  if (a & MSGFILE)
    s += "[FILE] ";
  if (a & MSGTRANSIT)
    s += "[TRANSIT] ";
  if (a & MSGORPHAN)
    s += "[ORPHAN]";
  if (a & MSGKILL)
    s += "[KILL]";
  if (a & MSGLOCAL)
    s += "[LOCAL]";
  if (a & MSGHOLD)
    s += "[HOLD]";
  if (a & MSGUNUSED)
    s += "[UNUSED]";
  if (a & MSGFREQ)
    s += "[FREQ]";
  if (a & MSGRRREQ)
    s += "[RRREQ]";
  if (a & MSGISRR)
    s += "[ISRR]";
  if (a & MSGAREQ)
    s += "[AREQ]";
  if (a & MSGFUPDREQ)
    s += "[UPDREQ]";

  return s;
}

static int dump_stored_message(const std::string& filename) {
  File f(filename);
  if (!f.Open(File::modeBinary | File::modeReadOnly)) {
    LOG(ERROR) << "Unable to open file: " << filename;
    return 1;
  }

  FidoStoredMessage msg;
  const auto response = read_stored_message(f, msg);
  if (response == ReadPacketResponse::END_OF_FILE) {
    return 0;
  }
  if (response == ReadPacketResponse::ERROR) {
    return 1;
  }

  const auto& h = msg.nh;
  const auto from_address = get_address_from_stored_message(msg);
  const auto dt = fido_to_daten(h.date_time);
  const auto roundtrip_dt = daten_to_fido(dt);

  std::cout << std::dec << "=============================================================================="
       << std::endl;
  std::cout << "cost:    " << h.cost << std::endl;
  std::cout << "to:      " << h.to << "(" << h.dest_zone << ":" << h.dest_net << "/" << h.dest_node
       << "." << h.dest_point << ")" << std::endl;
  std::cout << "from:    " << h.from << "(" << from_address.as_string() << "." << h.orig_point << ")"
       << std::endl;
  std::cout << "subject: " << h.subject << std::endl;
  std::cout << "date:    " << h.date_time << "; [" << roundtrip_dt << "]" << std::endl;
  std::cout << "# read:  " << h.times_read << "; reply_to: " << h.reply_to << std::endl;
  std::cout << "attrib:  " << fido_attrib_to_string(h.attribute);
  std::cout << std::endl;
  std::cout << "text: " << std::endl << std::endl;
  for (const auto ch : FidoToWWIVText(msg.text, false)) {
    dump_char(std::cout, ch);
  }
  std::cout << std::endl;
  return 0;
}

static int dump_packet_file(const std::string& filename) {
  File f(filename);
  if (!f.Open(File::modeBinary | File::modeReadOnly)) {
    LOG(ERROR) << "Unable to open file: " << filename;
    return 1;
  }

  auto done = false;
  packet_header_2p_t header = {};
  auto num_header_read = f.Read(&header, sizeof(packet_header_2p_t));
  if (num_header_read < static_cast<int>(sizeof(packet_header_2p_t))) {
    LOG(ERROR) << "Read less than packet header";
    return 1;
  }

  while (!done) {
    FidoPackedMessage msg;
    auto response = read_packed_message(f, msg);
    if (response == ReadPacketResponse::END_OF_FILE) {
      return 0;
    }
    if (response == ReadPacketResponse::ERROR) {
      return 1;
    }

    auto from_address = get_address_from_packet(msg, header);
    auto dt = fido_to_daten(msg.vh.date_time);
    auto roundtrip_dt = daten_to_fido(dt);

    std::cout << "==[ HEADER ]=================================================================="
         << std::endl;
    auto year = header.year;
    if (year < 1000) {
      year += 1900;
    }
    std::cout << "Packet date: " << (1900 + year) << "-" << header.month + 1 << "-" << header.day
         << std::endl;
    std::cout << "         PW: '" << header.password << "'" << std::endl;
    uint32_t hc{}, mc{};
    if (wwiv::sdk::FtnMessageDupe::GetMessageCrc32s(msg, hc, mc)) {
      std::cout << " Header CRC: '" << std::hex << hc << "'" << std::endl;
      std::cout << "Message CRC: '" << std::hex << mc << "'" << std::endl;
    }
    std::cout << std::dec << "=============================================================================="
         << std::endl;
    std::cout << "   msg_type: " << msg.nh.message_type << std::endl;
    std::cout << "       cost: " << msg.nh.cost << std::endl;
    std::cout << "         to: " << msg.vh.to_user_name << "(" << msg.nh.dest_net << "/"
         << msg.nh.dest_node << ")" << std::endl;
    std::cout << "       from: " << msg.vh.from_user_name << "(" << from_address << ")" << std::endl;
    std::cout << "    subject: " << msg.vh.subject << std::endl;
    std::cout << "       date: " << msg.vh.date_time << "; [" << roundtrip_dt << "]" << std::endl;
    std::cout << "       text: " << std::endl << std::endl;
    for (const auto ch : FidoToWWIVText(msg.vh.text, false)) {
      dump_char(std::cout, ch);
    }
    std::cout << std::endl;
    std::cout << "=============================================================================="
         << std::endl;
  }
  return 0;
}

static int dump_file(const std::string& filename) {
  const auto s = ToStringLowerCase(filename);

  if (ends_with(s, ".msg")) {
    return dump_stored_message(filename);
  }
  if (ends_with(s, ".pkt")) {
    return dump_packet_file(filename);
  }

  std::cout << "Can't tell filetype for: " << filename;
  return 1;
}

std::string DumpFidoPacketCommand::GetUsage() const {
  std::ostringstream ss;
  ss << "Usage:   dump <filename>" << std::endl;
  ss << "Example: dump 02054366.PKT" << std::endl;
  return ss.str();
}

int DumpFidoPacketCommand::Execute() {
  if (remaining().empty()) {
    std::cout << GetUsage() << GetHelp() << std::endl;
    return 2;
  }
  const auto filename(remaining().front());
  return dump_file(filename);
}

bool DumpFidoPacketCommand::AddSubCommands() { return true; }

} 
