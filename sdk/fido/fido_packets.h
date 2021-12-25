/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*           Copyright (C)2016-2021, WWIV Software Services               */
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
#ifndef INCLUDED_SDK_FIDO_FIDO_PACKETS_H
#define INCLUDED_SDK_FIDO_FIDO_PACKETS_H

#include "core/clock.h"
#include "core/datetime.h"
#include "core/file.h"
#include "sdk/config.h"
#include "sdk/net/packets.h"
#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <utility>

namespace wwiv::sdk::fido {

#ifndef __MSDOS__
#pragma pack(push, 1)
#endif // __MSDOS__

#ifdef ERROR
#undef ERROR
#endif

/**
 * Reference: http://ftsc.org/docs/fsc-0039.004
 *
  Type-2 Packet Format (proposed, but currently in use)
  -----------------------------------------------------
  Field    Ofs Siz Type  Description                Expected value(s)
  -------  --- --- ----  -------------------------- -----------------
  OrgNode  0x0   2 Word  Origination node address   0-65535
  DstNode    2   2 Word  Destination node address   1-65535
  Year       4   2  Int  Year packet generated      19??-2???
  Month      6   2  Int  Month  "        "          0-11 (0=Jan)
  Day        8   2  Int  Day    "        "          1-31
  Hour       A   2  Int  Hour   "        "          0-23
  Min        C   2  Int  Minute "        "          0-59
  Sec        E   2  Int  Second "        "          0-59
  Baud      10   2  Int  Baud Rate (not in use)     ????
  PktVer    12   2  Int  Packet Version             Always 2
  OrgNet    14   2 Word  Origination net address    1-65535
  DstNet    16   2 Word  Destination net address    1-65535
  PrdCodL   18   1 Byte  FTSC Product Code     (lo) 1-255
* PVMajor   19   1 Byte  FTSC Product Rev   (major) 1-255
  Password  1A   8 Char  Packet password            A-Z,0-9
* QOrgZone  22   2  Int  Orig Zone (ZMailQ,QMail)   1-65535
* QDstZone  24   2  Int  Dest Zone (ZMailQ,QMail)   1-65535
  Filler    26   2 Byte  Spare Change               ?
* CapValid  28   2 Word  CW Byte-Swapped Valid Copy BitField
* PrdCodH   2A   1 Byte  FTSC Product Code     (hi) 1-255
* PVMinor   2B   1 Byte  FTSC Product Rev   (minor) 1-255
* CapWord   2C   2 Word  Capability Word            BitField
* OrigZone  2E   2  Int  Origination Zone           1-65535
* DestZone  30   2  Int  Destination Zone           1-65535
* OrigPoint 32   2  Int  Origination Point          1-65535
* DestPoint 34   2  Int  Destination Point          1-65535
* ProdData  36   4 Long  Product-specific data      Whatever
  PktTerm   3A   2 Word  Packet terminator          0000
*/
struct packet_header_2p_t { /* FSC-0039 Type 2.+ */
  uint16_t orig_node, dest_node, year, month, day, hour, minute, second, baud, packet_ver, orig_net,
      dest_net;

  uint8_t product_code_low;
  uint8_t product_rev_major;
  char password[8];

  uint16_t qm_orig_zone, qm_dest_zone;

  uint8_t filler[2];

  uint16_t capabilities_valid;

  uint8_t product_code_high, product_rev_minor;

  uint16_t capabilities, orig_zone, dest_zone, orig_point, dest_point;

  uint32_t product_data;
};

/*
Stored Message (*.MSG format)
From http://ftsc.org/docs/fts-0001.016
-----------------------------------------------------

Message    =
fromUserName(36)  (* Null terminated *)
toUserName(36)    (* Null terminated *)
subject(72)       (* see FileList below *)
DateTime          (* message body was last edited *)
timesRead         (* number of times msg has been read *)
destNode          (* of message *)
origNode          (* of message *)
cost              (* in lowest unit of originator's
currency *)
origNet           (* of message *)
destNet           (* of message *)
destZone          (* of message *)
origZone          (* of message *)
destPoint         (* of message *)
origPoint         (* of message *)
replyTo           (* msg to which this replies *)
AttributeWord
nextReply         (* msg which replies to this *)
text(unbounded)   (* Null terminated *)
*/
struct fido_stored_message_t {
  char from[36];
  char to[36];
  char subject[72];
  char date_time[20];
  int16_t times_read, dest_node, orig_node, cost, orig_net, dest_net, dest_zone, orig_zone,
      dest_point, orig_point, reply_to, attribute, next_reply;
};

static_assert(sizeof(packet_header_2p_t) == 58, "packet_header_2p_t != 58 bytes");
static_assert(sizeof(fido_stored_message_t) == 190, "fido_stored_message_t != 190 bytes");
/*
,-------------------------------------------------------------------.
| Name      | Offset | Bytes | Type  | Description                  |
+-----------+--------+-------+-------+------------------------------+
| msgType   | 0      | 2     | Int16 | MUST have a value of 2       |
| origNode  | 2      | 2     | Int16 | Node number packet is from   |
| destNode  | 4      | 2     | Int16 | Node number packet is to     |
| origNet   | 6      | 2     | Int16 | Network number packet is from|
| destNet   | 8      | 2     | Int16 | Network number packet is to  |
| attribute | 10     | 2     | Int16 | See Notes                    |
| cost      | 12     | 2     | Int16 | See Notes                    |
`-----------+--------+-------+-------+------------------------------'
 */

struct fido_packed_message_t {
  uint16_t message_type, orig_node, dest_node, orig_net, dest_net, attribute, cost;
};

#ifndef __MSDOS__
#pragma pack(pop)
#endif // __MSDOS__

/////////////////////////////////////////////////////////////////////////////
//
// Non DOS packed structs and classes.
//
//

// Private message
static constexpr uint16_t MSGPRIVATE = 0x0001;
// High priority
static constexpr uint16_t MSGCRASH = 0x0002;
// Read by addressee
static constexpr uint16_t MSGREAD = 0x0004;
// Has been sent
static constexpr uint16_t MSGSENT = 0x0008;
// File attached to msg
static constexpr uint16_t MSGFILE = 0x0010;
// In transit
static constexpr uint16_t MSGTRANSIT = 0x0020;
// Unknown node
static constexpr uint16_t MSGORPHAN = 0x0040;
// Kill after mailing
static constexpr uint16_t MSGKILL = 0x0080;
// Message was entered here
static constexpr uint16_t MSGLOCAL = 0x0100;
// Hold for pickup
static constexpr uint16_t MSGHOLD = 0x0200;
// Unused
static constexpr uint16_t MSGUNUSED = 0x0400;
// File req uest
static constexpr uint16_t MSGFREQ = 0x0800;
// Return receipt request
static constexpr uint16_t MSGRRREQ = 0x1000;
// Is return receipt
static constexpr uint16_t MSGISRR = 0x2000;
// Audit request
static constexpr uint16_t MSGAREQ = 0x4000;
// File update request
static constexpr uint16_t MSGFUPDREQ = 0x8000;

struct fido_variable_length_header_t {
  std::string date_time;
  std::string to_user_name;
  std::string from_user_name;
  std::string subject;
  std::string text;
};

/**
 * Represents a message in a .PKT file in FidoNET.
 */
class FidoPackedMessage {
public:
  FidoPackedMessage(const fido_packed_message_t& h, fido_variable_length_header_t v)
      : nh(h), vh(std::move(v)) {}

  FidoPackedMessage() = default;
  virtual ~FidoPackedMessage() = default;

  fido_packed_message_t nh{};
  fido_variable_length_header_t vh;
};

/**
 * Represents a .MSG file in FidoNET.
 */
class FidoStoredMessage {
public:
  FidoStoredMessage(const fido_stored_message_t& h, std::string t) : nh(h), text(std::move(t)) {}
  FidoStoredMessage() = default;
  virtual ~FidoStoredMessage();

  fido_stored_message_t nh{};
  std::string text;
};

/**
 * Represents a .PKT file in FidoNET.
 */
class FidoPacket {
public:
  FidoPacket(wwiv::core::File&& f, bool writable) : file_(std::move(f)), writable_(writable) {}
  FidoPacket(wwiv::core::File&& f, bool writable, const packet_header_2p_t& header)
      : file_(std::move(f)), writable_(writable), header_(header) {}

  static std::optional<FidoPacket> Create(const std::filesystem::path& outbound_path,
                                          const packet_header_2p_t& header,
                                          wwiv::core::Clock& clock);
  static std::optional<FidoPacket> Open(const std::filesystem::path& path);

  FidoPacket(FidoPacket&& o) noexcept
      : file_(std::move(o.file_)), writable_(o.writable_), header_(o.header_) {}

  bool Write(const FidoPackedMessage& packet);
  [[nodiscard]] std::tuple<wwiv::sdk::net::ReadPacketResponse, FidoPackedMessage> Read();
  [[nodiscard]] packet_header_2p_t& header() { return header_; }

  // Gets the packet password as a UPPER case string.
  [[nodiscard]] std::string password() const;

  // Close the packet
  void Close() { file_.Close(); }

private:
  bool write_fido_packet_header();
  wwiv::core::File file_;
  bool writable_{false};
  packet_header_2p_t header_{};
};
  
bool write_fido_packet_header(wwiv::core::File& f, const packet_header_2p_t& header);
bool write_packed_message(wwiv::core::File& f, const FidoPackedMessage& packet);
bool write_stored_message(wwiv::core::File& f, FidoStoredMessage& packet);

wwiv::sdk::net::ReadPacketResponse read_packed_message(wwiv::core::File& file,
                                                       FidoPackedMessage& packet);
wwiv::sdk::net::ReadPacketResponse read_stored_message(wwiv::core::File& file,
                                                       FidoStoredMessage& packet);
packet_header_2p_t CreateType2PlusPacketHeader(const FidoAddress& from_address,
                                               const FidoAddress& dest, const wwiv::core::DateTime& now,
                                               const std::string& packet_password);

} // namespace wwiv::sdk::fido

#endif  // INCLUDED_SDK_FIDO_FIDO_PACKETS_H
