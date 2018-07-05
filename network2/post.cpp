/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2016-2017, WWIV Software Services             */
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

// WWIV5 Network2
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "core/command_line.h"
#include "core/connection.h"
#include "core/datafile.h"
#include "core/file.h"
#include "core/log.h"
#include "core/os.h"
#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "network2/context.h"
#include "networkb/binkp.h"
#include "networkb/binkp_config.h"
#include "networkb/net_util.h"
#include "networkb/packets.h"
#include "networkb/ppp_config.h"

#include "sdk/bbslist.h"
#include "sdk/callout.h"
#include "sdk/config.h"
#include "sdk/connect.h"
#include "sdk/contact.h"
#include "sdk/datetime.h"
#include "sdk/filenames.h"
#include "sdk/msgapi/message_api_wwiv.h"
#include "sdk/msgapi/msgapi.h"
#include "sdk/networks.h"
#include "sdk/subscribers.h"
#include "sdk/subxtr.h"
#include "sdk/usermanager.h"

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
using namespace wwiv::stl;
using namespace wwiv::strings;

namespace wwiv {
namespace net {
namespace network2 {

static bool find_sub(Context& context, const string& netname, subboard_t& sub) {
  int current = 0;
  for (const auto& x : context.subs.subs()) {
    for (const auto& n : x.nets) {
      if (n.net_num == context.network_number) {
        if (iequals(netname, n.stype)) {
          // Since the subtype matches, we need to find the subboard base filename.
          // and return that.
          sub = context.subs.sub(current);
          return true;
        }
      }
    }
    ++current;
  }
  return false;
}

static std::string change_subtype_to(const std::string& org_text, const std::string& new_subtype) {
  auto iter = org_text.begin();
  auto subtype = get_message_field(org_text, iter, {'\0', '\r', '\n'}, 80);
  std::string result = new_subtype;
  result.push_back(0); // Add NULL
  result += string(iter, std::end(org_text));
  // Should implicitly move by RVO
  return result;
}

// Alpha subtypes are seven characters -- the first must be a letter, but the rest can be any
// character allowed in a DOS filename.This main_type covers both subscriber - to - host and
// host - to - subscriber messages. Minor type is always zero(since it's ignored), and the
// subtype appears as the first part of the message text, followed by a NUL.Thus, the message
// header info at the beginning of the message text is in the format
// SUBTYPE<nul>TITLE<nul>SENDER_NAME<cr / lf>DATE_STRING<cr / lf>MESSAGE_TEXT.
bool handle_inbound_post(Context& context, Packet& p) {

  ScopeExit at_exit;

  auto raw_text = p.text;
  auto iter = raw_text.begin();
  auto subtype = get_message_field(raw_text, iter, {'\0', '\r', '\n'}, 80);
  auto title = get_message_field(raw_text, iter, {'\0', '\r', '\n'}, 80);
  auto sender_name = get_message_field(raw_text, iter, {'\0', '\r', '\n'}, 80);
  auto date_string = get_message_field(raw_text, iter, {'\0', '\r', '\n'}, 80);
  auto text = string(iter, raw_text.end());
  if (VLOG_IS_ON(1)) {
    at_exit.swap(
        [] { LOG(INFO) << "=============================================================="; });
    VLOG(1) << "==============================================================";
    VLOG(1) << "  Processing New Post on subtype: " << subtype;
    VLOG(1) << "  Title:   " << title;
    VLOG(1) << "  Sender:  " << sender_name;
    VLOG(1) << "  Date:    " << date_string;
  }

  subboard_t sub;
  if (!find_sub(context, subtype, sub)) {
    LOG(INFO) << "    ! ERROR: Unable to find message of subtype: " << subtype;
    LOG(INFO) << "      title: " << title << "; writing to dead.net.";
    return write_wwivnet_packet(DEAD_NET, context.net, p);
  }

  if (!context.api(sub.storage_type).Exist(sub)) {
    LOG(INFO) << "WARNING Message area: '" << sub.filename << "' does not exist.";
    ;
    LOG(INFO) << "WARNING Attempting to create it.";
    // Since the area does not exist, let's create it automatically
    // like WWIV always does.
    auto created = context.api(sub.storage_type).Create(sub, -1);
    if (!created) {
      LOG(INFO) << "    ! ERROR: Failed to create message area: " << sub.filename
                << "; writing to dead.net.";
      return write_wwivnet_packet(DEAD_NET, context.net, p);
    }
  }

  unique_ptr<MessageArea> area(context.api(sub.storage_type).Open(sub, -1));
  if (!area) {
    LOG(INFO) << "    ! ERROR Unable to open message area: " << sub.filename
              << "; writing to dead.net.";
    return write_wwivnet_packet(DEAD_NET, context.net, p);
  }

  if (area->Exists(p.nh.daten, title, p.nh.fromsys, p.nh.fromuser)) {
    LOG(INFO) << "    - Discarding Duplicate Message on sub: " << subtype << "; title: " << title
              << ".";
    // Returning true since we properly handled this by discarding it.
    return true;
  }

  auto msg = area->CreateMessage();
  msg->header().set_from_system(p.nh.fromsys);
  msg->header().set_from_usernum(p.nh.fromuser);
  msg->header().set_title(title);
  msg->header().set_from(sender_name);
  msg->header().set_daten(p.nh.daten);
  msg->text().set_text(text);

  if (!area->AddMessage(*msg)) {
    LOG(ERROR) << "     ! Failed to add message: " << title << "; writing to dead.net";
    return write_wwivnet_packet(DEAD_NET, context.net, p);
  }
  LOG(INFO) << "    + Posted  '" << title << "' on sub: '" << subtype << "'.";
  return true;
}

static bool write_wwivnet_packet_or_log(const net_networks_rec& net, const net_header_rec& h,
                                        std::vector<uint16_t> list, const std::string& text) {
  Packet gp(h, list, text);
  const auto fn = create_pend(net.dir, false, '2');
  if (!write_wwivnet_packet(fn, net, gp)) {
    LOG(ERROR) << "Error writing packet: " << net.dir << " " << fn;
    return false;
  } else {
    VLOG(1) << "Wrote packet: " << fn;
    return true;
  }
}

bool send_post_to_subscribers(Context& context, Packet& orig_packet) {
  LOG(INFO) << "DEBUG: send_post_to_subscribers";

  if (orig_packet.nh.main_type != main_type_new_post) {
    LOG(ERROR) << "Called send_post_to_subscribers on packet of wrong type.";
    LOG(ERROR) << "expected send_post_to_subscribers, got: "
               << main_type_name(orig_packet.nh.main_type);
    return false;
  }
  auto original_subtype = get_subtype_from_packet_text(orig_packet.text);
  VLOG(1) << "DEBUG: send_post_to_subscribers; original subtype: " << original_subtype;

  if (original_subtype.empty()) {
    LOG(ERROR) << "No subtype found for packet text.";
    return false;
  }

  subboard_t sub;
  if (!find_sub(context, original_subtype, sub)) {
    LOG(INFO) << "    ! ERROR: Unable to find message of subtype: " << original_subtype
              << " writing to dead.net.";
    Packet p(orig_packet.nh, {}, orig_packet.text);
    return write_wwivnet_packet(DEAD_NET, context.net, p);
  }
  VLOG(1) << "DEBUG: Found sub: " << sub.name;

  for (const auto& subnet : sub.nets) {
    auto h = orig_packet.nh;
    VLOG(1) << "DEBUG: Current network subtype: " << subnet.stype;
    VLOG(1) << "DEBUG: Current network is: " << context.networks()[subnet.net_num].name;
    // if subnet.host == 0, we are the host.
    // if subnet.net_num != context.network_number then we are
    // gating the sub to another network.
    bool are_we_hosting = subnet.host == 0;
    bool are_we_gating = subnet.net_num != context.network_number;
    VLOG(1) << "DEBUG: are_we_hosting: " << std::boolalpha << are_we_hosting;
    VLOG(1) << "DEBUG: are_we_gating:  " << std::boolalpha << are_we_gating;

    if (!are_we_hosting && !are_we_gating) {
      // Nothing to do here, so move on to the next subnet in the list
      continue;
    }
    const auto& current_net = context.networks()[subnet.net_num];
    if (are_we_gating) {
      // update fromsys
      h.fromsys = current_net.sysnum;
      h.fromuser = 0;
    }
    // If the subtype has changed, then change the subtype in the
    // packet text.
    const auto text = (subnet.stype == original_subtype)
                          ? orig_packet.text
                          : change_subtype_to(orig_packet.text, subnet.stype);
    if (subnet.stype != original_subtype) {
      // we also have to update the nh.length to reflect this change.
      // TODO(rushfan): Really need higher level interface to manipulating
      // WWIVnet packets...
      h.length -= original_subtype.size();
      h.length += subnet.stype.size();
    }
    if (current_net.type == network_type_t::ftn) {
      h.tosys = FTN_FAKE_OUTBOUND_NODE;
      VLOG(1) << "current network is FTN";
      h.list_len = 0;
      write_wwivnet_packet_or_log(current_net, h, {}, text);
    } else if (current_net.type == network_type_t::wwivnet) {
      if (subnet.host == 0) {
        // We are the host.
        std::set<uint16_t> subscribers;
        bool subscribers_read =
            ReadSubcriberFile(current_net.dir, StrCat("n", subnet.stype, ".net"), subscribers);
        if (subscribers_read) {
          // Remove the original sender from the set of systems
          // that we will resend this to.
          subscribers.erase(orig_packet.nh.fromsys);
          VLOG(1) << "Removing subscriber (sender): " << orig_packet.nh.fromsys;
          VLOG(1) << "Read subscribers #: " << subscribers.size();
          VLOG(1) << "Creating wwivnet packet to: ";
          for (const auto x : subscribers) {
            VLOG(1) << "        @" << x;
          }
          h.list_len = static_cast<uint16_t>(subscribers.size());
          h.tosys = 0;
          write_wwivnet_packet_or_log(
              current_net, h, std::vector<uint16_t>(subscribers.begin(), subscribers.end()), text);
        } else {
          LOG(ERROR) << "Unable to read subscribers for " << current_net.dir << " " << subnet.stype;
        }
      } else {
        // We are not the host.  Send message to host.
        h.tosys = subnet.host;
        h.list_len = 0;
        write_wwivnet_packet_or_log(current_net, h, {}, text);
      }
    }
  }
  LOG(INFO) << "DEBUG: send_post_to_subscribers"
            << "exiting with true";
  return true;
}

} // namespace network2
} // namespace net
} // namespace wwiv
