/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*            Copyright (C)2016-2017, WWIV Software Services              */
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
#include "sdk/ftn_msgdupe.h"

#include <algorithm>
#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

#include "core/crc32.h"
#include "core/datafile.h"
#include "core/file.h"
#include "core/stl.h"
#include "sdk/config.h"
#include "sdk/fido/fido_packets.h"
#include "sdk/fido/fido_util.h"
#include "sdk/filenames.h"
#include "sdk/vardec.h"

using std::string;
using namespace wwiv::core;
using namespace wwiv::stl;
using namespace wwiv::sdk::fido;
using namespace wwiv::strings;

namespace wwiv {
namespace sdk {

FtnMessageDupe::FtnMessageDupe(const Config& config) : FtnMessageDupe(config.datadir()) {}

FtnMessageDupe::FtnMessageDupe(const std::string& datadir) : datadir_(datadir) {
  if (!datadir_.empty()) {
    initialized_ = Load();
  } else {
    initialized_ = false;
  }
}

bool FtnMessageDupe::Load() {
  DataFile<msgids> file(datadir_, MSGDUPE_DAT,
                        File::modeReadWrite | File::modeBinary | File::modeCreateFile);
  if (!file) {
    LOG(ERROR) << "Unable to initialize FtnDupe: Unable to create file.";
    return false;
  }

  const int num_records = file.number_of_records();
  if (num_records == 0) {
    // nothing to read.
    return true;
  }
  if (!file.ReadVector(dupes_)) {
    LOG(ERROR) << "Unable to initialize FtnDupe: Read Failed";
    return false;
  }
  for (const auto& d : dupes_) {
    header_dupes_.insert(d.header);
    msgid_dupes_.insert(d.msgid);
  }
  return true;
}

bool FtnMessageDupe::Save() {
  DataFile<msgids> file(datadir_, MSGDUPE_DAT,
                        File::modeReadWrite | File::modeBinary | File::modeCreateFile |
                            File::modeTruncate);
  if (!file) {
    return false;
  }
  return file.WriteVector(dupes_);
}

const std::string FtnMessageDupe::CreateMessageID(const wwiv::sdk::fido::FidoAddress& a) {
  if (!initialized_) {
    string address_string;
    if (a.point() != 0) {
      address_string = to_zone_net_node_point(a);
    } else {
      address_string = to_zone_net_node(a);
    }
    return StrCat(address_string, " DEADBEEF");
  }

  DataFile<uint64_t> file(datadir_, MSGID_DAT,
                          File::modeReadWrite | File::modeBinary | File::modeCreateFile,
                          File::shareDenyReadWrite);
  if (!file) {
    throw std::runtime_error("Unable to open file: " + file.file().full_pathname());
  }
  uint64_t now = time(nullptr);
  uint64_t msg_num;
  if (!file.Read(0, &msg_num)) {
    msg_num = now;
  }
  if (msg_num < now) {
    // We always want to be at least equal to the current time.
    msg_num = now;
  }
  ++msg_num;
  file.file().Seek(0, File::Whence::begin);
  file.Write(0, &msg_num);

  string address_string;

  if (a.point() != 0) {
    address_string = to_zone_net_node_point(a);
  } else {
    address_string = to_zone_net_node(a);
  }
  return StringPrintf("%s %08X", address_string.c_str(), msg_num);
}

// static
std::string FtnMessageDupe::GetMessageIDFromText(const std::string& text) {
  static const string kMSGID = "MSGID: ";
  std::vector<std::string> lines = wwiv::sdk::fido::split_message(text);
  for (const auto& line : lines) {
    if (line.empty() || line.front() != '\001' || line.size() < 2) {
      continue;
    }
    auto s = line.substr(1);
    if (starts_with(s, kMSGID)) {
      // Found the message ID, mail here.
      auto msgid = s.substr(kMSGID.size());
      StringTrim(&msgid);
      return msgid;
    }
  }
  return "";
}

// static
std::string FtnMessageDupe::GetMessageIDFromWWIVText(const std::string& text) {
  static const string kMSGID = "0MSGID: ";
  std::vector<std::string> lines = wwiv::sdk::fido::split_message(text);
  for (const auto& line : lines) {
    if (line.empty() || line.front() != '\004' || line.size() < 2) {
      continue;
    }
    auto s = line.substr(1);
    if (starts_with(s, kMSGID)) {
      // Found the message ID, mail here.
      auto msgid = s.substr(kMSGID.size());
      StringTrim(&msgid);
      return msgid;
    }
  }
  return "";
}

static bool crc32(const FidoPackedMessage& msg, uint32_t& header_crc32, uint32_t& msgid_crc32) {
  std::ostringstream s;
  s << msg.nh.orig_net << "/" << msg.nh.orig_node << "\r\n";
  s << msg.nh.dest_net << "/" << msg.nh.dest_node << "\r\n";
  s << msg.vh.date_time << "\r\n";
  s << msg.vh.from_user_name << "\r\n";
  s << msg.vh.subject << "\r\n";
  s << msg.vh.to_user_name << "\r\n";

  header_crc32 = crc32string(s.str());
  string msgid = FtnMessageDupe::GetMessageIDFromText(msg.vh.text);
  msgid_crc32 = crc32string(msgid);
  return true;
}

uint64_t as64(uint32_t header, uint32_t msgid) {
  return (static_cast<uint64_t>(header) << 32) | msgid;
}

bool FtnMessageDupe::add(const FidoPackedMessage& msg) {
  uint32_t header_crc32 = 0;
  uint32_t msgid_crc32 = 0;

  if (!crc32(msg, header_crc32, msgid_crc32)) {
    return false;
  }

  return add(header_crc32, msgid_crc32);
}

bool FtnMessageDupe::add(uint32_t header_crc32, uint32_t msgid_crc32) {
  if (header_crc32 != 0) {
    header_dupes_.insert(header_crc32);
  }
  if (msgid_crc32 != 0) {
    msgid_dupes_.insert(msgid_crc32);
  }

  msgids ids{};
  ids.header = header_crc32;
  ids.msgid = msgid_crc32;

  dupes_.emplace_back(ids);
  return Save();
}

bool FtnMessageDupe::remove(uint32_t header_crc32, uint32_t msgid_crc32) {
  bool found = false;
  for (auto it = dupes_.begin(); it != std::end(dupes_);) {
    const auto& d = *it;
    if (d.header == header_crc32 && d.msgid == msgid_crc32) {
      dupes_.erase(it);
      found = true;
      break;
    } else {
      it++;
    }
  }
  if (!found) {
    return false;
  }
  return Save();
}

bool FtnMessageDupe::is_dupe(uint32_t header_crc32, uint32_t msgid_crc32) const {
  if (contains(header_dupes_, header_crc32)) {
    return true;
  }
  if (contains(msgid_dupes_, msgid_crc32)) {
    return true;
  }
  return false;
}

bool FtnMessageDupe::is_dupe(const wwiv::sdk::fido::FidoPackedMessage& msg) const {
  uint32_t header_crc32 = 0;
  uint32_t msgid_crc32 = 0;

  if (!crc32(msg, header_crc32, msgid_crc32)) {
    return false;
  }
  return is_dupe(header_crc32, msgid_crc32);
}

} // namespace sdk
} // namespace wwiv
