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
#include <string>

#include "bbs/datetime.h"
#include "bbs/bbs.h"
#include "bbs/fcns.h"
#include "bbs/instmsg.h"
#include "bbs/vars.h"
#include "bbs/remote_io.h"
#include "bbs/wconstants.h"
#include "bbs/wstatus.h"
#include "core/strings.h"
#include "core/textfile.h"

using std::string;

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

// Local functions
int GetDoor32Emulation();
int GetDoor32CommType();
void GetNamePartForDropFile(bool lastName, char *name);
void create_drop_files();
string GetComSpeedInDropfileFormat(unsigned long lComSpeed);

const string create_filename(int nDropFileType) {
  std::ostringstream os;
  os << syscfgovr.tempdir;
  switch (nDropFileType) {
  case CHAINFILE_CHAIN:
    os << "chain.txt";
    break;
  case CHAINFILE_DORINFO:
    os << "dorinfo1.def";
    break;
  case CHAINFILE_PCBOARD:
    os << "pcboard.sys";
    break;
  case CHAINFILE_CALLINFO:
    os << "callinfo.bbs";
    break;
  case CHAINFILE_DOOR:
    os << "door.sys";
    break;
  case CHAINFILE_DOOR32:
    os << "door32.sys";
    break;
  default:
    // Default to CHAIN.TXT since this is the native WWIV dormat
    os << "chain.txt";
    break;
  }
  return string(os.str());
}

/**
 * Returns first or last name from string (s) back into s
 */
void GetNamePartForDropFile(bool lastName, char *name) {
  if (!lastName) {
    char *ss = strchr(name, ' ');
    if (ss) {
      name[ strlen(name) - strlen(ss) ] = '\0';
    }
  } else {
    char *ss = strrchr(name, ' ');
    sprintf(name, "%s", (ss) ? ++ss : "");
  }
}

string GetComSpeedInDropfileFormat(unsigned long lComSpeed) {
  if (lComSpeed == 1 || lComSpeed == 49664) {
    lComSpeed = 115200;
  }
  std::ostringstream os;
  os << lComSpeed;
  return string(os.str());
}

long GetMinutesRemainingForDropFile() {
  long time_left = std::max<long>((static_cast<long>(nsl() / 60)) - 1L, 0);
  bool using_modem = session()->using_modem != 0;
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
                                  incom ? syscfgovr.primaryport : 0);
    fileDorInfoSys.WriteFormatted("%u ", ((session()->using_modem) ? com_speed : 0));
    fileDorInfoSys.WriteFormatted("BAUD,N,8,1\n0\n");
    if (syscfg.sysconfig & sysconfig_no_alias) {
      char szTemp[81];
      strcpy(szTemp, session()->user()->GetRealName());
      GetNamePartForDropFile(false, szTemp);
      fileDorInfoSys.WriteFormatted("%s\n", szTemp);
      strcpy(szTemp, session()->user()->GetRealName());
      GetNamePartForDropFile(true, szTemp);
      fileDorInfoSys.WriteFormatted("%s\n", szTemp);
    } else {
      fileDorInfoSys.WriteFormatted("%s\n\n", session()->user()->GetName());
    }
    if (syscfg.sysconfig & sysconfig_extended_info) {
      fileDorInfoSys.WriteFormatted("%s, %s\n", session()->user()->GetCity(),
                                    session()->user()->GetState());
    } else {
      fileDorInfoSys.WriteFormatted("\n");
    }
    fileDorInfoSys.WriteFormatted("%c\n%d\n%ld\n", session()->user()->HasAnsi() ? '1' : '0',
                                  session()->user()->GetSl(), GetMinutesRemainingForDropFile());
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
    strcpy(pcb.display, "-1");
    strcpy(pcb.printer, "0"); // -1 if logging is to the printer, 0 otherwise;
    strcpy(pcb.page_bell, " 0");
    strcpy(pcb.alarm, " 0");
    strcpy(pcb.errcheck, "-1");
    if (okansi()) {
      pcb.graphics = 'Y';
      pcb.ansi = '1';
    } else {
      pcb.graphics = 'N';
      pcb.ansi = '0';
    }
    pcb.nodechat = 32;
    string com_speed_str = GetComSpeedInDropfileFormat(com_speed);
    sprintf(pcb.openbps, "%-5.5s", com_speed_str.c_str());
    if (!incom) {
      strcpy(pcb.connectbps, "Local");
    } else {
      sprintf(pcb.connectbps, "%-5.5d", modem_speed);
    }
    pcb.usernum = static_cast<int16_t>(session()->usernum);
    char szName[255];
    sprintf(szName, "%-25.25s", session()->user()->GetName());
    char *pszFirstName = strtok(szName, " \t");
    sprintf(pcb.firstname, "%-15.15s", pszFirstName);
    // Don't write password  security
    strcpy(pcb.password, "XXX");
    pcb.time_on = static_cast<int16_t>(session()->user()->GetTimeOn() / 60);
    pcb.prev_used = 0;
    double d = session()->user()->GetTimeOn() / 60;
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
    pcb.curconf = static_cast<char>(session()->GetCurrentConferenceMessageArea());
    strcpy(pcb.slanguage, session()->cur_lang_name.c_str());
    strcpy(pcb.name, session()->user()->GetName());
    pcb.sminsleft = pcb.time_limit;
    pcb.snodenum = static_cast<char>((num_instances() > 1) ? session()->instance_number() : 0);
    strcpy(pcb.seventtime, "01:00");
    strcpy(pcb.seventactive, " 0");
    strcpy(pcb.sslide, " 0");
    pcb.scomport = syscfgovr.primaryport + '0';
    pcb.packflag = 27;
    pcb.bpsflag = 32;
    // Added for PCB 14.5 Revision
    std::unique_ptr<WStatus> pStatus(session()->status_manager()->GetStatus());
    strcpy(pcb.lastevent, pStatus->GetLastDate());
    pcb.exittodos = '0';
    pcb.eventupcoming = '0';
    pcb.lastconfarea = static_cast<int16_t>(session()->GetCurrentConferenceMessageArea());
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
    file.WriteFormatted("%s\n", session()->user()->GetRealName());
    switch (modem_speed) {
    case 300:
      file.WriteFormatted("1\n");
    case 1200:
      file.WriteFormatted("2\n");
    case 2400:
      file.WriteFormatted("0\n");
    case 19200:
      file.WriteFormatted("4\n");
    default:
      file.WriteFormatted("3\n");
    }
    string t = times();
    file.WriteFormatted(" \n%d\n%ld\n%s\n%s\n%d\n%d\n%.5s\n0\nABCD\n0\n0\n0\n0\n",
                        session()->user()->GetSl(),
			GetMinutesRemainingForDropFile(),
                        session()->user()->HasAnsi() ? "COLOR" : "MONO",
                        "X" /* session()->user()->GetPassword() */,
			session()->usernum,
			static_cast<int>(timeon / 60),
                        t.c_str());
    file.WriteFormatted("%s\n%s 00:01\nEXPERT\nN\n%s\n%d\n%d\n1\n%d\n%d\n%s\n%s\n%d\n",
                        session()->user()->GetVoicePhoneNumber(),
                        session()->user()->GetLastOn(),
                        session()->user()->GetLastOn(),
                        session()->user()->GetNumLogons(),
                        session()->user()->GetScreenLines(),
                        session()->user()->GetFilesUploaded(),
                        session()->user()->GetFilesDownloaded(),
                        "8N1",
                        (incom) ? "REMOTE" : "LOCAL",
                        (incom) ? 0 : syscfgovr.primaryport);
    char szDate[81], szTemp[81];
    strcpy(szDate, "00/00/00");
    sprintf(szTemp, "%d", session()->user()->GetBirthdayMonth());
    szTemp[2] = '\0';
    memmove(&(szDate[2 - strlen(szTemp)]), &(szTemp[0]), strlen(szTemp));
    sprintf(szTemp, "%d", session()->user()->GetBirthdayDay());
    szTemp[2] = '\0';
    memmove(&(szDate[ 5 - strlen(szTemp) ]), &(szTemp[0]), strlen(szTemp));
    sprintf(szTemp, "%d", session()->user()->GetBirthdayYearShort());
    szTemp[2] = '\0';
    memmove(&(szDate[ 8 - strlen(szTemp) ]), &(szTemp[0]), strlen(szTemp));
    file.WriteFormatted("%s\n", szDate);
    string cspeed = GetComSpeedInDropfileFormat(com_speed);
    file.WriteFormatted("%s\n", (incom) ? cspeed.c_str() : "14400");
    file.Close();
  }
}

static unsigned int GetDoorHandle() {
  if (session()->remoteIO()) {
    return session()->remoteIO()->GetDoorHandle();
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
    file.WriteFormatted("%d\n",     GetDoor32CommType());
    file.WriteFormatted("%u\n",     GetDoorHandle());
    string cspeed = GetComSpeedInDropfileFormat(com_speed);
    file.WriteFormatted("%s\n",      cspeed.c_str());
    file.WriteFormatted("WWIV %s\n", wwiv_version);
    file.WriteFormatted("999999\n"); // we don't want to share this
    file.WriteFormatted("%s\n",      session()->user()->GetRealName());
    file.WriteFormatted("%s\n",      session()->user()->GetName());
    file.WriteFormatted("%d\n",      session()->user()->GetSl());
    file.WriteFormatted("%d\n",      60 * GetMinutesRemainingForDropFile());
    file.WriteFormatted("%d\n",      GetDoor32Emulation());
    file.WriteFormatted("%u\n",      session()->instance_number());
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
    string cspeed = GetComSpeedInDropfileFormat(com_speed);
    sprintf(szLine, "COM%d\n%s\n%c\n%u\n%d\n%c\n%c\n%c\n%c\n%s\n%s, %s\n",
            (session()->using_modem) ? syscfgovr.primaryport : 0,
            cspeed.c_str(),
            '8',
            session()->instance_number(),                       // node
            (session()->using_modem) ? modem_speed : 14400,
            'Y',                            // screen display
            'N',              // log to printer
            'N',                            // page bell
            'N',                            // caller alarm
            session()->user()->GetRealName(),
            session()->user()->GetCity(),
            session()->user()->GetState());
    file.WriteFormatted(szLine);
    sprintf(szLine, "%s\n%s\n%s\n%d\n%u\n%s\n%u\n%ld\n",
            session()->user()->GetVoicePhoneNumber(),
            session()->user()->GetDataPhoneNumber(),
            "X",                            // session()->user()->GetPassword()
            session()->user()->GetSl(),
            session()->user()->GetNumLogons(),
            session()->user()->GetLastOn(),
            static_cast<uint32_t>(60L * GetMinutesRemainingForDropFile()),
            GetMinutesRemainingForDropFile());
    file.WriteFormatted(szLine);
    string ansiStatus = (okansi()) ? "GR" : "NG";
    sprintf(szLine, "%s\n%u\n%c\n%s\n%d\n%s\n%d\n%c\n%u\n%u\n%u\n%u\n",
            ansiStatus.c_str(),
            session()->user()->GetScreenLines(),
            session()->user()->IsExpert() ? 'Y' : 'N',
            "1,2,3",                        // conferences
            session()->GetCurrentMessageArea(),  // current 'conference'
            "12/31/99",                     // expiration date
            session()->usernum,
            'Y',                            // default protocol
            session()->user()->GetFilesUploaded(),
            session()->user()->GetFilesDownloaded(),
            0,                              // kb dl today
            0);                             // kb dl/day max
    file.WriteFormatted(szLine);
    char szDate[21], szTemp[81];
    strcpy(szDate, "00/00/00");
    sprintf(szTemp, "%d", session()->user()->GetBirthdayMonth());
    szTemp[2] = '\0';
    memmove(&(szDate[2 - strlen(szTemp)]), &(szTemp[0]), strlen(szTemp));
    sprintf(szTemp, "%d", session()->user()->GetBirthdayDay());
    szTemp[2] = '\0';
    memmove(&(szDate[5 - strlen(szTemp)]), &(szTemp[0]), strlen(szTemp));
    sprintf(szTemp, "%d", session()->user()->GetBirthdayYearShort());
    szTemp[2] = '\0';
    memmove(&(szDate[8 - strlen(szTemp)]), &(szTemp[0]), strlen(szTemp));
    szDate[9] = '\0';
    sprintf(szLine, "%s\n%s\n%s\n%s\n%s\n%s\n%c\n%c\n%c\n%u\n%u\n%s\n%-.5s\n%s\n",
            szDate,
            syscfg.datadir,
            syscfg.gfilesdir,
            syscfg.sysopname,
            session()->user()->GetName(),
            "00:01",                        // event time
            'Y',
            (okansi()) ? 'N' : 'Y',           // ansi ok but graphics turned off
            'N',                            // record-locking
            session()->user()->GetColor(0),
            session()->user()->GetTimeBankMinutes(),
            session()->user()->GetLastOn(),                // last n-scan date
            times(),
            "00:01");                       // time last call
    file.WriteFormatted(szLine);
    sprintf(szLine, "%u\n%u\n%ld\n%ld\n%s\n%u\n%d\n",
            99,                             // max files dl/day
            0,                              // files dl today so far
            session()->user()->GetUploadK(),
            session()->user()->GetDownloadK(),
            session()->user()->GetNote(),
            session()->user()->GetNumChainsRun(),
            session()->user()->GetNumMessagesPosted());
    file.WriteFormatted(szLine);
    file.Close();
  }
}

void create_drop_files() {
  CreateDoorInfoDropFile();
  CreatePCBoardSysDropFile();
  CreateCallInfoBbsDropFile();
  CreateDoorSysDropFile();
  CreateDoor32SysDropFile();
}

const string create_chain_file() {
  string cspeed;

  unsigned char nSaveComPortNum = syscfgovr.primaryport;
  if (syscfgovr.primaryport == 0 && ok_modem_stuff) {
    // This is so that we'll use COM1 in DOORS even though our comport is set
    // to 0 in init.  It's not perfect, but it'll make sure doors work more
    // often than not.
    syscfgovr.primaryport = 1;
  }

  if (com_speed == 1 || com_speed == 49664) {
    cspeed = "115200";
  } else {
    cspeed = std::to_string(com_speed);
  }

  create_drop_files();
  long l = static_cast<long>(timeon);
  if (l < 0) {
    l += SECONDS_PER_HOUR * HOURS_PER_DAY;
  }
  long l1 = static_cast<long>(timer() - timeon);
  if (l1 < 0) {
    l1 += SECONDS_PER_HOUR * HOURS_PER_DAY;
  }

  string fileName = create_filename(CHAINFILE_CHAIN);

  File::Remove(fileName);
  TextFile file(fileName, "wt");
  if (file.IsOpen()) {
    file.WriteFormatted("%d\n%s\n%s\n%s\n%d\n%c\n%10.2f\n%s\n%d\n%d\n%d\n",
                        session()->usernum,
                        session()->user()->GetName(),
                        session()->user()->GetRealName(),
                        session()->user()->GetCallsign(),
                        session()->user()->GetAge(),
                        session()->user()->GetGender(),
                        session()->user()->GetGold(),
                        session()->user()->GetLastOn(),
                        session()->user()->GetScreenChars(),
                        session()->user()->GetScreenLines(),
                        session()->user()->GetSl());
    char szTemporaryLogFileName[MAX_PATH];
    GetTemporaryInstanceLogFileName(szTemporaryLogFileName);
    file.WriteFormatted("%d\n%d\n%d\n%u\n%10.2f\n%s\n%s\n%s\n",
                        cs(), so(), okansi(), incom, nsl(), syscfg.gfilesdir, syscfg.datadir, szTemporaryLogFileName);
    if (session()->using_modem) {
      file.WriteFormatted("%d\n", modem_speed);
    } else {
      file.WriteFormatted("KB\n");
    }
    file.WriteFormatted("%d\n%s\n%s\n%ld\n%ld\n%lu\n%u\n%lu\n%u\n%s\n%s\n%u\n",
                        syscfgovr.primaryport,
                        syscfg.systemname,
                        syscfg.sysopname,
                        l,
                        l1,
                        session()->user()->GetUploadK(),
                        session()->user()->GetFilesUploaded(),
                        session()->user()->GetDownloadK(),
                        session()->user()->GetFilesDownloaded(),
                        "8N1",
                        cspeed.c_str(),
                        net_sysnum);
    file.WriteFormatted("N\nN\nN\n");
    file.WriteFormatted("%u\n%u\n", session()->user()->GetAr(), session()->user()->GetDar());
    file.Close();
  }
  syscfgovr.primaryport = nSaveComPortNum;

  return string(fileName);
}

static const int NULL_HANDLE = 0;

int GetDoor32CommType() {
  if (!session()->using_modem) {
    return 0;
  }
#ifdef _WIN32
  return (session()->remoteIO()->GetHandle() == NULL_HANDLE) ? 1 : 2;
#else
  return 0;
#endif
}

int GetDoor32Emulation() {
  return (okansi()) ? 1 : 0;
}
