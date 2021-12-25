/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*            Copyright (C)2016-2021, WWIV Software Services              */
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
#include "sdk/fido/fido_packets.h"

#include "core/file.h"
#include "core/log.h"
#include "core/os.h"
#include "core/strings.h"
#include "core/version.h"
#include "sdk/fido/fido_util.h"
#include "sdk/net/packets.h"
#include <algorithm>
#include <optional>
#include <string>

using namespace wwiv::core;
using namespace wwiv::strings;
using namespace wwiv::sdk::net;

namespace wwiv::sdk::fido {

static char ReadCharFromFile(File& f) {
  char buf[1];
  if (const auto num_read = f.Read(buf, 1); num_read == 0) {
    return 0;
  }
  return buf[0];
}

static std::string ReadRestOfFile(File& f, int max_size) {
  auto current = f.current_position();
  const auto size = f.length();
  std::string s;

  const auto to_read = std::min<decltype(current)>(max_size, size - current);
  s.resize(to_read + 1);
  const auto num_read = f.Read(&s[0], to_read);
  s.resize(num_read);
  return s;
}

/**
 * Reads a field of length {len}.  Will trim the field to  remove
 * any trailing nulls.
 */
static std::string ReadFixedLengthField(File& f, int len) {
  std::string s;
  s.resize(len + 1);
  const auto num_read = f.Read(&s[0], len);
  s.resize(num_read);
  while (!s.empty() && s.back() == '\0') {
    // Remove trailing null characters.
    s.pop_back();
  }
  return s;
}

/**
 * Reads a null-terminated field of up to length {len} or the first null
 * character.
 */
static std::string ReadVariableLengthField(File& f, int max_len) {
  std::string s;
  for (auto i = 0; i < max_len; i++) {
    const auto ch = ReadCharFromFile(f);
    if (ch == 0) {
      return s;
    }
    s.push_back(ch);
  }
  return s;
}

FidoStoredMessage::~FidoStoredMessage()  = default;

bool write_fido_packet_header(File& f, const packet_header_2p_t& header) {
  if (const auto num_written = f.Write(&header, sizeof(packet_header_2p_t));
      num_written != sizeof(packet_header_2p_t)) {
    LOG(ERROR) << "short write to packet, wrote " << num_written
               << "; expected: " << sizeof(packet_header_2p_t);
    return false;
  }
  return true;
}

bool write_packed_message(File& f, const FidoPackedMessage& packet) {
  if (const auto num_written = f.Write(&packet.nh, sizeof(fido_packed_message_t));
      num_written != sizeof(fido_packed_message_t)) {
    LOG(ERROR) << "short write to packet, wrote " << num_written
               << "; expected: " << sizeof(fido_packed_message_t);
    return false;
  }
  f.Write(packet.vh.date_time.c_str(), 19);
  f.Write("\0", 1);
  f.Write(packet.vh.to_user_name);
  f.Write("\0", 1);
  f.Write(packet.vh.from_user_name);
  f.Write("\0", 1);
  f.Write(packet.vh.subject);
  f.Write("\0", 1);
  f.Write(packet.vh.text);
  f.Write("\0", 1);
  // End of packet.
  f.Write("\0\0", 2);
  return true;
}

bool write_stored_message(File& f, FidoStoredMessage& packet) {
  if (const auto num = f.Write(&packet.nh, sizeof(fido_stored_message_t));
      num != sizeof(fido_stored_message_t)) {
    LOG(ERROR) << "Short write on write_stored_message. Wrote: " << num
               << "; expected: " << sizeof(fido_stored_message_t);
    return false;
  }
  return f.Write(packet.text) == ssize(packet.text);
}

/**
 * Reads a packed message.
 * See http://ftsc.org/docs/fts-0001.016
 */
ReadPacketResponse read_packed_message(File& f, FidoPackedMessage& packet) {
  const auto num_read = f.Read(&packet.nh, sizeof(fido_packed_message_t));
  if (num_read == 0) {
    // at the end of the packet.
    return ReadPacketResponse::END_OF_FILE;
  }
  if (num_read == 2) {
    // FIDO packets have 2 bytes of NULL at the end;
    if (packet.nh.message_type == 0) {
      return ReadPacketResponse::END_OF_FILE;
    }
  }

  if (num_read != sizeof(fido_packed_message_t)) {
    LOG(INFO) << "error reading header, got short read of size: " << num_read
              << "; expected: " << sizeof(fido_packed_message_t);
    return ReadPacketResponse::ERROR;
  }

  if (packet.nh.message_type != 2) {
    LOG(INFO) << "invalid message_type: " << packet.nh.message_type << "; expected: 2";
  }
  packet.vh.date_time = ReadFixedLengthField(f, 20);
  packet.vh.to_user_name = ReadVariableLengthField(f, 36);
  packet.vh.from_user_name = ReadVariableLengthField(f, 36);
  packet.vh.subject = ReadVariableLengthField(f, 72);
  packet.vh.text = ReadVariableLengthField(f, 256 * 1024);
  return ReadPacketResponse::OK;
}

ReadPacketResponse read_stored_message(File& f, FidoStoredMessage& packet) {
  if (const auto num_read = f.Read(&packet.nh, sizeof(fido_stored_message_t)); num_read == 0) {
    // at the end of the packet.
    return ReadPacketResponse::END_OF_FILE;
  }

  packet.text = ReadRestOfFile(f, 32 * 1024);
  return ReadPacketResponse::OK;
}

// FidoPacket

packet_header_2p_t CreateType2PlusPacketHeader(const FidoAddress& from_address,
                                               const FidoAddress& dest, const DateTime& now,
                                               const std::string& packet_password) {

  packet_header_2p_t header = {};
  header.orig_zone = from_address.zone();
  header.orig_net = from_address.net();
  header.orig_node = from_address.node();
  header.orig_point = from_address.point();
  header.dest_zone = dest.zone();
  header.dest_net = dest.net();
  header.dest_node = dest.node();
  header.dest_point = dest.point();

  const auto tm = now.to_tm();
  header.year = static_cast<uint16_t>(tm.tm_year);
  header.month = static_cast<uint16_t>(tm.tm_mon);
  header.day = static_cast<uint16_t>(tm.tm_mday);
  header.hour = static_cast<uint16_t>(tm.tm_hour);
  header.minute = static_cast<uint16_t>(tm.tm_min);
  header.second = static_cast<uint16_t>(tm.tm_sec);
  header.baud = 33600;
  header.packet_ver = 2;
  header.product_code_high = 0x1d;
  header.product_code_low = 0xff;
  header.qm_dest_zone = dest.zone();
  header.qm_orig_zone = from_address.zone();
  header.capabilities = 0x0001;
  // Ideally we'd just use bswap_16 if it was available everywhere.
  header.capabilities_valid =
      (header.capabilities & 0xff) << 8 | (header.capabilities & 0xff00) >> 8;
  header.product_rev_major = 0;
  header.product_rev_minor = static_cast<uint8_t>(wwiv_network_compatible_version() & 0xff);
  // Add in packet password.  We don't want to ensure we have
  // a trailing null since we may want all 8 bytes to be usable
  // by the password.
  to_char_array_no_null(header.password, packet_password);
  return header;
}


// static
std::optional<FidoPacket> FidoPacket::Open(const std::filesystem::path& path) {
  File f(path);
  if (!f.Open(File::modeBinary | File::modeReadOnly)) {
    VLOG(2) << "Unable to open file: " << path.string();
    return std::nullopt;
  }

  FidoPacket packet(std::move(f), true);
  auto num_header_read = packet.file_.Read(&packet.header_, sizeof(packet_header_2p_t));
  if (num_header_read < static_cast<int>(sizeof(packet_header_2p_t))) {
    LOG(ERROR) << "Read less than packet header";
    return std::nullopt;
  }

  return packet;
}

// static
std::optional<FidoPacket> FidoPacket::Create(const std::filesystem::path& outbound_path,
                                             const packet_header_2p_t& header,
                                             wwiv::core::Clock& clock) {
  for (auto tries = 0; tries < 10; tries++) {
    auto now = clock.Now().now();
    File file(FilePath(outbound_path, packet_name(now)));
    if (!file.Open(File::modeCreateFile | File::modeExclusive | File::modeReadWrite |
                       File::modeBinary,
                   File::shareDenyReadWrite)) {
      LOG(INFO) << "Will try again: Unable to create packet file: " << file;
      clock.SleepFor(std::chrono::seconds(1));
      continue;
    }

    // Create packet with file and original header
    FidoPacket packet(std::move(file), true, header);

    // Then update packet header time to match when we actually created the packet on disk.
    const auto tm = now.to_tm();
    packet.header_.year = static_cast<uint16_t>(tm.tm_year);
    packet.header_.month = static_cast<uint16_t>(tm.tm_mon);
    packet.header_.day = static_cast<uint16_t>(tm.tm_mday);
    packet.header_.hour = static_cast<uint16_t>(tm.tm_hour);
    packet.header_.minute = static_cast<uint16_t>(tm.tm_min);
    packet.header_.second = static_cast<uint16_t>(tm.tm_sec);

    if (!packet.write_fido_packet_header()) {
      return std::nullopt;
    }
    return std::move(packet);
  }
  return std::nullopt;
}

bool FidoPacket::write_fido_packet_header() {
  if (const auto num_written = file_.Write(&header_, sizeof(packet_header_2p_t));
      num_written != sizeof(packet_header_2p_t)) {
    LOG(ERROR) << "short write to packet, wrote " << num_written
               << "; expected: " << sizeof(packet_header_2p_t);
    return false;
  }
  return true;
}

bool FidoPacket::Write(const FidoPackedMessage& packet) {
  return write_packed_message(file_, packet);
}

std::tuple<wwiv::sdk::net::ReadPacketResponse, FidoPackedMessage> FidoPacket::Read() {
  FidoPackedMessage msg;
  auto response = read_packed_message(file_, msg);
  return std::make_tuple(response, msg);
}

std::string FidoPacket::password() const {
  // Do this dance to ensure that if there's no trailing null on header.password, we add one.
  char temp[9];
  memset(temp, 0, sizeof(temp));
  strncpy(temp, header_.password, 8);
  temp[8] = '\0';
  std::string actual = temp;
  return ToStringUpperCase(actual);
}

} // namespace wwiv
