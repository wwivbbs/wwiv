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
#include "bbs/printfile.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "bbs/bbs.h"
#include "bbs/platform/wlocal_io.h"
#include "bbs/fcns.h"
#include "bbs/keycodes.h"
#include "bbs/vars.h"
#include "bbs/wconstants.h"
#include "bbs/wsession.h"
#include "core/wwivport.h"
#include "core/file.h"
#include "core/strings.h"
#include "core/wwivassert.h"

using std::string;
using std::unique_ptr;
using namespace wwiv::strings;

/**
 * Creates the fully qualified filename to display adding extensions and directories as needed.
 */
const string CreateFullPathToPrint(const string& basename) {
  if (File::Exists(basename)) {
    return basename;
  }
  string langdir(session()->language_dir);
  string gfilesdir(syscfg.gfilesdir);
  std::vector<string> dirs { langdir, gfilesdir };
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
    const string root_filename = file.full_pathname();
    if (session()->user()->HasAnsi()) {
      if (session()->user()->HasColor()) {
        // ANSI and color
        string candidate = StringPrintf("%s.ans", root_filename.c_str());
        if (File::Exists(candidate)) {
          return candidate;
        }
      }
      // ANSI.
      string candidate = StringPrintf("%s.b&w", root_filename.c_str());
      if (File::Exists(candidate)) {
        return candidate;
      }
    }
    // ANSI/Color optional
    string candidate = StringPrintf("%s.msg", root_filename.c_str());
    if (File::Exists(candidate)) {
      return candidate;
    }
  }
  // Nothing matched, return the input.
  return basename;
}

/**
 * Prints the file pszFileName.  Returns true if the file exists and is not
 * zero length.  Returns false if the file does not exist or is zero length
 *
 * @param pszFileName Name of the file to display
 * @param bAbortable If true, a keyboard input may abort the display
 * @param bForcePause Should pauses be used even for ANSI files - Normally
 *        pause on screen is disabled for ANSI files.
 *
 * @return true if the file exists and is not zero length
 */
bool printfile(const string& filename, bool bAbortable, bool bForcePause) {
  string full_path_name = CreateFullPathToPrint(filename);
  long lFileSize;
  unique_ptr<char[], void (*)(void*)> ss(get_file(full_path_name.c_str(), &lFileSize), &std::free);
  if (!ss) {
    return false;
  }
  long lCurPos = 0;
  bool bHasAnsi = false;
  while (lCurPos < lFileSize && !hangup) {
    if (ss[lCurPos] == ESC) {
      // if we have an ESC, then this file probably contains
      // an ansi sequence
      bHasAnsi = true;
    }
    if (bHasAnsi && !bForcePause) {
      // If this is an ANSI file, then don't pause
      // (since we may be moving around
      // on the screen, unless the caller tells us to pause anyway)
      lines_listed = 0;
    }
    bputch(ss[lCurPos++], true);
    if (bAbortable) {
      bool abort = false;
      checka(&abort);
      if (abort) {
        break;
      }
      if (bkbhit()) {
        char ch = bgetch();
        if (ch == ' ') {
          break;
        }
      }
    }
  }
  FlushOutComChBuffer();
  // If the file is empty, lets' return false here since nothing was displayed.
  return lFileSize > 0;
}

/**
 * Displays a file locally, using LIST util if so defined in WWIV.INI,
 * otherwise uses normal TTY output.
 */
void print_local_file(const string& ss) {
  printfile(ss);
  bout.nl(2);
  pausescr();
}

