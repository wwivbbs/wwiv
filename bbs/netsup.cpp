/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2016, WWIV Software Services             */
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
#include <memory>
#include <string>
#include <vector>

#include "bbs/datetime.h"
#include "bbs/execexternal.h"
#include "bbs/input.h"
#include "bbs/keycodes.h"
#include "sdk/status.h"
#include "bbs/bbs.h"
#include "bbs/fcns.h"
#include "bbs/instmsg.h"
#include "bbs/vars.h"
#include "bbs/wconstants.h"
#include "bbs/wfc.h"
#include "core/datafile.h"
#include "core/file.h"
#include "core/inifile.h"
#include "core/os.h"
#include "core/scope_exit.h"
#include "core/strings.h"
#include "core/wfndfile.h"
#include "core/wwivport.h"
#include "sdk/filenames.h"

static int netw;
time_t last_time_c;

using std::chrono::seconds;
using std::string;
using std::to_string;
using std::unique_ptr;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::os;
using namespace wwiv::sdk;
using namespace wwiv::strings;

static void rename_pend(const string& directory, const string& filename) {
  const string pend_filename = StringPrintf("%s%s", directory.c_str(), filename.c_str());
  const string num = filename.substr(1);
  const string prefix = (atoi(num.c_str())) ? "p1-" : "p0-";

  for (int i = 0; i < 1000; i++) {
    const string new_filename = StringPrintf("%s%s%u.net", directory.c_str(), prefix.c_str(), i);
    if (File::Rename(pend_filename, new_filename) || errno != EACCES) {
      return;
    }
  }
}

static bool checkup2(const time_t tFileTime, const char *file_name) {
  File file(session()->network_directory(), file_name);

  if (file.Open(File::modeReadOnly)) {
    time_t tNewFileTime = file.last_write_time();
    file.Close();
    return (tNewFileTime > (tFileTime + 2));
  }
  return true;
}

static bool check_bbsdata() {
  unique_ptr<WStatus> wwiv_status_ro(session()->status_manager()->GetStatus());
  File bbsdataNet(session()->network_directory().c_str(), BBSDATA_NET);
  if (bbsdataNet.Open(File::modeReadOnly)) {
    time_t tFileTime = bbsdataNet.last_write_time();
    bbsdataNet.Close();
    bool ok = checkup2(tFileTime, BBSLIST_NET) || checkup2(tFileTime, CONNECT_NET);
    bool ok2 = checkup2(tFileTime, CALLOUT_NET);
    if (!ok && !ok2) {
      return false;
    }
  }
  if (!File::Exists(session()->network_directory(), BBSLIST_NET)) {
    return false;
  }
  if (!File::Exists(session()->network_directory().c_str(), CONNECT_NET)) {
    return false;
  }
  if (!File::Exists(session()->network_directory().c_str(), CALLOUT_NET)) {
    return false;
  }
  const string network3 = StringPrintf("network3 .%d Y", session()->net_num());
  ExecuteExternalProgram(network3, EFLAG_NETPROG);
  WStatus* wwiv_status = session()->status_manager()->BeginTransaction();
  wwiv_status->IncrementFileChangedFlag(WStatus::fileChangeNet);
  session()->status_manager()->CommitTransaction(wwiv_status);

  zap_call_out_list();
  zap_contacts();
  zap_bbs_list();
  return true;
}

static int cleanup_net1() {
  int ok, ok2, nl = 0, anynew = 0, i = 0;
  bool abort;     

  session()->SetCleanNetNeeded(false);

  if (session()->net_networks.empty()) {
    return 0;
  }
  if (session()->net_networks[0].sysnum == 0 && session()->max_net_num() == 1) {
    return 0;
  }

  int any = 1;

  while (any && (nl++ < 10)) {
    any = 0;

    for (int nNetNumber = 0; nNetNumber < session()->max_net_num(); nNetNumber++) {
      set_net_num(nNetNumber);

      if (!net_sysnum) {
        continue;
      }

      session()->ClearTopScreenProtection();

      ok2 = 1;
      abort = false;
      while (ok2 && !abort) {
        ok2 = 0;
        ok = 0;
        WFindFile fnd;
        string s = StringPrintf("%sp*%s", 
          session()->network_directory().c_str(), session()->network_extension().c_str());
        bool bFound = fnd.open(s, 0);
        while (bFound) {
          ok = 1;
          ++i;
          rename_pend(session()->network_directory(), fnd.GetFileName());
          anynew = 1;
          bFound = fnd.next();
        }

        bool supports_process_net = session()->HasConfigFlag(OP_FLAGS_NET_PROCESS);
        if (supports_process_net) {
          if (!ok) {
            WFindFile fnd_net;
            ok = fnd_net.open(StrCat(session()->network_directory(), "p*.net"), 0);
          }
          if (ok) {
            if (session()->wfc_status == 0) {
              // WFC addition
              session()->localIO()->LocalCls();
            } else {
              wfc_cls();
            }
            ++i;
            hangup = false;
            session()->using_modem = 0;
            if (ExecuteExternalProgram(StringPrintf("network1 .%d", session()->net_num()), EFLAG_NETPROG) < 0) {
              abort = true;
            } else {
              any = 1;
            }
            ok2 = 1;
          }
          if (File::Exists(session()->network_directory(), LOCAL_NET)) {
            if (session()->wfc_status == 0) {
              // WFC addition
              session()->localIO()->LocalCls();
            } else {
              wfc_cls();
            }
            ++i;
            any = 1;
            ok = 1;
            hangup = false;
            session()->using_modem = 0;
            if (ExecuteExternalProgram(StringPrintf("network2 .%d", session()->net_num()), EFLAG_NETPROG) < 0) {
              abort = true;
            } else {
              any = 1;
            }
            ok2 = 1;
            session()->status_manager()->RefreshStatusCache();
            session()->SetCurrentReadMessageArea(-1);
            session()->ReadCurrentUser(1);
          }
          if (check_bbsdata()) {
            ok2 = 1;
          }
          if (ok2) {
            session()->localIO()->LocalCls();
            zap_contacts();
            ++i;
          }
        }
      }
    }
  }
  if (anynew && (session()->instance_number() != 1)) {
    send_inst_cleannet();
  }
  return i;
}

void cleanup_net() {
  if (cleanup_net1() && session()->HasConfigFlag(OP_FLAGS_NET_CALLOUT)) {
    if (session()->wfc_status == 0) {
      session()->localIO()->LocalCls();
    }

    IniFile iniFile(FilePath(session()->GetHomeDir(), WWIV_INI), INI_TAG);
    if (iniFile.IsOpen()) {
      const char *pszValue = iniFile.GetValue("NET_CLEANUP_CMD1");
      if (pszValue != nullptr) {
        ExecuteExternalProgram(pszValue, session()->GetSpawnOptions(SPAWNOPT_NET_CMD1));
        cleanup_net1();
      }
      pszValue = iniFile.GetValue("NET_CLEANUP_CMD2");
      if (pszValue != nullptr) {
        ExecuteExternalProgram(pszValue, session()->GetSpawnOptions(SPAWNOPT_NET_CMD2));
        cleanup_net1();
      }
      iniFile.Close();
    }
  }

}

void do_callout(int sn) {
  time_t tCurrentTime = time(nullptr);
  int i = -1;
  int i2 = -1;
  if (!session()->current_net().con) {
    read_call_out_list();
  }
  for (int i1 = 0; i1 < session()->current_net().num_con; i1++) {
    if (session()->current_net().con[i1].sysnum == sn) {
      i = i1;
    }
  }
  if (!session()->current_net().ncn) {
    read_contacts();
  }
  for (int i1 = 0; i1 < session()->current_net().num_ncn; i1++) {
    if (session()->current_net().ncn[i1].systemnumber == sn) {
      i2 = i1;
    }
  }

  if (i == -1) {
    return;
  }
  net_system_list_rec *csne = next_system(session()->current_net().con[i].sysnum);
  if (!csne) {
    return;
  }
  std::ostringstream cmd;
  cmd << "network /N" << sn 
      << " /A" << ((session()->current_net().con[i].options & options_sendback) ? "1" : "0")
      << " /P" << csne->phone 
      << " /S0 /T" << tCurrentTime;

  if (session()->current_net().con[i].macnum) {
    cmd << " /M" << static_cast<uint32_t>(session()->current_net().con[i].macnum);
  }
  cmd << " ." << session()->net_num();
  if (strncmp(csne->phone, "000", 3)) {
    run_exp();
    bout << "|#7Calling out to: |#2" << csne->name << " - ";
    if (session()->max_net_num() > 1) {
      bout << session()->network_name() << " ";
    }
    bout << "@" << sn << wwiv::endl;
    const string regions_filename = StringPrintf("%s%s%c%s.%-3u", syscfg.datadir,
            REGIONS_DAT, File::pathSeparatorChar, REGIONS_DAT, atoi(csne->phone));
    string region = "Unknown Region";
    if (File::Exists(regions_filename)) {
      const string town = StringPrintf("%c%c%c", csne->phone[4], csne->phone[5], csne->phone[6]);
      region = describe_area_code_prefix(atoi(csne->phone), atoi(town.c_str()));
    } else {
      region = describe_area_code(atoi(csne->phone));
    }
    bout << "|#7Sys located in: |#2" << region << wwiv::endl;
    if (i2 != -1 && session()->current_net().ncn[i2].bytes_waiting) {
      bout << "|#7Amount pending: |#2"
            << bytes_to_k(session()->current_net().ncn[i2].bytes_waiting)
            << "k" << wwiv::endl;
    }
    bout << "|#7Commandline is: |#2" << cmd.str() << wwiv::endl
         << "|#7" << std::string(80, '\xCD') << "|#0..." << wwiv::endl;
    ExecuteExternalProgram(cmd.str(), EFLAG_NETPROG);
    zap_contacts();
    session()->status_manager()->RefreshStatusCache();
    last_time_c = static_cast<int>(tCurrentTime);
    cleanup_net();
    run_exp();
    send_inst_cleannet();
  }
}

static bool ok_to_call(int i) {
  net_call_out_rec *con = &(session()->current_net().con[i]);

  bool ok = ((con->options & options_no_call) == 0) ? true : false;
  if (con->options & options_receive_only) {
    ok = false;
  }
  int nDow = dow();

  time_t t;
  time(&t);
  struct tm * pTm = localtime(&t);

  if (con->options & options_ATT_night) {
    if (nDow != 0 && nDow != 6) {
      if (pTm->tm_hour < 23 && pTm->tm_hour >= 7) {
        ok = false;
      }
    }
    if (nDow == 0) {
      if (pTm->tm_hour < 23 && pTm->tm_hour >= 16) {
        ok = false;
      }
    }
  }
  char l = con->min_hr;
  char h = con->max_hr;
  if (l > -1 && h > -1 && h != l) {
    if (h == 0 || h == 24) {
      if (pTm->tm_hour < l) {
        ok = false;
      } else if (pTm->tm_hour == l && pTm->tm_min < 12) {
        ok = false;
      } else if (pTm->tm_hour == 23 && pTm->tm_min > 30) {
        ok = false;
      }
    } else if (l == 0 || l == 24) {
      if (pTm->tm_hour >= h) {
        ok = false;
      } else if (pTm->tm_hour == (h - 1) && pTm->tm_min > 30) {
        ok = false;
      } else if (pTm->tm_hour == 0 && pTm->tm_min < 12) {
        ok = false;
      }
    } else if (h > l) {
      if (pTm->tm_hour < l || pTm->tm_hour >= h) {
        ok = false;
      } else if (pTm->tm_hour == l && pTm->tm_min < 12) {
        ok = false;
      } else if (pTm->tm_hour == (h - 1) && pTm->tm_min > 30) {
        ok = false;
      }
    } else {
      if (pTm->tm_hour >= h && pTm->tm_hour < l) {
        ok = false;
      } else if (pTm->tm_hour == l && pTm->tm_min < 12) {
        ok = false;
      } else if (pTm->tm_hour == (h - 1) && pTm->tm_min > 30) {
        ok = false;
      }
    }
  }
  return ok;
}

#define WEIGHT 30.0

void fixup_long(uint32_t *f, time_t l) {
  if (*f > static_cast<uint32_t>(l)) {
    *f = static_cast<uint32_t>(l);
  }

  if (*f + (SECONDS_PER_DAY * 30L) < static_cast<uint32_t>(l)) {
    *f = static_cast<uint32_t>(l) - (SECONDS_PER_DAY * 30L);
  }
}

class NodeAndWeight {
public:
  NodeAndWeight() {}
  NodeAndWeight(int net_num, int node_num, uint64_t weight)
    : net_num_(net_num), node_num_(node_num), weight_(weight) {}
  int net_num_ = 0;
  int node_num_ = 0;
  uint64_t weight_ = 0;
};

/**
 * Checks the net_contact_rec and net_call_out_rec to ensure the node specified
 * is ok to call and does not violate any constraints.
 */
static bool ok_to_call_from_contact_rec(const net_contact_rec& ncn, const net_call_out_rec& con) {
  time_t tCurrentTime = time(nullptr);
  if (ncn.bytes_waiting == 0L) {
    if (!con.call_anyway) {
      return false;
    }
    time_t next_contact_time = ncn.lastcontact + SECONDS_PER_HOUR * con.call_anyway;
    if (tCurrentTime < next_contact_time) {
      return false;
    }
  } else {
    // Only retry connections hourly if we have bytes waiting.
    time_t next_contact_time = ncn.lastcontact + SECONDS_PER_HOUR * 1;
    if (tCurrentTime < next_contact_time) {
      return false;
    }
  }
  if (con.options & options_once_per_day) {
    if (std::abs(tCurrentTime - ncn.lastcontactsent) <
      (20L * SECONDS_PER_HOUR / con.times_per_day)) {
      return false;
    }
  }
  if ((bytes_to_k(ncn.bytes_waiting) < con.min_k)
    && (std::abs(tCurrentTime - ncn.lastcontact) < SECONDS_PER_DAY)) {
    return false;
  }
  return true;
}

bool attempt_callout() {
  int i, i1;
  session()->status_manager()->RefreshStatusCache();

  time_t tCurrentTime = time(nullptr);
  if (last_time_c > tCurrentTime || last_time_c == 0) {
    last_time_c = tCurrentTime;
    return false;
  }
  if (std::abs(last_time_c - tCurrentTime) < 10) {
    return false;
  }

  bool any = false;

  // Set the last connect time to now since we are attempting to connect.
  last_time_c = tCurrentTime;
  wwiv::core::ScopeExit set_net_num_zero([&]() { set_net_num(0); });
  vector<NodeAndWeight> to_call(session()->max_net_num());

  for (int nNetNumber = 0; nNetNumber < session()->max_net_num(); nNetNumber++) {
    set_net_num(nNetNumber);
    if (!net_sysnum) {
      continue;
    }

    read_call_out_list();
    read_contacts();

    net_call_out_rec *con = session()->current_net().con;
    net_contact_rec *ncn = session()->current_net().ncn;
    int num_call_sys = session()->current_net().num_con;
    int num_ncn = session()->current_net().num_ncn;

    for (i = 0; i < num_call_sys; i++) {
      bool ok = ok_to_call(i);
      if (!ok) {
        continue;
      }

      int ncn_index = -1;
      for (i1 = 0; i1 < num_ncn; i1++) {
        if (ncn[i1].systemnumber == con[i].sysnum) {
          ncn_index = i1;
        }
      }
      if (ncn_index == -1) {
        continue;
      }

      ok = ok_to_call_from_contact_rec((ncn[ncn_index]), con[i]);

      fixup_long(&(ncn[ncn_index].lastcontactsent), tCurrentTime);
      fixup_long(&(ncn[ncn_index].lasttry), tCurrentTime);

      if (ok) {
        uint64_t time_weight = tCurrentTime - ncn[ncn_index].lasttry;

        if (ncn[ncn_index].bytes_waiting == 0L) {
          if (to_call.at(nNetNumber).weight_ < time_weight) {
            to_call[nNetNumber] = NodeAndWeight(
              nNetNumber, session()->current_net().con[i].sysnum, time_weight);
          }
        } else {
          uint64_t bytes_weight = ncn[ncn_index].bytes_waiting * 60 + time_weight;
          if (to_call.at(nNetNumber).weight_ < bytes_weight) {
            to_call[nNetNumber] = NodeAndWeight(
              nNetNumber, session()->current_net().con[i].sysnum, bytes_weight);
          }
        }
        any = true;
      }

    }
  }

  if (!any) {
    return false;
  }
  for (const auto& node : to_call) {
    set_net_num(node.net_num_);
    if (node.weight_ != 0) {
      do_callout(node.node_num_);
    }
  }
  return true;
}

void print_pending_list() {
  int i = 0,
      i1 = 0,
      i2 = 0,
      i3 = 0,
      num_ncn = 0,
      num_call_sys = 0;
  int adjust = 0, lines = 0;
  char s1[81], s2[81], s3[81], s4[81], s5[81];
  time_t tCurrentTime;
  time_t t = time(nullptr);
  struct tm* pTm = localtime(&t);

  long ss = session()->user()->GetStatus();

  int nDow = dow();

  if (session()->net_networks.empty()) {
    return;
  }
  if (session()->net_networks[0].sysnum == 0 && session()->max_net_num() == 1) {
    return;
  }

  time(&tCurrentTime);

  bout.nl(2);
  bout << "                           |#3-> |#9Network Status |#3<-\r\n";
  bout.nl();

  bout << "|#7\xDA\xC4\xC4\xC4\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC2\xC4\xC4\xC4\xC4\xC4\xBF\r\n";
  bout << "|#7\xB3 |#1Ok? |#7\xB3 |#1Network  |#7\xB3 |#1 Node |#7\xB3  |#1 Sent  |#7\xB3|#1Received |#7\xB3|#1Ready |#7\xB3|#1Fails|#7\xB3  |#1Elapsed  |#7\xB3|#1/HrWt|#7\xB3\r\n";
  bout << "|#7\xC3\xC4\xC4\xC4\xC4\xC4\xC5\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC5\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC5\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC5\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC5\xC4\xC4\xC4\xC4\xC4\xC4\xC5\xC4\xC4\xC4\xC4\xC4\xC5\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC5\xC4\xC4\xC4\xC4\xC4\xB4\r\n";

  int nNetNumber;
  for (nNetNumber = 0; nNetNumber < session()->max_net_num(); nNetNumber++) {
    set_net_num(nNetNumber);

    if (!net_sysnum) {
      continue;
    }

    if (!session()->current_net().con) {
      read_call_out_list();
    }

    if (!session()->current_net().ncn) {
      read_contacts();
    }

    net_call_out_rec* con = session()->current_net().con;
    net_contact_rec* ncn = session()->current_net().ncn;
    num_call_sys = session()->current_net().num_con;
    num_ncn = session()->current_net().num_ncn;

    for (i1 = 0; i1 < num_call_sys; i1++) {
      i2 = -1;

      for (i = 0; i < num_ncn; i++) {
        if ((con[i1].options & options_hide_pend)) {
          continue;
        }
        if (con[i1].sysnum == ncn[i].systemnumber) {
          i2 = i;
          break;
        }
      }
      if (i2 != -1) {
        if (ok_to_call(i1)) {
          strcpy(s2, "|#5Yes");
        } else {
          strcpy(s2, "|#3---");
        }

        int32_t m = 0, h = 0;
        if (ncn[i2].lastcontactsent) {
          time_t tLastContactTime = tCurrentTime - ncn[i2].lastcontactsent;
          int32_t se = tLastContactTime % 60;
          tLastContactTime = (tLastContactTime - se) / 60;
          m = static_cast<int32_t>(tLastContactTime % 60);
          h = static_cast<int32_t>(tLastContactTime / 60);
          sprintf(s1, "|#2%02d:%02d:%02d", h, m, se);
        } else {
          strcpy(s1, "|#6     -    ");
        }

        sprintf(s3, "%dk", ((ncn[i2].bytes_sent) + 1023) / 1024);
        sprintf(s4, "%dk", ((ncn[i2].bytes_received) + 1023) / 1024);
        sprintf(s5, "%dk", ((ncn[i2].bytes_waiting) + 1023) / 1024);

        if (con[i1].options & options_ATT_night) {
          if ((nDow != 0) && (nDow != 6)) {
            if (!((pTm->tm_hour < 23) && (pTm->tm_hour >= 7))) {
              adjust = (7 - pTm->tm_hour);
              if (pTm->tm_hour == 23) {
                adjust = 8;
              }
            }
          }
        }
        if (m >= 30) {
          h++;
        }
        i3 = ((con[i1].call_x_days * 24) - static_cast<int>(h) - adjust);
        if (m < 31) {
          h--;
        }
        if (i3 < 0) {
          i3 = 0;
        }

        bout.bprintf("|#7\xB3 %-3s |#7\xB3 |#2%-8.8s |#7\xB3 |#2%5u |#7\xB3|#2%8s |#7\xB3|#2%8s "
            "|#7\xB3|#2%5s |#7\xB3|#2%4d |#7\xB3|#2%13.13s |#7\xB3|#2%4d |#7\xB3\r\n",
            s2, session()->network_name(), ncn[i2].systemnumber, s3, s4, s5, ncn[i2].numfails, s1, i3);
        if (!session()->user()->HasPause() && ((lines++) == 20)) {
          pausescr();
          lines = 0;
        }
      }
    }
  }

  for (nNetNumber = 0; nNetNumber < session()->max_net_num(); nNetNumber++) {
    set_net_num(nNetNumber);

    if (!net_sysnum) {
      continue;
    }

    File deadNetFile(session()->network_directory(), DEAD_NET);
    if (deadNetFile.Open(File::modeReadOnly | File::modeBinary)) {
      long lFileSize = deadNetFile.GetLength();
      deadNetFile.Close();
      sprintf(s3, "%ldk", (lFileSize + 1023) / 1024);
      bout.bprintf("|#7\xB3 |#3--- |#7\xB3 |#2%-8s |#7\xB3 |#6DEAD! |#7\xB3 |#2------- |#7\xB3 |#2------- |#7\xB3|#2%5s "
                   "|#7\xB3|#2 --- |#7\xB3 |#2--------- |#7\xB3|#2 --- |#7\xB3\r\n",
          session()->network_name(), s3);
    }
  }

  for (nNetNumber = 0; nNetNumber < session()->max_net_num(); nNetNumber++) {
    set_net_num(nNetNumber);

    if (!net_sysnum) {
      continue;
    }

    File checkNetFile(session()->network_directory(), CHECK_NET);
    if (checkNetFile.Open(File::modeReadOnly | File::modeBinary)) {
      long lFileSize = checkNetFile.GetLength();
      checkNetFile.Close();
      sprintf(s3, "%ldk", (lFileSize + 1023) / 1024);
      strcat(s3, "k");
      bout.bprintf("|#7\xB3 |#3--- |#7\xB3 |#2%-8s |#7\xB3 |#6CHECK |#7\xB3 |#2------- |#7\xB3 |#2------- |#7\xB3|#2%5s |#7\xB3|#2 --- |#7\xB3 |#2--------- |#7\xB3|#2 --- |#7\xB3\r\n",
                                        session()->network_name(), s3);
    }
  }

  bout << "|#7\xc0\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xD9\r\n";
  bout.nl();
  session()->user()->SetStatus(ss);
  if (!session()->IsUserOnline() && lines_listed) {
    pausescr();
  }
}

void gate_msg(net_header_rec * nh, char *messageText, int nNetNumber, const char *pszAuthorName,
  vector<uint16_t> list, int nFromNetworkNumber) {
  char newname[256], qn[200], on[200];
  char nm[205];
  int i;

  if (strlen(messageText) >= 80) {
    return;
  }

  char *pszOriginalText = messageText;
  messageText += strlen(pszOriginalText) + 1;
  unsigned short ntl = static_cast<uint16_t>(nh->length - strlen(pszOriginalText) - 1);
  char *ss = strchr(messageText, '\r');
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

    if (nFromNetworkNumber == 65535 || nh->fromsys == session()->net_networks[nFromNetworkNumber].sysnum) {

      strcpy(newname, nm);
      ss = strrchr(newname, '@');
      if (ss) {
        sprintf(ss + 1, "%u", session()->net_networks[nNetNumber].sysnum);
        ss = strrchr(nm, '@');
        if (ss) {
          ++ss;
          while ((*ss >= '0') && (*ss <= '9')) {
            ++ss;
          }
          strcat(newname, ss);
        }
        strcat(newname, "\r\n");
        nh->fromsys = session()->net_networks[nNetNumber].sysnum;
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
      if (session()->net_networks[nFromNetworkNumber].sysnum == 1 && on[0] &&
          IsEqualsIgnoreCase(session()->net_networks[nFromNetworkNumber].name, "Internet")) {
        sprintf(newname, "%s%s", qn, on);
      } else {
        if (on[0]) {
          sprintf(newname, "%s%s@%u.%s\r\n", qn, on, nh->fromsys,
                  session()->net_networks[nFromNetworkNumber].name);
        } else {
          sprintf(newname, "%s#%u@%u.%s\r\n", qn, nh->fromuser, nh->fromsys,
                  session()->net_networks[nFromNetworkNumber].name);
        }
      }
      nh->fromsys = session()->net_networks[nNetNumber].sysnum;
      nh->fromuser = 0;
    }


    nh->length += strlen(newname);
    if ((nh->main_type == main_type_email_name) ||
        (nh->main_type == main_type_new_post)) {
      nh->length += strlen(pszAuthorName) + 1;
    }
    const string packet_filename = StringPrintf("%sp1%s",
      session()->net_networks[nNetNumber].dir, session()->network_extension().c_str());
    File file(packet_filename);
    if (file.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
      file.Seek(0L, File::seekEnd);
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
        file.Write(pszAuthorName, strlen(pszAuthorName) + 1);
      }
      file.Write(pszOriginalText, strlen(pszOriginalText) + 1);
      file.Write(newname, strlen(newname));
      file.Write(messageText, ntl);
      file.Close();
    }
  }
}

// begin callout additions

static void print_call(int sn, int nNetNumber, int i2) {
  static int color, got_color = 0;
  time_t tCurrentTime = time(nullptr);

  set_net_num(nNetNumber);
  read_call_out_list();
  read_contacts();

  net_contact_rec *ncn = session()->current_net().ncn;
  net_system_list_rec *csne = next_system(sn);

  color = 30;
  if (!got_color) {
    got_color = 1;
    IniFile iniFile(FilePath(session()->GetHomeDir(), WWIV_INI), INI_TAG);
    if (iniFile.IsOpen()) {
      color = StringToInt(iniFile.GetValue("CALLOUT_COLOR_TEXT", "30"));
    }
  }
  curatr = color;
  string s1 = to_string(bytes_to_k(ncn[i2].bytes_waiting));
  session()->localIO()->LocalXYAPrintf(58, 17, color, "%-10.16s", s1.c_str());

  s1 = to_string(bytes_to_k(ncn[i2].bytes_received));
  session()->localIO()->LocalXYAPrintf(23, 17, color, "%-10.16s", s1.c_str());

  s1 = to_string(bytes_to_k(ncn[i2].bytes_sent));
  session()->localIO()->LocalXYAPrintf(23, 18, color, "%-10.16s", s1.c_str());

  if (ncn[i2].firstcontact) {
    s1 = StrCat(to_string((tCurrentTime - ncn[i2].firstcontact) / SECONDS_PER_HOUR), ":");
    time_t tTime = (((tCurrentTime - ncn[i2].firstcontact) % SECONDS_PER_HOUR) / 60);
    string s = to_string(tTime);
    if (tTime < 10) {
      s1 += StrCat("0", s);
    } else {
      s1 += s;
    }
    s1 += " Hrs";
  } else {
    s1 += "NEVER";
  }
  session()->localIO()->LocalXYAPrintf(23, 16, color, "%-17.16s", s1.c_str());

  if (ncn[i2].lastcontactsent) {
    s1 = StrCat(to_string((tCurrentTime - ncn[i2].lastcontactsent) / SECONDS_PER_HOUR), ":");
    time_t tTime = (((tCurrentTime - ncn[i2].lastcontactsent) % SECONDS_PER_HOUR) / 60);
    string s = to_string(tTime);
    if (tTime < 10) {
      s1 += StrCat("0", s);
    } else {
      s1 += s;
    }
    s1 += " Hrs";
  } else {
    s1 += "NEVER";
  }
  session()->localIO()->LocalXYAPrintf(58, 16, color, "%-17.16s", s1.c_str());

  if (ncn[i2].lasttry) {
    string tmp = to_string((tCurrentTime - ncn[i2].lasttry) / SECONDS_PER_HOUR);
    s1 = StrCat(tmp, ":");
    time_t tTime = (((tCurrentTime - ncn[i2].lasttry) % SECONDS_PER_HOUR) / 60);
    string s = to_string(tTime);
    if (tTime < 10) {
      s1 += StrCat("0", s);
    } else {
      s1 += s;
    }
    s1 += " Hrs";
  } else {
    s1 += "NEVER";
  }
  session()->localIO()->LocalXYAPrintf(58, 15, color, "%-17.16s", s1.c_str());
  session()->localIO()->LocalXYAPrintf(23, 15, color, "%-16u", ncn[i2].numcontacts);
  session()->localIO()->LocalXYAPrintf(41, 3, color, "%-30.30s", csne->name);
  session()->localIO()->LocalXYAPrintf(23, 19, color, "%-17.17s", csne->phone);
  s1 = StrCat(to_string(csne->speed), " BPS");
  session()->localIO()->LocalXYAPrintf(58, 18, color, "%-10.16s", s1.c_str());
  const string areacode= describe_area_code(atoi(csne->phone));
  const string stripped = stripcolors(areacode);
  session()->localIO()->LocalXYAPrintf(58, 19, color, "%-17.17s", stripped.c_str());
  session()->localIO()->LocalXYAPrintf(14, 3, color, "%-11.16s", session()->network_name());
}

static void fill_call(int color, int row, int netmax, std::map<int, int>& nodenum) {
  int i, x = 0, y = 0;
  char s1[6];

  curatr = color;
  for (i = row * 10; (i < ((row + 6) * 10)); i++) {
    if (x > 69) {
      x = 0;
      y++;
    }
    if (i < netmax) {
      sprintf(s1, "%-5u", nodenum[i]);
    } else {
      strcpy(s1, "     ");
    }
    session()->localIO()->LocalXYPuts(6 + x, 5 + y, s1);
    x += 7;
  }
}

static const int MAX_CONNECTS = 2000;

static int ansicallout() {
  static int callout_ansi, color1, color2, color3, color4, got_info = 0;
  char ch = 0;
  int i, i1, nNetNumber, netnum = 0, x = 0, y = 0, pos = 0, sn = 0;
  int num_ncn, num_call_sys, rownum = 0;
  net_contact_rec *ncn;
  net_call_out_rec *con;
  session()->localIO()->SetCursor(LocalIO::cursorNone);
  if (!got_info) {
    got_info = 1;
    callout_ansi = 0;
    color1 = 31;
    color2 = 59;
    color3 = 7;
    color4 = 30;
    IniFile iniFile(FilePath(session()->GetHomeDir(), WWIV_INI), INI_TAG);
    if (iniFile.IsOpen()) {
      callout_ansi = iniFile.GetBooleanValue("CALLOUT_ANSI") ? 1 : 0;
      color1 = iniFile.GetNumericValue("CALLOUT_COLOR", color1);
      color2 = iniFile.GetNumericValue("CALLOUT_HIGHLIGHT", color2);
      color3 = iniFile.GetNumericValue("CALLOUT_NORMAL", color3);
      color4 = iniFile.GetNumericValue("CALLOUT_COLOR_TEXT", color4);
      iniFile.Close();
    }
  }

  if (callout_ansi) {
    std::map<int, int> nodenum;
    std::map<int, int> netpos;
    std::map<int, int> ipos;
    for (nNetNumber = 0; nNetNumber < session()->max_net_num(); nNetNumber++) {
      set_net_num(nNetNumber);
      read_call_out_list();
      read_contacts();

      con = session()->current_net().con;
      ncn = session()->current_net().ncn;
      num_call_sys = session()->current_net().num_con;
      num_ncn = session()->current_net().num_ncn;

      for (i1 = 0; i1 < num_call_sys; i1++) {
        for (i = 0; i < num_ncn; i++) {
          if ((!(con[i1].options & options_hide_pend)) &&
              (con[i1].sysnum == ncn[i].systemnumber) &&
              (valid_system(con[i1].sysnum))) {
            ipos[netnum] = i;
            netpos[netnum] = nNetNumber;
            nodenum[netnum++] = ncn[i].systemnumber;
            break;
          }
        }
      }
      if (netnum > MAX_CONNECTS) {
        break;
      }
    }

    session()->localIO()->LocalCls();
    curatr = color1;
    session()->localIO()->MakeLocalWindow(3, 2, 73, 10);
    session()->localIO()->LocalXYAPrintf(3, 4, color1, "\xC3%s\xB4", charstr(71, '\xC4'));
    session()->localIO()->MakeLocalWindow(3, 14, 73, 7);
    session()->localIO()->LocalXYAPrintf(5, 3,   color3, "NetWork:");
    session()->localIO()->LocalXYAPrintf(31, 3,  color3, "BBS Name:");
    session()->localIO()->LocalXYAPrintf(5, 15,  color3, "Contact Number  :");
    session()->localIO()->LocalXYAPrintf(5, 16,  color3, "First Contact   :");
    session()->localIO()->LocalXYAPrintf(5, 17,  color3, "Bytes Received  :");
    session()->localIO()->LocalXYAPrintf(5, 18,  color3, "Bytes Sent      :");
    session()->localIO()->LocalXYAPrintf(5, 19,  color3, "Phone Number    :");
    session()->localIO()->LocalXYAPrintf(40, 15, color3, "Last Try Contact:");
    session()->localIO()->LocalXYAPrintf(40, 16, color3, "Last Contact    :");
    session()->localIO()->LocalXYAPrintf(40, 17, color3, "Bytes Waiting   :");
    session()->localIO()->LocalXYAPrintf(40, 18, color3, "Max Speed       :");
    session()->localIO()->LocalXYAPrintf(40, 19, color3, "System Location :");

    fill_call(color4, rownum, netnum, nodenum);
    curatr = color2;
    x = 0;
    y = 0;
    session()->localIO()->LocalXYAPrintf(6, 5, color2, "%-5u", nodenum[pos]);
    print_call(nodenum[pos], netpos[pos], ipos[pos]);

    bool done = false;
    do {
      ch = wwiv::UpperCase<char>(static_cast<char>(session()->localIO()->LocalGetChar()));
      switch (ch) {
      case ' ':
      case RETURN:
        sn = nodenum[pos];
        done = true;
        break;
      case 'Q':
      case ESC:
        sn = 0;
        done = true;
        break;
      case -32: // (224) I don't know MS's CRT returns this on arrow keys....
      case 0:
        ch = wwiv::UpperCase<char>(static_cast<char>(session()->localIO()->LocalGetChar()));
        switch (ch) {
        case RARROW:                        // right arrow
          if ((pos < netnum - 1) && (x < 63)) {
            session()->localIO()->LocalXYAPrintf(6 + x, 5 + y, color4, "%-5u", nodenum[pos]);
            pos++;
            x += 7;
            session()->localIO()->LocalXYAPrintf(6 + x, 5 + y, color2, "%-5u", nodenum[pos]);
            print_call(nodenum[pos], netpos[pos], ipos[pos]);
          }
          break;
        case LARROW:                        // left arrow
          if (x > 0) {
            curatr = color4;
            session()->localIO()->LocalXYAPrintf(6 + x, 5 + y, color4, "%-5u", nodenum[pos]);
            pos--;
            x -= 7;
            curatr = color2;
            session()->localIO()->LocalXYAPrintf(6 + x, 5 + y, color2, "%-5u", nodenum[pos]);
            print_call(nodenum[pos], netpos[pos], ipos[pos]);
          }
          break;
        case UPARROW:                        // up arrow
          if (y > 0) {
            session()->localIO()->LocalXYAPrintf(6 + x, 5 + y, color4, "%-5u", nodenum[pos]);
            pos -= 10;
            y--;
            session()->localIO()->LocalXYAPrintf(6 + x, 5 + y, color2, "%-5u", nodenum[pos]);
            print_call(nodenum[pos], netpos[pos], ipos[pos]);
          } else if (rownum > 0) {
            pos -= 10;
            rownum--;
            fill_call(color4, rownum, netnum, nodenum);
            session()->localIO()->LocalXYAPrintf(6 + x, 5 + y, color2, "%-5u", nodenum[pos]);
            print_call(nodenum[pos], netpos[pos], ipos[pos]);
          }
          break;
        case DNARROW:                        // down arrow
          if ((y < 5) && (pos + 10 < netnum)) {
            session()->localIO()->LocalXYAPrintf(6 + x, 5 + y, color4, "%-5u", nodenum[pos]);
            pos += 10;
            y++;
          } else if ((rownum + 6) * 10 < netnum) {
            rownum++;
            fill_call(color4, rownum, netnum, nodenum);
            if (pos + 10 < netnum) {
              pos += 10;
            } else {
              --y;
            }
          }
          curatr = color2;
          session()->localIO()->LocalXYAPrintf(6 + x, 5 + y, color2, "%-5u", nodenum[pos]);
          print_call(nodenum[pos], netpos[pos], ipos[pos]);
          break;
        case HOME:                        // home
          if (pos > 0) {
            x = 0;
            y = 0;
            pos = 0;
            rownum = 0;
            fill_call(color4, rownum, netnum, nodenum);
            session()->localIO()->LocalXYAPrintf(6, 5, color2, "%-5u", nodenum[pos]);
            print_call(nodenum[pos], netpos[pos], ipos[pos]);
          }
        case PAGEUP:                        // page up
          if (y > 0) {
            session()->localIO()->LocalXYAPrintf(6 + x, 5 + y, color4, "%-5u", nodenum[pos]);
            pos -= 10 * y;
            y = 0;
            session()->localIO()->LocalXYAPrintf(6 + x, 5 + y, color2, "%-5u", nodenum[pos]);
            print_call(nodenum[pos], netpos[pos], ipos[pos]);
          } else {
            if (rownum > 5) {
              pos -= 60;
              rownum -= 6;
            } else {
              pos -= 10 * rownum;
              rownum = 0;
            }
            fill_call(color4, rownum, netnum, nodenum);
            session()->localIO()->LocalXYAPrintf(6 + x, 5 + y, color2, "%-5u", nodenum[pos]);
            print_call(nodenum[pos], netpos[pos], ipos[pos]);
          }
          break;
        case PAGEDN:                        // page down
          if (y < 5) {
            session()->localIO()->LocalXYAPrintf(6 + x, 5 + y, color4, "%-5u", nodenum[pos]);
            pos += 10 * (5 - y);
            y = 5;
            if (pos >= netnum) {
              pos -= 10;
              --y;
            }
            session()->localIO()->LocalXYAPrintf(6 + x, 5 + y, color2, "%-5u", nodenum[pos]);
            print_call(nodenum[pos], netpos[pos], ipos[pos]);
          } else if ((rownum + 6) * 10 < netnum) {
            for (i1 = 0; i1 < 6; i1++) {
              if ((rownum + 6) * 10 < netnum) {
                rownum++;
                pos += 10;
              }
            }
            fill_call(color4, rownum, netnum, nodenum);
            if (pos >= netnum) {
              pos -= 10;
              --y;
            }
            session()->localIO()->LocalXYAPrintf(6 + x, 5 + y, color2, "%-5u", nodenum[pos]);
            print_call(nodenum[pos], netpos[pos], ipos[pos]);
          }
          break;
        }
      }
    } while (!done);
    session()->localIO()->SetCursor(LocalIO::cursorNormal);
    curatr = color3;
    session()->localIO()->LocalCls();
    netw = (netpos[pos]);
  } else {
    bout.nl();
    bout << "|#2Which system: ";
    char szSystemNumber[11];
    input(szSystemNumber, 5, true);
    sn = atoi(szSystemNumber);
  }

  session()->localIO()->SetCursor(LocalIO::cursorNormal);
  return sn;
}

void force_callout(int dw) {
  int i, i1, i2;
  bool  abort = false;
  bool ok;
  unsigned int total_attempts = 1, current_attempt = 0;
  time_t tCurrentTime;
  char ch, s[101];
  net_system_list_rec *csne;
  unsigned long lc, cc;

  time(&tCurrentTime);
  int sn = ansicallout();
  if (!sn) {
    return;
  }

  int nv = 0;
  unique_ptr<char[]> ss(new char[session()->max_net_num() * 3]);
  char *ss1 = ss.get() + session()->max_net_num();
  char *ss2 = ss1 + session()->max_net_num();

  for (int nNetNumber = 0; nNetNumber < session()->max_net_num(); nNetNumber++) {
    set_net_num(nNetNumber);
    if (!net_sysnum || net_sysnum == sn) {
      continue;
    }

    if (!session()->current_net().con) {
      read_call_out_list();
    }

    i = -1;
    for (i1 = 0; i1 < session()->current_net().num_con; i1++) {
      if (session()->current_net().con[i1].sysnum == sn) {
        i = i1;
        break;
      }
    }

    if (i != -1) {
      if (!session()->current_net().ncn) {
        read_contacts();
      }

      i2 = -1;
      for (i1 = 0; i1 < session()->current_net().num_ncn; i1++) {
        if (session()->current_net().ncn[i1].systemnumber == sn) {
          i2 = i1;
          break;
        }
      }

      if (i2 != -1) {
        ss[nv] = static_cast<char>(nNetNumber);
        ss1[nv] = static_cast<char>(i);
        ss2[nv++] = static_cast<char>(i2);
      }
    }
  }

  int nitu = -1;
  if (nv) {
    if (nv == 1) {
      nitu = 0;
    } else {
      bout.nl();
      for (i = 0; i < nv; i++) {
        set_net_num(ss[i]);
        csne = next_system(sn);
        if (csne) {
          if (IsEqualsIgnoreCase(session()->net_networks[netw].name, session()->network_name())) {
            nitu = i;
          }
        }
      }
    }
  }
  if (nitu != -1) {
    set_net_num(ss[nitu]);
    ok = ok_to_call(ss1[nitu]);

    if (!ok) {
      bout.nl();
      bout <<  "|#5Are you sure? ";
      if (yesno()) {
        ok = true;
      }
    }
    if (ok) {
      if (session()->current_net().ncn[ss2[nitu]].bytes_waiting == 0L) {
        if (!(session()->current_net().con[ss1[nitu]].options & options_sendback)) {
          ok = false;
        }
      }
      if (ok) {
        if (dw) {
          bout.nl();
          bout << "|#2Num Retries : ";
          input(s, 5, true);
          total_attempts = atoi(s);
        }
        if (dw == 2) {
          if (session()->IsUserOnline()) {
            session()->WriteCurrentUser();
            write_qscn(session()->usernum, qsc, false);
            session()->SetUserOnline(false);
          }
          hang_it_up();
          sleep_for(seconds(5));
        }
        if (!dw || total_attempts < 1) {
          total_attempts = 1;
        }

        read_contacts();
        lc = session()->current_net().ncn[ss2[nitu]].lastcontact;
        while ((current_attempt < total_attempts) && (!abort)) {
          if (session()->localIO()->LocalKeyPressed()) {
            while (session()->localIO()->LocalKeyPressed()) {
              ch = wwiv::UpperCase<char>(session()->localIO()->LocalGetChar());
              if (!abort) {
                abort = (ch == ESC) ? true : false;
              }
            }
          }
          current_attempt++;
          set_net_num(ss[nitu]);
          read_contacts();
          cc = session()->current_net().ncn[ss2[nitu]].lastcontact;
          if (abort || cc != lc) {
            break;
          } else {
            session()->localIO()->LocalCls();
            bout << "|#9Retries |#0= |#2" << total_attempts 
                 << "|#9, Current |#0= |#2" << current_attempt
                 << "|#9, Remaining |#0= |#2" << total_attempts - current_attempt
                 << "|#9. ESC to abort.\r\n";
            do_callout(sn);
          }
        }
      }
    }
  }
}

// end callout additions

long next_system_reg(int ts) {

  if (session()->net_num() != -1) {
    read_bbs_list_index();
  }

  DataFile<int32_t> file(session()->network_directory(), BBSDATA_REG);
  if (!file) { 
    return 0;
  }

  for (size_t i = 0; i < session()->csn_index.size(); i++) {
    if (session()->csn_index[i] == ts) {
      int32_t reg_num = 0;
      if (file.Read(i, &reg_num)) {
        return reg_num;
      }
    }
  }
  return 0;
}

#ifndef _UNUX
void run_exp() {
  int nOldNetworkNumber = session()->net_num();
  int nFileNetNetworkNumber = getnetnum("FILEnet");
  if (nFileNetNetworkNumber == -1) {
    return;
  }
  set_net_num(nFileNetNetworkNumber);

  char szExpCommand[MAX_PATH];
  sprintf(szExpCommand, "exp s32767.net %s %d %s %s %s", session()->network_directory().c_str(), net_sysnum,
          session()->internetEmailName.c_str(), session()->internetEmailDomain.c_str(), session()->network_name());
  ExecuteExternalProgram(szExpCommand, EFLAG_NETPROG);

  set_net_num(nOldNetworkNumber);
  session()->localIO()->LocalCls();
}
#endif
