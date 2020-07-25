/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*            Copyright (C)2016-2020, WWIV Software Services              */
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
#include "sdk/net/ftn_msgdupe.h"

#include "core/crc32.h"
#include "core/datafile.h"
#include "core/file.h"
#include "core/stl.h"
#include "fmt/printf.h"
#include "sdk/config.h"
#include "sdk/fido/fido_packets.h"
#include "sdk/fido/fido_util.h"
#include "sdk/filenames.h"
#include <cstdint>
#include <ctime>
#include <string>
#include <utility>
#include <vector>

using std::string;
using namespace wwiv::core;
using namespace wwiv::stl;
using namespace wwiv::sdk::fido;
using namespace wwiv::strings;

namespace wwiv::sdk {

FtnMessageDupe::FtnMessageDupe(const Config& config) : FtnMessageDupe(config.datadir(), true) {}

FtnMessageDupe::FtnMessageDupe(std::string datadir, bool use_filesystem)
    : datadir_(std::move(datadir)), use_filesystem_(use_filesystem) {
  if (!datadir_.empty()) {
    initialized_ = Load();
  } else {
    initialized_ = false;
  }
}

bool FtnMessageDupe::Load() {
  if (!use_filesystem_) {
    return true;
  }
  DataFile<msgids> file(FilePath(datadir_, MSGDUPE_DAT),
                        File::modeReadWrite | File::modeBinary | File::modeCreateFile);
  if (!file) {
    LOG(ERROR) << "Unable to initialize FtnMessageDupe: Unable to create file.";
    return false;
  }

  const auto num_records = file.number_of_records();
  if (num_records == 0) {
    // nothing to read.
    return true;
  }
  if (!file.ReadVector(dupes_)) {
    LOG(ERROR) << "Unable to initialize FtnMessageDupe: Read Failed";
    return false;
  }
  for (const auto& d : dupes_) {
    header_dupes_.insert(d.header);
    msgid_dupes_.insert(d.msgid);
  }
  return true;
}

bool FtnMessageDupe::Save() {
  if (!use_filesystem_) {
    return true;
  }
  DataFile<msgids> file(FilePath(datadir_, MSGDUPE_DAT),
                        File::modeReadWrite | File::modeBinary | File::modeCreateFile |
                            File::modeTruncate);
  if (!file) {
    return false;
  }
  return file.WriteVector(dupes_);
}

std::string FtnMessageDupe::CreateMessageID(const wwiv::sdk::fido::FidoAddress& a) {
  if (!initialized_) {
    string address_string;
    if (a.point() != 0) {
      address_string = to_zone_net_node_point(a);
    } else {
      address_string = to_zone_net_node(a);
    }
    return StrCat(address_string, " DEADBEEF");
  }

  DataFile<uint64_t> file(FilePath(datadir_, MSGID_DAT),
                          File::modeReadWrite | File::modeBinary | File::modeCreateFile,
                          File::shareDenyReadWrite);
  if (!file) {
    throw std::runtime_error(StrCat("Unable to open file: ", file.file()));
  }
  const uint64_t now = time(nullptr);
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
  return fmt::sprintf("%s %08X", address_string, msg_num);
}

// static
std::string FtnMessageDupe::GetMessageIDFromText(const std::string& text) {
  static const string kMSGID = "MSGID: ";
  auto lines = wwiv::sdk::fido::split_message(text);
  for (const auto& line : lines) {
    if (line.empty() || line.front() != '\001' || line.size() < 2) {
      continue;
    }
    auto s = line.substr(1);
    if (starts_with(s, kMSGID)) {
      // Found the message ID, mail here.
      return StringTrim(s.substr(kMSGID.size()));
    }
  }
  return "";
}

// static
std::string FtnMessageDupe::GetMessageIDFromWWIVText(const std::string& text) {
  static const string kMSGID = "0MSGID: ";
  auto lines = wwiv::sdk::fido::split_message(text);
  for (const auto& line : lines) {
    if (line.empty() || line.front() != '\004' || line.size() < 2) {
      continue;
    }
    auto s = line.substr(1);
    if (starts_with(s, kMSGID)) {
      // Found the message ID, mail here.
      return StringTrim(s.substr(kMSGID.size()));
    }
  }
  return "";
}

// static
bool FtnMessageDupe::GetMessageCrc32s(const wwiv::sdk::fido::FidoPackedMessage& msg,
                                      uint32_t& header_crc32, uint32_t& msgid_crc32) {
  std::ostringstream s;
  s << msg.nh.orig_net << "/" << msg.nh.orig_node << "\r\n";
  s << msg.nh.dest_net << "/" << msg.nh.dest_node << "\r\n";
  s << msg.vh.date_time << "\r\n";
  s << msg.vh.from_user_name << "\r\n";
  s << msg.vh.subject << "\r\n";
  s << msg.vh.to_user_name << "\r\n";

  header_crc32 = crc32string(s.str());
  const auto msgid = FtnMessageDupe::GetMessageIDFromText(msg.vh.text);
  msgid_crc32 = crc32string(msgid);
  return true;
}

bool FtnMessageDupe::add(const FidoPackedMessage& msg) {
  uint32_t header_crc32 = 0;
  uint32_t msgid_crc32 = 0;

  if (!GetMessageCrc32s(msg, header_crc32, msgid_crc32)) {
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
  for (auto it = dupes_.begin(); it != std::end(dupes_);) {
    const auto& d = *it;
    if (d.header == header_crc32 && d.msgid == msgid_crc32) {
      dupes_.erase(it);
      header_dupes_.erase(header_crc32);
      msgid_dupes_.erase(msgid_crc32);
      return Save();
    }
    ++it;
  }
  return false;
}

bool FtnMessageDupe::is_dupe(uint32_t header_crc32, uint32_t msgid_crc32) const {
  if (contains(header_dupes_, header_crc32) && header_crc32 != 0) {
    return true;
  }
  if (contains(msgid_dupes_, msgid_crc32) && msgid_crc32 != 0) {
    return true;
  }
  return false;
}

bool FtnMessageDupe::is_dupe(const FidoPackedMessage& msg) const {
  uint32_t header_crc32 = 0;
  uint32_t msgid_crc32 = 0;

  if (!GetMessageCrc32s(msg, header_crc32, msgid_crc32)) {
    return false;
  }
  return is_dupe(header_crc32, msgid_crc32);
}

} // namespace wwiv
