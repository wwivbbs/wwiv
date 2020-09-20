/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2016-2020, WWIV Software Services             */
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
#include "network2/post.h"

#include "core/log.h"
#include "core/os.h"
#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "network2/context.h"
#include "net_core/net_cmdline.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/subxtr.h"
#include "sdk/msgapi/msgapi.h"
#include "sdk/net/packets.h"
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using std::cout;
using std::endl;
using std::make_unique;
using std::map;
using std::set;
using std::string;
using std::unique_ptr;
using std::vector;

using namespace wwiv::core;
using namespace wwiv::net;
using namespace wwiv::os;
using namespace wwiv::sdk;
using namespace wwiv::sdk::msgapi;
using namespace wwiv::sdk::net;
using namespace wwiv::stl;
using namespace wwiv::strings;

namespace wwiv::net::network2 {

static bool find_sub(const Subs& subs, int network_number, const string& netname, subboard_t& sub) {
  auto current = 0;
  for (const auto& x : subs.subs()) {
    for (const auto& n : x.nets) {
      if (n.net_num == network_number) {
        if (iequals(netname, n.stype)) {
          // Since the subtype matches, we need to find the subboard base filename.
          // and return that.
          sub = subs.sub(current);
          return true;
        }
      }
    }
    ++current;
  }
  return false;
}

// Alpha subtypes are seven characters -- the first must be a letter, but the rest can be any
// character allowed in a DOS filename.This main_type covers both subscriber - to - host and
// host - to - subscriber messages. Minor type is always zero(since it's ignored), and the
// subtype appears as the first part of the message text, followed by a NUL.Thus, the message
// header info at the beginning of the message text is in the format
// SUBTYPE<nul>TITLE<nul>SENDER_NAME<cr / lf>DATE_STRING<cr / lf>MESSAGE_TEXT.
bool handle_inbound_post(Context& context, Packet& p) {

  ScopeExit at_exit;

  auto ppt = ParsedPacketText::FromPacket(p);
  if (VLOG_IS_ON(1)) {
    at_exit.swap(
        [] { LOG(INFO) << "=============================================================="; });
    VLOG(1) << "==============================================================";
    VLOG(1) << "  Processing New Post on subtype: " << ppt.subtype();
    VLOG(1) << "  Title:   " << ppt.title();
    VLOG(1) << "  Sender:  " << ppt.sender();
    VLOG(1) << "  Date:    " << ppt.date();
  }

  subboard_t sub;
  if (!find_sub(context.subs, context.network_number, ppt.subtype(), sub)) {
    LOG(INFO) << "    ! ERROR: Unable to find message of subtype: " << ppt.subtype();
    LOG(INFO) << "      title: " << ppt.title() << "; writing to dead.net.";
    const auto msg = fmt::format("Unable to find message of subtype: '{}'; writing to dead.net", ppt.subtype());
    context.netdat().add_message(NetDat::netdat_msgtype_t::error, msg);
    return write_wwivnet_packet(DEAD_NET, context.net, p);
  }

  if (!context.api(sub.storage_type).Exist(sub)) {
    LOG(INFO) << "WARNING Message area: '" << sub.filename << "' does not exist.";
    LOG(INFO) << "WARNING Attempting to create it.";
    // Since the area does not exist, let's create it automatically
    // like WWIV always does.
    auto created = context.api(sub.storage_type).Create(sub, -1);
    if (!created) {
      const auto msg = fmt::format("Failed to create message area: '{}'; writing to dead.net", sub.filename);
      context.netdat().add_message(NetDat::netdat_msgtype_t::error, msg);
      LOG(INFO) << "    ! ERROR: Failed to create message area: '" << sub.filename
                << "'; writing to dead.net.";
      return write_wwivnet_packet(DEAD_NET, context.net, p);
    }
  }

  unique_ptr<MessageArea> area(context.api(sub.storage_type).Open(sub, -1));
  if (!area) {
    const auto msg = fmt::format("Failed to open message area: '{}'; writing to dead.net", sub.filename);
    context.netdat().add_message(NetDat::netdat_msgtype_t::error, msg);
    LOG(INFO) << "    ! ERROR Unable to open message area: '" << sub.filename
              << "'; writing to dead.net.";
    return write_wwivnet_packet(DEAD_NET, context.net, p);
  }

  if (area->Exists(p.nh.daten, ppt.title(), p.nh.fromsys, p.nh.fromuser)) {
    const auto msg = fmt::format("Discarding Duplicate Message on sub: {}; daten: {}; title: {}", ppt.subtype(),  p.nh.daten, ppt.title());
    context.netdat().add_message(NetDat::netdat_msgtype_t::normal, msg);
    LOG(INFO) << msg;
    // Returning true since we properly handled this by discarding it.
    return true;
  }

  // TODO(rushfan): Should we let CreateMessage accept the packet directly
  // then we could also check the main_type to ensure it's fine.
  auto msg = area->CreateMessage();
  msg->header().set_from_system(p.nh.fromsys);
  msg->header().set_from_usernum(p.nh.fromuser);
  msg->header().set_title(ppt.title());
  msg->header().set_from(ppt.sender());
  msg->header().set_daten(p.nh.daten);
  msg->text().set_text(ppt.text());

  MessageAreaOptions options{};
  options.send_post_to_network = false;
  if (!area->AddMessage(*msg, options)) {
    const auto errmsg = fmt::format("Failed to add message: '{}'; writing to dead.net", ppt.title());
    context.netdat().add_message(NetDat::netdat_msgtype_t::error, errmsg);
    LOG(ERROR) << "    ! ERROR " << errmsg;
    return write_wwivnet_packet(DEAD_NET, context.net, p);
  }
  LOG(INFO) << "    + Posted  '" << ppt.title() << "' on sub: '" << ppt.subtype() << "'.";
    context.netdat().add_message(NetDat::netdat_msgtype_t::post, fmt::format("Posted  '{}' on sub: '{}'",
    ppt.title(), ppt.subtype()));

  return true;
}

static std::string set_to_string(const set<uint16_t>& lines) {
  std::ostringstream ss;
  for (const auto& line : lines) {
    ss << line << std::endl;
  }
  return ss.str();
}

bool send_post_to_subscribers(Context& context, Packet& template_packet,
                              const set<uint16_t>& subscribers_to_skip) {
  LOG(INFO) << "DEBUG: send_post_to_subscribers; skipping: " << set_to_string(subscribers_to_skip);

  if (template_packet.nh.main_type != main_type_new_post) {
    LOG(ERROR) << "Called send_post_to_subscribers on packet of wrong type.";
    LOG(ERROR) << "expected send_post_to_subscribers, got: "
               << main_type_name(template_packet.nh.main_type);
    return false;
  }
  const auto original_subtype = get_subtype_from_packet_text(template_packet.text());
  VLOG(1) << "DEBUG: send_post_to_subscribers; original subtype: " << original_subtype;

  if (original_subtype.empty()) {
    LOG(ERROR) << "No subtype found for packet text.";
    return false;
  }

  subboard_t sub;
  if (!find_sub(context.subs, context.network_number, original_subtype, sub)) {
    const auto msg = fmt::format("Unable to find message of subtype: '{}'; writing to dead.net", original_subtype);
    context.netdat().add_message(NetDat::netdat_msgtype_t::error, msg);
    LOG(INFO) << msg;
    Packet p(template_packet.nh, {}, template_packet.text());
    return write_wwivnet_packet(DEAD_NET, context.net, p);
  }
  VLOG(1) << "DEBUG: Found sub: " << sub.name;

  return send_post_to_subscribers(context.networks(), context.network_number, original_subtype, sub,
                                  template_packet, subscribers_to_skip,
                                  subscribers_send_to_t::hosted_and_gated_only);
}

} // namespace wwiv
