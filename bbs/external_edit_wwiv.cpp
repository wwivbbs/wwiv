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
#include "bbs/external_edit_wwiv.h"

#include <string>

#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "common/message_editor_data.h"
#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"

#include "sdk/filenames.h"

using std::string;
using wwiv::core::ScopeExit;
using namespace wwiv::bbs;
using namespace wwiv::core;
using namespace wwiv::strings;

struct fedit_data_rec {
  char tlen, ttl[81], anon;
};

static void RemoveEditorFileFromTemp(const string& filename) {
  File::Remove(FilePath(a()->temp_directory(), filename), true);
}

std::string ExternalWWIVMessageEditor::editor_filename() const { return INPUT_MSG; }

ExternalWWIVMessageEditor ::~ExternalWWIVMessageEditor() { this->CleanupControlFiles(); }

void ExternalWWIVMessageEditor::CleanupControlFiles() {
  RemoveEditorFileFromTemp(FEDIT_INF);
  RemoveEditorFileFromTemp(RESULT_ED);
  RemoveEditorFileFromTemp(EDITOR_INF);
}

static void ReadWWIVResultFiles(string* title, int* anon) {
  auto fp = FilePath(a()->temp_directory(), RESULT_ED);
  if (File::Exists(fp)) {
    TextFile file(fp, "rt");
    string anon_string;
    if (file.ReadLine(&anon_string)) {
      *anon = to_number<int>(anon_string);
      if (file.ReadLine(title)) {
        // Strip whitespace from title to avoid issues like bug #29
        StringTrim(title);
      }
    }
    file.Close();
  } else if (File::Exists(FilePath(a()->temp_directory(), FEDIT_INF))) {
    fedit_data_rec fedit_data{};
    memset(&fedit_data, '\0', sizeof(fedit_data_rec));
    File file(FilePath(a()->temp_directory(), FEDIT_INF));
    file.Open(File::modeBinary | File::modeReadOnly);
    if (file.Read(&fedit_data, sizeof(fedit_data))) {
      title->assign(fedit_data.ttl);
      *anon = fedit_data.anon;
    }
  }
}

static void WriteWWIVEditorControlFiles(const string& title, const string& sub_name,
                                        const string& to_name, int flags) {
  TextFile f(FilePath(a()->temp_directory(), EDITOR_INF), "wt");
  if (f.IsOpen()) {
    if (!to_name.empty()) {
      flags |= MSGED_FLAG_HAS_REPLY_NAME;
    }
    if (!title.empty()) {
      flags |= MSGED_FLAG_HAS_REPLY_TITLE;
    }
    f.WriteLine(title);
    f.WriteLine(sub_name);
    f.WriteLine(a()->usernum);
    f.WriteLine(a()->user()->GetName());
    f.WriteLine(a()->user()->GetRealName());
    f.WriteLine(a()->user()->GetSl());
    f.WriteLine(flags);
    f.WriteLine(a()->localIO()->GetTopLine());
    f.WriteLine(a()->user()->GetLanguage());
  }
  if (flags & MSGED_FLAG_NO_TAGLINE) {
    // disable tag lines by creating a DISABLE.TAG file
    TextFile fileDisableTag(DISABLE_TAG, "w");
  } else {
    RemoveEditorFileFromTemp(DISABLE_TAG);
  }
  if (title.empty()) {
    RemoveEditorFileFromTemp(QUOTES_TXT);
  }

  // Write FEDIT.INF
  fedit_data_rec fedit_data{};
  memset(&fedit_data, '\0', sizeof(fedit_data_rec));
  fedit_data.tlen = 60;
  to_char_array(fedit_data.ttl, title);
  fedit_data.anon = 0;

  File fedit_inf(FilePath(a()->temp_directory(), FEDIT_INF));
  if (fedit_inf.Open(File::modeDefault | File::modeCreateFile | File::modeTruncate,
                     File::shareDenyReadWrite)) {
    fedit_inf.Write(&fedit_data, sizeof(fedit_data));
    fedit_inf.Close();
  }
}

bool ExternalWWIVMessageEditor::Before() {
  CleanupControlFiles();
  WriteWWIVEditorControlFiles(data_.title, data_.sub_name, data_.to_name, data_.msged_flags);

  return true;
}

bool ExternalWWIVMessageEditor::After() { 
  ReadWWIVResultFiles(&data_.title, setanon_); 
  return true;
}

