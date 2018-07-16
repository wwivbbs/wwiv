/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2017, WWIV Software Services             */
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
#include "bbs/keycodes.h"
#include "bbs/misccmd.h"
#include "bbs/pause.h"
#include "bbs/platform/platformfcns.h"
#include "bbs/vars.h"
#include "bbs/wconstants.h"
#include "bbs/wfc.h"
#include "bbs/xfer.h"
#include "core/datafile.h"
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
#include "sdk/status.h"

using namespace std::chrono;
using std::string;
using std::to_string;
using std::unique_ptr;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::os;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

constexpr int MAX_CONNECTS = 1000;
std::chrono::steady_clock::time_point last_time_c_;

/** Returns a full path to exe under the WWIV_DIR */
static string CreateNetworkBinary(const std::string exe) {
  return (StrCat(a()->GetHomeDir(), exe));
}

struct CalloutEntry {
  CalloutEntry(uint16_t o, int e) : node(o), net(e) {}
  uint16_t node;
  int net;
};

static void rename_pend(const string& directory, const string& filename) {
  const string pend_filename = StringPrintf("%s%s", directory.c_str(), filename.c_str());
  const string num = filename.substr(1);
  const string prefix = (to_number<int>(num)) ? "p1-" : "p0-";

  for (int i = 0; i < 1000; i++) {
    const string new_filename = StringPrintf("%s%s%u.net", directory.c_str(), prefix.c_str(), i);
    if (File::Rename(pend_filename, new_filename) || errno != EACCES) {
      return;
    }
  }
}

static bool checkup2(const time_t tFileTime, const char* file_name) {
  File file(a()->network_directory(), file_name);

  if (file.Open(File::modeReadOnly)) {
    time_t tNewFileTime = file.last_write_time();
    file.Close();
    return (tNewFileTime > (tFileTime + 2));
  }
  return true;
}

static bool check_bbsdata() {
  unique_ptr<WStatus> wwiv_status_ro(a()->status_manager()->GetStatus());
  File bbsdataNet(a()->network_directory().c_str(), BBSDATA_NET);
  if (bbsdataNet.Open(File::modeReadOnly)) {
    time_t tFileTime = bbsdataNet.last_write_time();
    bbsdataNet.Close();
    bool ok = checkup2(tFileTime, BBSLIST_NET) || checkup2(tFileTime, CONNECT_NET);
    bool ok2 = checkup2(tFileTime, CALLOUT_NET);
    if (!ok && !ok2) {
      return false;
    }
  }
  if (!File::Exists(a()->network_directory(), BBSLIST_NET)) {
    return false;
  }
  if (!File::Exists(a()->network_directory().c_str(), CONNECT_NET)) {
    return false;
  }
  if (!File::Exists(a()->network_directory().c_str(), CALLOUT_NET)) {
    return false;
  }
  const string network3 = StrCat(CreateNetworkBinary("network3"), " .", a()->net_num(), " Y");
  ExecuteExternalProgram(network3, EFLAG_NETPROG);

  a()->status_manager()->Run(
      [](WStatus& s) { s.IncrementFileChangedFlag(WStatus::fileChangeNet); });

  return true;
}

static int cleanup_net1() {
  int ok, ok2, nl = 0, anynew = 0, i = 0;
  bool abort;

  a()->SetCleanNetNeeded(false);

  if (a()->net_networks.empty()) {
    return 0;
  }
  if (a()->net_networks[0].sysnum == 0 && a()->max_net_num() == 1) {
    return 0;
  }

  bool any = true;

  while (any && (nl++ < 10)) {
    any = false;

    for (int nNetNumber = 0; nNetNumber < a()->max_net_num(); nNetNumber++) {
      set_net_num(nNetNumber);

      if (!a()->current_net().sysnum) {
        continue;
      }

      a()->ClearTopScreenProtection();

      ok2 = 1;
      abort = false;
      while (ok2 && !abort) {
        ok2 = 0;
        ok = 0;
        string s = StrCat(a()->network_directory(), "p*", a()->network_extension());
        FindFiles ff(s, FindFilesType::files);
        for (const auto& f : ff) {
          ok = 1;
          ++i;
          rename_pend(a()->network_directory(), f.name);
          anynew = 1;
        }

        bool supports_process_net = a()->HasConfigFlag(OP_FLAGS_NET_PROCESS);
        if (supports_process_net) {
          if (!ok) {
            ok = File::ExistsWildcard(FilePath(a()->network_directory(), "p*.net"));
          }
          if (ok) {
            wfc_cls(a());
            ++i;
            hangup = false;
            a()->using_modem = 0;
            if (a()->IsUserOnline()) {
              hang_it_up();
            }
            // Try to run network3 before network1.
            if (!File::Exists(a()->network_directory(), BBSDATA_NET)) {
              check_bbsdata();
            }
            const string network1_cmd =
                StrCat(CreateNetworkBinary("network1"), " .", a()->net_num());
            if (ExecuteExternalProgram(network1_cmd, EFLAG_NETPROG) < 0) {
              abort = true;
            } else {
              any = true;
            }
            ok2 = 1;
          }
          if (File::Exists(a()->network_directory(), LOCAL_NET)) {
            wfc_cls(a());
            ++i;
            any = true;
            ok = 1;
            hangup = false;
            a()->using_modem = 0;
            string network2_cmd = StrCat(CreateNetworkBinary("network2"), " .", a()->net_num());
            if (ExecuteExternalProgram(network2_cmd, EFLAG_NETPROG) < 0) {
              abort = true;
            } else {
              any = true;
            }
            ok2 = 1;
            a()->status_manager()->RefreshStatusCache();
            a()->SetCurrentReadMessageArea(-1);
            a()->ReadCurrentUser(1);
          }
          if (check_bbsdata()) {
            ok2 = 1;
          }
          if (ok2) {
            a()->localIO()->Cls();
            ++i;
          }
        }
      }
    }
  }
  if (anynew && (a()->instance_number() != 1)) {
    send_inst_cleannet();
  }
  return i;
}

void cleanup_net() {
  if (cleanup_net1() && a()->HasConfigFlag(OP_FLAGS_NET_CALLOUT)) {
    wfc_cls(a());

    IniFile ini(FilePath(a()->GetHomeDir(), WWIV_INI),
                {StrCat("WWIV-", a()->instance_number()), INI_TAG});
    if (ini.IsOpen()) {
      const string cmd1 = ini.value<string>("NET_CLEANUP_CMD1");
      if (!cmd1.empty()) {
        ExecuteExternalProgram(cmd1, a()->GetSpawnOptions(SPAWNOPT_NET_CMD1));
        cleanup_net1();
      }
      const string cmd2 = ini.value<string>("NET_CLEANUP_CMD2");
      if (!cmd2.empty()) {
        ExecuteExternalProgram(cmd2, a()->GetSpawnOptions(SPAWNOPT_NET_CMD2));
        cleanup_net1();
      }
      ini.Close();
    }
  }
}

void do_callout(uint16_t sn) {
  Callout callout(a()->current_net());
  Contact contact(a()->current_net(), false);
  Binkp binkp(a()->current_net().dir);

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

  const string cmd = StrCat(CreateNetworkBinary("network"), " /N", sn, " .", a()->net_num());
  if (strncmp(csne->phone, "000", 3)) {
    // TODO(rushfan): Figure out a better way to see if we need to call WINS exp.exe
    run_exp();
    bout << "|#7Calling out to: |#2" << csne->name << " - " << a()->network_name() << " @" << sn
         << wwiv::endl;
    const string regions_filename =
        StringPrintf("%s.%-3u", REGIONS_DAT, to_number<unsigned int>(csne->phone));
    string region = "Unknown Region";
    if (File::Exists(FilePath(a()->config()->datadir(), REGIONS_DAT), regions_filename)) {
      const string town = StringPrintf("%c%c%c", csne->phone[4], csne->phone[5], csne->phone[6]);
      region = describe_area_code_prefix(to_number<int>(csne->phone), to_number<int>(town));
    } else {
      region = describe_area_code(to_number<int>(csne->phone));
    }
    bout << "|#7Sys located in: |#2" << region << wwiv::endl;
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
    run_exp();
    send_inst_cleannet();
  }
}

static bool ok_to_call(const net_call_out_rec* con) {

  bool ok = ((con->options & options_no_call) == 0) ? true : false;
  if (con->options & options_receive_only) {
    ok = false;
  }

  auto dt = DateTime::now();

  auto l = con->min_hr;
  auto h = con->max_hr;
  if (l > -1 && h > -1 && h != l) {
    if (h == 0 || h == 24) {
      if (dt.hour() < l) {
        ok = false;
      } else if (dt.hour() == l && dt.minute() < 12) {
        ok = false;
      } else if (dt.hour() == 23 && dt.minute() > 30) {
        ok = false;
      }
    } else if (l == 0 || l == 24) {
      if (dt.hour() >= h) {
        ok = false;
      } else if (dt.hour() == (h - 1) && dt.minute() > 30) {
        ok = false;
      } else if (dt.hour() == 0 && dt.minute() < 12) {
        ok = false;
      }
    } else if (h > l) {
      if (dt.hour() < l || dt.hour() >= h) {
        ok = false;
      } else if (dt.hour() == l && dt.minute() < 12) {
        ok = false;
      } else if (dt.hour() == (h - 1) && dt.minute() > 30) {
        ok = false;
      }
    } else {
      if (dt.hour() >= h && dt.hour() < l) {
        ok = false;
      } else if (dt.hour() == l && dt.minute() < 12) {
        ok = false;
      } else if (dt.hour() == (h - 1) && dt.minute() > 30) {
        ok = false;
      }
    }
  }
  return ok;
}

class NodeAndWeight {
public:
  NodeAndWeight() {}
  NodeAndWeight(int net_num, uint16_t node_num, uint64_t weight)
      : net_num_(net_num), node_num_(node_num), weight_(weight) {}
  int net_num_ = 0;
  uint16_t node_num_ = 0;
  uint64_t weight_ = 0;
};

/**
 * Checks the net_contact_rec and net_call_out_rec to ensure the node specified
 * is ok to call and does not violate any constraints.
 */
static bool ok_to_call_from_contact_rec(const NetworkContact& ncn, const net_call_out_rec& con) {
  const auto dt = DateTime::now();
  auto now = dt.to_system_clock();
  VLOG(2) << "ok_to_call_from_contact_rec: @" << con.sysnum << "." << a()->current_net().name;
  if (ncn.bytes_waiting() == 0L && !con.call_anyway) {
    VLOG(2) << "Skipping: No bytes waiting and !call anyway";
    return false;
  }
  auto min_minutes = std::max<int>(con.call_anyway, 1);
  auto last_contact = DateTime::from_time_t(ncn.lastcontact()).to_system_clock();
  auto last_contact_sent = DateTime::from_time_t(ncn.lastcontactsent()).to_system_clock();
  auto next_contact_time = last_contact + minutes(min_minutes);
  auto time_since_last_contact = now - last_contact;
  auto time_since_last_contact_sent = now - last_contact_sent;

  if ((con.options & options_once_per_day) && time_since_last_contact_sent < hours(24)) {
    VLOG(2) << "Skipping, it's not been a day (options_once_per_day)";
    return false;
  }
  if (con.call_anyway && now >= next_contact_time) {
    VLOG(2) << "Calling anyway since it's been time: ";
    VLOG(2) << "Last Contact: " << DateTime::from_time_t(ncn.lastcontactsent()).to_string();
    return true;
  }
  // TODO(rushfan): Should we just nix this and use / only?  
  auto daily_attempt_time = hours(20) / std::max<int>(1, con.times_per_day);
  if (time_since_last_contact > daily_attempt_time) {
    VLOG(2) << "Calling, daily_attempt_time:  min: " << duration_cast<minutes>(daily_attempt_time).count();
    VLOG(2) << "time_since_last_contact:      min: " << duration_cast<minutes>(time_since_last_contact).count();
    return true;
  }
  if (bytes_to_k(ncn.bytes_waiting()) > con.min_k) {
    VLOG(2) << "Calling: min_k";
    return true;
  }
  VLOG(2) << "Skipping; No reason to call.";
  return false;
}

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
  vector<NodeAndWeight> to_call(a()->max_net_num());

  for (int nn = 0; nn < a()->max_net_num(); nn++) {
    set_net_num(nn);
    if (!a()->current_net().sysnum) {
      continue;
    }

    Callout callout(a()->current_net());
    Contact contact(a()->current_net(), false);

    for (const auto& p : callout.callout_config()) {
      bool ok = ok_to_call(&p.second);
      if (!ok) {
        VLOG(2) << "!ok to call: @" << p.second.sysnum << "." << a()->current_net().name;
        continue;
      }

      const NetworkContact* ncr = contact.contact_rec_for(p.first);
      const net_call_out_rec* ncor = callout.net_call_out_for(p.first);
      if (!ncr || !ncor) {
        VLOG(2) << "skipping because: !ncr || !ncor";
        continue;
      }
      ok = ok_to_call_from_contact_rec(*ncr, *ncor);

      if (ok) {
        auto diff = time(nullptr) - ncr->lasttry();
        uint64_t time_weight = std::max<int64_t>(diff, 0);

        if (ncr->bytes_waiting() == 0L) {
          if (to_call.at(nn).weight_ < time_weight) {
            to_call[nn] = NodeAndWeight(nn, ncor->sysnum, time_weight);
          }
        } else {
          uint64_t bytes_weight = ncr->bytes_waiting() * 60 + time_weight;
          if (to_call.at(nn).weight_ < bytes_weight) {
            to_call[nn] = NodeAndWeight(nn, ncor->sysnum, bytes_weight);
          }
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
      do_callout(node.node_num_);
    }
  }
  return true;
}

void print_pending_list() {
  int adjust = 0, lines = 0;
  char s1[81], s2[81], s3[81], s4[81], s5[81];
  long ss = a()->user()->GetStatus();

  if (a()->net_networks.empty()) {
    return;
  }
  if (a()->net_networks[0].sysnum == 0 && a()->max_net_num() == 1) {
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

  int nNetNumber;
  for (nNetNumber = 0; nNetNumber < a()->max_net_num(); nNetNumber++) {
    set_net_num(nNetNumber);

    if (!a()->current_net().sysnum) {
      continue;
    }

    Callout callout(a()->current_net());
    Contact contact(a()->current_net(), false);

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

      if (ok_to_call(&con)) {
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
                   s2, a()->network_name(), r->systemnumber(), s3, s4, s5, r->numfails(), s1);
      if (!a()->user()->HasPause() && ((lines++) == 20)) {
        pausescr();
        lines = 0;
      }
    }
  }

  for (nNetNumber = 0; nNetNumber < a()->max_net_num(); nNetNumber++) {
    set_net_num(nNetNumber);

    if (!a()->current_net().sysnum) {
      continue;
    }

    File deadNetFile(a()->network_directory(), DEAD_NET);
    if (deadNetFile.Open(File::modeReadOnly | File::modeBinary)) {
      auto lFileSize = deadNetFile.length();
      deadNetFile.Close();
      sprintf(s3, "%ldk", (lFileSize + 1023) / 1024);
      bout.bprintf("|#7\xB3 |#3--- |#7\xB3 |#2%-8s |#7\xB3 |#6DEAD! |#7\xB3 |#2------- |#7\xB3 "
                   "|#2------- |#7\xB3|#2%5s "
                   "|#7\xB3|#2 --- |#7\xB3 |#2--------- |#7\xB3\r\n",
                   a()->network_name(), s3);
    }
  }

  for (nNetNumber = 0; nNetNumber < a()->max_net_num(); nNetNumber++) {
    set_net_num(nNetNumber);

    if (!a()->current_net().sysnum) {
      continue;
    }

    File checkNetFile(a()->network_directory(), CHECK_NET);
    if (checkNetFile.Open(File::modeReadOnly | File::modeBinary)) {
      auto lFileSize = checkNetFile.length();
      checkNetFile.Close();
      sprintf(s3, "%ldk", (lFileSize + 1023) / 1024);
      strcat(s3, "k");
      bout.bprintf("|#7\xB3 |#3--- |#7\xB3 |#2%-8s |#7\xB3 |#6CHECK |#7\xB3 |#2------- |#7\xB3 "
                   "|#2------- |#7\xB3|#2%5s |#7\xB3|#2 --- |#7\xB3 |#2--------- |#7\xB3\r\n",
                   a()->network_name(), s3);
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

    if (nFromNetworkNumber == -1 || nh->fromsys == a()->net_networks[nFromNetworkNumber].sysnum) {

      strcpy(newname, nm);
      ss = strrchr(newname, '@');
      if (ss) {
        sprintf(ss + 1, "%u", a()->net_networks[nNetNumber].sysnum);
        ss = strrchr(nm, '@');
        if (ss) {
          ++ss;
          while ((*ss >= '0') && (*ss <= '9')) {
            ++ss;
          }
          strcat(newname, ss);
        }
        strcat(newname, "\r\n");
        nh->fromsys = a()->net_networks[nNetNumber].sysnum;
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
      if (a()->net_networks[nFromNetworkNumber].sysnum == 1 && on[0] &&
          a()->net_networks[nFromNetworkNumber].type == network_type_t::internet) {
        sprintf(newname, "%s%s", qn, on);
      } else {
        if (on[0]) {
          sprintf(newname, "%s%s@%u.%s\r\n", qn, on, nh->fromsys,
                  a()->net_networks[nFromNetworkNumber].name);
        } else {
          sprintf(newname, "%s#%u@%u.%s\r\n", qn, nh->fromuser, nh->fromsys,
                  a()->net_networks[nFromNetworkNumber].name);
        }
      }
      nh->fromsys = a()->net_networks[nNetNumber].sysnum;
      nh->fromuser = 0;
    }

    nh->length += strlen(newname);
    if ((nh->main_type == main_type_email_name) || (nh->main_type == main_type_new_post)) {
      nh->length += subtype_or_author.size() + 1;
    }
    const auto packet_filename =
        StrCat(a()->net_networks[nNetNumber].dir, "p1", a()->network_extension());
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

static void print_call(uint16_t sn, int nNetNumber) {
  static int color, got_color = 0;
  time_t tCurrentTime = time(nullptr);

  set_net_num(nNetNumber);
  Callout callout(a()->current_net());
  Contact contact(a()->current_net(), false);
  Binkp binkp(a()->current_net().dir);

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
    IniFile ini(FilePath(a()->GetHomeDir(), WWIV_INI),
                {StrCat("WWIV-", a()->instance_number()), INI_TAG});
    if (ini.IsOpen()) {
      color = ini.value("CALLOUT_COLOR_TEXT", 14);
    }
  }
  string s1 = to_string(bytes_to_k(ncn->bytes_waiting()));
  a()->localIO()->PrintfXYA(58, 17, color, "%-10.16s", s1.c_str());

  s1 = to_string(bytes_to_k(ncn->bytes_received()));
  a()->localIO()->PrintfXYA(23, 17, color, "%-10.16s", s1.c_str());

  s1 = to_string(bytes_to_k(ncn->bytes_sent()));
  a()->localIO()->PrintfXYA(23, 18, color, "%-10.16s", s1.c_str());

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
  a()->localIO()->PrintfXYA(23, 16, color, "%-17.16s", s1.c_str());

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
  a()->localIO()->PrintfXYA(58, 16, color, "%-17.16s", s1.c_str());

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
  a()->localIO()->PrintfXYA(58, 15, color, "%-17.16s", s1.c_str());
  a()->localIO()->PrintfXYA(23, 15, color, "%-16u", ncn->numcontacts());
  a()->localIO()->PrintfXYA(41, 3, color, "%-30.30s", csne->name);
  auto binkp_node = binkp.binkp_session_config_for(csne->sysnum);
  string hostname = csne->phone;
  string speed = StrCat(to_string(csne->speed), " BPS");
  if (binkp_node != nullptr) {
    // Use host:port is we have it.
    hostname = StrCat(binkp_node->host, ":", binkp_node->port);
    speed = "BinkP";
  }
  a()->localIO()->PrintfXYA(23, 19, color, "%-30.30s", hostname.c_str());
  a()->localIO()->PrintfXYA(58, 18, color, "%-10.16s", speed.c_str());
  a()->localIO()->PrintfXYA(14, 3, color, "%-11.16s", a()->network_name());
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
    curatr = color;
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
    IniFile ini(FilePath(a()->GetHomeDir(), WWIV_INI),
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
  for (int nNetNumber = 0; nNetNumber < a()->max_net_num(); nNetNumber++) {
    set_net_num(nNetNumber);
    Callout callout(a()->current_net());
    Contact contact(a()->current_net(), false);

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

  a()->localIO()->Cls();
  curatr = color1;
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
  a()->localIO()->PrintfXYA(6, 5, color2, "%-5u", entries[pos].node);
  print_call(entries[pos].node, entries[pos].net);
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
          a()->localIO()->PrintfXYA(6 + x, 5 + y, color4, "%-5u", entries[pos].node);
          pos++;
          x += 7;
          a()->localIO()->PrintfXYA(6 + x, 5 + y, color2, "%-5u", entries[pos].node);
          print_call(entries[pos].node, entries[pos].net);
        }
        break;
      case LARROW: // left arrow
        if (x > 0) {
          a()->localIO()->PrintfXYA(6 + x, 5 + y, color4, "%-5u", entries[pos].node);
          pos--;
          x -= 7;
          a()->localIO()->PrintfXYA(6 + x, 5 + y, color2, "%-5u", entries[pos].node);
          print_call(entries[pos].node, entries[pos].net);
        }
        break;
      case UPARROW: // up arrow
        if (y > 0) {
          a()->localIO()->PrintfXYA(6 + x, 5 + y, color4, "%-5u", entries[pos].node);
          pos -= 10;
          y--;
          a()->localIO()->PrintfXYA(6 + x, 5 + y, color2, "%-5u", entries[pos].node);
          print_call(entries[pos].node, entries[pos].net);
        } else if (rownum > 0) {
          pos -= 10;
          rownum--;
          fill_call(color4, rownum, entries);
          a()->localIO()->PrintfXYA(6 + x, 5 + y, color2, "%-5u", entries[pos].node);
          print_call(entries[pos].node, entries[pos].net);
        }
        break;
      case DNARROW: // down arrow
        if ((y < 5) && (pos + 10 < size_int(entries))) {
          a()->localIO()->PrintfXYA(6 + x, 5 + y, color4, "%-5u", entries[pos].node);
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
        a()->localIO()->PrintfXYA(6 + x, 5 + y, color2, "%-5u", entries[pos].node);
        print_call(entries[pos].node, entries[pos].net);
        break;
      case HOME: // home
        if (pos > 0) {
          x = 0;
          y = 0;
          pos = 0;
          rownum = 0;
          fill_call(color4, rownum, entries);
          a()->localIO()->PrintfXYA(6, 5, color2, "%-5u", entries[pos].node);
          print_call(entries[pos].node, entries[pos].net);
        }
      case PAGEUP: // page up
        if (y > 0) {
          a()->localIO()->PrintfXYA(6 + x, 5 + y, color4, "%-5u", entries[pos].node);
          pos -= 10 * y;
          y = 0;
          a()->localIO()->PrintfXYA(6 + x, 5 + y, color2, "%-5u", entries[pos].node);
          print_call(entries[pos].node, entries[pos].net);
        } else {
          if (rownum > 5) {
            pos -= 60;
            rownum -= 6;
          } else {
            pos -= 10 * rownum;
            rownum = 0;
          }
          fill_call(color4, rownum, entries);
          a()->localIO()->PrintfXYA(6 + x, 5 + y, color2, "%-5u", entries[pos].node);
          print_call(entries[pos].node, entries[pos].net);
        }
        break;
      case PAGEDN: // page down
        if (y < 5) {
          a()->localIO()->PrintfXYA(6 + x, 5 + y, color4, "%-5u", entries[pos].node);
          pos += 10 * (5 - y);
          y = 5;
          while (pos >= size_int(entries)) {
            pos -= 10;
            --y;
          }
          a()->localIO()->PrintfXYA(6 + x, 5 + y, color2, "%-5u", entries[pos].node);
          print_call(entries[pos].node, entries[pos].net);
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
          a()->localIO()->PrintfXYA(6 + x, 5 + y, color2, "%-5u", entries[pos].node);
          print_call(entries[pos].node, entries[pos].net);
        }
        break;
      }
    }
  } while (!done);

  curatr = color3;
  a()->localIO()->Cls();
  a()->localIO()->SetCursor(LocalIO::cursorNormal);
  return std::make_pair(sn, snn);
}

static int FindNetworkNumberForNode(int sn) {
  for (int nNetNumber = 0; nNetNumber < a()->max_net_num(); nNetNumber++) {
    set_net_num(nNetNumber);
    Callout callout(a()->current_net());
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

  set_net_num(network_number);
  Callout callout(a()->current_net());

  if (!ok_to_call(callout.net_call_out_for(sn.first))) {
    return;
  }

  a()->localIO()->Cls();
  do_callout(sn.first);
}

void run_exp() {
  int nOldNetworkNumber = a()->net_num();
  int internet_net_num = getnetnum_by_type(network_type_t::internet);
  if (internet_net_num == -1) {
    return;
  }
  set_net_num(internet_net_num);

  const string exp_command = StringPrintf(
      "exp s32767.net %s %d %s %s %s", a()->network_directory().c_str(), a()->current_net().sysnum,
      a()->internetEmailName.c_str(), a()->internetEmailDomain.c_str(), a()->network_name());
  ExecuteExternalProgram(exp_command, EFLAG_NETPROG);

  set_net_num(nOldNetworkNumber);
  a()->localIO()->Cls();
}

std::chrono::steady_clock::time_point last_network_attempt() { return last_time_c_; }