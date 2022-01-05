/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*           Copyright (C)2016-2022, WWIV Software Services               */
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
#ifndef INCLUDED_SDK_NET_PACKETS_H
#define INCLUDED_SDK_NET_PACKETS_H

#include "core/file.h"
#include "sdk/bbslist.h"
#include "sdk/msgapi/message_wwiv.h"
#include "sdk/net/net.h"
#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace wwiv::sdk::net {

#ifdef ERROR
#undef ERROR
#endif
enum class ReadNetPacketResponse { NOT_OPENED, OK, ERROR, END_OF_FILE };

// fwd
class ParsedNetPacketText;

class NetPacket final {
public:
  NetPacket(const net_header_rec& h, std::vector<uint16_t> l, std::string t);
  NetPacket(const net_header_rec& h, const std::vector<uint16_t>& l, const ParsedNetPacketText& t);

  NetPacket();
  ~NetPacket() = default;

  bool UpdateRouting(const Network& net);
  static std::filesystem::path wwivnet_packet_path(const Network& net, uint16_t node);

  [[nodiscard]] const std::string& text() const noexcept;
  void set_text(const std::string& text);
  void set_text(std::string&& text);
  // Updates the lengths in the header for list and text.
  void update_header();
  int length() const;

  net_header_rec nh{};
  std::vector<uint16_t> list;
  std::string text_;
};

// Alpha subtypes are seven characters -- the first must be a letter, but the rest can be any
// character allowed in a DOS filename.This main_type covers both subscriber - to - host and
// host - to - subscriber messages. Minor type is always zero(since it's ignored), and the
// subtype appears as the first part of the message text, followed by a NUL.Thus, the message
// header info at the beginning of the message text is in the format for new_post is:
// SUBTYPE<nul>TITLE<nul>SENDER_NAME<cr/lf>DATE_STRING<cr/lf>MESSAGE_TEXT.
//
// The format for email_name is:
// TO_NAME<nul>TITLE<nul>SENDER_NAME<cr/lf>DATE_STRING<cr/lf>MESSAGE_TEXT.
// 
// The format for email (not by name) and post (not new post) is:
// TITLE<nul>SENDER_NAME<cr/lf>DATE_STRING<cr/lf>MESSAGE_TEXT.
//
// Also remember that inside of the type-2 message text, the 1st 2 lines
// are sender and date string, so once placed in type-2 message the
// format is:
// SENDER_NAME<cr/lf>DATE_STRING<cr/lf>MESSAGE_TEXT.


/**
 * Represents a parsed NetPacket text for main types: email, email_name, and new_post.
 * 
 * That means the text portion here just includes the message body and not the 
 * WWIV message fields encoded into the first few lines of message text that
 * exists both in message base format as well as on the network.
 */
class ParsedNetPacketText {
public:
  ParsedNetPacketText(uint16_t typ);
  void set_date(daten_t d);
  void set_date(const std::string& d);
  void set_subtype(const std::string& s);
  void set_to(const std::string& s);
  void set_title(const std::string& t);
  void set_sender(const std::string& s);
  void set_text(const std::string& t);
  void set_main_type(uint16_t t);
  [[nodiscard]] uint16_t main_type() const noexcept;
  [[nodiscard]] const std::string& subtype() const noexcept;
  [[nodiscard]] const std::string& subtype_or_email_to() const noexcept;
  [[nodiscard]] const std::string& to() const noexcept;
  [[nodiscard]] const std::string& title() const noexcept;
  [[nodiscard]] const std::string& sender() const noexcept;
  [[nodiscard]] const std::string& date() const noexcept;
  [[nodiscard]] const std::string& text() const noexcept;

  static ParsedNetPacketText FromText(uint16_t typ, const std::string& raw);
  static ParsedNetPacketText FromNetPacket(const NetPacket& p);
  static std::string ToPacketText(const ParsedNetPacketText& ppt);

private:
  uint16_t main_type_;
  std::string subtype_or_email_to_;
  std::string title_;
  std::string sender_;
  std::string date_;
  std::string text_;
};

/**
 * Class for reading a WWIVnet mail file, which contians a series of WWIVnet packets.  
 * 
 * // Example:
 * NetMailFile NetPackets(path, true);
 * for (auto NetPacket : NetPackets) {
 *   // Process NetPacket.
 *   process_wwivnet_packet(NetPacket);
 * }
 */
class NetMailFile {
public:
  
  class iterator {
  public:
    // iterator traits
    using difference_type = std::ptrdiff_t;
    using value_type = NetPacket;
    using pointer = const NetPacket*;
    using reference = const NetPacket&;
    using iterator_category = std::forward_iterator_tag;

    // Constructor and bits to make it work.
    iterator(NetMailFile& f);
    iterator(NetMailFile& f, ReadNetPacketResponse response);
    // prefix (++iter)
    iterator& operator++() {
      std::tie(packet_, response_) = f_.Read();
      ++num_;
      return *this;
    }
    // postfix (iter++)
    iterator operator++(int) {
      iterator retval = *this;
      ++(*this);
      return retval;
    }
    bool operator==(iterator other) const noexcept {
      if (response_ == ReadNetPacketResponse::END_OF_FILE &&
          other.response_ == ReadNetPacketResponse::END_OF_FILE) {
        return true;
      }
      return (num_ == other.num_ && response_ == other.response_);
    }
    bool operator!=(iterator other) const noexcept { return !(*this == other); }
    NetPacket operator*() { return packet_; }

  private:
    int num_{0};
    NetMailFile& f_;
    NetPacket packet_;
    ReadNetPacketResponse response_{ReadNetPacketResponse::NOT_OPENED};
  };
  
  NetMailFile(const std::filesystem::path& path, bool process_de);
  virtual ~NetMailFile();

  iterator begin();
  iterator end() { return iterator(*this, ReadNetPacketResponse::END_OF_FILE); }

private:
  // Only used by iterator class.
  std::tuple<NetPacket, ReadNetPacketResponse> Read();

  wwiv::core::File file_;
  bool process_de_{false};
  bool open_{false};
};


/**
 * Gets the next message field from a NetPacket text c with iterator iter.
 * The next message field will be the next set of characters that do not include
 * anything in the set of stop characters (stop) and less than a total of max.
 */
template <typename C, typename I>
static std::string get_message_field(const C& c, I& iter, std::set<char> stop, std::size_t max) {
  // No need to continue if we're already at the end.
  if (iter == c.end()) {
    return {};
  }

  const auto begin = iter;
  std::size_t count = 0;
  while (iter != std::end(c) && stop.find(*iter) == std::end(stop) && ++count < max) {
    ++iter;
  }
  std::string result(begin, iter);

  // Stop over stop chars
  while (iter != std::end(c) && stop.find(*iter) != std::end(stop)) {
    ++iter;
  }

  return result;
}

/**
 * Gets the WWIVnet system number in the path to system node.
 * For example: if you are @1 and you want to send to @3 and
 * you only connect to @2 (and @2 connects to @3) then this will
 * return @2 as the forward system for @3.
 */
uint16_t get_forsys(const wwiv::sdk::BbsListNet& b, uint16_t node);

std::tuple<NetPacket, ReadNetPacketResponse> read_packet(wwiv::core::File& file, bool process_de);

bool write_wwivnet_packet(const std::filesystem::path& path, const NetPacket& NetPacket);

bool send_local_email(const Network& network, net_header_rec& nh, const std::string& text,
                      const std::string& byname, const std::string& title);

bool send_network_email(const std::string& filename, const Network& network,
                        net_header_rec& nh, const std::vector<uint16_t>& list, const std::string& text,
                        const std::string& byname, const std::string& title);

struct NetInfoFileInfo {
  std::string filename;
  std::string data;
  bool overwrite{false};
  bool valid{false};
};

NetInfoFileInfo GetNetInfoFileInfo(NetPacket& p);

void rename_pend(const std::filesystem::path& directory, const std::string& filename, char network_app_id);
std::string create_pend(const std::filesystem::path& directory, bool local, char network_app_id);

std::string main_type_name(uint16_t typ);
std::string net_info_minor_type_name(uint16_t typ);

/**
 * Gets the subtype from a main_type_new_post message NetPacket's text.
 * Returns empty string on error.
 */
std::string get_subtype_from_packet_text(const std::string& text);

/** Creates an outbound NetPacket to be sent */
NetPacket create_packet_from_wwiv_message(const wwiv::sdk::msgapi::WWIVMessage& m,
                                       const std::string& subtype, std::set<uint16_t> receipients);

// Writes out a WWIVnet NetPacket to the pending file and logs success and failure to the 
// system Logger.
bool write_wwivnet_packet_or_log(const Network& net, char network_app_id, const NetPacket& p);

enum class subscribers_send_to_t { hosted_and_gated_only, all_subscribers };
bool send_post_to_subscribers(const std::vector<Network>& nets, int original_net_num,
                              const std::string& original_subtype, const subboard_t& sub,
                              NetPacket& template_packet, const std::set<uint16_t>& subscribers_to_skip,
                              const subscribers_send_to_t& send_to);

} // namespace

#endif
