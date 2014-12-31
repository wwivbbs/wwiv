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
#include <chrono>
#include <cmath>
#ifdef _WIN32
#include <sys/utime.h>

#else
#include <utime.h>
#endif  // WIN32

#include <string>
#include <vector>

#include "bbs/datetime.h"
#include "bbs/input.h"
#include "bbs/common.h"
#include "bbs/keycodes.h"
#include "bbs/wconstants.h"
#include "bbs/wwiv.h"
#include "core/os.h"
#include "core/strings.h"
#include "core/wfndfile.h"
#include "core/wwivassert.h"
#include "sdk/config.h"

using std::chrono::milliseconds;
using std::string;
using std::vector;
using namespace wwiv::os;
using namespace wwiv::strings;

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
  WWIV_ASSERT(pszFileName);
  WWIV_ASSERT(pszDirectoryName);

  const stirng filespec = StrCat(pszDirectoryName, stripfn(pszFileName));
  WFindFile fnd;
  bool bFound = fnd.open(filespec, 0);
  bout.nl();
  while (bFound) {
    const string filename = fnd.GetFileName();
    // We don't want to delete ".", "..".
    if (filename != "." && filename != "..") {
      if (bPrintStatus) {
        std::clog << "Deleting TEMP file: " << pszDirectoryName << szFileName << std::endl;
      }
      File::Remove(pszDirectoryName, szFileName);
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
  return session()->user()->HasAnsi() && !x_only;
}

/**
 * Should be called after a user is logged off, and will initialize
 * screen-access variables.
 */
void frequent_init() {
  setiia(90);
  g_flags = 0;
  session()->tagging = 0;
  newline = true;
  session()->SetCurrentReadMessageArea(-1);
  session()->SetCurrentConferenceMessageArea(0);
  session()->SetCurrentConferenceFileArea(0);
  ansiptr = 0;
  curatr = 0x07;
  outcom = false;
  incom = false;
  charbufferpointer = 0;
  session()->localIO()->SetTopLine(0);
  session()->screenlinest = defscreenbottom + 1;
  endofline[0] = '\0';
  hangup = false;
  hungup = false;
  chatcall = false;
  session()->localIO()->ClearChatReason();
  session()->SetUserOnline(false);
  change_color = 0;
  chatting = 0;
  local_echo = true;
  irt[0] = '\0';
  irt_name[0] = '\0';
  lines_listed = 0;
  session()->ReadCurrentUser(1);
  read_qscn(1, qsc, false);
  fwaiting = (session()->user()->IsUserDeleted()) ? 0 : session()->user()->GetNumMailWaiting();
  okmacro = true;
  okskey = true;
  mailcheck = false;
  smwcheck = false;
  use_workspace = false;
  extratimecall = 0.0;
  session()->using_modem = 0;
  session()->localIO()->set_global_handle(false);
  File::SetFilePermissions(g_szDSZLogFileName, File::permReadWrite);
  File::Remove(g_szDSZLogFileName);
  session()->SetTimeOnlineLimited(false);
  session()->localIO()->set_x_only(false, nullptr, false);
  set_net_num(0);
  set_language(session()->user()->GetLanguage());
  reset_disable_conf();
}


/**
 * Gets the current users upload/download ratio.
 */
double ratio() {
  if (session()->user()->GetDownloadK() == 0) {
    return 99.999;
  }
  double r = static_cast<float>(session()->user()->GetUploadK()) /
             static_cast<float>(session()->user()->GetDownloadK());

  return (r > 99.998) ? 99.998 : r;
}

/**
 * Gets the current users post/call ratio.
 */
double post_ratio() {
  if (session()->user()->GetNumLogons() == 0) {
    return 99.999;
  }
  double r = static_cast<float>(session()->user()->GetNumMessagesPosted()) /
             static_cast<float>(session()->user()->GetNumLogons());
  return (r > 99.998) ? 99.998 : r;
}

double nsl() {
  double rtn = 1.00;

  double dd = timer();
  if (session()->IsUserOnline()) {
    if (timeon > (dd + SECONDS_PER_MINUTE_FLOAT)) {
      timeon -= SECONDS_PER_HOUR_FLOAT * HOURS_PER_DAY_FLOAT;
    }
    double tot = dd - timeon;
    double tpl = static_cast<double>(getslrec(session()->GetEffectiveSl()).time_per_logon) * MINUTES_PER_HOUR_FLOAT;
    double tpd = static_cast<double>(getslrec(session()->GetEffectiveSl()).time_per_day) * MINUTES_PER_HOUR_FLOAT;
    double tlc = tpl - tot + session()->user()->GetExtraTime() + extratimecall;
    double tlt = tpd - tot - static_cast<double>(session()->user()->GetTimeOnToday()) +
                 session()->user()->GetExtraTime();

    tlt = std::min<double>(tlc, tlt);
    rtn = in_range<double>(0.0, 32767.0, tlt);
  }

  session()->SetTimeOnlineLimited(false);
  if (syscfg.executetime) {
    double tlt = time_event - dd;
    if (tlt < 0.0) {
      tlt += HOURS_PER_DAY_FLOAT * SECONDS_PER_HOUR_FLOAT;
    }
    if (rtn > tlt) {
      rtn = tlt;
      session()->SetTimeOnlineLimited(true);
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
  return File::GetFreeSpaceForPath(pszPathName);
}

void send_net(net_header_rec * nh, unsigned short int *list, const char *text, const char *byname) {
  WWIV_ASSERT(nh);

  const string filename = StringPrintf("%sp1%s",
    session()->GetNetworkDataDirectory().c_str(),
    application()->GetNetworkExtension().c_str());
  File file(filename);
  if (!file.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
    return;
  }
  file.Seek(0L, File::seekEnd);
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
  sleep_for(milliseconds(100));
  yield();

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

  bout.nl();
  bout << "|#1Caching message areas";
  int i1 = 3;
  for (session()->SetMessageAreaCacheNumber(0);
       session()->GetMessageAreaCacheNumber() < session()->num_subs && !abort;
       session()->SetMessageAreaCacheNumber(session()->GetMessageAreaCacheNumber() + 1)) {
    if (!session()->m_SubDateCache[session()->GetMessageAreaCacheNumber()]) {
      iscan1(session()->GetMessageAreaCacheNumber(), true);
    }
    bout << "\x03" << i1 << ".";
    if ((session()->GetMessageAreaCacheNumber() % 5) == 4) {
      i1++;
      if (i1 == 4) {
        i1++;
      }
      if (i1 == 10) {
        i1 = 3;
      }
      bout << "\b\b\b\b\b";
    }
    checka(&abort);
  }
  if (!abort) {
    bout << "|#1...done!\r\n";
  }
  bout.nl();
  g_preloaded = true;
}


char *get_wildlist(char *pszFileMask) {
  int mark = 0;
  char *pszPath, t;
  WFindFile fnd;

  WWIV_ASSERT(pszFileMask);

  if (!fnd.open(pszFileMask, 0)) {
    bout << "No files found\r\n";
    pszFileMask[0] = '\0';
    return pszFileMask;
  } else {
    bout.bprintf("%12.12s ", fnd.GetFileName());
  }

  if (strchr(pszFileMask, File::pathSeparatorChar) == nullptr) {
    pszFileMask[0] = '\0';
  } else {
    for (int i = 0; i < wwiv::strings::GetStringLength(pszFileMask); i++) {
      if (pszFileMask[i] == File::pathSeparatorChar) {
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
  for (int i = 1;; i++) {
    if (i % 5 == 0) {
      bout.nl();
    }
    if (!fnd.next()) {
      break;
    }
    bout.bprintf("%12.12s ", fnd.GetFileName());
    if (bgetch() == SPACE) {
      bout.nl();
      break;
    }
  }
  bout.nl();
  if (i == 1) {
    bout << "One file found: " << fnd.GetFileName() << wwiv::endl;
    bout << "Use this file? ";
    if (yesno()) {
      return pszPath;
    } else {
      pszPath[0] = '\0';
      return pszPath;
    }
  }
  pszPath[t] = '\0';
  bout << "Filename: ";
  input(pszFileMask, 12, true);
  strcat(pszPath, pszFileMask);
  return pszPath;
}

int side_menu(int *menu_pos, bool bNeedsRedraw, const vector<string>& menu_items, int xpos, int ypos, side_menu_colors * smc) {
  static int positions[20], amount = 1;

  WWIV_ASSERT(menu_pos);
  WWIV_ASSERT(smc);

  session()->localIO()->tleft(true);

  if (bNeedsRedraw) {
    amount = 1;
    positions[0] = xpos;
    for (const string& menu_item : menu_items) {
      positions[amount] = positions[amount-1] + menu_item.length() + 2;
      ++amount;
    }

    int x = 0;
    bout.SystemColor(smc->normal_menu_item);

    for (const string& menu_item : menu_items) {
      if (hangup) {
        break;
      }
      bout.GotoXY(positions[x], ypos);

      if (*menu_pos == x) {
        bout.SystemColor(smc->current_highlight);
        bputch(menu_item[0]);
        bout.SystemColor(smc->current_menu_item);
        bout.bputs(menu_item.substr(1));
      } else {
        bout.SystemColor(smc->normal_highlight);
        bputch(menu_item[0]);
        bout.SystemColor(smc->normal_menu_item);
        bout.bputs(menu_item.substr(1));
      }
      ++x;
    }
  }
  bout.SystemColor(smc->normal_menu_item);

  while (!hangup) {
    int event = get_kb_event(NOTNUMBERS);
    if (event < 128) {
      int x = 0;
      for (const string& menu_item : menu_items) {
        if (event == wwiv::UpperCase<int>(menu_item[0]) || event == wwiv::LowerCase<int>(menu_item[0])) {
          bout.GotoXY(positions[*menu_pos], ypos);
          bout.SystemColor(smc->normal_highlight);
          bputch(menu_items[*menu_pos][0]);
          bout.SystemColor(smc->normal_menu_item);
          bout.bputs(menu_items[*menu_pos].substr(1));
          *menu_pos = x;
          bout.SystemColor(smc->current_highlight);
          bout.GotoXY(positions[*menu_pos], ypos);
          bputch(menu_items[*menu_pos][0]);
          bout.SystemColor(smc->current_menu_item);
          bout.bputs(menu_items[*menu_pos].substr(1));
          bout.GotoXY(positions[*menu_pos], ypos);
          return EXECUTE;
        }
        ++x;
      }
      return event;
    } else {
      switch (event) {
      case COMMAND_LEFT:
        bout.GotoXY(positions[*menu_pos], ypos);
        bout.SystemColor(smc->normal_highlight);
        bputch(menu_items[*menu_pos][0]);
        bout.SystemColor(smc->normal_menu_item);
        bout.bputs(menu_items[*menu_pos].substr(1));
        if (!*menu_pos) {
          *menu_pos = menu_items.size() - 1;
        } else {
          --* menu_pos;
        }
        bout.SystemColor(smc->current_highlight);
        bout.GotoXY(positions[*menu_pos], ypos);
        bputch(menu_items[*menu_pos][0]);
        bout.SystemColor(smc->current_menu_item);
        bout.bputs(menu_items[*menu_pos].substr(1));
        bout.GotoXY(positions[*menu_pos], ypos);
        break;

      case COMMAND_RIGHT:
        bout.GotoXY(positions[*menu_pos], ypos);
        bout.SystemColor(smc->normal_highlight);
        bputch(menu_items[*menu_pos][0]);
        bout.SystemColor(smc->normal_menu_item);
        bout.bputs(menu_items[*menu_pos].substr(1));
        if (*menu_pos == static_cast<int>(menu_items.size() - 1)) {
          *menu_pos = 0;
        } else {
          ++* menu_pos;
        }
        bout.SystemColor(smc->current_highlight);
        bout.GotoXY(positions[*menu_pos], ypos);
        bputch(menu_items[*menu_pos][0]);
        bout.SystemColor(smc->current_menu_item);
        bout.bputs(menu_items[*menu_pos].substr(1));
        bout.GotoXY(positions[*menu_pos], ypos);
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

  wwiv::sdk::Config config;
  if (!config.IsInitialized()) {
    // Bad ju ju here.
    application()->AbortBBS();
  }
  nCurSl = nSl;
  CurSlRec = config.config()->sl[nSl];
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
          !session()->user()->GetDefaultEditor() ||
          (session()->user()->GetDefaultEditor() > session()->GetNumberOfEditors()))
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
  StringUpperCase(&mode);

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
      if (session()->user()->IsUse24HourClock()) {
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
