/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2014, WWIV Software Services             */
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
#include <algorithm>
#include <cmath>
#ifdef _WIN32
#include <sys/utime.h>
#endif  // WIN32

#include "wwiv.h"
#include "common.h"
#include "core/wfndfile.h"

extern const unsigned char *translate_letters[];

template<class _Ty> inline const _Ty& in_range(const _Ty& minValue, const _Ty& maxValue, const _Ty& value);

/**
 * Deletes files from a directory.  This is meant to be used only in the temp
 * directories of WWIV.
 *
 * @param pszFileName       Wildcard file specification to delete
 * @param pszDirectoryName  Name of the directory to delete files from
 * @param bPrintStatus      Print out locally as files are deleted
 */
void remove_from_temp(const char *pszFileName, const char *pszDirectoryName, bool bPrintStatus) {
  char szFileSpecification[ MAX_PATH ];

  WWIV_ASSERT(pszFileName);
  WWIV_ASSERT(pszDirectoryName);

  sprintf(szFileSpecification, "%s%s", pszDirectoryName, stripfn(pszFileName));
  WFindFile fnd;
  bool bFound = fnd.open(szFileSpecification, 0);
  GetSession()->bout.NewLine();
  while (bFound) {
    char szFileName[MAX_PATH];
    strcpy(szFileName, fnd.GetFileName());

    //
    // We don't want to delete ".", "..", or any of the door drop files
    // (you can't delete . or .. anyway!  but it's a waste of time to try...
    //
    if (!wwiv::strings::IsEqualsIgnoreCase(szFileName, "chain.txt") &&
        !wwiv::strings::IsEqualsIgnoreCase(szFileName, "door.sys") &&
        !wwiv::strings::IsEqualsIgnoreCase(szFileName, "door32.sys") &&
        !wwiv::strings::IsEqualsIgnoreCase(szFileName, "dorinfo1.def") &&
        !wwiv::strings::IsEqualsIgnoreCase(szFileName, "callinfo.bbs") &&
        !wwiv::strings::IsEqualsIgnoreCase(szFileName, "pcboard.sys") &&
        !wwiv::strings::IsEqualsIgnoreCase(szFileName, ".") &&
        !wwiv::strings::IsEqualsIgnoreCase(szFileName, "..")) {
      if (bPrintStatus) {
        std::cout << "Deleting TEMP file: " << pszDirectoryName << szFileName << std::endl;
      }
      WFile::Remove(pszDirectoryName, szFileName);
    }
    bFound = fnd.next();
  }
}



/**
 * Does the currently online user have ANSI.  The user record is
 * checked for this information
 *
 * @return true if the user wants ANSI, false otherwise.
 */
bool okansi() {
  return GetSession()->GetCurrentUser()->HasAnsi() && !x_only;
}


/**
 * Should be called after a user is logged off, and will initialize
 * screen-access variables.
 */
void frequent_init() {
  setiia(90);
  g_flags = 0;
  GetSession()->tagging = 0;
  newline = true;
  GetSession()->SetCurrentReadMessageArea(-1);
  GetSession()->SetCurrentConferenceMessageArea(0);
  GetSession()->SetCurrentConferenceFileArea(0);
  ansiptr = 0;
  curatr = 0x07;
  outcom = false;
  incom = false;
  charbufferpointer = 0;
  GetSession()->localIO()->SetTopLine(0);
  GetSession()->screenlinest = defscreenbottom + 1;
  endofline[0] = '\0';
  hangup = false;
  hungup = false;
  chatcall = false;
  GetSession()->localIO()->ClearChatReason();
  GetSession()->SetUserOnline(false);
  change_color = 0;
  chatting = 0;
  local_echo = true;
  irt[0] = '\0';
  irt_name[0] = '\0';
  lines_listed = 0;
  GetSession()->ReadCurrentUser(1);
  read_qscn(1, qsc, false);
  fwaiting = (GetSession()->GetCurrentUser()->IsUserDeleted()) ? 0 : GetSession()->GetCurrentUser()->GetNumMailWaiting();
  okmacro = true;
  okskey = true;
  mailcheck = false;
  smwcheck = false;
  use_workspace = false;
  extratimecall = 0.0;
  GetSession()->using_modem = 0;
  GetSession()->localIO()->set_global_handle(false);
  WFile::SetFilePermissions(g_szDSZLogFileName, WFile::permReadWrite);
  WFile::Remove(g_szDSZLogFileName);
  GetSession()->SetTimeOnlineLimited(false);
  GetSession()->localIO()->set_x_only(0, NULL, 0);
  set_net_num(0);
  set_language(GetSession()->GetCurrentUser()->GetLanguage());
  reset_disable_conf();
}


/**
 * Gets the current users upload/download ratio.
 */
double ratio() {
  if (GetSession()->GetCurrentUser()->GetDownloadK() == 0) {
    return 99.999;
  }
  double r = static_cast<float>(GetSession()->GetCurrentUser()->GetUploadK()) /
             static_cast<float>(GetSession()->GetCurrentUser()->GetDownloadK());

  return (r > 99.998) ? 99.998 : r;
}

/**
 * Gets the current users post/call ratio.
 */
double post_ratio() {
  if (GetSession()->GetCurrentUser()->GetNumLogons() == 0) {
    return 99.999;
  }
  double r = static_cast<float>(GetSession()->GetCurrentUser()->GetNumMessagesPosted()) /
             static_cast<float>(GetSession()->GetCurrentUser()->GetNumLogons());
  return (r > 99.998) ? 99.998 : r;
}

double nsl() {
  double rtn = 1.00;

  double dd = timer();
  if (GetSession()->IsUserOnline()) {
    if (timeon > (dd + SECONDS_PER_MINUTE_FLOAT)) {
      timeon -= SECONDS_PER_HOUR_FLOAT * HOURS_PER_DAY_FLOAT;
    }
    double tot = dd - timeon;
    double tpl = static_cast<double>(getslrec(GetSession()->GetEffectiveSl()).time_per_logon) * MINUTES_PER_HOUR_FLOAT;
    double tpd = static_cast<double>(getslrec(GetSession()->GetEffectiveSl()).time_per_day) * MINUTES_PER_HOUR_FLOAT;
    double tlc = tpl - tot + GetSession()->GetCurrentUser()->GetExtraTime() + extratimecall;
    double tlt = tpd - tot - static_cast<double>(GetSession()->GetCurrentUser()->GetTimeOnToday()) +
                 GetSession()->GetCurrentUser()->GetExtraTime();

    tlt = std::min<double>(tlc, tlt);
    rtn = in_range<double>(0.0, 32767.0, tlt);
  }

  GetSession()->SetTimeOnlineLimited(false);
  if (syscfg.executetime) {
    double tlt = time_event - dd;
    if (tlt < 0.0) {
      tlt += HOURS_PER_DAY_FLOAT * SECONDS_PER_HOUR_FLOAT;
    }
    if (rtn > tlt) {
      rtn = tlt;
      GetSession()->SetTimeOnlineLimited(true);
    }
    check_event();
    if (do_event) {
      rtn = 0.0;
    }
  }
  return in_range<double>(0.0, 32767.0, rtn);
}

void Wait(double d) {
  WWIV_ASSERT(d >= 0);
  if (d < 0) {
    return ;
  }
  const long lStartTime = timer1();
  auto l = d * 18.2;
  while (labs(timer1() - lStartTime) < l) {
    giveup_timeslice();
  }
}

/**
 * Returns the number of bytes free on the disk/volume specified.
 *
 * @param pszPathName Directory or Drive of which to list the free space.
 */
long freek1(const char *pszPathName) {
  WWIV_ASSERT(pszPathName);
  return WWIV_GetFreeSpaceForPath(pszPathName);
}

void send_net(net_header_rec * nh, unsigned short int *list, const char *text, const char *byname) {
  WWIV_ASSERT(nh);

  char szFileName[MAX_PATH];
  sprintf(szFileName, "%sp1%s", GetSession()->GetNetworkDataDirectory(), GetApplication()->GetNetworkExtension());
  WFile file(szFileName);
  if (!file.Open(WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile, WFile::shareUnknown,
                 WFile::permReadWrite)) {
    return;
  }
  file.Seek(0L, WFile::seekEnd);
  if (!list) {
    nh->list_len = 0;
  }
  if (!text) {
    nh->length = 0;
  }
  if (nh->list_len) {
    nh->tosys = 0;
  }
  long lNetHeaderSize = nh->length;
  if (byname && *byname) {
    nh->length += strlen(byname) + 1;
  }
  file.Write(nh, sizeof(net_header_rec));
  if (nh->list_len) {
    file.Write(list, 2 * (nh->list_len));
  }
  if (byname && *byname) {
    file.Write(byname, strlen(byname) + 1);
  }
  if (nh->length) {
    file.Write(text, lNetHeaderSize);
  }
  file.Close();
}



/**
 * Tells the OS that it is safe to preempt this task now.
 */
void giveup_timeslice() {
  WWIV_Delay(100);
  WWIV_Delay(0);

  if (!in_chatroom || !bChatLine) {
    if (inst_msg_waiting()) {
      process_inst_msgs();
    }
  }
}


char *stripfn(const char *pszFileName) {
  static char szStaticFileName[15];
  char szTempFileName[ MAX_PATH ];

  WWIV_ASSERT(pszFileName);

  int nSepIndex = -1;
  for (int i = 0; i < wwiv::strings::GetStringLength(pszFileName); i++) {
    if (pszFileName[i] == '\\' || pszFileName[i] == ':' || pszFileName[i] == '/') {
      nSepIndex = i;
    }
  }
  if (nSepIndex != -1) {
    strcpy(szTempFileName, &(pszFileName[nSepIndex + 1]));
  } else {
    strcpy(szTempFileName, pszFileName);
  }
  for (int i1 = 0; i1 < wwiv::strings::GetStringLength(szTempFileName); i1++) {
    if (szTempFileName[i1] >= 'A' && szTempFileName[i1] <= 'Z') {
      szTempFileName[i1] = szTempFileName[i1] - 'A' + 'a';
    }
  }
  int j = 0;
  while (szTempFileName[j] != 0) {
    if (szTempFileName[j] == SPACE) {
      strcpy(&szTempFileName[j], &szTempFileName[j + 1]);
    } else {
      ++j;
    }
  }
  strcpy(szStaticFileName, szTempFileName);
  return szStaticFileName;
}


void stripfn_inplace(char *pszFileName) {
  strcpy(pszFileName, stripfn(pszFileName));
}


void preload_subs() {
  bool abort = false;

  if (g_preloaded) {
    return;
  }

  GetSession()->bout.NewLine();
  GetSession()->bout << "|#1Caching message areas";
  int i1 = 3;
  for (GetSession()->SetMessageAreaCacheNumber(0);
       GetSession()->GetMessageAreaCacheNumber() < GetSession()->num_subs && !abort;
       GetSession()->SetMessageAreaCacheNumber(GetSession()->GetMessageAreaCacheNumber() + 1)) {
    if (!GetSession()->m_SubDateCache[GetSession()->GetMessageAreaCacheNumber()]) {
      iscan1(GetSession()->GetMessageAreaCacheNumber(), true);
    }
    GetSession()->bout << "\x03" << i1 << ".";
    if ((GetSession()->GetMessageAreaCacheNumber() % 5) == 4) {
      i1++;
      if (i1 == 4) {
        i1++;
      }
      if (i1 == 10) {
        i1 = 3;
      }
      GetSession()->bout << "\b\b\b\b\b";
    }
    checka(&abort);
  }
  if (!abort) {
    GetSession()->bout << "|#1...done!\r\n";
  }
  GetSession()->bout.NewLine();
  g_preloaded = true;
}


char *get_wildlist(char *pszFileMask) {
  int mark = 0;
  char *pszPath, t;
  WFindFile fnd;

  WWIV_ASSERT(pszFileMask);

  if (!fnd.open(pszFileMask, 0)) {
    GetSession()->bout << "No files found\r\n";
    pszFileMask[0] = '\0';
    return pszFileMask;
  } else {
    GetSession()->bout.WriteFormatted("%12.12s ", fnd.GetFileName());
  }

  if (strchr(pszFileMask, WFile::pathSeparatorChar) == NULL) {
    pszFileMask[0] = '\0';
  } else {
    for (int i = 0; i < wwiv::strings::GetStringLength(pszFileMask); i++) {
      if (pszFileMask[i] == WFile::pathSeparatorChar) {
        mark = i + 1;
      }
    }
  }
  t = pszFileMask[mark];
  pszFileMask[mark] = 0;
  pszPath = pszFileMask;
  pszFileMask[mark] = t;
  t = static_cast<char>(wwiv::strings::GetStringLength(pszPath));
  strcat(pszPath, fnd.GetFileName());
  int i = 1;
  for (i = 1;; i++) {
    if (i % 5 == 0) {
      GetSession()->bout.NewLine();
    }
    if (!fnd.next()) {
      break;
    }
    GetSession()->bout.WriteFormatted("%12.12s ", fnd.GetFileName());
    if (bgetch() == SPACE) {
      GetSession()->bout.NewLine();
      break;
    }
  }
  GetSession()->bout.NewLine();
  if (i == 1) {
    GetSession()->bout << "One file found: " << fnd.GetFileName() << wwiv::endl;
    GetSession()->bout << "Use this file? ";
    if (yesno()) {
      return pszPath;
    } else {
      pszPath[0] = '\0';
      return pszPath;
    }
  }
  pszPath[t] = '\0';
  GetSession()->bout << "Filename: ";
  input(pszFileMask, 12, true);
  strcat(pszPath, pszFileMask);
  return pszPath;
}


int side_menu(int *menu_pos, bool bNeedsRedraw, char *menu_items[], int xpos, int ypos, struct side_menu_colors * smc) {
  static int positions[20], amount = 1;

  WWIV_ASSERT(menu_pos);
  WWIV_ASSERT(menu_items);
  WWIV_ASSERT(smc);

  GetSession()->localIO()->tleft(true);

  if (bNeedsRedraw) {
    amount = 1;
    positions[0] = xpos;
    while (menu_items[amount] && menu_items[amount][0] && !hangup) {
      positions[amount] = positions[amount - 1] + strlen(menu_items[amount - 1]) + 2;
      ++amount;
    }

    int x = 0;
    GetSession()->bout.SystemColor(smc->normal_menu_item);

    while (menu_items[x] && menu_items[x][0] && !hangup) {
      GetSession()->bout.GotoXY(positions[x], ypos);

      if (*menu_pos == x) {
        GetSession()->bout.SystemColor(smc->current_highlight);
        bputch(menu_items[x][0]);
        GetSession()->bout.SystemColor(smc->current_menu_item);
        GetSession()->bout.WriteFormatted(menu_items[x] + 1);
      } else {
        GetSession()->bout.SystemColor(smc->normal_highlight);
        bputch(menu_items[x][0]);
        GetSession()->bout.SystemColor(smc->normal_menu_item);
        GetSession()->bout.WriteFormatted(menu_items[x] + 1);
      }
      ++x;
    }
  }
  GetSession()->bout.SystemColor(smc->normal_menu_item);

  while (!hangup) {
    int event = get_kb_event(NOTNUMBERS);
    if (event < 128) {
      int x = 0;
      while (menu_items[x] && menu_items[x][0] && !hangup) {
        if (event == wwiv::UpperCase<int>(menu_items[x][0]) || event == wwiv::LowerCase<int>(menu_items[x][0])) {
          GetSession()->bout.GotoXY(positions[*menu_pos], ypos);
          GetSession()->bout.SystemColor(smc->normal_highlight);
          bputch(menu_items[*menu_pos][0]);
          GetSession()->bout.SystemColor(smc->normal_menu_item);
          GetSession()->bout.WriteFormatted(menu_items[*menu_pos] + 1);
          *menu_pos = x;
          GetSession()->bout.SystemColor(smc->current_highlight);
          GetSession()->bout.GotoXY(positions[*menu_pos], ypos);
          bputch(menu_items[*menu_pos][0]);
          GetSession()->bout.SystemColor(smc->current_menu_item);
          GetSession()->bout.WriteFormatted(menu_items[*menu_pos] + 1);
          if (modem_speed > 2400 || !GetSession()->using_modem) {
            GetSession()->bout.GotoXY(positions[*menu_pos], ypos);
          }
          return EXECUTE;
        }
        ++x;
      }
      return event;
    } else {
      switch (event) {
      case COMMAND_LEFT:
        GetSession()->bout.GotoXY(positions[*menu_pos], ypos);
        GetSession()->bout.SystemColor(smc->normal_highlight);
        bputch(menu_items[*menu_pos][0]);
        GetSession()->bout.SystemColor(smc->normal_menu_item);
        GetSession()->bout.WriteFormatted(menu_items[*menu_pos] + 1);
        if (!*menu_pos) {
          *menu_pos = amount - 1;
        } else {
          --* menu_pos;
        }
        GetSession()->bout.SystemColor(smc->current_highlight);
        GetSession()->bout.GotoXY(positions[*menu_pos], ypos);
        bputch(menu_items[*menu_pos][0]);
        GetSession()->bout.SystemColor(smc->current_menu_item);
        GetSession()->bout.WriteFormatted(menu_items[*menu_pos] + 1);
        if (modem_speed > 2400 || !GetSession()->using_modem) {
          GetSession()->bout.GotoXY(positions[*menu_pos], ypos);
        }
        break;

      case COMMAND_RIGHT:
        GetSession()->bout.GotoXY(positions[*menu_pos], ypos);
        GetSession()->bout.SystemColor(smc->normal_highlight);
        bputch(menu_items[*menu_pos][0]);
        GetSession()->bout.SystemColor(smc->normal_menu_item);
        GetSession()->bout.WriteFormatted(menu_items[*menu_pos] + 1);
        if (*menu_pos == amount - 1) {
          *menu_pos = 0;
        } else {
          ++* menu_pos;
        }
        GetSession()->bout.SystemColor(smc->current_highlight);
        GetSession()->bout.GotoXY(positions[*menu_pos], ypos);
        bputch(menu_items[*menu_pos][0]);
        GetSession()->bout.SystemColor(smc->current_menu_item);
        GetSession()->bout.WriteFormatted(menu_items[*menu_pos] + 1);
        if (modem_speed > 2400 || !GetSession()->using_modem) {
          GetSession()->bout.GotoXY(positions[*menu_pos], ypos);
        }
        break;

      default:
        return event;
      }
    }
  }
  return 0;
}


slrec getslrec(int nSl) {
  static int nCurSl = -1;
  static slrec CurSlRec;

  if (nSl == nCurSl) {
    return CurSlRec;
  }

  WFile file(GetApplication()->GetHomeDir(), CONFIG_DAT);
  if (!file.Open(WFile::modeBinary | WFile::modeReadOnly)) {
    // Bad ju ju here.
    GetApplication()->AbortBBS();
  }
  configrec c;
  file.Read(&c, sizeof(configrec));

  nCurSl = nSl;
  CurSlRec = c.sl[nSl];

  return CurSlRec;
}


void WWIV_SetFileTime(const char* pszFileName, const time_t tTime) {
  struct utimbuf utbuf;

  utbuf.actime  = tTime;
  utbuf.modtime = tTime;

  WWIV_ASSERT(pszFileName);

  utime(pszFileName, &utbuf);
}


bool okfsed() {
  return (!okansi() ||
          !GetSession()->GetCurrentUser()->GetDefaultEditor() ||
          (GetSession()->GetCurrentUser()->GetDefaultEditor() > GetSession()->GetNumberOfEditors()))
         ? false : true;
}


//************************************************
// Purpose      : Properizes msg.daten to a date/time string
// Parameters   : time_t daten    - daten attribute of a message
//                char* mode    - mode string
//                              - W = Day of the week
//                              - D = Date
//                              - T = Time
//                              - Z = Zone
//                              - Y = Year
//                              - Modes can be combined i.e
//                                W, D, T, Z, WDT, TZ, DTZ
//                char* delim   - delimiter that appears before time
// Returns      : Properized date/time string as requested
// Author(s)    : WSS
//************************************************
std::string W_DateString(time_t tDateTime, const std::string& origMode , const std::string& timeDelim) {
  char    s[40];                  // formattable string
  std::string str;
  struct tm * pTm = localtime(&tDateTime);

  std::string mode = origMode;
  std::transform(mode.begin(), mode.end(), mode.begin(), ::toupper);

  bool first = true;
  // cycle thru mode string
  for (auto& ch : mode) {
    if (!first) {
      str += " ";
    }
    first = false;
    switch (ch) {
    case 'W':
      strftime(s, 40, "%A,", pTm);
      break;
    case 'D':
      strftime(s, 40, "%B %d, %Y", pTm);
      break;
    case 'T':
      if (timeDelim.length() > 0) {
        // if there is a delimiter, add it with spaces
        str += timeDelim;
        str += " ";
      }

      // which form of the clock is in use?
      if (GetSession()->GetCurrentUser()->IsUse24HourClock()) {
        strftime(s, 40, "%H:%M", pTm);
      } else {
        strftime(s, 40, "%I:%M %p", pTm);
      }
      break;
    case 'Z':
      strftime(s, 40, "[%Z]", pTm);
      break;
    case 'Y':
      strftime(s, 40, "%Y", pTm);
      break;
    } //end switch(szMode2[i])
    // add the component
    str += s;
  } //end for(i = 0;....)
  return std::string(str.c_str());
} //end W_DateString

template<class _Ty> inline const _Ty& in_range(const _Ty& minValue, const _Ty& maxValue, const _Ty& value) {
  return std::max(std::min(maxValue, value), minValue);
}
