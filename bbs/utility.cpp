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

#include "bbs/bbsovl3.h"
#include "bbs/bbs.h"
#include "bbs/bgetch.h"
#include "bbs/com.h"
#include "bbs/connect1.h"
#include "bbs/events.h"
#include "bbs/fcns.h"
#include "bbs/instmsg.h"
#include "bbs/datetime.h"
#include "bbs/input.h"
#include "bbs/instmsg.h"
#include "bbs/common.h"
#include "bbs/keycodes.h"
#include "bbs/wconstants.h"
#include "bbs/workspace.h"
#include "bbs/vars.h"
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
 * @param file_name       Wildcard file specification to delete
 * @param pszDirectoryName  Name of the directory to delete files from
 * @param bPrintStatus      Print out locally as files are deleted
 */
void remove_from_temp(const std::string& file_name, const std::string& directory_name, bool bPrintStatus) {

  const string filespec = StrCat(directory_name, stripfn(file_name.c_str()));
  WFindFile fnd;
  bool bFound = fnd.open(filespec, 0);
  bout.nl();
  while (bFound) {
    const string filename = fnd.GetFileName();
    // We don't want to delete ".", "..".
    if (filename != "." && filename != "..") {
      if (bPrintStatus) {
        std::clog << "Deleting TEMP file: " << directory_name << filename << std::endl;
      }
      File::Remove(directory_name, filename);
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
  return session()->user()->HasAnsi();
}

/**
 * Should be called after a user is logged off, and will initialize
 * screen-access variables.
 */
void frequent_init() {
  setiia(90);
  g_flags = 0;
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
  bout.clear_endofline();
  hangup = false;
  chatcall = false;
  session()->SetChatReason("");
  session()->SetUserOnline(false);
  change_color = 0;
  chatting = 0;
  local_echo = true;
  irt[0] = '\0';
  irt_name[0] = '\0';
  lines_listed = 0;
  session()->ReadCurrentUser(1);
  read_qscn(1, qsc, false);
  okmacro = true;
  okskey = true;
  smwcheck = false;
  use_workspace = false;
  extratimecall = 0;
  session()->using_modem = 0;
  File::SetFilePermissions(session()->dsz_logfile_name_, File::permReadWrite);
  File::Remove(session()->dsz_logfile_name_);
  session()->SetTimeOnlineLimited(false);
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

long nsl() {
  long rtn = 1;

  auto dd = timer();
  if (session()->IsUserOnline()) {
    if (timeon > (dd + SECONDS_PER_MINUTE)) {
      timeon -= SECONDS_PER_DAY;
    }
    auto tot = dd - timeon;
    auto tpl = static_cast<long>(getslrec(session()->GetEffectiveSl()).time_per_logon) * MINUTES_PER_HOUR;
    auto tpd = static_cast<long>(getslrec(session()->GetEffectiveSl()).time_per_day) * MINUTES_PER_HOUR;
    auto tlc = tpl - tot + std::lround(session()->user()->GetExtraTime()) + extratimecall;
    auto tlt = tpd - tot - std::lround(
        session()->user()->GetTimeOnToday()) + std::lround(session()->user()->GetExtraTime());

    tlt = std::min<long>(tlc, tlt);
    rtn = in_range<long>(0, 32767, tlt);
  }

  session()->SetTimeOnlineLimited(false);
  if (syscfg.executetime) {
    auto tlt = time_event - dd;
    if (tlt < 0) {
      tlt += SECONDS_PER_DAY;
    }
    if (rtn > tlt) {
      rtn = tlt;
      session()->SetTimeOnlineLimited(true);
    }
    check_event();
    if (do_event) {
      rtn = 0;
    }
  }
  return in_range<long>(0, 32767, rtn);
}

void send_net(net_header_rec* nh, std::vector<uint16_t> list, const std::string& text, const std::string& byname) {
  WWIV_ASSERT(nh);

  const string filename = StrCat(
    session()->network_directory().c_str(),
    "p1",
    session()->network_extension().c_str());
  File file(filename);
  if (!file.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
    return;
  }
  file.Seek(0L, File::seekEnd);
  if (list.empty()) {
    nh->list_len = 0;
  }
  if (text.empty()) {
    nh->length = 0;
  }
  if (nh->list_len) {
    nh->tosys = 0;
  }
  if (!byname.empty()) {
    nh->length += byname.size() + 1;
  }
  file.Write(nh, sizeof(net_header_rec));
  if (nh->list_len) {
    file.Write(&list[0], sizeof(uint16_t) * (nh->list_len));
  }
  if (!byname.empty()) {
    file.Write(byname);
    char nul[1] = {0};
    file.Write(nul, 1);
  }
  if (nh->length) {
    file.Write(text);
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

char *stripfn(const char *file_name) {
  static char szStaticFileName[15];
  char szTempFileName[MAX_PATH];

  WWIV_ASSERT(file_name);

  int nSepIndex = -1;
  for (int i = 0; i < wwiv::strings::GetStringLength(file_name); i++) {
    if (file_name[i] == '\\' || file_name[i] == ':' || file_name[i] == '/') {
      nSepIndex = i;
    }
  }
  if (nSepIndex != -1) {
    strcpy(szTempFileName, &(file_name[nSepIndex + 1]));
  } else {
    strcpy(szTempFileName, file_name);
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

void stripfn_inplace(char *file_name) {
  strcpy(file_name, stripfn(file_name));
}

char *get_wildlist(char *file_mask) {
  int mark = 0;
  char *pszPath, t;
  WFindFile fnd;

  WWIV_ASSERT(file_mask);

  if (!fnd.open(file_mask, 0)) {
    bout << "No files found\r\n";
    file_mask[0] = '\0';
    return file_mask;
  } else {
    bout.bprintf("%12.12s ", fnd.GetFileName());
  }

  if (strchr(file_mask, File::pathSeparatorChar) == nullptr) {
    file_mask[0] = '\0';
  } else {
    for (int i = 0; i < wwiv::strings::GetStringLength(file_mask); i++) {
      if (file_mask[i] == File::pathSeparatorChar) {
        mark = i + 1;
      }
    }
  }
  t = file_mask[mark];
  file_mask[mark] = 0;
  pszPath = file_mask;
  file_mask[mark] = t;
  t = static_cast<char>(wwiv::strings::GetStringLength(pszPath));
  strcat(pszPath, fnd.GetFileName());
  int i = 1;
  for (i = 1;; i++) {
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
  input(file_mask, 12, true);
  strcat(pszPath, file_mask);
  return pszPath;
}

int side_menu(int *menu_pos, bool bNeedsRedraw, const vector<string>& menu_items, int xpos, int ypos, side_menu_colors * smc) {
  static int positions[20], amount = 1;

  WWIV_ASSERT(menu_pos);
  WWIV_ASSERT(smc);

  session()->tleft(true);

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
        bout.bputch(menu_item[0]);
        bout.SystemColor(smc->current_menu_item);
        bout.bputs(menu_item.substr(1));
      } else {
        bout.SystemColor(smc->normal_highlight);
        bout.bputch(menu_item[0]);
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
          bout.bputch(menu_items[*menu_pos][0]);
          bout.SystemColor(smc->normal_menu_item);
          bout.bputs(menu_items[*menu_pos].substr(1));
          *menu_pos = x;
          bout.SystemColor(smc->current_highlight);
          bout.GotoXY(positions[*menu_pos], ypos);
          bout.bputch(menu_items[*menu_pos][0]);
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
        bout.bputch(menu_items[*menu_pos][0]);
        bout.SystemColor(smc->normal_menu_item);
        bout.bputs(menu_items[*menu_pos].substr(1));
        if (!*menu_pos) {
          *menu_pos = menu_items.size() - 1;
        } else {
          --* menu_pos;
        }
        bout.SystemColor(smc->current_highlight);
        bout.GotoXY(positions[*menu_pos], ypos);
        bout.bputch(menu_items[*menu_pos][0]);
        bout.SystemColor(smc->current_menu_item);
        bout.bputs(menu_items[*menu_pos].substr(1));
        bout.GotoXY(positions[*menu_pos], ypos);
        break;

      case COMMAND_RIGHT:
        bout.GotoXY(positions[*menu_pos], ypos);
        bout.SystemColor(smc->normal_highlight);
        bout.bputch(menu_items[*menu_pos][0]);
        bout.SystemColor(smc->normal_menu_item);
        bout.bputs(menu_items[*menu_pos].substr(1));
        if (*menu_pos == static_cast<int>(menu_items.size() - 1)) {
          *menu_pos = 0;
        } else {
          ++* menu_pos;
        }
        bout.SystemColor(smc->current_highlight);
        bout.GotoXY(positions[*menu_pos], ypos);
        bout.bputch(menu_items[*menu_pos][0]);
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
    session()->AbortBBS();
  }
  nCurSl = nSl;
  CurSlRec = config.config()->sl[nSl];
  return CurSlRec;
}

bool okfsed() {
  return okansi()
         && session()->user()->GetDefaultEditor() > 0 
         && session()->user()->GetDefaultEditor() <= session()->editors.size();
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
