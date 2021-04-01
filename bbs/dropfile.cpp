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
#include "bbs/dropfile.h"

#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "common/datetime.h"
#include "bbs/instmsg.h"
#include "bbs/sysoplog.h"
#include "bbs/utility.h"
#include "common/output.h"
#include "core/textfile.h"
#include "core/version.h"
#include "fmt/printf.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/status.h"
#include <algorithm>
#include <memory>
#include <string>

using std::chrono::duration_cast;
using std::chrono::seconds;
using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

#pragma pack(push, 1)

// See http://wiki.bbs.nz/doors:dropfiles:pcboardsys
// Note, this doesn't seem to match perfectly what that states, but this
// is the same pcboard.sys format wwiv has done for years.
struct pcboard_sys_rec {
  char display[2], printer[2], page_bell[2], alarm[2], sysop_next, errcheck[2], graphics, nodechat,
      openbps[5], connectbps[5];

  int16_t usernum;

  char firstname[15], password[12];

  int16_t time_on, prev_used;

  char time_logged[5];

  int16_t time_limit, down_limit;

  char curconf, bitmap1[5], bitmap2[5];

  int16_t time_added, time_credit;

  char slanguage[4], name[25];

  int16_t sminsleft;

  char snodenum, seventtime[5], seventactive[2], sslide[2], smemmsg[4], scomport, packflag, bpsflag;

  // PCB 14.5 extra stuff
  char ansi, lastevent[8];

  int16_t lasteventmin;

  char exittodos, eventupcoming;

  int16_t lastconfarea;

  char hconfbitmap;
  // end PCB 14.5 additions
};
#pragma pack(pop)

[[maybe_unused]] static constexpr auto sizeof_pcboard_sys = sizeof(pcboard_sys_rec);

static constexpr int NULL_HANDLE = 0;

static int GetDoor32CommType() {
  if (!a()->sess().using_modem()) {
    return 0;
  }
#ifdef _WIN32
  return bout.remoteIO()->GetHandle() == NULL_HANDLE ? 1 : 2;
#else
  return 0;
#endif
}

static int GetDoor32Emulation() { return (okansi()) ? 1 : 0; }

std::filesystem::path create_dropfile_path(drop_file_t dropfile_type) {
  const auto tempdir = a()->sess().dirs().temp_directory();
  switch (dropfile_type) {
  case drop_file_t::CHAIN_TXT:
    return FilePath(tempdir, DROPFILE_CHAIN_TXT);
  case drop_file_t::DORINFO_DEF:
    return FilePath(tempdir, "dorinfo1.def");
  case drop_file_t::PCBOARD_SYS:
    return FilePath(tempdir, "pcboard.sys");
  case drop_file_t::CALLINFO_BBS:
    return FilePath(tempdir, "callinfo.bbs");
  case drop_file_t::DOOR_SYS:
    return FilePath(tempdir, "door.sys");
  case drop_file_t::DOOR32_SYS:
    return FilePath(tempdir, "door32.sys");
  default:
    // Default to CHAIN.TXT since this is the native WWIV format
    return FilePath(tempdir, DROPFILE_CHAIN_TXT);
  }
}

std::string create_dropfile_filename(drop_file_t dropfile_type) {
  return create_dropfile_path(dropfile_type).string();
}

/**
 * Returns first or last name from string (s) back into s
 */
static std::string GetNamePartForDropFile(bool last_name, const std::string& name) {
  if (const auto idx = name.find(' '); idx != std::string::npos) {
    // We have a space
    if (last_name) {
      return name.substr(idx + 1);
    }
    return name.substr(0, idx);
  }
  if (last_name) {
    // If we only have one name, don't return it for the last name
    return {};
  }
  return name;
}

static long GetMinutesRemainingForDropFile() {
  const auto time_left = std::max<long>((nsl() / 60) - 1L, 0);
  if (!a()->sess().using_modem()) {
    // When we generate a dropfile from the WFC, give it a suitable amount
    // of time remaining vs. 1 minute since we don't have an active session.
    // Also allow at least an hour for all local users.
    return std::max<long>(60, time_left);
  }
  return time_left;
}

/** make DORINFO1.DEF (RBBS and many others) dropfile */
void CreateDoorInfoDropFile() {
  const auto fileName = create_dropfile_filename(drop_file_t::DORINFO_DEF);
  const auto& u = *a()->user();
  File::Remove(fileName);
  TextFile f(fileName, "wd");
  if (f.IsOpen()) {
    f.WriteLine(a()->config()->system_name());
    f.WriteLine(a()->config()->sysop_name());
    f.WriteLine();
    f.WriteLine(fmt::sprintf("COM%d",a()->sess().incom() ? a()->primary_port() : 0));
    f.WriteLine(fmt::sprintf ("%u BAUD,N,8,1", ((a()->sess().using_modem()) ? a()->modem_speed_ : 0)));
    f.WriteLine("0");
    if (!(a()->config()->sysconfig_flags() & sysconfig_allow_alias)) {
      f.WriteLine(GetNamePartForDropFile(false, u.real_name()));
      f.WriteLine(GetNamePartForDropFile(true, u.real_name()));
    } else {
      f.WriteLine(u.name());
      f.WriteLine();
    }
    if (!u.city().empty() && !u.state().empty()) {
      f.WriteLine(StrCat(u.city(), ", ", u.state()));
    } else {
      f.WriteLine();
    }
    f.WriteLine(u.ansi() ? "1" : "0");
    f.WriteLine(std::to_string(u.sl()));
    f.WriteLine(std::to_string(GetMinutesRemainingForDropFile()));
    f.Close();
  }
}

/** make PCBOARD.SYS (PC Board) drop file */
void CreatePCBoardSysDropFile() {
  const auto fileName = create_dropfile_filename(drop_file_t::PCBOARD_SYS);
  const auto& u = *a()->user();
  File::Remove(fileName);
  File pcbFile(fileName);
  if (pcbFile.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
    pcboard_sys_rec pcb{};
    memset(&pcb, 0, sizeof(pcb));
    memcpy(pcb.display, "-1", 2);
    memcpy(pcb.printer, " 0", 2); // -1 if logging is to the printer, 0 otherwise;
    memcpy(pcb.page_bell, " 0", 2);
    memcpy(pcb.alarm, " 0", 2);
    memcpy(pcb.errcheck, "-1", 2);
    if (okansi()) {
      pcb.graphics = 'Y';
      pcb.ansi = '1';
    } else {
      pcb.graphics = 'N';
      pcb.ansi = '0';
    }
    pcb.nodechat = 32;
    auto modem_speed_str = std::to_string(a()->modem_speed_);
    sprintf(pcb.openbps, "%-5.5s", modem_speed_str.c_str());
    if (!a()->sess().incom()) {
      memcpy(pcb.connectbps, "Local", 5);
    } else {
      snprintf(pcb.connectbps, 5, "%-5.5d", a()->modem_speed_);
    }
    pcb.usernum = static_cast<int16_t>(u.usernum());
    char szName[255];
    sprintf(szName, "%-25.25s", u.GetName());
    char* firstname = strtok(szName, " \t");
    sprintf(pcb.firstname, "%-15.15s", firstname);
    // Don't write password  security
    strcpy(pcb.password, "XXX");
    const auto timeon_sec = duration_cast<seconds>(u.timeon()).count();
    pcb.time_on = static_cast<int16_t>(timeon_sec / 60);
    pcb.prev_used = 0;
    auto time_has_hhmmss = ctim(u.timeon());
    pcb.time_logged[0] = time_has_hhmmss[0];
    pcb.time_logged[1] = time_has_hhmmss[1];
    pcb.time_logged[2] = ':';
    pcb.time_logged[3] = time_has_hhmmss[3];
    pcb.time_logged[4] = time_has_hhmmss[4];
    pcb.time_limit = static_cast<int16_t>(nsl());
    pcb.down_limit = 1024;
    pcb.curconf = static_cast<char>(a()->sess().current_user_sub_conf_num());
    strcpy(pcb.slanguage, a()->sess().current_language().c_str());
    strcpy(pcb.name, u.GetName());
    pcb.sminsleft = pcb.time_limit;
    pcb.snodenum = static_cast<char>((num_instances() > 1) ? a()->sess().instance_number() : 0);
    memcpy(pcb.seventtime, "01:00", 5);
    memcpy(pcb.seventactive, " 0", 2);
    memcpy(pcb.sslide, " 0", 2);
    pcb.scomport = a()->primary_port() + '0';
    pcb.packflag = 27;
    pcb.bpsflag = 32;
    // Added for PCB 14.5 Revision
    auto status = a()->status_manager()->get_status();
    to_char_array_no_null(pcb.lastevent, status->last_date());
    pcb.exittodos = '0';
    pcb.eventupcoming = '0';
    pcb.lastconfarea = static_cast<int16_t>(a()->sess().current_user_sub_conf_num());
    // End Additions

    pcbFile.Write(&pcb, sizeof(pcb));
    pcbFile.Close();
  }
}

// See https://github.com/jquast/x84/blob/master/x84/bbs/door.py#L290
// Also
// https://github.com/sdudley/maximus/blob/1cc413a28df645aeb7170ac7524a05abf501e6ab/mec/misc/callinfo.mec
void CreateCallInfoBbsDropFile() {
  // make CALLINFO.BBS (WildCat!)
  const auto& u = *a()->user();
  const auto fileName = create_dropfile_filename(drop_file_t::CALLINFO_BBS);
  File::Remove(fileName);
  TextFile file(fileName, "wd");
  if (file.IsOpen()) {
    file.WriteLine(u.real_name());
    switch (a()->modem_speed_) {
    case 300:
      file.WriteLine("1");
      break;
    case 1200:
      file.WriteLine("2");
      break;
    case 2400:
      file.WriteLine("0");
      break;
    case 19200:
      file.WriteLine("4");
      break;
    default:
      file.WriteLine("3");
    }
    const auto t = times();
    const auto start_duration = duration_since_midnight(a()->sess().system_logon_time());
    const auto start_minute = std::chrono::duration_cast<std::chrono::minutes>(start_duration).count();
    file.WriteLine(" ");
    file.WriteLine(u.sl());
    file.WriteLine(GetMinutesRemainingForDropFile());
    file.WriteLine(u.ansi() ? "COLOR" : "MONO");
    file.WriteLine("X");
    file.WriteLine(u.usernum());
    file.WriteLine(start_minute);
    file.WriteLine(t.substr(0, 5));
    file.WriteLine("0");
    file.WriteLine("ABCD");
    file.WriteLine("0");
    file.WriteLine("0");
    file.WriteLine("0");
    file.WriteLine("0");
    file.Write(fmt::sprintf("%s\n%s 00:01\nEXPERT\nN\n%s\n%d\n%d\n1\n%d\n%d\n%s\n%s\n%d\n",
                        u.voice_phone(), u.laston(),
                        u.laston(), u.logons(),
                        u.screen_lines(), u.uploaded(),
                        u.downloaded(), "8N1",
                        (a()->sess().incom()) ? "REMOTE" : "LOCAL",
                        (a()->sess().incom()) ? a()->primary_port() : 0));
    file.WriteLine(u.birthday_mmddyy());
    file.WriteLine(a()->sess().incom() ? std::to_string(a()->modem_speed_) : "38400");
    file.Close();
  }
}

static unsigned int GetDoorHandle() {
  if (bout.remoteIO()) {
    return bout.remoteIO()->GetDoorHandle();
  }
  return 0;
}

/** Make DOOR32.SYS drop file */
void CreateDoor32SysDropFile() {
  /* =========================================================================
     File Format: (available at http://www.mysticbbs.com/door32/d32spec1.txt)
     =========================================================================

     0                       Line 1 : Comm type (0=local, 1=serial, 2=telnet)
     0                       Line 2 : Comm or socket handle
     38400                   Line 3 : Baud rate
     Mystic 1.07             Line 4 : BBSID (software name and version)
     1                       Line 5 : User record position (1-based)
     James Coyle             Line 6 : User's real name
     g00r00                  Line 7 : User's handle/alias
     255                     Line 8 : User's security level
     58                      Line 9 : User's time left (in minutes)
     1                       Line 10: Emulation *See Below
     1                       Line 11: Current node number

    * The following are values we've predefined for the emulation:

      0 = Ascii
      1 = Ansi
      2 = Avatar
      3 = RIP
      4 = Max Graphics

     ========================================================================= */
  const auto fileName = create_dropfile_filename(drop_file_t::DOOR32_SYS);
  const auto& u = *a()->user();
  File::Remove(fileName);

  TextFile file(fileName, "wt");
  if (!file.IsOpen()) {
    return;
  }
  file.WriteLine(GetDoor32CommType());
  file.WriteLine(GetDoorHandle());
  const auto cspeed = std::to_string(a()->modem_speed_);
  file.WriteLine(cspeed);
  file.WriteLine(StrCat("WWIV ", short_version()));
  file.WriteLine(u.usernum());
  file.WriteLine(u.real_name());
  file.WriteLine(u.name());
  file.WriteLine(u.sl());
  file.WriteLine(60 * GetMinutesRemainingForDropFile());
  file.WriteLine(GetDoor32Emulation());
  file.WriteLine(a()->sess().instance_number());
}

/** Create generic DOOR.SYS dropfile.
 * See https://github.com/wwivbbs/wwiv/wiki/DOOR.SYS-format
 * or http://files.mpoli.fi/unpacked/software/dos/bbs/drwy231.zip/doorsys.doc
 */
void CreateDoorSysDropFile() {
  const auto fileName = create_dropfile_filename(drop_file_t::DOOR_SYS);
  File::Remove(fileName);
  const auto& u = *a()->user();

  TextFile file(fileName, "wd");
  if (file.IsOpen()) {
    const auto cspeed = std::to_string(a()->modem_speed_);
    const auto real_name = u.real_name();
    auto line = fmt::sprintf("COM%d\n%s\n%c\n%u\n%d\n%c\n%c\n%c\n%c\n%s\n%s, %s\n",
            a()->sess().using_modem() ? a()->primary_port() : 0, cspeed, '8',
            a()->sess().instance_number(), // node
            a()->sess().using_modem() ? a()->modem_speed_ : 38400,
            'Y', // screen display
            'N', // log to printer
            'N', // page bell
            'N', // caller alarm
            real_name, u.city(), u.state());
    file.Write(line);
    line = fmt::sprintf("%s\n%s\n%s\n%d\n%u\n%s\n%u\n%ld\n", u.voice_phone(),
            real_name,
            "X", // u.password()
            u.sl(), u.logons(), u.laston(),
            static_cast<uint32_t>(60L * GetMinutesRemainingForDropFile()),
            GetMinutesRemainingForDropFile());
    file.Write(line);
    const auto* const ansiStatus = okansi() ? "GR" : "NG";
    line = fmt::sprintf("%s\n%u\n%c\n%s\n%u\n%s\n%u\n%c\n%u\n%u\n%u\n%d\n", ansiStatus,
            u.screen_lines(), u.IsExpert() ? 'Y' : 'N',
            "1,2,3",                     // conferences
            a()->current_user_sub_num(), // current 'conference'
            "12/31/99",                  // expiration date
            u.usernum(),
            'Y', // default protocol
            u.uploaded(), u.downloaded(),
            0,  // kb dl today
            0); // kb dl/day max
    file.Write(line);
    const auto birthday_date = u.birthday_mmddyy();
    const auto gfilesdir = a()->config()->gfilesdir();
    const auto t = times();
    line = fmt::sprintf("%s\n%s\n%s\n%s\n%s\n%s\n%c\n%c\n%c\n%u\n%u\n%s\n%-.5s\n%s\n", 
            birthday_date,
            a()->config()->datadir(), gfilesdir,
            a()->config()->sysop_name(), u.name(),
            "00:01", // event time
            'Y',
            okansi() ? 'N' : 'Y', // ansi ok but graphics turned off
            'N',                    // record-locking
            u.color(0), u.banktime_minutes(),
            u.laston(), // last n-scan date
            t,
            "00:01"); // time last call
    file.Write(line);
    line = fmt::sprintf("%u\n%u\n%d\n%d\n%s\n%u\n%d\n",
            99, // max files dl/day
            0,  // files dl today so far
            u.uk(), u.dk(), u.note(),
            u.chains_run(), u.messages_posted());
    file.Write(line);
    file.Close();
  }
}

static void create_drop_files() {
  CreateDoorInfoDropFile();
  CreatePCBoardSysDropFile();
  CreateCallInfoBbsDropFile();
  CreateDoorSysDropFile();
  CreateDoor32SysDropFile();
}

/**
 * \verbatim 
CHAIN.TXT Definition File by MrBill.
-----------CHAIN.TXT-----------------------------------
1                  User number
MRBILL             User alias
Bill               User real name
User callsign (HAM radio)
21                 User age
M                  User sex
16097.00           User gold
05/19/89           User last logon date
80                 User colums
25                 User width
255                User security level (0-255)
1                  1 if Co-SysOp, 0 if not
1                  1 if SysOp, 0 if not
1                  1 if ANSI, 0 if not
0                  1 if at remote, 0 if local console
2225.78            User number of seconds left till logoff
F:\WWIV\GFILES\    System GFILES directory (gen. txt files)
F:\WWIV\DATA\      System DATA directory
890519.LOG         System log of the day
2400               User baud rate
2                  System com port
MrBill's Abode     System name
MrBill             System SysOp
83680              Time user logged on/# of secs. from midn
554                User number of seconds on system so far
5050               User number of uploaded k
22                 User number of uploads
42                 User amount of downloaded k
1                  User number of downloads
8N1                User parity
2400               Com port baud rate
7400               WWIVnet node number
\endverbatim 
 */
std::string create_chain_file() {
  const auto cspeed = std::to_string(a()->modem_speed_);
  const auto& u = *a()->user();

  create_drop_files();
  const auto start_duration = duration_since_midnight(a()->sess().system_logon_time());
  const auto start_second =
      static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(start_duration).count());
  const auto used_duration = std::chrono::system_clock::now() - a()->sess().system_logon_time();
  const auto seconds_used =
      static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(used_duration).count());

  auto fileName = create_dropfile_filename(drop_file_t::CHAIN_TXT);
  File::Remove(fileName);
  TextFile file(fileName, "wd");
  if (file.IsOpen()) {
    file.Write(fmt::sprintf(
        "%d\n%s\n%s\n%s\n%d\n%c\n%10.2f\n%s\n%d\n%d\n%d\n", u.usernum(), u.name(),
        u.real_name(), u.callsign(), u.age(),
        u.gender(), u.gold(), u.laston(),
        u.screen_width(), u.screen_lines(), u.sl()));
    const auto temporary_log_filename = GetTemporaryInstanceLogFileName();
    const auto gfilesdir = a()->config()->gfilesdir();
    file.Write(fmt::sprintf("%d\n%d\n%d\n%u\n%8ld.00\n%s\n%s\n%s\n", cs(), so(), okansi(),
                            a()->sess().incom(), nsl(), gfilesdir, a()->config()->datadir(),
                            temporary_log_filename));
    if (a()->sess().using_modem()) {
      file.WriteLine(a()->modem_speed_);
    } else {
      file.WriteLine("KB");
    }
    file.Write(fmt::sprintf("%d\n%s\n%s\n%d\n%d\n%lu\n%u\n%lu\n%u\n%s\n%s\n%u\n", a()->primary_port(),
                        a()->config()->system_name(), a()->config()->sysop_name(),
                        start_second, seconds_used, u.uk(),
                        u.uploaded(), u.dk(),
                        u.downloaded(), "8N1", cspeed,
                        a()->current_net().sysnum));
    file.Write(fmt::sprintf("N\nN\nN\n"));
    file.Write(fmt::sprintf("%u\n%u\n", u.ar_int(), u.dar_int()));
    file.Close();
  }

  return fileName;
}

std::string nf_path(const std::string& cmd) {
  const auto path = FilePath(a()->netfoss_dir(), cmd);
  return path.string();
}

std::optional<std::string> create_netfoss_bat() {

  if (!File::Exists(a()->netfoss_dir())) {
    LOG(ERROR) << "NetFoss not installed into NETFOSS_DIR: " << a()->netfoss_dir().string();
    return std::nullopt;
  }
  const auto& tempdir = a()->sess().dirs().temp_directory();
  const auto path = FilePath(tempdir, "nf.bat");

  TextFile f(path, "wt");
  if (!f.IsOpen()) {
    return std::nullopt;
  }

  f.WriteLine("@echo off");
  f.WriteLine(StrCat("rem WWIV Generated NF.BAT: ", DateTime::now().to_string()));
  f.WriteLine(nf_path("ansi.com"));
  f.WriteLine(nf_path("netfoss.com"));
  f.WriteLine("if errorlevel 1 goto end");
  f.WriteLine(nf_path("netcom.exe %1 %2 %3 %4 %5 %6 %7 %8 %9"));
  f.WriteLine(nf_path("netfoss.com /u"));
  f.WriteLine(":end");

  return f.full_pathname();
}