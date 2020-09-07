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
#include "bbs/extract.h"

#include "bbs/bbs.h"
#include "common/com.h"
#include "common/input.h"
#include "bbs/misccmd.h"
#include "common/pause.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include <string>

using std::string;
using namespace wwiv::core;
using namespace wwiv::strings;

static std::optional<std::string> get_extract_dir() {
  bout.print_help_file(MEXTRACT_NOEXT);
  do {
    bout << "|#5(Q=Quit) Which (D,G,T) ? ";
    const auto ch1 = onek("QGDT?");
    switch (ch1) {
    case 'G':
      return a()->config()->gfilesdir();
    case 'D':
      return a()->config()->datadir();
    case 'T':
      return a()->sess().dirs().temp_directory();
    case '?':
      bout.print_help_file(MEXTRACT_NOEXT);
      break;
    case 'Q':
      return std::nullopt;
    }
  } while (!a()->sess().hangup());
  return std::nullopt;
}

void extract_out(const std::string& text, const std::string& title) {
  const auto odir = get_extract_dir();
  if (!odir) {
    return;
  }
  const auto& dir = odir.value();
  auto done = false;
  std::filesystem::path path;
  std::string mode = "wt";
  do {
    bout << "|#2Save under what filename? ";
    const auto fn = bin.input_filename(50);
    if (fn.empty()) {
      return;
    }
    path = FilePath(dir, fn);
    
    if (!File::Exists(path)) {
      break;
    }
    bout << "\r\nFilename already in use.\r\n\n";
    bout << "|#0O|#1)verwrite, |#0A|#1)ppend, |#0N|#1)ew name, |#0Q|#1)uit? |#0";
    const auto ch1 = onek("QOAN");
    switch (ch1) {
    case 'Q':
      return;
    case 'N':
      continue;
    case 'A':
      mode = "at";
      done = true;
      break;
    case 'O':
      done = true;
      File::Remove(path);
      break;
    }
    bout.nl();
  } while (!a()->sess().hangup() && !done);

  if (a()->sess().hangup()) {
    return;
  }
  TextFile file(path, mode);
  if (!file) {
    bout << "|#6Could not open file for writing.\r\n";
    return;
  }
  file.WriteLine(title);
  file.Write(text);
  bout <<  "|#9Message written to|#0: |#2" << file << wwiv::endl;
  bout.nl();
  bout.pausescr();
}

