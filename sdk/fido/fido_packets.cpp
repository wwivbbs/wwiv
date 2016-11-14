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
#include "sdk/fido/fido_packets.h"

#include <string>

#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "sdk/datetime.h"
#include "sdk/filenames.h"

using std::string;
using namespace wwiv::core;
using namespace wwiv::strings;

namespace wwiv {
namespace sdk {
namespace fido {

static char ReadCharFromFile(File& f) {
  char buf[1];
  auto num_read = f.Read(buf, 1);
  if (num_read == 0) {
    return 0;
  }
  return buf[0];
}

static std::string ReadRestOfFile(File& f, int max_size) {
  auto current = f.current_position();
  auto size = f.GetLength();
  string s;

  auto to_read = std::min<size_t>(max_size, size - current);
  s.resize(to_read + 1);
  auto num_read = f.Read(&s[0], to_read);
  s.resize(num_read);
  return s;
}


static std::string ReadFixedLengthField(File& f, int len) {
  string s;
  s.resize(len + 1);
  auto num_read = f.Read(&s[0], len + 1);
  s.resize(num_read);
  return s;
}

static std::string ReadVariableLengthField(File& f, int max_len) {
  string s;
  for (int i = 0; i < max_len; i++) {
    char ch = ReadCharFromFile(f);
    if (ch == 0) {
      return s;
    } else {
      s.push_back(ch);
    }
  }
  return s;
}

bool write_fido_packet_header(File& f, packet_header_2p_t& header) {
  auto num_written = f.Write(&header, sizeof(packet_header_2p_t));
  if (num_written != sizeof(packet_header_2p_t)) {
    LOG(ERROR) << "short write to packet, wrote " << num_written << "; expected: " << sizeof(packet_header_2p_t);
    return false;
  }
  return true;
}

bool write_packed_message(File& f, FidoPackedMessage& packet) {
  auto num_written = f.Write(&packet.nh, sizeof(fido_packed_message_t));
  if (num_written != sizeof(fido_packed_message_t)) {
    LOG(ERROR) << "short write to packet, wrote " << num_written << "; expected: " << sizeof(fido_packed_message_t);
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
  auto num = f.Write(&packet.nh, sizeof(fido_stored_message_t));
  if (num != sizeof(fido_stored_message_t)) {
    LOG(ERROR) << "Short write on write_stored_message. Wrote: " << num
      << "; expected: " << sizeof(fido_stored_message_t);
    return false;
  }
  num = f.Write(packet.text);
  return true;
}

ReadPacketResponse read_packed_message(File& f, FidoPackedMessage& packet) {
  auto num_read = f.Read(&packet.nh, sizeof(fido_packed_message_t));
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
      << "; expected: " << sizeof(net_header_rec);
    return ReadPacketResponse::ERROR;
  }

  if (packet.nh.message_type != 2) {
    LOG(INFO) << "invalid message_type: " << packet.nh.message_type << "; expected: 2";
  }
  packet.vh.date_time = ReadFixedLengthField(f, 19);
  packet.vh.to_user_name = ReadVariableLengthField(f, 36);
  packet.vh.from_user_name = ReadVariableLengthField(f, 36);
  packet.vh.subject = ReadVariableLengthField(f, 36);
  packet.vh.text = ReadVariableLengthField(f, 256 * 1024);
  return ReadPacketResponse::OK;
}

ReadPacketResponse read_stored_message(File& f, FidoStoredMessage& packet) {
  auto num_read = f.Read(&packet.nh, sizeof(fido_stored_message_t));
  if (num_read == 0) {
    // at the end of the packet.
    return ReadPacketResponse::END_OF_FILE;
  }

  packet.text = ReadRestOfFile(f, 32 * 1024);
  return ReadPacketResponse::OK;
}

}
}  // namespace net
}  // namespace wwiv

