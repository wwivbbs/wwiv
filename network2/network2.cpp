/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*                Copyright (C)2016, WWIV Software Services               */
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
#include <cctype>
#include <cstdlib>
#include <ctime>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <set>
#include <string>
#include <vector>

#include "core/command_line.h"
#include "core/datafile.h"
#include "core/file.h"
#include "core/log.h"
#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/os.h"
#include "core/textfile.h"
#include "core/wfndfile.h"
#include "networkb/binkp.h"
#include "networkb/binkp_config.h"
#include "networkb/connection.h"
#include "networkb/net_util.h"
#include "networkb/packets.h"
#include "networkb/ppp_config.h"
#include "network2/context.h"
#include "network2/email.h"
#include "network2/post.h"
#include "network2/subs.h"

#include "sdk/bbslist.h"
#include "sdk/callout.h"
#include "sdk/connect.h"
#include "sdk/config.h"
#include "sdk/contact.h"
#include "sdk/datetime.h"
#include "sdk/filenames.h"
#include "sdk/networks.h"
#include "sdk/ssm.h"
#include "sdk/subxtr.h"
#include "sdk/vardec.h"
#include "sdk/usermanager.h"
#include "sdk/msgapi/msgapi.h"
#include "sdk/msgapi/message_api_wwiv.h"

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
using namespace wwiv::net::network2;
using namespace wwiv::os;
using namespace wwiv::sdk;
using namespace wwiv::sdk::msgapi;
using namespace wwiv::stl;
using namespace wwiv::strings;


static void ShowHelp(CommandLine& cmdline) {
  cout << cmdline.GetHelp()
       << ".####      Network number (as defined in INIT)" << endl
       << endl;
  exit(1);
}

static bool handle_ssm(Context& context, const net_header_rec& nh, 
  std::vector<uint16_t>& list,  const std::string& text) {
  ScopeExit at_exit([] {
    VLOG(1) << "==============================================================";
  });
  VLOG(1) << "==============================================================";
  VLOG(1) << "  Receiving SSM for user: #" << nh.touser;
  SSM ssm(context.config, context.user_manager);
  if (!ssm.send_local(nh.touser, text)) {
    LOG(ERROR) << "  ERROR writing SSM: '" << text << "'; writing to dead.net";
    return write_packet(DEAD_NET, context.net, nh, list, text);
  }

  LOG(INFO) << "    + SSM  '" << text << "'";
  return true;
}

static string NetInfoFileName(uint16_t type) {
  switch (type) {
  case net_info_bbslist: return BBSLIST_NET;
  case net_info_connect: return CONNECT_NET;
  case net_info_sub_lst: return SUBS_LST;
  case net_info_wwivnews: return "wwivnews.net";
  case net_info_more_wwivnews: return "wwivnews.net";
  case net_info_categ_net: return CATEG_NET;
  case net_info_network_lst: return "networks.lst";
  case net_info_file: return "";
  case net_info_binkp: return BINKP_NET;
  }
  return "";
}

static bool handle_net_info_file(const net_networks_rec& net,
  const net_header_rec& nh, std::vector<uint16_t>& list, const string& text) {

  string filename = NetInfoFileName(nh.minor_type);
  if (nh.minor_type == net_info_file) {
    // we don't know the filename
    LOG(ERROR) << "ERROR: net_info_file not supported; writing to dead.net";
    return write_packet(DEAD_NET, net, nh, list, text);
  } else if (!filename.empty()) {
    // we know the name.
    File file(net.dir, filename);
    if (!file.Open(File::modeWriteOnly | File::modeBinary | File::modeCreateFile | File::modeTruncate, 
      File::shareDenyReadWrite)) {
      // We couldn't create or open the file.
      LOG(ERROR) << "ERROR: Unable to create or open file: " << filename << " writing to dead.net";
      return write_packet(DEAD_NET, net, nh, list, text);
    }
    file.Write(text);
    LOG(INFO) << "  + Got " << filename;
    return true;
  }
  // error.
  LOG(ERROR) << "ERROR: Fell through handle_net_info_file; writing to dead.net";
  return write_packet(DEAD_NET, net, nh, list, text);
}

static bool handle_packet(
  Context& context,
  const net_header_rec& nh, std::vector<uint16_t>& list, const string& text) {
  LOG(INFO) << "Processing message with type: " << main_type_name(nh.main_type)
      << "/" << nh.minor_type;

  switch (nh.main_type) {
    /*
    These messages contain various network information
    files, encoded with method 1 (requiring DE1.EXE).
    Once DE1.EXE has verified the source and returned to
    the analyzer, the file is created in the network's
    DATA directory with the filename determined by the
    minor_type (except minor_type 1).
    */
  case main_type_net_info:
    if (nh.minor_type == 0) {
      // Feedback to sysop from the NC.  
      // This is sent to the #1 account as source verified email.
      return handle_email(context, 1, nh, list, text);
    } else {
      return handle_net_info_file(context.net, nh, list, text);
    }
  break;
  case main_type_email:
    // This is regular email sent to a user number at this system.
    // Email has no minor type, so minor_type will always be zero.
    return handle_email(context, nh.touser, nh, list, text);
    break;
  // The other email type.  The touser field is zero, and the name is found at
  // the beginning of the message text, followed by a NUL character.
  // Minor_type will always be zero.
  case main_type_email_name:
    // This is regular email sent to a user number at this system.
    // Email has no minor type, so minor_type will always be zero.
    return handle_email_byname(context, nh, list, text);
    break;
  case main_type_new_post:
  {
    return handle_post(context, nh, list, text);
  } break;
  case main_type_ssm:
  {
    return handle_ssm(context, nh, list, text);
  } break;
  // Subs add/drop support.
  case main_type_sub_add_req:
    return handle_sub_add_req(context, nh, text);
  case main_type_sub_drop_req:
    return handle_sub_drop_req(context, nh, text);
  case main_type_sub_add_resp:
    return handle_sub_add_drop_resp(context, nh, "add", text);
  case main_type_sub_drop_resp:
    return handle_sub_add_drop_resp(context, nh, "drop", text);

  // Sub ping.
  // In many WWIV networks, the subs list coordinator (SLC) occasionally sends
  // out "pings" to all network members.
  case main_type_sub_list_info:
    if (nh.minor_type == 0) {
      return handle_sub_list_info_request(context, nh);
    } else {
      return handle_sub_list_info_response(context, nh, text);
    }

  // Legacy numeric only post types.
  case main_type_post:
  case main_type_pre_post:

  // EPROGS.NET support 
  case main_type_external:
  case main_type_new_external:

  // NetEdiit.
  case main_type_net_edit:

  // *.### support
  case main_type_sub_list:
  case main_type_group_bbslist:
  case main_type_group_connect:
  case main_type_group_info:
    // Anything undefined or anything we missed.
  default:
    LOG(ERROR) << "Writing message to dead.net for unhandled type: " << main_type_name(nh.main_type);
    return write_packet(DEAD_NET, context.net, nh, list, text);
  }

  return false;
}

static bool handle_file(Context& context, const string& name) {
  File f(context.net.dir, name);
  if (!f.Open(File::modeBinary | File::modeReadOnly)) {
    LOG(ERROR) << "Unable to open file: " << context.net.dir << name;
    return false;
  }

  bool done = false;
  while (!done) {
    net_header_rec nh;
    string text;
    int num_read = f.Read(&nh, sizeof(net_header_rec));
    if (num_read == 0) {
      // at the end of the packet.
      return true;
    }
    if (num_read != sizeof(net_header_rec)) {
      LOG(INFO) << "error reading header, got short read of size: " << num_read
        << "; expected: " << sizeof(net_header_rec);
      return false;
    }
    if (nh.method > 0) {
      LOG(INFO) << "compression: de" << nh.method;
    }

    std::vector<uint16_t> list;
    if (nh.list_len > 0) {
      // skip list of addresses.
      list.resize(nh.list_len);
      f.Read(&list[0], 2 * nh.list_len);
    }
    if (nh.length > 0) {
      int length = nh.length;
      if (nh.method > 0) {
        length -= 146; // sizeof EN/DE header.
        // HACK - this should do this in a shim DE
        nh.length -= 146; 
        char header[147];
        f.Read(header, 146);
      }
      text.resize(length);
      f.Read(&text[0], length);
    }
    if (!handle_packet(context, nh, list, text)) {
      LOG(ERROR) << "Error handing packet: type: " << nh.main_type;
    }
  }
  return true;
}

INITIALIZE_EASYLOGGINGPP
int main(int argc, char** argv) {
  Logger::Init(argc, argv);
  try {
    ScopeExit at_exit(Logger::ExitLogger);
    CommandLine cmdline(argc, argv, "network_number");
    cmdline.set_no_args_allowed(true);
    cmdline.AddStandardArgs();
    AddStandardNetworkArgs(cmdline, File::current_directory());

    if (!cmdline.Parse() || cmdline.arg("help").as_bool()) {
      ShowHelp(cmdline);
      return 2;
    }
    NetworkCommandLine net_cmdline(cmdline);
    if (!net_cmdline.IsInitialized()) {
      return 1;
    }

    const auto& net = net_cmdline.network();
    LOG(INFO) << "NETWORK2 for network: " << net.name;

    if (!File::Exists(net.dir, LOCAL_NET)) {
      LOG(INFO) << "No local.net exists. exiting.";
      return 0;
    }

    const auto& bbsdir = net_cmdline.bbsdir();
    const auto& config = net_cmdline.config();
    const auto& networks = net_cmdline.networks();
    unique_ptr<WWIVMessageApi> api = make_unique<WWIVMessageApi>(
      bbsdir, config.datadir(), config.msgsdir(), networks.networks());
    unique_ptr<UserManager> user_manager = make_unique<UserManager>(
      config.config()->datadir, config.config()->userreclen, config.config()->maxusers);
    Context context(config, net, *user_manager.get(), *api.get());
    context.subs = std::move(read_subs(config.datadir()));
    context.network_number = net_cmdline.network_number();
    if (!read_subs_xtr(config.datadir(), networks.networks(), context.subs, context.xsubs)) {
      LOG(ERROR) << "ERROR: Failed to read file: " << SUBS_XTR;
      return 5;
    }

    LOG(INFO) << "Processing: " << net.dir << LOCAL_NET;
    if (handle_file(context, LOCAL_NET)) {
      LOG(INFO) << "Deleting: " << net.dir << LOCAL_NET;
      if (!File::Remove(net.dir, LOCAL_NET)) {
        LOG(ERROR) << "ERROR: Unable to delete " << net.dir << LOCAL_NET;
      }
      return 0;
    } else {
      LOG(ERROR) << "ERROR: handle_file returned false";
      return 1;
    }
  } catch (const std::exception& e) {
    LOG(ERROR) << "ERROR: [network]: " << e.what();
  }

  return 255;
}
