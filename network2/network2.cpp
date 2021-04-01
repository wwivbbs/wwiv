/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2016-2021, WWIV Software Services             */
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

// WWIV5 Network2
#include "core/command_line.h"
#include "core/datafile.h"
#include "core/file.h"
#include "core/log.h"
#include "core/os.h"
#include "core/scope_exit.h"
#include "core/semaphore_file.h"
#include "core/stl.h"
#include "core/strings.h"
#include "network2/context.h"
#include "network2/email.h"
#include "network2/post.h"
#include "network2/subs.h"
#include "net_core/netdat.h"
#include "net_core/net_cmdline.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/ssm.h"
#include "sdk/status.h"
#include "sdk/usermanager.h"
#include "sdk/msgapi/message_api_wwiv.h"
#include "sdk/msgapi/msgapi.h"
#include "sdk/net/networks.h"
#include "sdk/net/packets.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <set>
#include <string>

using namespace wwiv::core;
using namespace wwiv::net;
using namespace wwiv::net::network2;
using namespace wwiv::os;
using namespace wwiv::sdk;
using namespace wwiv::sdk::msgapi;
using namespace wwiv::sdk::net;
using namespace wwiv::stl;
using namespace wwiv::strings;

static bool email_changed = false;
static bool posts_changed = false;

static void update_filechange_status_dat(const std::string& datadir, bool email, bool posts) {
  StatusMgr sm(datadir);
  sm.Run([=](Status& s)
  {
    if (email) {
      s.increment_filechanged(Status::file_change_email);
    }
    if (posts) {
      s.increment_filechanged(Status::file_change_posts);
    }
  });
}

static void ShowHelp(const NetworkCommandLine& cmdline) {
  std::cout << cmdline.GetHelp() << std::endl;
  exit(1);
}

static bool handle_ssm(Context& context, Packet& p) {
  ScopeExit at_exit(
      [] { VLOG(1) << "=============================================================="; });
  VLOG(1) << "==============================================================";
  if (!context.ssm.send_local(p.nh.touser, p.text())) {
    LOG(ERROR) << "    ! ERROR writing SSM: '" << p.nh.touser << "; text: '" << p.text()
               << "'; writing to dead.net";
    return write_wwivnet_packet(DEAD_NET, context.net, p);
  }

  LOG(INFO) << "    + SSM  to: " << p.nh.touser << "; text: '" << p.text() << "'";
  return true;
}

static bool write_net_received_file(Context& context, const Network& net, Packet& p, NetInfoFileInfo info) {
  if (!info.valid) {
    LOG(ERROR) << "    ! ERROR NetInfoFileInfo is not valid; writing to dead.net";
    return write_wwivnet_packet(DEAD_NET, net, p);
  }

  if (info.filename.empty()) {
    LOG(ERROR) << "    ! ERROR Fell through handle_net_info_file; writing to dead.net";
    return write_wwivnet_packet(DEAD_NET, net, p);
  }
  // we know the name.
  context.ssm.send_local(1, StrCat("Received ", info.filename));
  const auto fn = FilePath(net.dir, info.filename);
  if (!info.overwrite && File::Exists(fn)) {
    LOG(ERROR) << "    ! ERROR File [" << fn
               << "] already exists, and packet not set to overwrite; writing to dead.net";
    return write_wwivnet_packet(DEAD_NET, net, p);
  }
  File file(fn);
  if (!file.Open(File::modeWriteOnly | File::modeBinary | File::modeCreateFile | File::modeTruncate,
                 File::shareDenyReadWrite)) {
    // We couldn't create or open the file.
    LOG(ERROR) << "    ! ERROR Unable to create or open file: '" << info.filename
               << "'; writing to dead.net";
    return write_wwivnet_packet(DEAD_NET, net, p);
  }
  file.Write(info.data);
  LOG(INFO) << "  + Got " << info.filename;
  return true;
}

static bool handle_net_info_file(Context& context, const Network& net, Packet& p) {
  const auto info = GetNetInfoFileInfo(p);
  return write_net_received_file(context, net, p, info);
}

static bool handle_sub_list(Context& context, Packet& p) {
  const auto& net = context.net;
  // Handle legacy type 9 main_type_sub_list (SUBS.LST)
  NetInfoFileInfo info{};
  info.filename = SUBS_LST;
  info.data = p.text();
  info.valid = true;
  info.overwrite = true;
  return write_net_received_file(context, net, p, info);
}

static bool handle_packet(Context& context, Packet& p) {
  LOG(INFO) << "Processing message with type: " << main_type_name(p.nh.main_type) << "/"
            << p.nh.minor_type;

  switch (p.nh.main_type) {
    /*
    These messages contain various network information
    files, encoded with method 1 (requiring DE1.EXE).
    Once DE1.EXE has verified the source and returned to
    the analyzer, the file is created in the network's
    DATA directory with the filename determined by the
    minor_type (except minor_type 1).
    */
  case main_type_net_info: {
    if (p.nh.minor_type == 0) {
      // Feedback to sysop from the NC.
      // This is sent to the #1 account as source verified email.
      email_changed = true;
      return handle_email(context, 1, p);
    }
    return handle_net_info_file(context, context.net, p);
  }
  case main_type_email:
    // This is regular email sent to a user number at this system.
    // Email has no minor type, so minor_type will always be zero.
    email_changed = true;
    return handle_email(context, p.nh.touser, p);
  case main_type_email_name:
    // The other email type.  The "touser" field is zero, and the name is found at
    // the beginning of the message text, followed by a NUL character.
    // Minor_type will always be zero.
    email_changed = true;
    return handle_email_byname(context, p);
  case main_type_new_post: {
    posts_changed = true;
    if (!handle_inbound_post(context, p)) {
      LOG(ERROR) << "Error on handle_inbound_post";
      return false;
    }
    return send_post_to_subscribers(context, p, {p.nh.fromsys});
  }
  case main_type_ssm: {
    return handle_ssm(context, p);
  }
  // Subs add/drop support.
  case main_type_sub_add_req:
    return handle_sub_add_req(context, p);
  case main_type_sub_drop_req:
    return handle_sub_drop_req(context, p);
  case main_type_sub_add_resp:
    return handle_sub_add_drop_resp(context, p, "add");
  case main_type_sub_drop_resp:
    return handle_sub_add_drop_resp(context, p, "drop");

  // Sub ping.
  // In many WWIV networks, the subs list coordinator (SLC) occasionally sends
  // out "pings" to all network members.
  case main_type_sub_list_info: {
    if (p.nh.minor_type == 0) {
      return handle_sub_list_info_request(context, p);
    }
    return handle_sub_list_info_response(context, p);
  }

  case main_type_sub_list:
    return handle_sub_list(context, p);

  // Legacy numeric only post types.
  case main_type_post:
  case main_type_pre_post:

  // EPROGS.NET support
  case main_type_external:
  case main_type_new_external:

  // NetEdit.
  case main_type_net_edit:

  // *.### support
  case main_type_group_bbslist:
  case main_type_group_connect:
  case main_type_group_info:
    // Anything undefined or anything we missed.
  default:
    LOG(ERROR) << "    ! ERROR Writing message to dead.net for unhandled type: '"
               << main_type_name(p.nh.main_type) << "'; writing to dead.net";
    return write_wwivnet_packet(DEAD_NET, context.net, p);
  }
}

static bool handle_file(Context& context, const std::string& name) {
  File f(FilePath(context.net.dir, name));
  if (!f.Open(File::modeBinary | File::modeReadOnly)) {
    LOG(ERROR) << "Unable to open file: " << context.net.dir << name;
    return false;
  }

  for (;;) {
    auto [packet, response] = read_packet(f, true);
    if (response == ReadPacketResponse::END_OF_FILE) {
      return true;
    }
    if (response == ReadPacketResponse::ERROR) {
      return false;
    }

    if (!handle_packet(context, packet)) {
      LOG(ERROR) << "Error handing packet: type: " << packet.nh.main_type;
    }
  }
}

int network2_main(const NetworkCommandLine& net_cmdline) {
  try {
    const auto& net = net_cmdline.network();
    if (!File::Exists(FilePath(net.dir, LOCAL_NET))) {
      LOG(INFO) << "No local.net exists. exiting.";
      return 0;
    }

    const auto& config = net_cmdline.config();
    const auto& networks = net_cmdline.networks();
    // TODO(rushfan): Load sub data here;
    // TODO(rushfan): Create the right API type for the right message area.
    MessageApiOptions options{};
    // By default, delete excess messages like net37 did.
    options.overflow_strategy = OverflowStrategy::delete_all;

    const auto user_manager = std::make_unique<UserManager>(config);
    SystemClock clock{};
    NetDat netdat(config.gfilesdir(), config.logdir(), net, net_cmdline.net_cmd(), clock);

    Context context(config, net, *user_manager, networks.networks(), netdat);
    context.network_number = net_cmdline.network_number();
    context.set_email_api(
        std::make_unique<WWIVMessageApi>(options, config, networks.networks(), new NullLastReadImpl()));
    context.set_api(2, std::make_unique<WWIVMessageApi>(options, config, networks.networks(),
                                                        new NullLastReadImpl()));

    LOG(INFO) << "Processing: " << net.dir << LOCAL_NET;
    if (handle_file(context, LOCAL_NET)) {
      if (net_cmdline.skip_delete()) {
        backup_file(FilePath(net.dir, LOCAL_NET));
      }
      LOG(INFO) << "Deleting: " << net.dir << LOCAL_NET;
      if (!File::Remove(FilePath(net.dir, LOCAL_NET))) {
        LOG(ERROR) << "ERROR: Unable to delete " << net.dir << LOCAL_NET;
      }
      update_filechange_status_dat(context.config.datadir(), email_changed, posts_changed);
      return 0;
    }
    LOG(ERROR) << "ERROR: handle_file returned false";
    return 1;
  } catch (const std::exception& e) {
    LOG(ERROR) << "ERROR: [network]: " << e.what();
  }

  return 255;
}

int main(int argc, char** argv) {
  LoggerConfig config(LogDirFromConfig);
  Logger::Init(argc, argv, config);
  ScopeExit at_exit(Logger::ExitLogger);
  CommandLine cmdline(argc, argv, "net");
  const NetworkCommandLine net_cmdline(cmdline, '2');
  if (!net_cmdline.IsInitialized() || net_cmdline.cmdline().help_requested()) {
    ShowHelp(net_cmdline);
    return 1;
  }

  try {
    auto semaphore =
        SemaphoreFile::try_acquire(net_cmdline.semaphore_path(), net_cmdline.semaphore_timeout());
    return network2_main(net_cmdline);
  } catch (const semaphore_not_acquired& e) {
    LOG(ERROR) << "ERROR: [network" << net_cmdline.net_cmd()
               << "]: Unable to Acquire Network Semaphore: " << e.what();
  }
}