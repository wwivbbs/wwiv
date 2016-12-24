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
#include "bbs/dropfile.h"
#include <algorithm>
#include <memory>
#include <sstream>
#include <string>

#include "bbs/bbsutl.h"
#include "bbs/datetime.h"
#include "bbs/bbs.h"
#include "bbs/instmsg.h"
#include "bbs/vars.h"
#include "bbs/remote_io.h"
#include "bbs/sysoplog.h"
#include "bbs/utility.h"
#include "bbs/wconstants.h"
#include "sdk/status.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "core/version.h"

using std::string;
using namespace wwiv::sdk;
using namespace wwiv::strings;

struct pcboard_sys_rec {
  char    display[2], printer[2], page_bell[2], alarm[2], sysop_next,
          errcheck[2], graphics, nodechat, openbps[5], connectbps[5];

  int16_t usernum;

  char firstname[15], password[12];

  int16_t time_on, prev_used;

  char time_logged[5];

  int16_t time_limit, down_limit;

  char curconf, bitmap1[5], bitmap2[5];

  int16_t time_added, time_credit;

  char slanguage[4], name[25];

  int16_t sminsleft;

  char snodenum, seventtime[5], seventactive[2],
       sslide[2], smemmsg[4], scomport, packflag, bpsflag;

  // PCB 14.5 extra stuff
  char ansi, lastevent[8];

  int16_t lasteventmin;

  char exittodos, eventupcoming;

  int16_t lastconfarea;

  char hconfbitmap;
  // end PCB 14.5 additions
};

static constexpr int NULL_HANDLE = 0;

static int GetDoor32CommType() {
  if (!a()->using_modem) {
    return 0;
  }
#ifdef _WIN32
  return (a()->remoteIO()->GetHandle() == NULL_HANDLE) ? 1 : 2;
#else
  return 0;
#endif
}

static int GetDoor32Emulation() {
  return (okansi()) ? 1 : 0;
}

const string create_filename(int nDropFileType) {
  switch (nDropFileType) {
  case CHAINFILE_CHAIN:
    return StrCat(a()->temp_directory(), "chain.txt");
  case CHAINFILE_DORINFO:
    return  StrCat(a()->temp_directory(), "dorinfo1.def");
  case CHAINFILE_PCBOARD:
    return  StrCat(a()->temp_directory(), "pcboard.sys");
  case CHAINFILE_CALLINFO:
    return  StrCat(a()->temp_directory(), "callinfo.bbs");
  case CHAINFILE_DOOR:
    return  StrCat(a()->temp_directory(), "door.sys");
  case CHAINFILE_DOOR32:
    return  StrCat(a()->temp_directory(), "door32.sys");
  default:
    // Default to CHAIN.TXT since this is the native WWIV dormat
    return  StrCat(a()->temp_directory(), "chain.txt");
  }
}

/**
 * Returns first or last name from string (s) back into s
 */
static void GetNamePartForDropFile(bool lastName, char *name) {
  if (!lastName) {
    char *ss = strchr(name, ' ');
    if (ss) {
      name[strlen(name) - strlen(ss)] = '\0';
    }
  } else {
    char *ss = strrchr(name, ' ');
    sprintf(name, "%s", (ss) ? ++ss : "");
  }
}

long GetMinutesRemainingForDropFile() {
  long time_left = std::max<long>((static_cast<long>(nsl() / 60)) - 1L, 0);
  bool using_modem = a()->using_modem != 0;
  if (!using_modem) {
    // When we generate a dropfile from the WFC, give it a suitable amount
    // of time remaining vs. 1 minute since we don't have an active session.
    // Also allow at least an hour for all local users.
    return std::max<long>(60, time_left);
  }
  return time_left;
}

/** make DORINFO1.DEF (RBBS and many others) dropfile */
void CreateDoorInfoDropFile() {
  string fileName = create_filename(CHAINFILE_DORINFO);
  File::Remove(fileName);
  TextFile fileDorInfoSys(fileName, "wt");
  if (fileDorInfoSys.IsOpen()) {
    fileDorInfoSys.WriteFormatted("%s\n%s\n\nCOM%d\n", syscfg.systemname, syscfg.sysopname,
                                  incom ? a()->primary_port() : 0);
    fileDorInfoSys.WriteFormatted("%u ", ((a()->using_modem) ? com_speed : 0));
    fileDorInfoSys.WriteFormatted("BAUD,N,8,1\n0\n");
    if (syscfg.sysconfig & sysconfig_no_alias) {
      char szTemp[81];
      strcpy(szTemp, a()->user()->GetRealName());
      GetNamePartForDropFile(false, szTemp);
      fileDorInfoSys.WriteFormatted("%s\n", szTemp);
      strcpy(szTemp, a()->user()->GetRealName());
      GetNamePartForDropFile(true, szTemp);
      fileDorInfoSys.WriteFormatted("%s\n", szTemp);
    } else {
      fileDorInfoSys.WriteFormatted("%s\n\n", a()->user()->GetName());
    }
    if (syscfg.sysconfig & sysconfig_extended_info) {
      fileDorInfoSys.WriteFormatted("%s, %s\n", a()->user()->GetCity(),
                                    a()->user()->GetState());
    } else {
      fileDorInfoSys.WriteFormatted("\n");
    }
    fileDorInfoSys.WriteFormatted("%c\n%d\n%ld\n", a()->user()->HasAnsi() ? '1' : '0',
                                  a()->user()->GetSl(), GetMinutesRemainingForDropFile());
    fileDorInfoSys.Close();
  }
}

/** make PCBOARD.SYS (PC Board) drop file */
void CreatePCBoardSysDropFile() {
  string fileName = create_filename(CHAINFILE_PCBOARD);
  File pcbFile(fileName);
  pcbFile.Delete();
  if (pcbFile.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
    pcboard_sys_rec pcb;
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
    string com_speed_str = std::to_string(com_speed);
    sprintf(pcb.openbps, "%-5.5s", com_speed_str.c_str());
    if (!incom) {
      memcpy(pcb.connectbps, "Local", 5);
    } else {
      snprintf(pcb.connectbps, 5, "%-5.5d", modem_speed);
    }
    pcb.usernum = static_cast<int16_t>(a()->usernum);
    char szName[255];
    sprintf(szName, "%-25.25s", a()->user()->GetName());
    char *pszFirstName = strtok(szName, " \t");
    sprintf(pcb.firstname, "%-15.15s", pszFirstName);
    // Don't write password  security
    strcpy(pcb.password, "XXX");
    pcb.time_on = static_cast<int16_t>(a()->user()->GetTimeOn() / 60);
    pcb.prev_used = 0;
    double d = a()->user()->GetTimeOn() / 60;
    int h1 = static_cast<int>(d / 60) / 10;
    int h2 = static_cast<int>(d / 60) - (h1 * 10);
    int m1 = static_cast<int>(d - ((h1 * 10 + h2) * 60)) / 10;
    int m2 = static_cast<int>(d - ((h1 * 10 + h2) * 60)) - (m1 * 10);
    pcb.time_logged[0] = static_cast<char>(h1 + '0');
    pcb.time_logged[1] = static_cast<char>(h2 + '0');
    pcb.time_logged[2] = ':';
    pcb.time_logged[3] = static_cast<char>(m1 + '0');
    pcb.time_logged[4] = static_cast<char>(m2 + '0');
    pcb.time_limit = static_cast<int16_t>(nsl());
    pcb.down_limit = 1024;
    pcb.curconf = static_cast<char>(a()->GetCurrentConferenceMessageArea());
    strcpy(pcb.slanguage, a()->cur_lang_name.c_str());
    strcpy(pcb.name, a()->user()->GetName());
    pcb.sminsleft = pcb.time_limit;
    pcb.snodenum = static_cast<char>((num_instances() > 1) ? a()->instance_number() : 0);
    memcpy(pcb.seventtime, "01:00", 5);
    memcpy(pcb.seventactive, " 0", 2);
    memcpy(pcb.sslide, " 0", 2);
    pcb.scomport = a()->primary_port() + '0';
    pcb.packflag = 27;
    pcb.bpsflag = 32;
    // Added for PCB 14.5 Revision
    std::unique_ptr<WStatus> pStatus(a()->status_manager()->GetStatus());
    strcpy(pcb.lastevent, pStatus->GetLastDate());
    pcb.exittodos = '0';
    pcb.eventupcoming = '0';
    pcb.lastconfarea = static_cast<int16_t>(a()->GetCurrentConferenceMessageArea());
    // End Additions

    pcbFile.Write(&pcb, sizeof(pcb));
    pcbFile.Close();
  }
}

void CreateCallInfoBbsDropFile() {
  // make CALLINFO.BBS (WildCat!)
  string fileName = create_filename(CHAINFILE_CALLINFO);
  File::Remove(fileName);
  TextFile file(fileName, "wt");
  if (file.IsOpen()) {
    file.WriteLine(a()->user()->GetRealName());
    switch (modem_speed) {
    case 300:
      file.WriteLine("1");
    case 1200:
      file.WriteLine("2");
    case 2400:
      file.WriteLine("0");
    case 19200:
      file.WriteLine("4");
    default:
      file.WriteLine("3");
    }
    string t = times();
    auto start_duration = duration_since_midnight(a()->system_logon_time());
    auto start_minute = std::chrono::duration_cast<std::chrono::minutes>(start_duration).count();
    file.WriteLine(" ");
    file.WriteLine(a()->user()->GetSl());
    file.WriteLine(GetMinutesRemainingForDropFile());
    file.WriteLine(a()->user()->HasAnsi() ? "COLOR" : "MONO");
    file.WriteLine("X");
    file.WriteLine(a()->usernum);
    file.WriteLine(start_minute);
    file.WriteLine(t.substr(0, 5));
    file.WriteLine("0");
    file.WriteLine("ABCD");
    file.WriteLine("0");
    file.WriteLine("0");
    file.WriteLine("0");
    file.WriteLine("0");
    file.WriteFormatted("%s\n%s 00:01\nEXPERT\nN\n%s\n%d\n%d\n1\n%d\n%d\n%s\n%s\n%d\n",
                        a()->user()->GetVoicePhoneNumber(),
                        a()->user()->GetLastOn(),
                        a()->user()->GetLastOn(),
                        a()->user()->GetNumLogons(),
                        a()->user()->GetScreenLines(),
                        a()->user()->GetFilesUploaded(),
                        a()->user()->GetFilesDownloaded(),
                        "8N1",
                        (incom) ? "REMOTE" : "LOCAL",
                        (incom) ? a()->primary_port() : 0);
    char szDate[81], szTemp[81];
    strcpy(szDate, "00/00/00");
    sprintf(szTemp, "%d", a()->user()->GetBirthdayMonth());
    szTemp[2] = '\0';
    memmove(&(szDate[2 - strlen(szTemp)]), &(szTemp[0]), strlen(szTemp));
    sprintf(szTemp, "%d", a()->user()->GetBirthdayDay());
    szTemp[2] = '\0';
    memmove(&(szDate[ 5 - strlen(szTemp) ]), &(szTemp[0]), strlen(szTemp));
    sprintf(szTemp, "%d", a()->user()->GetBirthdayYearShort());
    szTemp[2] = '\0';
    memmove(&(szDate[ 8 - strlen(szTemp) ]), &(szTemp[0]), strlen(szTemp));
    file.WriteFormatted("%s\n", szDate);
    string cspeed = std::to_string(com_speed);
    file.WriteFormatted("%s\n", (incom) ? cspeed.c_str() : "38400");
    file.Close();
  }
}

static unsigned int GetDoorHandle() {
  if (a()->remoteIO()) {
    return a()->remoteIO()->GetDoorHandle();
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
  string fileName = create_filename(CHAINFILE_DOOR32);
  File::Remove(fileName);

  TextFile file(fileName, "wt");
  if (file.IsOpen()) {
    file.WriteFormatted("%d\n", GetDoor32CommType());
    file.WriteFormatted("%u\n", GetDoorHandle());
    string cspeed = std::to_string(com_speed);
    file.WriteFormatted("%s\n", cspeed.c_str());
    file.WriteFormatted("WWIV %s\n", wwiv_version);
    file.WriteFormatted("%d\n", a()->usernum);
    file.WriteFormatted("%s\n", a()->user()->GetRealName());
    file.WriteFormatted("%s\n", a()->user()->GetName());
    file.WriteFormatted("%d\n", a()->user()->GetSl());
    file.WriteFormatted("%d\n", 60 * GetMinutesRemainingForDropFile());
    file.WriteFormatted("%d\n", GetDoor32Emulation());
    file.WriteFormatted("%u\n", a()->instance_number());
    file.Close();
  }
}

/** Create generic DOOR.SYS dropfile */
void CreateDoorSysDropFile() {
  string fileName = create_filename(CHAINFILE_DOOR);
  File::Remove(fileName);

  TextFile file(fileName, "wt");
  if (file.IsOpen()) {
    char szLine[255];
    string cspeed = std::to_string(com_speed);
    sprintf(szLine, "COM%d\n%s\n%c\n%u\n%d\n%c\n%c\n%c\n%c\n%s\n%s, %s\n",
            (a()->using_modem) ? a()->primary_port() : 0,
            cspeed.c_str(),
            '8',
            a()->instance_number(),                       // node
            (a()->using_modem) ? modem_speed : 38400,
            'Y',                            // screen display
            'N',              // log to printer
            'N',                            // page bell
            'N',                            // caller alarm
            a()->user()->GetRealName(),
            a()->user()->GetCity(),
            a()->user()->GetState());
    file.WriteFormatted(szLine);
    sprintf(szLine, "%s\n%s\n%s\n%d\n%u\n%s\n%u\n%ld\n",
            a()->user()->GetVoicePhoneNumber(),
            a()->user()->GetDataPhoneNumber(),
            "X",                            // a()->user()->GetPassword()
            a()->user()->GetSl(),
            a()->user()->GetNumLogons(),
            a()->user()->GetLastOn(),
            static_cast<uint32_t>(60L * GetMinutesRemainingForDropFile()),
            GetMinutesRemainingForDropFile());
    file.WriteFormatted(szLine);
    string ansiStatus = (okansi()) ? "GR" : "NG";
    sprintf(szLine, "%s\n%u\n%c\n%s\n%lu\n%s\n%u\n%c\n%u\n%u\n%u\n%d\n",
            ansiStatus.c_str(),
            a()->user()->GetScreenLines(),
            a()->user()->IsExpert() ? 'Y' : 'N',
            "1,2,3",                        // conferences
            a()->current_user_sub_num(),  // current 'conference'
            "12/31/99",                     // expiration date
            a()->usernum,
            'Y',                            // default protocol
            a()->user()->GetFilesUploaded(),
            a()->user()->GetFilesDownloaded(),
            0,                              // kb dl today
            0);                            // kb dl/day max
    file.WriteFormatted(szLine);
    char szDate[21], szTemp[81];
    strcpy(szDate, "00/00/00");
    sprintf(szTemp, "%d", a()->user()->GetBirthdayMonth());
    szTemp[2] = '\0';
    memmove(&(szDate[2 - strlen(szTemp)]), &(szTemp[0]), strlen(szTemp));
    sprintf(szTemp, "%d", a()->user()->GetBirthdayDay());
    szTemp[2] = '\0';
    memmove(&(szDate[5 - strlen(szTemp)]), &(szTemp[0]), strlen(szTemp));
    sprintf(szTemp, "%d", a()->user()->GetBirthdayYearShort());
    szTemp[2] = '\0';
    memmove(&(szDate[8 - strlen(szTemp)]), &(szTemp[0]), strlen(szTemp));
    szDate[9] = '\0';
    string gfilesdir = a()->config()->gfilesdir();
    sprintf(szLine, "%s\n%s\n%s\n%s\n%s\n%s\n%c\n%c\n%c\n%u\n%u\n%s\n%-.5s\n%s\n",
        szDate,
        a()->config()->datadir().c_str(),
        gfilesdir.c_str(),
        syscfg.sysopname,
        a()->user()->GetName(),
        "00:01",                        // event time
        'Y',
        (okansi()) ? 'N' : 'Y',           // ansi ok but graphics turned off
        'N',                            // record-locking
        a()->user()->GetColor(0),
        a()->user()->GetTimeBankMinutes(),
        a()->user()->GetLastOn(),                // last n-scan date
        times(),
        "00:01");                       // time last call
    file.WriteFormatted(szLine);
    sprintf(szLine, "%u\n%u\n%ld\n%ld\n%s\n%u\n%d\n",
            99,                             // max files dl/day
            0,                              // files dl today so far
            a()->user()->GetUploadK(),
            a()->user()->GetDownloadK(),
            a()->user()->GetNote(),
            a()->user()->GetNumChainsRun(),
            a()->user()->GetNumMessagesPosted());
    file.WriteFormatted(szLine);
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
CHAIN.TXT Definition File by MrBill.
-----------CHAIN.TXT-----------------------------------
1                  User number
MRBILL             User alias
Bill               User real name
User callsign (HAM radio)
21                 User age
M                  User sex
16097.00         User gold
05/19/89           User last logon date
80                 User colums
25                 User width
255                User security level (0-255)
1                  1 if Co-SysOp, 0 if not
1                  1 if SysOp, 0 if not
1                  1 if ANSI, 0 if not
0                  1 if at remote, 0 if local console
2225.78         User number of seconds left till logoff
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
 */
const string create_chain_file() {
  string cspeed;

  cspeed = std::to_string(com_speed);

  create_drop_files();
  auto start_duration = duration_since_midnight(a()->system_logon_time());
  int start_second = static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(start_duration).count());
  auto used_duration = std::chrono::system_clock::now() - a()->system_logon_time();
  int seconds_used = static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(used_duration).count());

  const string fileName = create_filename(CHAINFILE_CHAIN);
  File::Remove(fileName);
  TextFile file(fileName, "wt");
  if (file.IsOpen()) {
    file.WriteFormatted("%d\n%s\n%s\n%s\n%d\n%c\n%10.2f\n%s\n%d\n%d\n%d\n",
      a()->usernum,
      a()->user()->GetName(),
      a()->user()->GetRealName(),
      a()->user()->GetCallsign(),
      a()->user()->GetAge(),
      a()->user()->GetGender(),
      a()->user()->GetGold(),
      a()->user()->GetLastOn(),
      a()->user()->GetScreenChars(),
      a()->user()->GetScreenLines(),
      a()->user()->GetSl());
    char szTemporaryLogFileName[MAX_PATH];
    GetTemporaryInstanceLogFileName(szTemporaryLogFileName);
    string gfilesdir = a()->config()->gfilesdir();
    file.WriteFormatted("%d\n%d\n%d\n%u\n%8ld.00\n%s\n%s\n%s\n",
      cs(), so(), okansi(), incom, nsl(), 
      gfilesdir.c_str(), a()->config()->datadir().c_str(), szTemporaryLogFileName);
    if (a()->using_modem) {
      file.WriteFormatted("%d\n", modem_speed);
    } else {
      file.WriteFormatted("KB\n");
    }
    file.WriteFormatted("%d\n%s\n%s\n%d\n%d\n%lu\n%u\n%lu\n%u\n%s\n%s\n%u\n",
        a()->primary_port(),
        syscfg.systemname,
        syscfg.sysopname,
        start_second,
        seconds_used,
        a()->user()->GetUploadK(),
        a()->user()->GetFilesUploaded(),
        a()->user()->GetDownloadK(),
        a()->user()->GetFilesDownloaded(),
        "8N1",
        cspeed.c_str(),
      a()->current_net().sysnum);
    file.WriteFormatted("N\nN\nN\n");
    file.WriteFormatted("%u\n%u\n", a()->user()->GetAr(), a()->user()->GetDar());
    file.Close();
  }
  
  return fileName;
}

