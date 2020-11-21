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
#include "common/printfile.h"


#include "bbs/bbs.h"
#include "common/input.h"
#include "common/menu_data_util.h"
#include "common/pause.h"
#include "core/file.h"
#include "core/os.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "local_io/keycodes.h"
#include "sdk/config.h"
#include "core/scope_exit.h"
#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace wwiv::common {

using std::string;
using std::unique_ptr;
using namespace std::chrono;
using namespace std::chrono_literals;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

/**
 * Creates the fully qualified filename to display adding extensions and directories as needed.
 */
std::filesystem::path CreateFullPathToPrint(const std::vector<string>& dirs, const User& user,
                                            const string& basename) {
  for (const auto& base : dirs) {
    auto file{FilePath(base, basename)};
    if (basename.find('.') != string::npos) {
      // We have a file with extension.
      if (File::Exists(file)) {
        return file;
      }
      // Since no wwiv file names contain embedded dots skip to the next directory.
      continue;
    }
    auto candidate{file};
    if (user.HasAnsi()) {
      if (user.HasColor()) {
        // ANSI and color
        candidate.replace_extension(".ans");
        if (File::Exists(candidate)) {
          return candidate;
        }
      }
      // ANSI.
      candidate.replace_extension(".b&w");
      if (File::Exists(candidate)) {
        return candidate;
      }
    }
    // ANSI/Color optional
    candidate.replace_extension(".msg");
    if (File::Exists(candidate)) {
      return candidate;
    }
  }
  // Nothing matched, return the input.
  return basename;
}

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
  ScopeExit at_exit([this]() { sess().set_file_bps(0); });
  if (!File::Exists(file_path)) {
    // No need to print a file that does not exist.
    return false;
  }
  std::error_code ec;
  if (!is_regular_file(file_path, ec)) {
    // Not a file, no need to print a file that is not a file.
    return false;
  }

  TextFile tf(file_path, "rb");
  const auto v = tf.ReadFileIntoVector();

  const auto start_time = system_clock::now();
  auto num_written = 0;
  for (const auto& s : v) {
    // BPS will be either the file BPS or system bps or 0
    // use BPS in the loops since eventually MCI codes will be able
    // to change the BPS on the fly.
    //const auto cps = sess().bps() / 10;
    //num_written += bout.bputs(s);
    //if (sess().bps() > 0) {
    //  while (std::chrono::duration_cast<milliseconds>(system_clock::now() - start_time).count() < (num_written * 1000 / cps)) {
    //    bout.flush();
    //    os::sleep_for(milliseconds(100));
    //  }
    //}
    num_written += bout.bputs(s);
    bout.nl();
    const auto has_ansi = contains(s, ESC);
    // If this is an ANSI file, then don't pause
    // (since we may be moving around
    // on the screen, unless the caller tells us to pause anyway)
    if (has_ansi && !force_pause) {
      bout.clear_lines_listed();
    }
    if (contains(s, CZ)) {
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
    const auto actual_cps = num_written * 1000 / (elapsed_ms.count() + 1);
    VLOG(1) << "Record CPS for file: " << file_path.string() << "; CPS: " << actual_cps;
  }
  return !v.empty();
}

bool Output::printfile(const std::string& data, bool abortable, bool force_pause) {
  menu_data_and_options_t t(data);
  auto pause_at_end{false};
  auto pause_at_start{false};
  const auto saved_disable_pause = sess().disable_pause();
  if (!t.opts_empty()) {
    for (const auto& [key, value] : t.opts()) {
      if (key == "pause") {
        if (value == "on") {
          sess().disable_pause(false);
        } else if (value == "off") {
          sess().disable_pause(true);
        } else if (value == "start") {
          pause_at_start = true;
        } else if (value == "end") {
          pause_at_end = true;
        }
      } else if (key == "bps") {
        const auto bps = to_number<int>(value);
        sess().set_file_bps(bps);
      }
    }
  }

  const std::vector<string> dirs{sess().dirs().language_directory(), 
    sess().dirs().gfiles_directory()};

  const auto full_path_name = CreateFullPathToPrint(dirs, context().u(), t.data());
  if (pause_at_start) {
    pausescr();
  }
  const auto r = printfile_path(full_path_name, abortable, force_pause);

  sess().disable_pause(saved_disable_pause);
  if (pause_at_end) {
    pausescr();
  }

  return r;
}

/**
 * Displays a help file or an error that no help is available.
 *
 * A help file is a normal file displayed with printfile.
 */
bool Output::print_help_file(const std::string& filename) {
  if (!printfile(filename)) {
    bout << "No help available.  File '" << filename << "' does not exist.\r\n";
    return false;
  }
  return true;
}

/**
 * Displays a file locally.
 */
void Output::print_local_file(const string& filename) {
  printfile(filename);
  bout.nl(2);
  bout.pausescr();
}

bool Output::printfile_random(const std::string& base_fn) {
  const auto& dir = sess().dirs().language_directory();
  const auto dot_zero = FilePath(dir, StrCat(base_fn, ".0"));
  if (!File::Exists(dot_zero)) {
    return false;
  }
  auto screens = 0;
  for (auto i = 0; i < 1000; i++) {
    const auto dot_n = FilePath(dir, StrCat(base_fn, ".", i));
    if (File::Exists(dot_n)) {
      ++screens;
    } else {
      break;
    }
  }
  return printfile_path(FilePath(dir, StrCat(base_fn, ".", wwiv::os::random_number(screens))));
}

} // namespace wwiv::common
