/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*               Copyright (C)2016 WWIV Software Services                 */
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
#include <string>
#include <vector>

#include "core/crc32.h"
#include "core/datafile.h"
#include "core/file.h"
#include "core/stl.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/vardec.h"
#include "sdk/fido/fido_packets.h"
#include "networkb/fido_util.h"

using std::string;
using namespace wwiv::core;
using namespace wwiv::stl;
using namespace wwiv::net::fido;
using namespace wwiv::sdk::fido;
using namespace wwiv::strings;

namespace wwiv {
namespace sdk {

FtnMessageDupe::FtnMessageDupe(const Config& config)
    : initialized_(config.IsInitialized()), datadir_(config.datadir()) {
  if (initialized_) {
    initialized_ = Load();
  }
}

FtnMessageDupe::~FtnMessageDupe() {}

bool FtnMessageDupe::Load() {
  DataFile<uint64_t> file(datadir_, MSGDUPE_DAT,
      File::modeReadWrite | File::modeBinary | File::modeCreateFile);
  if (!file) {
    return false;
  }

  const int num_records = file.number_of_records();
  if (num_records == 0) {
    // nothing to read.
    return true;
  }
  std::vector<uint64_t> dupes(num_records);
  if (!file.Read(&dupes[0], num_records)) {
    return false;
  }
  for (const auto& dupe : dupes) {
    dupes_.insert(dupe);
  }
  return true;
}

bool FtnMessageDupe::Save() {
  DataFile<uint64_t> file(datadir_, MSGDUPE_DAT,
      File::modeReadWrite | File::modeBinary | File::modeCreateFile | File::modeTruncate);
  if (!file) {
    return false;
  }
  std::vector<uint64_t> dupes;
  for (const auto& d : dupes_) {
    dupes.push_back(d);
  }
  return file.Write(&dupes[0], dupes_.size());
}

const std::string FtnMessageDupe::CreateMessageID(const wwiv::sdk::fido::FidoAddress& a) {
  DataFile<uint64_t> file(datadir_, MSGDUPE_DAT,
    File::modeReadWrite | File::modeBinary | File::modeCreateFile, File::shareDenyReadWrite);
  if (!file) {
    throw std::runtime_error("Unable to open file: " + file.file().full_pathname());
  }
  uint64_t msg_num;
  if (!file.Read(0, &msg_num)) {
    msg_num = 0;
  }
  ++msg_num;
  file.file().Seek(0, File::Whence::begin);
  file.Write(0, &msg_num);

  string address_string = to_zone_net_node(a);
  return StringPrintf("%s %08X", address_string.c_str(), msg_num);
}

static string GetMessageIDFromText(const std::string& text) {
  static const string kMSGID = "MSGID: ";
  std::vector<std::string> lines = wwiv::net::fido::split_message(text);
  for (const auto& line : lines) {
    if (line.empty() || line.front() != '\001' || line.size() < 2) {
      continue;
    }
    string s = line.substr(1);
    if (starts_with(s, kMSGID)) {
      // Found the message ID, mail here.
      string msgid = s.substr(kMSGID.size());
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
  string msgid = GetMessageIDFromText(msg.vh.text);
  msgid_crc32 = crc32string(msgid);
  return true;
}

static inline uint64_t as64(uint32_t l, uint32_t r) {
  return (static_cast<uint64_t>(l) << 32) | r;
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
  uint64_t entry = as64(header_crc32, msgid_crc32);
  dupes_.insert(entry);
  return Save();
}

bool FtnMessageDupe::remove(uint32_t header_crc32, uint32_t msgid_crc32) {
  uint64_t entry = as64(header_crc32, msgid_crc32);
  dupes_.erase(entry);
  return Save();
}

bool FtnMessageDupe::is_dupe(uint32_t header_crc32, uint32_t msgid_crc32) const {
  uint64_t entry = as64(header_crc32, msgid_crc32);
  return contains(dupes_, entry);
}

bool FtnMessageDupe::is_dupe(const wwiv::sdk::fido::FidoPackedMessage& msg) const {
  uint32_t header_crc32 = 0;
  uint32_t msgid_crc32 = 0;

  if (!crc32(msg, header_crc32, msgid_crc32)) {
    return false;
  }
  return is_dupe(header_crc32, msgid_crc32);
}

}
}
