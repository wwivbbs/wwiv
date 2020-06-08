/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2020, WWIV Software Services             */
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

#include "bbs/utility.h"
#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/bgetch.h"
#include "bbs/com.h"
#include "bbs/common.h"
#include "bbs/connect1.h"
#include "bbs/input.h"
#include "bbs/instmsg.h"
#include "bbs/workspace.h"
#include "bbs/wqscn.h"
#include "core/findfiles.h"
#include "core/os.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "local_io/keycodes.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>
#include <vector>
#ifdef _WIN32
#include <sys/utime.h>
#else
#include <utime.h>
#endif // WIN32

using std::string;
using std::vector;
using namespace std::chrono;
using namespace wwiv::core;
using namespace wwiv::os;
using namespace wwiv::strings;

extern const unsigned char* translate_letters[];

template <class _Ty>
inline const _Ty& in_range(const _Ty& minValue, const _Ty& maxValue, const _Ty& value);

/**
 * Deletes files from a directory.  This is meant to be used only in the temp
 * directories of WWIV.
 *
 * @param file_name       Wildcard file specification to delete
 * @param directory_name  Name of the directory to delete files from
 * @param bPrintStatus    Print out locally as files are deleted
 */
void remove_from_temp(const std::string& file_name, const std::string& directory_name,
                      bool bPrintStatus) {
  const auto filespec = StrCat(directory_name, stripfn(file_name));
  FindFiles ff(filespec, FindFilesType::any);
  bout.nl();
  for (const auto& f : ff) {
    // We don't want to delete ".", "..".
    const auto fullpath = FilePath(directory_name, f.name);
    LOG_IF(bPrintStatus, INFO) << "Deleting TEMP file: '" << fullpath << "'";
    File::Remove(fullpath);
  }
}

/**
 * Does the currently online user have ANSI.  The user record is
 * checked for this information
 *
 * @return true if the user wants ANSI, false otherwise.
 */
bool okansi() { return a()->user()->HasAnsi(); }

/**
 * Should be called after a user is logged off, and will initialize
 * screen-access variables.
 */
void frequent_init() {
  setiia(seconds(5));

  // Context Globals to move to Application
  // Context Globals in Application
  a()->context().reset();
  use_workspace = false;

  set_net_num(0);
  read_qscn(1, a()->context().qsc, false);
  set_language(a()->user()->GetLanguage());
  reset_disable_conf();

  // Output context
  bout.reset();
  bout.okskey(true);

  // DSZ Log
  File::SetFilePermissions(a()->dsz_logfile_name_, File::permReadWrite);
  File::Remove(a()->dsz_logfile_name_);
}

/**
 * Gets the current users upload/download ratio.
 */
double ratio() {
  if (a()->user()->dk() == 0) {
    return 99.999;
  }
  double r = static_cast<float>(a()->user()->uk()) /
             static_cast<float>(a()->user()->dk());

  return (r > 99.998) ? 99.998 : r;
}

/**
 * Gets the current users post/call ratio.
 */
double post_ratio() {
  if (a()->user()->GetNumLogons() == 0) {
    return 99.999;
  }
  double r = static_cast<float>(a()->user()->GetNumMessagesPosted()) /
             static_cast<float>(a()->user()->GetNumLogons());
  return (r > 99.998) ? 99.998 : r;
}

long nsl() {
  const auto dd = system_clock::now();
  if (!a()->IsUserOnline()) {
    return 1;
  }
  auto tot = dd - a()->system_logon_time();

  const auto tpl = minutes(a()->effective_slrec().time_per_logon);
  const auto tpd = minutes(a()->effective_slrec().time_per_day);
  const auto extra_time =
      duration_cast<seconds>(a()->user()->extra_time() + a()->extratimecall());
  const auto tlc = tpl - tot + extra_time;
  const auto tlt = std::min(
      tlc, tpd - tot -
               seconds(std::lround(a()->user()->GetTimeOnToday() + a()->user()->GetExtraTime())));
  a()->SetTimeOnlineLimited(false);
  return static_cast<long>(in_range<int64_t>(0, 32767, duration_cast<seconds>(tlt).count()));
}

void send_net(net_header_rec* nh, std::vector<uint16_t> list, const std::string& text,
              const std::string& byname) {
  const string filename = StrCat(a()->network_directory(), "p1", a()->network_extension());
  File file(filename);
  if (!file.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
    return;
  }
  file.Seek(0L, File::Whence::end);
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

  if (!a()->in_chatroom_ || !a()->chatline_) {
    if (inst_msg_waiting()) {
      process_inst_msgs();
    }
  }
}

std::string stripfn(const std::string& file_name) {
  std::filesystem::path p(file_name);
  if (!p.has_filename()) {
    return {};
  }
  auto orig = wwiv::sdk::files::unalign(p.filename().string());
  // Since WWIV has a heavy DOS legacy, sometimes we have files in
  // DOS slash format.  On Windows std::filesystem handles both since
  // Windows uses either format internally.
#if !defined (_WIN32)
  const auto idx = orig.rfind("\\");
  if (idx != std::string::npos) {
    orig = orig.substr(idx + 1);
  }
#endif  // !defined(_WIN32)
  return orig;
}

std::string get_wildlist(const std::string& orig_file_mask) {
  char file_mask[1024];
  to_char_array(file_mask, orig_file_mask);
  auto mark = 0;

  FindFiles ff(file_mask, FindFilesType::any);
  if (ff.empty()) {
    bout << "No files found\r\n";
    return {};
  }
  auto f = ff.begin();
  bout << fmt::sprintf("%12.12s ", f->name);

  if (strchr(file_mask, File::pathSeparatorChar) == nullptr) {
    file_mask[0] = '\0';
  } else {
    for (int i = 0; i < ssize(file_mask); i++) {
      if (file_mask[i] == File::pathSeparatorChar) {
        mark = i + 1;
      }
    }
  }
  auto t = file_mask[mark];
  file_mask[mark] = 0;
  auto* pszPath = file_mask;
  file_mask[mark] = t;
  t = static_cast<char>(size(pszPath));
  strcat(pszPath, f->name.c_str());
  int i;
  for (i = 1;; i++) {
    if (i % 5 == 0) {
      bout.nl();
    }
    if (f == ff.end()) {
      break;
    }
    ++f;
    bout << fmt::sprintf("%12.12s ", f->name);
    if (bout.getkey() == SPACE) {
      bout.nl();
      break;
    }
  }
  bout.nl();
  if (i == 1) {
    bout << "One file found: " << f->name << wwiv::endl;
    bout << "Use this file? ";
    if (yesno()) {
      return pszPath;
    }
    pszPath[0] = '\0';
    return pszPath;
  }
  pszPath[t] = '\0';
  bout << "Filename: ";
  input(file_mask, 12, true);
  strcat(pszPath, file_mask);
  return pszPath;
}

int side_menu(int* menu_pos, bool bNeedsRedraw, const vector<string>& menu_items, int xpos,
              int ypos, side_menu_colors* smc) {
  static int positions[20], amount = 1;

  a()->tleft(true);

  if (bNeedsRedraw) {
    amount = 1;
    positions[0] = xpos;
    for (const auto& menu_item : menu_items) {
      positions[amount] = positions[amount - 1] + menu_item.length() + 2;
      ++amount;
    }

    int x = 0;
    bout.SystemColor(smc->normal_menu_item);

    for (const string& menu_item : menu_items) {
      if (a()->hangup_) {
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

  while (!a()->hangup_) {
    int event = bgetch_event(numlock_status_t::NOTNUMBERS);
    if (event < 128) {
      int x = 0;
      for (const string& menu_item : menu_items) {
        if (event == to_upper_case<int>(menu_item[0]) ||
            event == to_lower_case<int>(menu_item[0])) {
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
          *menu_pos = wwiv::stl::ssize(menu_items) - 1;
        } else {
          --*menu_pos;
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
          ++*menu_pos;
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

bool okfsed() {
  return okansi() && a()->user()->GetDefaultEditor() > 0 &&
         a()->user()->GetDefaultEditor() <= wwiv::stl::ssize(a()->editors);
}
template <class _Ty>
const _Ty& in_range(const _Ty& minValue, const _Ty& maxValue, const _Ty& value) {
  return std::max(std::min(maxValue, value), minValue);
}

int ansir_to_flags(uint8_t ansir) {
  int flags = 0;
  if (!(ansir & ansir_no_DOS)) {
    flags |= EFLAG_COMIO;
  }
  if (ansir & ansir_emulate_fossil) {
    flags |= EFLAG_FOSSIL;
  }
  if (ansir & ansir_temp_dir) {
    flags |= EFLAG_TEMP_DIR;
  }
  if (ansir & ansir_stdio) {
    flags |= EFLAG_STDIO;
  }
  if (ansir & ansir_binary) {
    flags |= EFLAG_BINARY;
  }
  return flags;
}
