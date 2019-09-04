/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2019, WWIV Software Services             */
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
/*                                                                        */
/**************************************************************************/
#include "bbs/netsup.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "bbs/bbs.h"
#include "bbs/bbsutl1.h"
#include "bbs/com.h"
#include "bbs/connect1.h"
#include "bbs/datetime.h"
#include "bbs/execexternal.h"
#include "bbs/input.h"
#include "bbs/instmsg.h"
#include "local_io/keycodes.h"
#include "bbs/misccmd.h"
#include "bbs/pause.h"

#include "local_io/wconstants.h"
#include "bbs/wfc.h"
#include "bbs/xfer.h"
#include "core/file.h"
#include "core/findfiles.h"
#include "core/inifile.h"
#include "core/os.h"
#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/wwivport.h"
#include "sdk/bbslist.h"
#include "sdk/binkp.h"
#include "sdk/callout.h"
#include "sdk/contact.h"
#include "sdk/filenames.h"
#include "sdk/net/callouts.h"
#include "sdk/status.h"

using namespace std::chrono;
using std::string;
using std::to_string;
using std::unique_ptr;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::os;
using namespace wwiv::sdk;
using namespace wwiv::sdk::net;
using namespace wwiv::stl;
using namespace wwiv::strings;

constexpr int MAX_CONNECTS = 1000;
std::chrono::steady_clock::time_point last_time_c_;

/** Returns a full path to exe under the WWIV_DIR */
static string CreateNetworkBinary(const std::string exe) {
  std::ostringstream ss;
  ss << FilePath(a()->bindir(), exe);
  ss << " --v=" << a()->verbose();
  ss << " --bbsdir=" << a()->bbsdir();
  ss << " --bindir=" << a()->bindir();
  ss << " --configdir=" << a()->configdir();

  return ss.str();
}

struct CalloutEntry {
  CalloutEntry(uint16_t o, int e) : node(o), net(e) {}
  uint16_t node;
  int net;
};

void cleanup_net() {
  a()->SetCleanNetNeeded(false);

  if (a()->net_networks.empty()) {
    return;
  }
  if (a()->net_networks[0].sysnum == 0 && wwiv::stl::size_int(a()->net_networks) == 1) {
    return;
  }
  a()->hangup_ = false;
  a()->using_modem = 0;
  if (a()->IsUserOnline()) {
    hang_it_up();
  }

  for (int nNetNumber = 0;
       nNetNumber < wwiv::stl::size_int(a()->net_networks);
       nNetNumber++) {
    set_net_num(nNetNumber);
    const auto& net = a()->net_networks[nNetNumber];
    VLOG(2) << "cleanup_net: Processing Network: " << net.name;

    if (!net.sysnum) {
      VLOG(1) << "Skipping network due to no sysnum: " << net.name;
      continue;
    }

    a()->ClearTopScreenProtection();

    std::ostringstream ss;
    ss << CreateNetworkBinary("networkc");
    ss << " --quiet";
    ss << " --process_instance=" << a()->instance_number();
    ss << " ." << a()->net_num();
    const auto networkc_cmd = ss.str();
    VLOG(1) << "Executing Network Command: '" << networkc_cmd << "'";
    ExecuteExternalProgram(networkc_cmd, EFLAG_NETPROG);
    a()->status_manager()->RefreshStatusCache();
    a()->SetCurrentReadMessageArea(-1);
    a()->ReadCurrentUser(1);
  }
  a()->Cls();
}

static void do_callout(const net_networks_rec& net, uint16_t sn) {
  Callout callout(net);
  Contact contact(net, false);
  Binkp binkp(net.dir);

  const net_call_out_rec* callout_rec = callout.net_call_out_for(sn); // i con
  if (callout_rec == nullptr) {
    return;
  }

  const NetworkContact* contact_rec = contact.contact_rec_for(sn); // i2 ncn
  if (!contact_rec) {
    return;
  }
  net_system_list_rec* csne = next_system(callout_rec->sysnum);
  if (!csne) {
    return;
  }

  const auto cmd = StrCat(CreateNetworkBinary("network"), " /N", sn, " .", a()->net_num());
  bout << "|#7Calling out to: |#2" << csne->name << " - " << net.name << " @" << sn << wwiv::endl;
  if (contact_rec->bytes_waiting() > 0) {
    bout << "|#7Amount pending: |#2" << bytes_to_k(contact_rec->bytes_waiting()) << "k"
         << wwiv::endl;
  }
  bout << "|#7Commandline is: |#2" << cmd << wwiv::endl
       << "|#7" << std::string(80, '\xCD') << "|#0..." << wwiv::endl;
  ExecuteExternalProgram(cmd, EFLAG_NETPROG);
  a()->status_manager()->RefreshStatusCache();
  last_time_c_ = steady_clock::now();
  cleanup_net();
  send_inst_cleannet();
}

class NodeAndWeight {
public:
  NodeAndWeight() {}
  NodeAndWeight(int net_num, uint16_t node_num, uint64_t weight)
      : net_num_(net_num), node_num_(node_num), weight_(weight) {}
  int net_num_ = 0;
  uint16_t node_num_ = 0;
  int64_t weight_ = 0;
};

bool attempt_callout() {
  a()->status_manager()->RefreshStatusCache();

  auto now = steady_clock::now();
  if (last_time_c_ == steady_clock::time_point::min()) {
    // Set it to 11s ago, so we try a callout right away.
    last_time_c_ = now - seconds(11);
    VLOG(2) << "Setting last time to now";
    return false;
  }
  if (now - last_time_c_ < seconds(10)) {
    VLOG(3) << "not attempting callout, been less to 10s";
    return false;
  }

  // Set the last connect time to now since we are attempting to connect.
  last_time_c_ = now;
  wwiv::core::ScopeExit set_net_num_zero([&]() { set_net_num(0); });
  vector<NodeAndWeight> to_call(wwiv::stl::size_int(a()->net_networks));

  for (int nn = 0; nn < wwiv::stl::size_int(a()->net_networks); nn++) {
    set_net_num(nn);
    const auto& net = a()->net_networks[nn];
    if (!net.sysnum) {
      continue;
    }

    Callout callout(net);
    Contact contact(net, false);

    for (const auto& p : callout.callout_config()) {
      const net_call_out_rec& ncor = p.second;
      const NetworkContact* ncr = contact.contact_rec_for(p.first);
      if (!ncr) {
        VLOG(2) << "skipping because: !ncr || !ncor";
        continue;
      }
      if (!should_call(*ncr, ncor, DateTime::now())) {
        continue;
      }

      auto diff = time(nullptr) - ncr->lasttry();
      auto time_weight = std::max<int64_t>(diff, 0);

      if (ncr->bytes_waiting() == 0L) {
        if (to_call.at(nn).weight_ < time_weight) {
          to_call[nn] = NodeAndWeight(nn, ncor.sysnum, time_weight);
        }
      } else {
        auto bytes_weight = ncr->bytes_waiting() * 60 + time_weight;
        if (to_call.at(nn).weight_ < bytes_weight) {
          to_call[nn] = NodeAndWeight(nn, ncor.sysnum, bytes_weight);
        }
      }
    }
  }

  if (to_call.empty()) {
    VLOG(2) << "Skipping: to_call is empty";
    return false;
  }
  for (const auto& node : to_call) {
    set_net_num(node.net_num_);
    if (node.weight_ != 0) {
      VLOG(1) << "Attempting callout on: @" << node.node_num_;
      do_callout(a()->net_networks[node.net_num_] , node.node_num_);
    }
  }
  return true;
}

void print_pending_list() {
  int adjust = 0, lines = 0;
  char s1[81], s2[81], s3[81], s4[81], s5[81];
  auto ss = a()->user()->GetStatus();

  if (a()->net_networks.empty()) {
    return;
  }
  if (a()->net_networks[0].sysnum == 0 && wwiv::stl::size_int(a()->net_networks) == 1) {
    return;
  }

  auto current_time = DateTime::now();

  bout.nl(2);
  bout << "                           |#3-> |#9Network Status |#3<-\r\n";
  bout.nl();

  bout << "|#"
          "7\xDA\xC4\xC4\xC4\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC2\xC4\xC4\xC4"
          "\xC4\xC4\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xC4\xC4"
          "\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xC4"
          "\xC4\xC4\xC4\xC4\xC4\xBF\r\n";
  bout << "|#7\xB3 |#1Ok? |#7\xB3 |#1Network  |#7\xB3 |#1 Node |#7\xB3  |#1 Sent  "
          "|#7\xB3|#1Received |#7\xB3|#1Ready |#7\xB3|#1Fails|#7\xB3  |#1Elapsed  |#7\xB3|#7\r\n";
  bout << "|#"
          "7\xC3\xC4\xC4\xC4\xC4\xC4\xC5\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC5\xC4\xC4\xC4"
          "\xC4\xC4\xC4\xC4\xC5\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC5\xC4\xC4\xC4\xC4\xC4\xC4\xC4"
          "\xC4\xC4\xC5\xC4\xC4\xC4\xC4\xC4\xC4\xC5\xC4\xC4\xC4\xC4\xC4\xC5\xC4\xC4\xC4\xC4\xC4\xC4"
          "\xC4\xC4\xC4\xC4\xC4\xB4\r\n";

  for (const auto& net : a()->net_networks) {
    if (!net.sysnum) {
      continue;
    }

    Callout callout(net);
    Contact contact(net, false);

    for (const auto& p : callout.callout_config()) {
      const NetworkContact* r = contact.contact_rec_for(p.first);
      NetworkContact empty_contact;
      if (!r) {
        // default it to a null entry.
        r = &empty_contact;
      }
      const auto& con = p.second;
      if (con.options & options_hide_pend) {
        // skip hidden ones.
        continue;
      }

      if (allowed_to_call(con, DateTime::now())) {
        strcpy(s2, "|#5Yes");
      } else {
        strcpy(s2, "|#3---");
      }

      int32_t m = 0, h = 0;
      if (r->lastcontactsent()) {
        auto then = DateTime::from_time_t(r->lastcontactsent());
        auto diff = current_time.to_system_clock() - then.to_system_clock();
        to_char_array(s1, ctim(diff));
      } else {
        strcpy(s1, "|#6     -    ");
      }

      sprintf(s3, "%dk", ((r->bytes_sent()) + 1023) / 1024);
      sprintf(s4, "%dk", ((r->bytes_received()) + 1023) / 1024);
      sprintf(s5, "%dk", ((r->bytes_waiting()) + 1023) / 1024);

      if (m >= 30) {
        h++;
      }
      if (m < 31) {
        h--;
      }

      bout.bprintf("|#7\xB3 %-3s |#7\xB3 |#2%-8.8s |#7\xB3 |#2%5u |#7\xB3|#2%8s |#7\xB3|#2%8s "
                   "|#7\xB3|#2%5s |#7\xB3|#2%4d |#7\xB3|#2%13.13s |#7\xB3|#7\r\n",
                   s2, a()->network_name().c_str(), r->systemnumber(), s3, s4, s5, r->numfails(), s1);
      if (!a()->user()->HasPause() && ((lines++) == 20)) {
        pausescr();
        lines = 0;
      }
    }
  }

  for (const auto& net : a()->net_networks) {
    if (!net.sysnum) {
      continue;
    }

    File deadNetFile(PathFilePath(net.dir, DEAD_NET));
    if (deadNetFile.Open(File::modeReadOnly | File::modeBinary)) {
      auto dead_net_file_size = deadNetFile.length();
      deadNetFile.Close();
      sprintf(s3, "%ldk", (dead_net_file_size + 1023) / 1024);
      bout.bprintf("|#7\xB3 |#3--- |#7\xB3 |#2%-8s |#7\xB3 |#6DEAD! |#7\xB3 |#2------- |#7\xB3 "
                   "|#2------- |#7\xB3|#2%5s "
                   "|#7\xB3|#2 --- |#7\xB3 |#2--------- |#7\xB3\r\n",
                   net.name, s3);
    }
  }

  for (const auto& net : a()->net_networks) {
    if (!net.sysnum) {
      continue;
    }

    File checkNetFile(PathFilePath(net.dir, CHECK_NET));
    if (checkNetFile.Open(File::modeReadOnly | File::modeBinary)) {
      auto check_net_file_size = checkNetFile.length();
      checkNetFile.Close();
      sprintf(s3, "%ldk", (check_net_file_size + 1023) / 1024);
      strcat(s3, "k");
      bout.bprintf("|#7\xB3 |#3--- |#7\xB3 |#2%-8s |#7\xB3 |#6CHECK |#7\xB3 |#2------- |#7\xB3 "
                   "|#2------- |#7\xB3|#2%5s |#7\xB3|#2 --- |#7\xB3 |#2--------- |#7\xB3\r\n",
                   net.name, s3);
    }
  }

  bout << "|#"
          "7\xc0\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4"
          "\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC4"
          "\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4"
          "\xC4\xC4\xC4\xC4\xC4\xD9\r\n";
  bout.nl();
  a()->user()->SetStatus(ss);
  if (!a()->IsUserOnline() && bout.lines_listed()) {
    pausescr();
  }
}

void gate_msg(net_header_rec* nh, char* messageText, int nNetNumber,
              const std::string& subtype_or_author, vector<uint16_t> list, int nFromNetworkNumber) {
  char newname[256], qn[200], on[200];
  char nm[205];
  int i;
  const auto& net = a()->net_networks[nFromNetworkNumber];

  if (strlen(messageText) >= 80) {
    return;
  }

  char* pszOriginalText = messageText;
  messageText += strlen(pszOriginalText) + 1;
  unsigned short ntl = static_cast<uint16_t>(nh->length - strlen(pszOriginalText) - 1);
  char* ss = strchr(messageText, '\r');
  if (ss && (ss - messageText < 200) && (ss - messageText < ntl)) {
    strncpy(nm, messageText, ss - messageText);
    nm[ss - messageText] = 0;
    ss++;
    if (*ss == '\n') {
      ss++;
    }
    nh->length -= (ss - messageText);
    ntl = ntl - static_cast<uint16_t>(ss - messageText);
    messageText = ss;

    qn[0] = on[0] = '\0';

    if (nFromNetworkNumber == -1 || nh->fromsys == net.sysnum) {

      strcpy(newname, nm);
      ss = strrchr(newname, '@');
      if (ss) {
        sprintf(ss + 1, "%u", net.sysnum);
        ss = strrchr(nm, '@');
        if (ss) {
          ++ss;
          while ((*ss >= '0') && (*ss <= '9')) {
            ++ss;
          }
          strcat(newname, ss);
        }
        strcat(newname, "\r\n");
        nh->fromsys = net.sysnum;
      }
    } else {
      if ((nm[0] == '`') && (nm[1] == '`')) {
        for (i = strlen(nm) - 2; i > 0; i--) {
          if ((nm[i] == '`') && (nm[i + 1] == '`')) {
            break;
          }
        }
        if (i > 0) {
          i += 2;
          strncpy(qn, nm, i);
          qn[i] = ' ';
          qn[i + 1] = 0;
        }
      } else {
        i = 0;
      }
      if (qn[0] == 0) {
        ss = strrchr(nm, '#');
        if (ss) {
          if ((ss[1] >= '0') && (ss[1] <= '9')) {
            *ss = 0;
            ss--;
            while ((ss > nm) && (*ss == ' ')) {
              *ss = 0;
              ss--;
            }
          }
        }
        if (nm[0]) {
          if (nh->fromuser) {
            sprintf(qn, "``%s`` ", nm);
          } else {
            strcpy(on, nm);
          }
        }
      }
      if ((on[0] == 0) && (nh->fromuser == 0)) {
        strcpy(on, nm + i);
      }
      if (net.sysnum == 1 && on[0] &&
          net.type == network_type_t::internet) {
        sprintf(newname, "%s%s", qn, on);
      } else if (net.sysnum == 1 && on[0] &&
                 net.type == network_type_t::news) {
        sprintf(newname, "%s%s", qn, on);
      }
      else {
        if (on[0]) {
          sprintf(newname, "%s%s@%u.%s\r\n", qn, on, nh->fromsys,
                  net.name);
        } else {
          sprintf(newname, "%s#%u@%u.%s\r\n", qn, nh->fromuser, nh->fromsys,
                  net.name);
        }
      }
      nh->fromsys = net.sysnum;
      nh->fromuser = 0;
    }

    nh->length += strlen(newname);
    if ((nh->main_type == main_type_email_name) || (nh->main_type == main_type_new_post)) {
      nh->length += subtype_or_author.size() + 1;
    }
    const auto packet_filename = StrCat(net.dir, "p1", a()->network_extension());
    File file(packet_filename);
    if (file.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
      file.Seek(0L, File::Whence::end);
      if (list.empty()) {
        nh->list_len = 0;
      }
      if (nh->list_len) {
        nh->tosys = 0;
      }
      file.Write(nh, sizeof(net_header_rec));
      if (nh->list_len) {
        file.Write(&list[0], sizeof(uint16_t) * (nh->list_len));
      }
      if ((nh->main_type == main_type_email_name) || (nh->main_type == main_type_new_post)) {
        file.Write(subtype_or_author.c_str(), subtype_or_author.size() + 1);
      }
      file.Write(pszOriginalText, strlen(pszOriginalText) + 1);
      file.Write(newname, strlen(newname));
      file.Write(messageText, ntl);
      file.Close();
    }
  }
}

// begin callout additions

static void print_call(uint16_t sn, const net_networks_rec& net) {
  static int color, got_color = 0;
  time_t tCurrentTime = time(nullptr);

  Callout callout(net);
  Contact contact(net, false);
  Binkp binkp(net.dir);

  const NetworkContact* ncn = contact.contact_rec_for(sn);
  if (!ncn) {
    return;
  }
  net_system_list_rec* csne = next_system(sn);
  if (!csne) {
    return;
  }

  if (!got_color) {
    got_color = 1;
    IniFile ini(PathFilePath(a()->bbsdir(), WWIV_INI),
                {StrCat("WWIV-", a()->instance_number()), INI_TAG});
    if (ini.IsOpen()) {
      color = ini.value("CALLOUT_COLOR_TEXT", 14);
    }
  }
  auto s1 = to_string(bytes_to_k(ncn->bytes_waiting()));
  a()->localIO()->PutsXYA(58, 17, color, s1);

  s1 = to_string(bytes_to_k(ncn->bytes_received()));
  a()->localIO()->PutsXYA(23, 17, color, s1);

  s1 = to_string(bytes_to_k(ncn->bytes_sent()));
  a()->localIO()->PutsXYA(23, 18, color, s1);

  if (ncn->firstcontact() > 0) {
    s1 = StrCat(to_string((tCurrentTime - ncn->firstcontact()) / SECONDS_PER_HOUR), ":");
    time_t tTime = (((tCurrentTime - ncn->firstcontact()) % SECONDS_PER_HOUR) / 60);
    string s = to_string(tTime);
    if (tTime < 10) {
      s1 += StrCat("0", s);
    } else {
      s1 += s;
    }
    s1 += " Hrs";
  } else {
    s1 = "NEVER";
  }
  a()->localIO()->PutsXYA(23, 16, color, s1);

  if (ncn->lastcontactsent() > 0) {
    s1 = StrCat(to_string((tCurrentTime - ncn->lastcontactsent()) / SECONDS_PER_HOUR), ":");
    time_t tTime = (((tCurrentTime - ncn->lastcontactsent()) % SECONDS_PER_HOUR) / 60);
    string s = to_string(tTime);
    if (tTime < 10) {
      s1 += StrCat("0", s);
    } else {
      s1 += s;
    }
    s1 += " Hrs";
  } else {
    s1 = "NEVER";
  }
  a()->localIO()->PutsXYA(58, 16, color, s1);

  if (ncn->lasttry() > 0) {
    string tmp = to_string((tCurrentTime - ncn->lasttry()) / SECONDS_PER_HOUR);
    s1 = StrCat(tmp, ":");
    time_t tTime = (((tCurrentTime - ncn->lasttry()) % SECONDS_PER_HOUR) / 60);
    string s = to_string(tTime);
    if (tTime < 10) {
      s1 += StrCat("0", s);
    } else {
      s1 += s;
    }
    s1 += " Hrs";
  } else {
    s1 = "NEVER";
  }
  a()->localIO()->PutsXYA(58, 15, color, s1);
  a()->localIO()->PutsXYA(23, 15, color, pad_to(std::to_string(ncn->numcontacts()), 16));
  a()->localIO()->PutsXYA(41, 3, color, pad_to(csne->name, 30));
  auto binkp_node = binkp.binkp_session_config_for(csne->sysnum);
  string hostname = csne->phone;
  auto speed = StrCat(to_string(csne->speed), " BPS");
  if (binkp_node != nullptr) {
    // Use host:port is we have it.
    hostname = StrCat(binkp_node->host, ":", binkp_node->port);
    speed = "BinkP";
  }
  a()->localIO()->PutsXYA(23, 19, color, pad_to(hostname, 30));
  a()->localIO()->PutsXYA(58, 18, color, pad_to(speed, 10));
  a()->localIO()->PutsXYA(14, 3, color, pad_to(a()->network_name(), 16));
}

static void fill_call(int color, int row, const std::vector<CalloutEntry>& entries) {
  int x = 0, y = 0;
  char s1[6];

  for (int i = row * 10; (i < ((row + 6) * 10)); i++) {
    if (x > 69) {
      x = 0;
      y++;
    }
    if (i < size_int(entries)) {
      sprintf(s1, "%-5u", entries.at(i).node);
    } else {
      strcpy(s1, "     ");
    }
    bout.curatr(color);
    a()->localIO()->PutsXY(6 + x, 5 + y, s1);
    x += 7;
  }
}

static std::pair<uint16_t, int> ansicallout() {
  static bool callout_ansi = true;
  static int color1, color2, color3, color4;
  static bool got_info = false;
  int rownum = 0;
  a()->localIO()->SetCursor(LocalIO::cursorNone);
  if (!got_info) {
    got_info = true;
    color1 = 9;
    color2 = 30;
    color3 = 3;
    color4 = 14;
    IniFile ini(PathFilePath(a()->bbsdir(), WWIV_INI),
                {StrCat("WWIV-", a()->instance_number()), INI_TAG});
    if (ini.IsOpen()) {
      callout_ansi = ini.value<bool>("CALLOUT_ANSI");
      color1 = ini.value("CALLOUT_COLOR", color1);
      color2 = ini.value("CALLOUT_HIGHLIGHT", color2);
      color3 = ini.value("CALLOUT_NORMAL", color3);
      color4 = ini.value("CALLOUT_COLOR_TEXT", color4);
      ini.Close();
    }
  }

  if (!callout_ansi) {
    bout.nl();
    bout << "|#2Which system: ";
    uint16_t sn = input_number<uint16_t>(0, 0, 32767);
    a()->localIO()->SetCursor(LocalIO::cursorNormal);
    return std::make_pair(sn, -1);
  }
  int pos = 0, sn = 0, snn = 0;
  std::vector<CalloutEntry> entries;
  for (int nNetNumber = 0; nNetNumber < wwiv::stl::size_int(a()->net_networks); nNetNumber++) {
    set_net_num(nNetNumber);
    const auto& net = a()->net_networks[nNetNumber];
    Callout callout(net);
    Contact contact(net, false);

    const auto& nodemap = callout.callout_config();
    for (const auto& p : nodemap) {
      auto con = contact.contact_rec_for(p.first);
      if (!con)
        continue;
      if ((!(p.second.options & options_hide_pend)) && valid_system(p.second.sysnum)) {
        entries.emplace_back(con->systemnumber(), nNetNumber);
      }
    }
    if (entries.size() > MAX_CONNECTS) {
      break;
    }
  }

  if (entries.empty()) {
    return std::make_pair<uint16_t, int>(0, -1);
  }

  a()->Cls();
  bout.curatr(color1);
  a()->localIO()->MakeLocalWindow(3, 2, 73, 10);
  const auto header = StrCat("\xC3", std::string(71, '\xC4'), "\xB4");
  a()->localIO()->PutsXYA(3, 4, color1, header);
  a()->localIO()->MakeLocalWindow(3, 14, 73, 7);
  a()->localIO()->PutsXYA(5, 3, color3, "Network:");
  a()->localIO()->PutsXYA(31, 3, color3, "BBS Name:");
  a()->localIO()->PutsXYA(5, 15, color3, "# Connections   :");
  a()->localIO()->PutsXYA(5, 16, color3, "First Contact   :");
  a()->localIO()->PutsXYA(5, 17, color3, "KB Received     :");
  a()->localIO()->PutsXYA(5, 18, color3, "KB Sent         :");
  a()->localIO()->PutsXYA(5, 19, color3, "Address         :");
  a()->localIO()->PutsXYA(40, 15, color3, "Last Attempt    :");
  a()->localIO()->PutsXYA(40, 16, color3, "Last Contact    :");
  a()->localIO()->PutsXYA(40, 17, color3, "KB Waiting      :");
  a()->localIO()->PutsXYA(40, 18, color3, "Max Speed       :");

  fill_call(color4, rownum, entries);
  int x = 0;
  int y = 0;
  a()->localIO()->PutsXYA(6, 5, color2, StringPrintf("%-5u", entries[pos].node));
  print_call(entries[pos].node, a()->net_networks[entries[pos].net]);
  char ch = 0;

  bool done = false;
  do {
    ch = to_upper_case<char>(static_cast<char>(a()->localIO()->GetChar()));
    switch (ch) {
    case ' ':
    case RETURN:
      sn = entries.at(pos).node;
      snn = entries.at(pos).net;
      done = true;
      break;
    case 'Q':
    case ESC:
      sn = 0;
      snn = -1;
      done = true;
      break;
    case -32: // (224) I don't know MS's CRT returns this on arrow keys....
    case 0:
      ch = to_upper_case<char>(static_cast<char>(a()->localIO()->GetChar()));
      switch (ch) {
      case RARROW: // right arrow
        if ((pos < size_int(entries) - 1) && (x < 63)) {
          a()->localIO()->PutsXYA(6 + x, 5 + y, color4, StringPrintf("%-5u", entries[pos].node));
          pos++;
          x += 7;
          a()->localIO()->PutsXYA(6 + x, 5 + y, color2, StringPrintf("%-5u", entries[pos].node));
          print_call(entries[pos].node, a()->net_networks[entries[pos].net]);
        }
        break;
      case LARROW: // left arrow
        if (x > 0) {
          a()->localIO()->PutsXYA(6 + x, 5 + y, color4, StringPrintf("%-5u", entries[pos].node));
          pos--;
          x -= 7;
          a()->localIO()->PutsXYA(6 + x, 5 + y, color2, StringPrintf("%-5u", entries[pos].node));
          print_call(entries[pos].node, a()->net_networks[entries[pos].net]);
        }
        break;
      case UPARROW: // up arrow
        if (y > 0) {
          a()->localIO()->PutsXYA(6 + x, 5 + y, color4, StringPrintf("%-5u", entries[pos].node));
          pos -= 10;
          y--;
          a()->localIO()->PutsXYA(6 + x, 5 + y, color2, StringPrintf("%-5u", entries[pos].node));
          print_call(entries[pos].node, a()->net_networks[entries[pos].net]);
        } else if (rownum > 0) {
          pos -= 10;
          rownum--;
          fill_call(color4, rownum, entries);
          a()->localIO()->PutsXYA(6 + x, 5 + y, color2, StringPrintf("%-5u", entries[pos].node));
          print_call(entries[pos].node, a()->net_networks[entries[pos].net]);
        }
        break;
      case DNARROW: // down arrow
        if ((y < 5) && (pos + 10 < size_int(entries))) {
          a()->localIO()->PutsXYA(6 + x, 5 + y, color4, StringPrintf("%-5u", entries[pos].node));
          pos += 10;
          y++;
        } else if ((rownum + 6) * 10 < size_int(entries)) {
          rownum++;
          fill_call(color4, rownum, entries);
          if (pos + 10 < size_int(entries)) {
            pos += 10;
          } else {
            --y;
          }
        }
        a()->localIO()->PutsXYA(6 + x, 5 + y, color2, StringPrintf("%-5u", entries[pos].node));
        print_call(entries[pos].node, a()->net_networks[entries[pos].net]);
        break;
      case HOME: // home
        if (pos > 0) {
          x = 0;
          y = 0;
          pos = 0;
          rownum = 0;
          fill_call(color4, rownum, entries);
          a()->localIO()->PutsXYA(6, 5, color2, StringPrintf("%-5u", entries[pos].node));
          print_call(entries[pos].node, a()->net_networks[entries[pos].net]);
        }
      case PAGEUP: // page up
        if (y > 0) {
          a()->localIO()->PutsXYA(6 + x, 5 + y, color4, StringPrintf("%-5u", entries[pos].node));
          pos -= 10 * y;
          y = 0;
          a()->localIO()->PutsXYA(6 + x, 5 + y, color2, StringPrintf("%-5u", entries[pos].node));
          print_call(entries[pos].node, a()->net_networks[entries[pos].net]);
        } else {
          if (rownum > 5) {
            pos -= 60;
            rownum -= 6;
          } else {
            pos -= 10 * rownum;
            rownum = 0;
          }
          fill_call(color4, rownum, entries);
          a()->localIO()->PutsXYA(6 + x, 5 + y, color2, StringPrintf("%-5u", entries[pos].node));
          print_call(entries[pos].node, a()->net_networks[entries[pos].net]);
        }
        break;
      case PAGEDN: // page down
        if (y < 5) {
          a()->localIO()->PutsXYA(6 + x, 5 + y, color4, StringPrintf("%-5u", entries[pos].node));
          pos += 10 * (5 - y);
          y = 5;
          while (pos >= size_int(entries)) {
            pos -= 10;
            --y;
          }
          a()->localIO()->PutsXYA(6 + x, 5 + y, color2, StringPrintf("%-5u", entries[pos].node));
          print_call(entries[pos].node, a()->net_networks[entries[pos].net]);
        } else if ((rownum + 6) * 10 < size_int(entries)) {
          for (int i1 = 0; i1 < 6; i1++) {
            if ((rownum + 6) * 10 < size_int(entries)) {
              rownum++;
              pos += 10;
            }
          }
          fill_call(color4, rownum, entries);
          if (pos >= size_int(entries)) {
            pos -= 10;
            --y;
          }
          a()->localIO()->PutsXYA(6 + x, 5 + y, color2, StringPrintf("%-5u", entries[pos].node));
          print_call(entries[pos].node, a()->net_networks[entries[pos].net]);
        }
        break;
      }
    }
  } while (!done);

  bout.curatr(color3);
  a()->Cls();
  a()->localIO()->SetCursor(LocalIO::cursorNormal);
  return std::make_pair(sn, snn);
}

static int FindNetworkNumberForNode(int sn) {
  for (int nNetNumber = 0; nNetNumber < wwiv::stl::size_int(a()->net_networks); nNetNumber++) {
    const auto net = a()->net_networks[nNetNumber];
    Callout callout(net);
    if (callout.net_call_out_for(sn) != nullptr) {
      return nNetNumber;
    }
  }
  return -1;
}

void force_callout() {
  auto sn = ansicallout();
  if (sn.first <= 0) {
    return;
  }

  int network_number = sn.second;
  if (network_number < 0) {
    network_number = FindNetworkNumberForNode(sn.first);
    if (network_number < 0) {
      return;
    }
  }
  const auto& net = a()->net_networks[network_number];

  set_net_num(network_number);
  Callout callout(net);

  if (!allowed_to_call(*callout.net_call_out_for(sn.first), DateTime::now())) {
    return;
  }

  a()->Cls();
  do_callout(net, sn.first);
}

std::chrono::steady_clock::time_point last_network_attempt() { return last_time_c_; }
