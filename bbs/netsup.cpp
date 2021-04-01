/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services             */
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

#include "bbs/bbs.h"
#include "bbs/bbsutl1.h"
#include "bbs/connect1.h"
#include "bbs/execexternal.h"
#include "bbs/instmsg.h"
#include "bbs/wfc.h"
#include "common/datetime.h"
#include "common/input.h"
#include "common/output.h"
#include "core/file.h"
#include "core/inifile.h"
#include "core/numbers.h"
#include "core/os.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "fmt/printf.h"
#include "local_io/keycodes.h"
#include "local_io/local_io.h"
#include "local_io/wconstants.h"
#include "sdk/filenames.h"
#include "sdk/status.h"
#include "sdk/net/binkp.h"
#include "sdk/net/net.h"
#include "sdk/net/networks.h"
#include "sdk/net/callout.h"
#include "sdk/net/callouts.h"
#include "sdk/net/contact.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

using namespace std::chrono;
using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::local::io;
using namespace wwiv::os;
using namespace wwiv::sdk;
using namespace wwiv::sdk::net;
using namespace wwiv::stl;
using namespace wwiv::strings;

constexpr int MAX_CONNECTS = 1000;

/** Returns a full path to exe under the WWIV_DIR */
static std::string CreateNetworkBinary(const std::string& exe) {
  std::ostringstream ss;
  ss << FilePath(a()->bindir(), exe).string();
#ifdef _WIN32
  // CreateProcess is failing on Windows with the .exe extension, and since
  // we don't use MakeAbsCmd on this, it's without .exe.
  ss << ".exe";
#endif  // _WIN32
  ss << " --v=" << a()->verbose();
  ss << " --bbsdir=" << a()->bbspath().string();
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
  if (a()->nets().empty()) {
    return;
  }
  if (a()->nets()[0].sysnum == 0 && wwiv::stl::ssize(a()->nets()) == 1) {
    return;
  }
  a()->sess().hangup(true);
  a()->sess().using_modem(false);
  if (a()->sess().IsUserOnline()) {
    hang_it_up();
  }

  for (int nNetNumber = 0;
       nNetNumber < wwiv::stl::ssize(a()->nets());
       nNetNumber++) {
    set_net_num(nNetNumber);
    const auto& net = a()->nets()[nNetNumber];
    VLOG(2) << "cleanup_net: Processing Network: " << net.name;

    if (!net.sysnum) {
      VLOG(1) << "Skipping network due to no sysnum: " << net.name;
      continue;
    }

    a()->ClearTopScreenProtection();

    std::ostringstream ss;
    ss << CreateNetworkBinary("networkc");
    ss << " --quiet";
    ss << " --process_instance=" << a()->sess().instance_number();
    ss << " ." << nNetNumber;
    const auto networkc_cmd = ss.str();
    VLOG(1) << "Executing Network Command: '" << networkc_cmd << "'";
    ExecuteExternalProgram(networkc_cmd, EFLAG_NETPROG | EFLAG_NOHUP);
    a()->status_manager()->reload_status();
    a()->sess().SetCurrentReadMessageArea(-1);
    a()->ReadCurrentUser(1);
  }
  a()->Cls();
}

static void do_callout(const Network& net, int sn) {
  const Callout callout(net, a()->config()->max_backups());
  Contact contact(net, false);
  Binkp binkp(net.dir);

  const auto* const callout_rec = callout.net_call_out_for(sn); // i con
  if (callout_rec == nullptr) {
    return;
  }

  const NetworkContact* contact_rec = contact.contact_rec_for(sn); // i2 ncn
  if (!contact_rec) {
    return;
  }
  auto const csne = next_system(callout_rec->sysnum);
  if (!csne) {
    return;
  }

  const auto cmd = StrCat(CreateNetworkBinary("network"), " -n", sn, " .", a()->net_num());
  bout << "|#7Calling out to: |#2" << csne->name << " - " << net.name << " @" << sn << wwiv::endl;
  if (contact_rec->bytes_waiting() > 0) {
    bout << "|#7Amount pending: |#2" << bytes_to_k(contact_rec->bytes_waiting()) << "k"
         << wwiv::endl;
  }
  bout << "|#7Commandline is: |#2" << cmd << wwiv::endl
       << "|#7" << std::string(80, '\xCD') << "|#0..." << wwiv::endl;
  ExecuteExternalProgram(cmd, EFLAG_NETPROG | EFLAG_NOHUP);
  a()->status_manager()->reload_status();
  cleanup_net();
}

void print_pending_list() {
  int lines = 0;
  auto ss = a()->user()->get_status();

  if (a()->nets().empty()) {
    return;
  }
  if (a()->nets()[0].sysnum == 0 && wwiv::stl::ssize(a()->nets()) == 1) {
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

  for (const auto& net : a()->nets().networks()) {
    if (!net.sysnum) {
      continue;
    }

    Callout callout(net, a()->config()->max_backups());
    Contact contact(net, false);

    for (const auto& p : callout.callout_config()) {
      const NetworkContact* r = contact.contact_rec_for(p.first);
      if (!r) {
        // skip
        continue;
      }
      const auto& con = p.second;
      if (con.options & options_hide_pend) {
        // skip hidden ones.
        continue;
      }
      if (!r->lastcontactsent() && !r->bytes_waiting()) {
        // Skip ones without bytes waiting that we've not contacted recently.
        continue;
      }

      std::string atc = allowed_to_call(con, DateTime::now()) ? "|#5Yes" : "|#3---";

      std::string last_contact_sent{"|#6     -    "};
      if (r->lastcontactsent()) {
        const auto then = DateTime::from_time_t(r->lastcontactsent());
        const auto diff = current_time.to_system_clock() - then.to_system_clock();
        last_contact_sent = ctim(diff);
      }

      auto bsent = fmt::format("{}k", (r->bytes_sent() + 1023) / 1024);
      auto brec = fmt::format("{}k", (r->bytes_received() + 1023) / 1024);
      auto bwait = fmt::format("{}k", (r->bytes_waiting() + 1023) / 1024);

      bout.format(
          "|#7\xB3 {:>3} |#7\xB3 |#2{:<8} |#7\xB3 |#2{:>5} |#7\xB3|#2{:>8} |#7\xB3|#2{:>8} "
          "|#7\xB3|#2{:>5} |#7\xB3|#2{:>4} |#7\xB3|#2{:>10} |#7\xB3|#7\r\n",
          atc, net.name, r->systemnumber(), bsent, brec, bwait, r->numfails(),
          last_contact_sent);
      if (!a()->user()->pause() && lines++ == 20) {
        bout.pausescr();
        lines = 0;
      }
    }
  }

  for (const auto& net : a()->nets().networks()) {
    if (!net.sysnum) {
      continue;
    }

    File deadNetFile(FilePath(net.dir, DEAD_NET));
    if (deadNetFile.Open(File::modeReadOnly | File::modeBinary)) {
      const auto dead_net_file_size = deadNetFile.length();
      deadNetFile.Close();
      const auto deadk = fmt::format("{}k", (dead_net_file_size + 1023) / 1024);
      bout.format(
          "|#7\xB3 |#3--- |#7\xB3 |#2{:>8} |#7\xB3 |#6DEAD! |#7\xB3 |#2------- |#7\xB3 "
          "|#2------- |#7\xB3|#2{:>5} |#7\xB3|#2 --- |#7\xB3 |#2--------- |#7\xB3\r\n",
          net.name, deadk);
    }
  }

  for (const auto& net : a()->nets().networks()) {
    if (!net.sysnum) {
      continue;
    }

    File checkNetFile(FilePath(net.dir, CHECK_NET));
    if (checkNetFile.Open(File::modeReadOnly | File::modeBinary)) {
      const auto check_net_file_size = checkNetFile.length();
      checkNetFile.Close();
      const auto checkk = fmt::format("{}k", (check_net_file_size + 1023) / 1024);
      bout.format(
          "|#7\xB3 |#3--- |#7\xB3 |#2{:>8} |#7\xB3 |#6CHECK |#7\xB3 |#2------- |#7\xB3 "
          "|#2------- |#7\xB3|#2{:>5} |#7\xB3|#2 --- |#7\xB3 |#2--------- |#7\xB3\r\n",
          net.name, checkk);
    }
  }

  bout << "|#"
          "7\xc0\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4"
          "\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC4"
          "\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4"
          "\xC4\xC4\xC4\xC4\xC4\xD9\r\n";
  bout.nl();
  a()->user()->SetStatus(ss);
  if (bout.lines_listed()) {
    bout.pausescr();
  }
}

void gate_msg(net_header_rec* nh, char* messageText, int nNetNumber,
              const std::string& subtype_or_author, std::vector<uint16_t> list, int nFromNetworkNumber) {
  char newname[256], qn[200], on[200];
  char nm[205];
  int i;
  const auto& from_net = a()->nets()[nFromNetworkNumber];
  const auto& to_net = a()->nets()[nNetNumber];

  if (strlen(messageText) >= 80) {
    return;
  }

  auto* original_text = messageText;
  messageText += strlen(original_text) + 1;
  auto ntl = static_cast<uint16_t>(nh->length - strlen(original_text) - 1);
  auto* ss = strchr(messageText, '\r');
  if (ss && (ss - messageText < 200) && (ss - messageText < ntl)) {
    strncpy(nm, messageText, ss - messageText);
    nm[ss - messageText] = 0;
    ss++;
    if (*ss == '\n') {
      ss++;
    }
    nh->length -= static_cast<uint32_t>(ss - messageText);
    ntl = ntl - static_cast<uint16_t>(ss - messageText);
    messageText = ss;

    qn[0] = on[0] = '\0';

    if (nFromNetworkNumber == -1 || nh->fromsys == from_net.sysnum) {

      strcpy(newname, nm);
      ss = strrchr(newname, '@');
      if (ss) {
        sprintf(ss + 1, "%u", to_net.sysnum);
        ss = strrchr(nm, '@');
        if (ss) {
          ++ss;
          while (*ss >= '0' && *ss <= '9') {
            ++ss;
          }
          strcat(newname, ss);
        }
        strcat(newname, "\r\n");
        nh->fromsys = to_net.sysnum;
      }
    } else {
      if (nm[0] == '`' && nm[1] == '`') {
        for (i = wwiv::strings::ssize(nm) - 2; i > 0; i--) {
          if (nm[i] == '`' && nm[i + 1] == '`') {
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
          if (ss[1] >= '0' && ss[1] <= '9') {
            *ss = 0;
            ss--;
            while (ss > nm && *ss == ' ') {
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
      if (on[0] == 0 && nh->fromuser == 0) {
        strcpy(on, nm + i);
      }
      if (from_net.sysnum == 1 && on[0] &&
          from_net.type == network_type_t::internet) {
        sprintf(newname, "%s%s", qn, on);
      } else if (from_net.sysnum == 1 && on[0] &&
                 from_net.type == network_type_t::news) {
        sprintf(newname, "%s%s", qn, on);
      }
      else {
        if (on[0]) {
          sprintf(newname, "%s%s@%u.%s\r\n", qn, on, nh->fromsys,
                  from_net.name.c_str());
        } else {
          sprintf(newname, "%s#%u@%u.%s\r\n", qn, nh->fromuser, nh->fromsys,
                  from_net.name.c_str());
        }
      }
      nh->fromsys = to_net.sysnum;
      nh->fromuser = 0;
    }

    nh->length += strlen(newname);
    if (nh->main_type == main_type_email_name || nh->main_type == main_type_new_post) {
      nh->length += size_uint32(subtype_or_author) + 1;
    }
    const auto packet_filename = StrCat(to_net.dir, "p1", a()->network_extension());
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
      if (nh->main_type == main_type_email_name || nh->main_type == main_type_new_post) {
        file.Write(subtype_or_author.c_str(), subtype_or_author.size() + 1);
      }
      file.Write(original_text, strlen(original_text) + 1);
      file.Write(newname, strlen(newname));
      file.Write(messageText, ntl);
      file.Close();
    }
  }
}

static std::string to_difftime_string(daten_t now, daten_t then) {
  if (then == 0) {
    return fmt::format("{:<16}", "NEVER");
  }
  const auto h = std::min<int>(999, static_cast<int>((now - then) / SECONDS_PER_HOUR));
  const auto m = static_cast<int>(((now - then) % SECONDS_PER_HOUR) / 60);
  const auto s = fmt::format("{}:{:0<2} Hrs", h, m);
  return fmt::format("{:<16}", s);
}

static void print_call(uint16_t sn, const Network& net) {
  static int color;
  auto now = daten_t_now();

  Callout callout(net, a()->config()->max_backups());
  Contact contact(net, false);
  Binkp binkp(net.dir);

  const NetworkContact* ncn = contact.contact_rec_for(sn);
  if (!ncn) {
    return;
  }
  auto csne = next_system(sn);
  if (!csne) {
    return;
  }

  static auto got_color = false;
  if (!got_color) {
    got_color = true;
    IniFile ini(FilePath(a()->bbspath(), WWIV_INI),
                {StrCat("WWIV-", a()->sess().instance_number()), INI_TAG});
    if (ini.IsOpen()) {
      color = ini.value("CALLOUT_COLOR_TEXT", 14);
    }
  }
  auto s1 = humanize(ncn->bytes_waiting());
  bout.localIO()->PutsXYA(58, 17, color, s1);

  s1 = humanize(ncn->bytes_received());
  bout.localIO()->PutsXYA(23, 17, color, s1);

  s1 = humanize(ncn->bytes_sent());
  bout.localIO()->PutsXYA(23, 18, color, s1);

  s1 = to_difftime_string(now, ncn->firstcontact());
  bout.localIO()->PutsXYA(23, 16, color, s1);

  s1 = to_difftime_string(now, ncn->lastcontactsent());
  bout.localIO()->PutsXYA(58, 16, color, s1);

  s1 = to_difftime_string(now, ncn->lasttry());
  bout.localIO()->PutsXYA(58, 15, color, s1);

  const auto ncns = fmt::format("{:<16}", ncn->numcontacts());
  bout.localIO()->PutsXYA(23, 15, color,  ncns);
  bout.localIO()->PutsXYA(41, 3, color, fmt::format("{:<30}", csne->name, 30));
  auto* binkp_node = binkp.binkp_session_config_for(csne->sysnum);
  std::string hostname = csne->phone;
  auto speed = StrCat(std::to_string(csne->speed), " BPS");
  if (binkp_node != nullptr) {
    // Use host:port is we have it.
    hostname = StrCat(binkp_node->host, ":", binkp_node->port);
    speed = "BinkP";
  }
  bout.localIO()->PutsXYA(23, 19, color, fmt::format("{:<30}", hostname));
  bout.localIO()->PutsXYA(58, 18, color, fmt::format("{:<10}", speed));
  bout.localIO()->PutsXYA(14, 3, color, fmt::format("{:<16}", a()->network_name()));
}

static void fill_call(int color, int row, const std::vector<CalloutEntry>& entries) {
  int x = 0, y = 0;
  char s1[6];

  for (int i = row * 10; (i < ((row + 6) * 10)); i++) {
    if (x > 69) {
      x = 0;
      y++;
    }
    if (i < ssize(entries)) {
      sprintf(s1, "%-5u", entries.at(i).node);
    } else {
      strcpy(s1, "     ");
    }
    bout.curatr(color);
    bout.localIO()->PutsXY(6 + x, 5 + y, s1);
    x += 7;
  }
}

static std::pair<int, int> ansicallout() {
  static auto callout_ansi = true;
  static int color1, color2, color3, color4;
  static auto got_info = false;
  auto rownum = 0;
  bout.localIO()->SetCursor(LocalIO::cursorNone);
  if (!got_info) {
    got_info = true;
    color1 = 9;
    color2 = 30;
    color3 = 3;
    color4 = 14;
    IniFile ini(FilePath(a()->bbspath(), WWIV_INI),
                {StrCat("WWIV-", a()->sess().instance_number()), INI_TAG});
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
    auto sn = bin.input_number<uint16_t>(0, 0, 32767);
    bout.localIO()->SetCursor(LocalIO::cursorNormal);
    return std::make_pair(sn, -1);
  }
  int pos = 0, sn = 0, snn = 0;
  std::vector<CalloutEntry> entries;
  for (int nn = 0; nn < wwiv::stl::ssize(a()->nets()); nn++) {
    set_net_num(nn);
    const auto& net = a()->nets()[nn];
    Callout callout(net, a()->config()->max_backups());
    Contact contact(net, false);

    const auto& nodemap = callout.callout_config();
    for (const auto& p : nodemap) {
      auto* const con = contact.contact_rec_for(p.first);
      if (!con) {
        continue;
      }
      if (!(p.second.options & options_hide_pend) && valid_system(p.second.sysnum)) {
        entries.emplace_back(con->systemnumber(), nn);
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
  bout.localIO()->MakeLocalWindow(3, 2, 73, 10);
  const auto header = StrCat("\xC3", std::string(71, '\xC4'), "\xB4");
  bout.localIO()->PutsXYA(3, 4, color1, header);
  bout.localIO()->MakeLocalWindow(3, 14, 73, 7);
  bout.localIO()->PutsXYA(5, 3, color3, "Network:");
  bout.localIO()->PutsXYA(31, 3, color3, "BBS Name:");
  bout.localIO()->PutsXYA(5, 15, color3, "# Connections   :");
  bout.localIO()->PutsXYA(5, 16, color3, "First Contact   :");
  bout.localIO()->PutsXYA(5, 17, color3, "KB Received     :");
  bout.localIO()->PutsXYA(5, 18, color3, "KB Sent         :");
  bout.localIO()->PutsXYA(5, 19, color3, "Address         :");
  bout.localIO()->PutsXYA(40, 15, color3, "Last Attempt    :");
  bout.localIO()->PutsXYA(40, 16, color3, "Last Contact    :");
  bout.localIO()->PutsXYA(40, 17, color3, "KB Waiting      :");
  bout.localIO()->PutsXYA(40, 18, color3, "Max Speed       :");

  fill_call(color4, rownum, entries);
  int x = 0;
  int y = 0;
  bout.localIO()->PutsXYA(6, 5, color2, fmt::sprintf("%-5u", entries[pos].node));
  print_call(entries[pos].node, a()->nets()[entries[pos].net]);

  auto done = false;
  do {
    char ch = to_upper_case_char(bout.localIO()->GetChar());
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
    case -32: 
      // Ignore warning of duplicate case
    case 224: // (224) I don't know MS's CRT returns this on arrow keys....
    case 0:
      ch = to_upper_case_char(bout.localIO()->GetChar());
      switch (ch) {
      case RARROW: // right arrow
        if (pos < ssize(entries) - 1 && x < 63) {
          bout.localIO()->PutsXYA(6 + x, 5 + y, color4, fmt::sprintf("%-5u", entries[pos].node));
          pos++;
          x += 7;
          bout.localIO()->PutsXYA(6 + x, 5 + y, color2, fmt::sprintf("%-5u", entries[pos].node));
          print_call(entries[pos].node, a()->nets()[entries[pos].net]);
        }
        break;
      case LARROW: // left arrow
        if (x > 0) {
          bout.localIO()->PutsXYA(6 + x, 5 + y, color4, fmt::sprintf("%-5u", entries[pos].node));
          pos--;
          x -= 7;
          bout.localIO()->PutsXYA(6 + x, 5 + y, color2, fmt::sprintf("%-5u", entries[pos].node));
          print_call(entries[pos].node, a()->nets()[entries[pos].net]);
        }
        break;
      case UPARROW: // up arrow
        if (y > 0) {
          bout.localIO()->PutsXYA(6 + x, 5 + y, color4, fmt::sprintf("%-5u", entries[pos].node));
          pos -= 10;
          y--;
          bout.localIO()->PutsXYA(6 + x, 5 + y, color2, fmt::sprintf("%-5u", entries[pos].node));
          print_call(entries[pos].node, a()->nets()[entries[pos].net]);
        } else if (rownum > 0) {
          pos -= 10;
          rownum--;
          fill_call(color4, rownum, entries);
          bout.localIO()->PutsXYA(6 + x, 5 + y, color2, fmt::sprintf("%-5u", entries[pos].node));
          print_call(entries[pos].node, a()->nets()[entries[pos].net]);
        }
        break;
      case DNARROW: // down arrow
        if (y < 5 && pos + 10 < ssize(entries)) {
          bout.localIO()->PutsXYA(6 + x, 5 + y, color4, fmt::sprintf("%-5u", entries[pos].node));
          pos += 10;
          y++;
        } else if ((rownum + 6) * 10 < ssize(entries)) {
          rownum++;
          fill_call(color4, rownum, entries);
          if (pos + 10 < ssize(entries)) {
            pos += 10;
          } else {
            --y;
          }
        }
        bout.localIO()->PutsXYA(6 + x, 5 + y, color2, fmt::sprintf("%-5u", entries[pos].node));
        print_call(entries[pos].node, a()->nets()[entries[pos].net]);
        break;
      case HOME: // home
        if (pos > 0) {
          x = 0;
          y = 0;
          pos = 0;
          rownum = 0;
          fill_call(color4, rownum, entries);
          bout.localIO()->PutsXYA(6, 5, color2, fmt::sprintf("%-5u", entries[pos].node));
          print_call(entries[pos].node, a()->nets()[entries[pos].net]);
        } break;
      case PAGEUP: // page up
        if (y > 0) {
          bout.localIO()->PutsXYA(6 + x, 5 + y, color4, fmt::sprintf("%-5u", entries[pos].node));
          pos -= 10 * y;
          y = 0;
          bout.localIO()->PutsXYA(6 + x, 5 + y, color2, fmt::sprintf("%-5u", entries[pos].node));
          print_call(entries[pos].node, a()->nets()[entries[pos].net]);
        } else {
          if (rownum > 5) {
            pos -= 60;
            rownum -= 6;
          } else {
            pos -= 10 * rownum;
            rownum = 0;
          }
          fill_call(color4, rownum, entries);
          bout.localIO()->PutsXYA(6 + x, 5 + y, color2, fmt::sprintf("%-5u", entries[pos].node));
          print_call(entries[pos].node, a()->nets()[entries[pos].net]);
        }
        break;
      case PAGEDN: // page down
        if (y < 5) {
          bout.localIO()->PutsXYA(6 + x, 5 + y, color4, fmt::sprintf("%-5u", entries[pos].node));
          pos += 10 * (5 - y);
          y = 5;
          while (pos >= ssize(entries)) {
            pos -= 10;
            --y;
          }
          bout.localIO()->PutsXYA(6 + x, 5 + y, color2, fmt::sprintf("%-5u", entries[pos].node));
          print_call(entries[pos].node, a()->nets()[entries[pos].net]);
        } else if ((rownum + 6) * 10 < ssize(entries)) {
          for (int i1 = 0; i1 < 6; i1++) {
            if ((rownum + 6) * 10 < ssize(entries)) {
              rownum++;
              pos += 10;
            }
          }
          fill_call(color4, rownum, entries);
          if (pos >= ssize(entries)) {
            pos -= 10;
            --y;
          }
          bout.localIO()->PutsXYA(6 + x, 5 + y, color2, fmt::sprintf("%-5u", entries[pos].node));
          print_call(entries[pos].node, a()->nets()[entries[pos].net]);
        }
        break;
      }
    }
  } while (!done);

  bout.curatr(color3);
  a()->Cls();
  bout.localIO()->SetCursor(LocalIO::cursorNormal);
  return std::make_pair(sn, snn);
}

static int FindNetworkNumberForNode(int sn) {
  for (auto net_number = 0; net_number < wwiv::stl::ssize(a()->nets()); net_number++) {
    const auto net = a()->nets()[net_number];
    if (Callout callout(net, a()->config()->max_backups()); callout.net_call_out_for(sn) != nullptr) {
      return net_number;
    }
  }
  return -1;
}

void force_callout() {
  auto [system_number, network_number] = ansicallout();
  if (system_number <= 0) {
    return;
  }
  if (network_number < 0) {
    network_number = FindNetworkNumberForNode(system_number);
  }
  if (network_number < 0) {
    return;
  }
  const auto& net = a()->nets()[network_number];

  set_net_num(network_number);
  const Callout callout(net, a()->config()->max_backups());

  if (!allowed_to_call(*callout.net_call_out_for(system_number), DateTime::now())) {
    return;
  }

  a()->Cls();
  do_callout(net, system_number);
}
