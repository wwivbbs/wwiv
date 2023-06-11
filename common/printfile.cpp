/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2022, WWIV Software Services             */
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
#include "common/printfile.h"

#include "common/input.h"
#include "common/menus/menu_data_util.h"
#include "core/file.h"
#include "core/os.h"
#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "local_io/keycodes.h"
#include <chrono>
#include <regex>
#include <string>
#include <vector>

namespace wwiv::common {

using namespace std::chrono;
using namespace std::chrono_literals;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

std::filesystem::path CreateFullPathToPrintWithCols(const std::filesystem::path& filename,
                                                    int screen_length) {
  // this shouldn't happen but let's warn about it
  if (filename.empty() || !File::Exists(filename)) {
    LOG(WARNING) << "CreateFullPathToPrintWithCols: filename does not exist: "
                 << filename.string();
    return filename;
  }

  const auto base = filename.stem();
  const auto ext = filename.extension();
  const auto dir = filename.parent_path();

  // u8string didn' twork with regex_match on C++20
  const std::regex pieces_regex(fmt::format("{}\\.(\\d+)\\{}", filename.stem().string(), ext.string()));
  std::smatch pieces_match;
  std::map<int, std::filesystem::path> col_fn_map;
  for (const auto& f : std::filesystem::directory_iterator(dir)) {
    auto fname = f.path().filename().string();
    if (std::regex_match(fname, pieces_match, pieces_regex)) {
      col_fn_map[wwiv::strings::to_number<int>(pieces_match[1].str())] = f.path();
    }
  }

  // be explicit that we need this to be non-empty.
  if (col_fn_map.empty()) {
    return filename;
  }

  for (auto it = std::rbegin(col_fn_map); it != std::rend(col_fn_map); it++) {
    if (it->first <= screen_length) {
      return it->second;
    }
  }
  return filename;
}

std::filesystem::path CreateFullPathToPrint(const std::vector<std::filesystem::path>& dirs,
                                            const User& user, const std::string& basename) {
  for (const auto& base : dirs) {
    auto file{FilePath(base, basename)};
    if (basename.find('.') != std::string::npos) {
      // We have a file with extension.
      if (File::Exists(file)) {
        return file;
      }
      // Since no wwiv file names contain embedded dots skip to the next directory.
      continue;
    }
    auto candidate{file};
    if (user.ansi()) {
      if (user.color()) {
        // ANSI and color
        candidate.replace_extension(".ans");
        if (File::Exists(candidate)) {
          return CreateFullPathToPrintWithCols(candidate, user.screen_width());
        }
      }
      // ANSI.
      candidate.replace_extension(".b&w");
      if (File::Exists(candidate)) {
        return CreateFullPathToPrintWithCols(candidate, user.screen_width());
      }
    }
    // ANSI/Color optional
    candidate.replace_extension(".msg");
    if (File::Exists(candidate)) {
      return CreateFullPathToPrintWithCols(candidate, user.screen_width());
    }
  }
  // Nothing matched, return the input.
  return basename;
}

class printfile_opts {
public:
  printfile_opts(SessionContext& sc, Output& out, const std::string& raw, bool abtable, bool forcep)
    : sess(sc), out_(out), abortable(abtable), force_pause(forcep) {
    menus::menu_data_and_options_t t(raw);
    data_ = t.data();
    saved_disable_pause = sess.disable_pause();
    saved_user_has_pause = out.user().pause();
    if (!t.opts_empty()) {
      for (const auto& [key, value] : t.opts()) {
        if (key == "pause") {
          if (value == "on") {
            sess.disable_pause(false);
            out.user().set_flag(User::pauseOnPage, true);
          } else if (value == "off") {
            sess.disable_pause(true);
            out.user().set_flag(User::pauseOnPage, false);
          } else if (value == "start") {
            bout.pausescr();
          } else if (value == "end") {
            pause_at_end = true;
          }
        } else if (key == "bps") {
          bps = to_number<int>(value);
          sess.set_file_bps(bps);
        }
      }
    }
  }
  ~printfile_opts() {
    sess.disable_pause(saved_disable_pause);
    out_.user().set_flag(User::pauseOnPage, saved_user_has_pause);
    if (pause_at_end) {
      bout.pausescr();
    }
  }

  [[nodiscard]] std::string data() const noexcept { return data_; }

private:
  std::string data_;
  SessionContext& sess;
  Output& out_;
public:
  bool abortable{true};
  bool force_pause{true};
  bool pause_at_start{false};
  bool pause_at_end{false};
  bool saved_disable_pause{false};
  bool saved_user_has_pause{false};
  int bps{0};
};


/**
 * Prints the file file_name.  Returns true if the file exists and is not
 * zero length.  Returns false if the file does not exist or is zero length
 *
 * @param file_path Full path to the file to display
 * @param abortable If true, a keyboard input may abort the display
 * @param force_pause Should pauses be used even for ANSI files - Normally
 *        pause on screen is disabled for ANSI files.
 *
 * @return true if the file exists and is not zero length
 */
// ReSharper disable once CppMemberFunctionMayBeConst
bool Output::printfile_path(const std::filesystem::path& file_path, bool abortable, bool force_pause) {
  auto at_exit = finally([this]() { sess().set_file_bps(0); });
  if (!File::Exists(file_path)) {
    // No need to print a file that does not exist.
    return false;
  }
  if (std::error_code ec; !is_regular_file(file_path, ec)) {
    // Not a file, no need to print a file that is not a file.
    return false;
  }

  const auto save_mci = bout.mci_enabled();
  auto at_exit_mci = finally([=]() { bout.set_mci_enabled(save_mci); });
  bout.enable_mci();
  TextFile tf(file_path, "rb");
  const auto v = tf.ReadFileIntoVector();

  const auto start_time = system_clock::now();
  auto num_written = 0;
  for (const auto& s : v) {
    num_written += bout.outstr(s);
    bout.nl();
    // If this is an ANSI file, then don't pause
    // (since we may be moving around
    // on the screen, unless the caller tells us to pause anyway)
    if (const auto has_ansi = contains(s, local::io::ESC); has_ansi && !force_pause) {
      bout.clear_lines_listed();
    }
    if (contains(s, local::io::CZ)) {
      // We are done here on a control-Z since that's DOS EOF.  Also ANSI
      // files created with PabloDraw expect that anything after a Control-Z
      // is fair game for metadata and includes SAUCE metadata after it which
      // we do not want to render in the bbs.
      break;
    }
    if (abortable && bin.checka()) {
      break;
    }
  }
  bout.flush();

  if (sess().bps() > 0) {
    const auto elapsed_ms = duration_cast<milliseconds>(system_clock::now() - start_time);
    const auto actual_cps = static_cast<long>(num_written) * 1000 / (elapsed_ms.count() + 1);
    VLOG(1) << "Record CPS for file: " << file_path.string() << "; CPS: " << actual_cps;
  }
  return !v.empty();
}

bool Output::printfile(const std::string& data, bool abortable, bool force_pause) {
  const printfile_opts opts(sess(), *this, data, abortable, force_pause);

  const std::vector<std::filesystem::path> dirs{sess().dirs().current_menu_gfiles_directory(), 
    sess().dirs().gfiles_directory()};

  const auto full_path_name = CreateFullPathToPrint(dirs, context().u(), opts.data());
  return printfile_path(full_path_name, abortable, force_pause);
}

bool Output::print_help_file(const std::string& filename) {
  if (!printfile(filename)) {
    bout.print("No help available.  File '{}' does not exist.\r\n", filename);
    return false;
  }
  return true;
}

void Output::print_local_file(const std::string& data) {
  printfile(data);
  bout.nl(2);
  bout.pausescr();
}

bool Output::printfile_random(const std::string& data) {
  const printfile_opts opts(sess(), *this, data, true, true);
  const auto& dir = sess().dirs().current_menu_gfiles_directory();
  const auto base_fn = opts.data();
  if (const auto dot_zero = FilePath(dir, StrCat(base_fn, ".0")); !File::Exists(dot_zero)) {
    return false;
  }
  auto screens = 0;
  for (auto i = 0; i < 1000; i++) {
    if (const auto dot_n = FilePath(dir, StrCat(base_fn, ".", i)); File::Exists(dot_n)) {
      ++screens;
    } else {
      break;
    }
  }
  return printfile_path(FilePath(dir, StrCat(base_fn, ".", os::random_number(screens))),
                        opts.abortable, opts.force_pause);
}

} // namespace wwiv::common
