/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2017, WWIV Software Services             */
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

#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/keycodes.h"
#include "bbs/pause.h"
#include "bbs/application.h"
#include "core/file.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"

using std::string;
using std::unique_ptr;
using namespace wwiv::stl;
using namespace wwiv::strings;

/**
 * Creates the fully qualified filename to display adding extensions and directories as needed.
 */
string CreateFullPathToPrint(const string& basename) {
  std::vector<string> dirs { a()->language_dir, a()->config()->gfilesdir()};
  for (const auto& base : dirs) {
    File file(base, basename);
    if (basename.find('.') != string::npos) {
      // We have a file with extension.
      if (file.Exists()) {
        return file.full_pathname();
      }
      // Since no wwiv filenames contain embedded dots skip to the next directory.
      continue;
    }
    const auto root_filename = file.full_pathname();
    if (a()->user()->HasAnsi()) {
      if (a()->user()->HasColor()) {
        // ANSI and color
        auto candidate = StringPrintf("%s.ans", root_filename.c_str());
        if (File::Exists(candidate)) {
          return candidate;
        }
      }
      // ANSI.
      auto candidate = StringPrintf("%s.b&w", root_filename.c_str());
      if (File::Exists(candidate)) {
        return candidate;
      }
    }
    // ANSI/Color optional
    auto candidate = StringPrintf("%s.msg", root_filename.c_str());
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
 * @param filename Name of the file to display
 * @param abortable If true, a keyboard input may abort the display
 * @param force_pause Should pauses be used even for ANSI files - Normally
 *        pause on screen is disabled for ANSI files.
 *
 * @return true if the file exists and is not zero length
 */
bool printfile(const string& filename, bool abortable, bool force_pause) {
  const auto full_path_name = CreateFullPathToPrint(filename);
  {
    File f(full_path_name);
    if (!f.Exists()) {
      // No need to print a file that does not exist.
      return false;
    }
    if (!f.IsFile()) {
      // Not a file, no need to print a file that is not a file.
      return false;
    }
  }

  TextFile tf(full_path_name, "rb");
  auto v = tf.ReadFileIntoVector();
  for (const auto& s : v) {
    bout.bputs(s);
    bout.nl();
    auto has_ansi = contains(s, ESC);
    // If this is an ANSI file, then don't pause
    // (since we may be moving around
    // on the screen, unless the caller tells us to pause anyway)
    if (has_ansi && !force_pause) bout.clear_lines_listed();
    if (contains(s, CZ)) {
      // We are done here on a control-Z since that's DOS EOF.  Also ANSI
      // files created with PabloDraw expect that anything after a Control-Z
      // is fair game for metadata and includes SAUCE metadata after it which
      // we do not want to render in the bbs.
      break;
    }
    if (abortable && checka()) break;
  }
  bout.flush();
  return !v.empty();
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

