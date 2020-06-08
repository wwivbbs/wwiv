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
#include "bbs/printfile.h"

#include <memory>
#include <string>
#include <vector>

#include "bbs/application.h"
#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/pause.h"
#include "core/file.h"
#include "core/os.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "local_io/keycodes.h"
#include "sdk/config.h"

using std::string;
using std::unique_ptr;
using namespace wwiv::core;
using namespace wwiv::stl;
using namespace wwiv::strings;

/**
 * Creates the fully qualified filename to display adding extensions and directories as needed.
 */
std::filesystem::path CreateFullPathToPrint(const string& basename) {
  std::vector<string> dirs{a()->language_dir, a()->config()->gfilesdir()};
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
    if (a()->user()->HasAnsi()) {
      if (a()->user()->HasColor()) {
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
bool printfile_path(const std::filesystem::path& file_path, bool abortable, bool force_pause) {
  if (!File::Exists(file_path)) {
    // No need to print a file that does not exist.
    return false;
  }
  std::error_code ec;
  if (!std::filesystem::is_regular_file(file_path, ec)) {
    // Not a file, no need to print a file that is not a file.
    return false;
  }

  TextFile tf(file_path, "rb");
  const auto v = tf.ReadFileIntoVector();
  for (const auto& s : v) {
    bout.bputs(s);
    bout.nl();
    const auto has_ansi = contains(s, ESC);
    // If this is an ANSI file, then don't pause
    // (since we may be moving around
    // on the screen, unless the caller tells us to pause anyway)
    if (has_ansi && !force_pause)
      bout.clear_lines_listed();
    if (contains(s, CZ)) {
      // We are done here on a control-Z since that's DOS EOF.  Also ANSI
      // files created with PabloDraw expect that anything after a Control-Z
      // is fair game for metadata and includes SAUCE metadata after it which
      // we do not want to render in the bbs.
      break;
    }
    if (abortable && checka()) {
      break;
    }
  }
  bout.flush();
  return !v.empty();
}

bool printfile(const std::string& filename, bool abortable, bool force_pause) {
  const auto full_path_name = CreateFullPathToPrint(filename);
  return printfile_path(full_path_name, abortable, force_pause);
}

/**
 * Displays a help file or an error that no help is available.
 *
 * A help file is a normal file displayed with printfile.
 */
bool print_help_file(const std::string& filename) {
  if (!printfile(filename)) {
    bout << "No help available.  File '" << filename << "' does not exist.\r\n";
    return false;
  }
  return true;
}

/**
 * Displays a file locally, using LIST util if so defined in WWIV.INI,
 * otherwise uses normal TTY output.
 */
void print_local_file(const string& filename) {
  printfile(filename);
  bout.nl(2);
  pausescr();
}

bool printfile_random(const std::string& base_fn) {
  const auto& dir = a()->language_dir;
  const auto dot_zero = FilePath(dir, StrCat(base_fn, ".0"));
  if (File::Exists(dot_zero)) {
    auto screens = 0;
    for (auto i = 0; i < 1000; i++) {
      const auto dot_n = FilePath(dir, StrCat(base_fn, ".", i));
      if (File::Exists(dot_n)) {
        screens++;
      } else {
        break;
      }
    }
    printfile_path(FilePath(dir, StrCat(base_fn, ".", wwiv::os::random_number(screens))));
    return true;
  }
  return false;
}
