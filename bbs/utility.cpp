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

#include "bbs/utility.h"
#include "bbs/bbs.h"
#include "bbs/common.h"
#include "bbs/connect1.h"
#include "common/input.h"
#include "common/output.h"
#include "common/workspace.h"
#include "core/file.h"
#include "core/findfiles.h"
#include "core/os.h"
#include "core/stl.h"
#include "core/strings.h"
#include "local_io/keycodes.h"
#include "sdk/files/files_ext.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <vector>
#ifdef _WIN32
#include <sys/utime.h>
#else
#include <utime.h>
#endif // WIN32

using namespace std::chrono;
using namespace wwiv::core;
using namespace wwiv::local::io;
using namespace wwiv::os;
using namespace wwiv::strings;

extern const unsigned char* translate_letters[];

template <class _Ty>
const _Ty& in_range(const _Ty& minValue, const _Ty& maxValue, const _Ty& value);

/**
 * Deletes files from a directory.  This is meant to be used only in the temp
 * directories of WWIV.
 *
 * @param file_name       Wildcard file specification to delete
 * @param directory_name  Name of the directory to delete files from
 * @param bPrintStatus    Print out locally as files are deleted
 */
void remove_from_temp(const std::string& file_name, const std::filesystem::path& directory_name,
                      bool bPrintStatus) {
  const auto filespec = FilePath(directory_name, stripfn(file_name));
  FindFiles ff(filespec, FindFiles::FindFilesType::any);
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
bool okansi() { return a()->user()->ansi(); }

/**
 * Gets the current users post/call ratio.
 */
float post_ratio() {
  if (a()->user()->logons() == 0) {
    return 99.999f;
  }
  const auto r = static_cast<float>(a()->user()->messages_posted()) /
                 static_cast<float>(a()->user()->logons());
  return r > 99.998f ? 99.998f : r;
}

long nsl() {
  const auto dd = system_clock::now();
  if (!a()->sess().IsUserOnline()) {
    return 1;
  }
  const auto tot = duration_cast<seconds>(dd - a()->sess().system_logon_time());

  const auto tpl = minutes(a()->config()->sl(a()->sess().effective_sl()).time_per_logon);
  const auto tpd = minutes(a()->config()->sl(a()->sess().effective_sl()).time_per_day);
  const auto extra_time_call =
      duration_cast<seconds>(a()->user()->extra_time() + a()->extratimecall());
  const auto tlc = std::chrono::duration_cast<seconds>(tpl - tot + extra_time_call);
  // time left today
  const auto timeontoday = a()->user()->timeontoday();
  const auto extra_time = a()->user()->extra_time();
  const auto tld = std::chrono::duration_cast<seconds>(tpd - tot - timeontoday + extra_time);
  const auto tlt = std::min<seconds>(tlc, tld);
  a()->sess().SetTimeOnlineLimited(false);
  const auto v = static_cast<int>(duration_cast<seconds>(tlt).count());
  return static_cast<long>(in_range<int64_t>(0, 32767, v));
}

void send_net(net_header_rec* nh, std::vector<uint16_t> list, const std::string& text,
              const std::string& byname) {
  const auto filename = StrCat(a()->network_directory(), "p1", a()->network_extension());
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
    nh->length += wwiv::stl::size_uint32(byname) + 1;
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

std::string stripfn(const std::string& file_name) {
  const std::filesystem::path p(file_name);
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

  FindFiles ff(file_mask, FindFiles::FindFilesType::any);
  if (ff.empty()) {
    bout << "No files found\r\n";
    return {};
  }
  auto f = ff.begin();
  bout.bprintf("%12.12s ", f->name);

  if (strchr(file_mask, File::pathSeparatorChar) == nullptr) {
    file_mask[0] = '\0';
  } else {
    for (int i = 0; i < ssize(file_mask); i++) {
      if (file_mask[i] == File::pathSeparatorChar) {
        mark = i + 1;
      }
    }
  }
  const auto m = file_mask[mark];
  file_mask[mark] = 0;
  auto* pszPath = file_mask;
  file_mask[mark] = m;
  const auto t = static_cast<int>(size(pszPath));
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
    bout.bprintf("%12.12s ", f->name);
    if (bin.getkey() == SPACE) {
      bout.nl();
      break;
    }
  }
  bout.nl();
  if (i == 1) {
    bout << "One file found: " << f->name << wwiv::endl;
    bout << "Use this file? ";
    if (bin.yesno()) {
      return pszPath;
    }
    pszPath[0] = '\0';
    return pszPath;
  }
  pszPath[t] = '\0';
  bout << "Filename: ";
  bin.input(file_mask, 12, true);
  strcat(pszPath, file_mask);
  return pszPath;
}

int side_menu(int* menu_pos, bool bNeedsRedraw, const std::vector<std::string>& menu_items, int xpos,
              int ypos, side_menu_colors* smc) {
  static int positions[20];
  a()->tleft(true);

  if (bNeedsRedraw) {
    int amount = 1;
    positions[0] = xpos;
    for (const auto& menu_item : menu_items) {
      positions[amount] = positions[amount - 1] + wwiv::stl::ssize(menu_item) + 2;
      ++amount;
    }

    int x = 0;
    bout.SystemColor(smc->normal_menu_item);

    for (const auto& menu_item : menu_items) {
      if (a()->sess().hangup()) {
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

  while (!a()->sess().hangup()) {
    const auto event = bin.bgetch_event(wwiv::common::Input::numlock_status_t::NOTNUMBERS);
    if (event < 128) {
      int x = 0;
      for (const auto& menu_item : menu_items) {
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
    }
    switch (event) {
    case COMMAND_LEFT:
      bout.GotoXY(positions[*menu_pos], ypos);
      bout.SystemColor(smc->normal_highlight);
      bout.bputch(menu_items[*menu_pos][0]);
      bout.SystemColor(smc->normal_menu_item);
      bout.bputs(menu_items[*menu_pos].substr(1));
      if (!*menu_pos) {
        *menu_pos = wwiv::stl::size_int(menu_items) - 1;
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
  return 0;
}

bool okfsed() { return ok_internal_fsed() || ok_external_fsed(); }

bool ok_external_fsed() {
  const auto ed = a()->user()->default_editor();
  return okansi() && ed > 0 && ed != 0xff && ed <= wwiv::stl::ssize(a()->editors);
}

bool ok_internal_fsed() { return okansi() && a()->user()->default_editor() == 0xff; }


template <class _Ty>
const _Ty& in_range(const _Ty& minValue, const _Ty& maxValue, const _Ty& value) {
  auto v = std::min(maxValue, value);
  return std::max(v, minValue);
}

int ansir_to_flags(uint16_t ansir) {
  auto flags = 0;
  if (!(ansir & ansir_no_DOS)) {
    flags |= EFLAG_COMIO;
  }
  if (ansir & ansir_emulate_fossil) {
    flags |= EFLAG_SYNC_FOSSIL;
  }
  if (ansir & ansir_netfoss) {
    flags |= EFLAG_NETFOSS;
    // NetFoss implies temp dir too.
    flags |= EFLAG_TEMP_DIR;
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
