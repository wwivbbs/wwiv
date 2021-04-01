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
#include "bbs/external_edit_wwiv.h"

#include "bbs/bbs.h"
#include "common/message_editor_data.h"
#include "common/output.h"
#include "core/scope_exit.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "local_io/local_io.h"
#include "sdk/filenames.h"
#include "sdk/user.h"

#include <string>

using wwiv::core::ScopeExit;
using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::strings;

struct fedit_data_rec {
  char tlen, ttl[81], anon;
};

std::string ExternalWWIVMessageEditor::editor_filename() const { return INPUT_MSG; }

ExternalWWIVMessageEditor ::~ExternalWWIVMessageEditor() { this->CleanupControlFiles(); }

void ExternalWWIVMessageEditor::CleanupControlFiles() {
  File::Remove(FilePath(temp_directory_, FEDIT_INF), true);
  File::Remove(FilePath(temp_directory_, RESULT_ED), true);
  File::Remove(FilePath(temp_directory_, EDITOR_INF), true);
}

static void ReadWWIVResultFiles(std::string* title, int* anon, const std::filesystem::path& tempdir) {
  const auto fp = FilePath(tempdir, RESULT_ED);
  if (File::Exists(fp)) {
    TextFile file(fp, "rt");
    std::string anon_string;
    if (file.ReadLine(&anon_string)) {
      *anon = to_number<int>(anon_string);
      if (file.ReadLine(title)) {
        // Strip whitespace from title to avoid issues like bug #29
        StringTrim(title);
      }
    }
    file.Close();
  } else if (File::Exists(FilePath(tempdir, FEDIT_INF))) {
    fedit_data_rec fedit_data{};
    memset(&fedit_data, '\0', sizeof(fedit_data_rec));
    File file(FilePath(tempdir, FEDIT_INF));
    file.Open(File::modeBinary | File::modeReadOnly);
    if (file.Read(&fedit_data, sizeof(fedit_data))) {
      title->assign(fedit_data.ttl);
      *anon = fedit_data.anon;
    }
  }
}

static void WriteWWIVEditorControlFiles(const std::string& title, const std::string& sub_name,
                                        const std::string& to_name, int flags,
                                        const std::filesystem::path& tempdir) {
  TextFile f(FilePath(tempdir, EDITOR_INF), "wt");
  if (f.IsOpen()) {
    if (!to_name.empty()) {
      flags |= MSGED_FLAG_HAS_REPLY_NAME;
    }
    if (!title.empty()) {
      flags |= MSGED_FLAG_HAS_REPLY_TITLE;
    }
    f.WriteLine(title);
    f.WriteLine(sub_name);
    f.WriteLine(a()->user()->usernum());
    f.WriteLine(a()->user()->name());
    f.WriteLine(a()->user()->real_name());
    f.WriteLine(a()->user()->sl());
    f.WriteLine(flags);
    f.WriteLine(bout.localIO()->GetTopLine());
    f.WriteLine("0"); // Was user.language
  }
  if (flags & MSGED_FLAG_NO_TAGLINE) {
    // disable tag lines by creating a DISABLE.TAG file
    TextFile fileDisableTag(FilePath(tempdir, DISABLE_TAG), "w");
    fileDisableTag.Write("");
  } else {
    File::Remove(FilePath(tempdir, DISABLE_TAG), true);
  }
  if (title.empty()) {
    File::Remove(FilePath(tempdir, QUOTES_TXT), true);
  }

  // Write FEDIT.INF
  fedit_data_rec fedit_data{};
  memset(&fedit_data, '\0', sizeof(fedit_data_rec));
  fedit_data.tlen = 60;
  to_char_array(fedit_data.ttl, title);
  fedit_data.anon = 0;

  File fedit_inf(FilePath(tempdir, FEDIT_INF));
  if (fedit_inf.Open(File::modeDefault | File::modeCreateFile | File::modeTruncate,
                     File::shareDenyReadWrite)) {
    fedit_inf.Write(&fedit_data, sizeof(fedit_data));
    fedit_inf.Close();
  }
}

bool ExternalWWIVMessageEditor::Before() {
  CleanupControlFiles();
  WriteWWIVEditorControlFiles(data_.title, data_.sub_name, data_.to_name, data_.msged_flags, temp_directory_);

  return true;
}

bool ExternalWWIVMessageEditor::After() { 
  ReadWWIVResultFiles(&data_.title, setanon_, temp_directory_); 
  return true;
}

