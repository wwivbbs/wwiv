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
#include "wwivutil/net/send.h"

#include "core/command_line.h"
#include "core/log.h"
#include "core/strings.h"
#include "sdk/bbslist.h"
#include "sdk/config.h"
#include "sdk/msgapi/message_api_wwiv.h"
#include "sdk/net/networks.h"
#include "sdk/net/packets.h"

#include <iostream>
#include <string>
#include <vector>

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::msgapi;
using namespace wwiv::sdk::net;
using namespace wwiv::strings;

namespace wwiv::wwivutil::net {

// Based off one from post.cpp
static std::optional<subboard_t> find_sub(const Subs& subs, int network_number, const std::string& netname) {
  auto current = 0;
  VLOG(3) << "find_sub: subs.subs().size(): " << subs.subs().size()
          << "; net_num: " << network_number;
  for (const auto& x : subs.subs()) {
    for (const auto& n : x.nets) {
      if (n.net_num == network_number) {
        VLOG(4) << "netname: " << netname << "; n.stype: " << n.stype;
        if (iequals(netname, n.stype)) {
          VLOG(2) << "MATCH: netname: " << netname << "; n.stype: " << n.stype;
          // Since the subtype matches, we need to find the sub-board base filename.
          // and return that.
          auto sub = subs.sub(current);
          return { sub };
        }
      }
    }
    ++current;
  }
  return std::nullopt;
}


// (from autosend.doc): AUTOSEND <sub type> <node> <.net> [num msgs]

std::string SubSendCommand::GetUsage() const {
  std::ostringstream ss;
  ss << "Usage:   send --net=N <sub type> <node> [num msgs]" << std::endl;
  ss << "Example: send --net=1 GENCHAT 1 25" << std::endl;
  return ss.str();
}

bool SubSendCommand::AddSubCommands() {
  add_argument({"net", "Network number to use (i.e. 0).", "0"});

  return true;
}

int SubSendCommand::Execute() {
  const auto& networks = config()->networks();
  if (!networks.IsInitialized()) {
    LOG(ERROR) << "Unable to load networks.";
    return 1;
  }

  const auto r = this->remaining();
  if (r.size() < 2) {
    std::cout << GetUsage() << GetHelp();
    return 2;
  }

  const auto net_num = arg("net").as_int();
  const auto net = stl::at(networks, net_num);
  const auto subtype = stl::at(r, 0);
  const auto host = to_number<uint16_t>(stl::at(r, 1));
  auto num = 10;
  if (r.size() > 2) {
    num = to_number<int>(stl::at(r, 2));
  }
  
  Subs subs(config()->config()->datadir(), networks.networks());
  if (!subs.Load()) {
    LOG(ERROR) << "Failed to load subs";
    return 1;
  }

  VLOG(1) << "SubType : " << subtype;
  VLOG(1) << "Host:     " << host;
  VLOG(1) << "Num:      " << net_num;
  VLOG(1) << "Network:  " << net.name;

  if (net.type != network_type_t::wwivnet) {
    LOG(ERROR) << "Can only send posts to WWIVnet type networks.";
    return 1;
  }

  auto so = find_sub(subs, net_num, subtype);

  if (!so) {
    LOG(ERROR) << "Unable to find subtype: '" << subtype << "' on network: " << net.name;
    return 1;
  }

  VLOG(1) << "Sub:      " << so->name;
  VLOG(1) << "Filename: " << so->filename;
  VLOG(1) << "Descr:    " << so->desc;

  // TODO(rushfan): Should this be enforced, if not we can
  // let people peer to peer seed subs if needed.
  for (const auto& n : so->nets) {
    if (n.net_num == net_num && n.host != 0) {
      LOG(WARNING) << "Should only send posts for subs we host";
    }
  }

  const MessageApiOptions options{};
  auto* x = new NullLastReadImpl();
  auto api = std::make_unique<WWIVMessageApi>(options, *config()->config(),
                                              config()->networks().networks(), x);
  if (!api->Exist(*so)) {
    LOG(ERROR) << "Sub doesn't exist in this message type";
    return 1;
  }
  auto area(api->Open(*so, -1));
  if (!area) {
    LOG(ERROR) << "Unable to open message area.";
    return 1;
  }
  for (auto i = std::max<int>(1, area->number_of_messages() - num); i <= area->number_of_messages();
       i++) {
    auto message = area->ReadMessage(i);
    auto packet = create_packet_from_wwiv_message(*message, subtype, {host});
    LOG(INFO) << "Writing packet for message: " << message->header().title();
    // TODO(rushfan): make documented list of what network_app_id codes
    // are used when writing pending packets.
    write_wwivnet_packet_or_log(net, 'u', packet);
  }

  return 0;
}

} // namespace wwiv::wwivutil::net