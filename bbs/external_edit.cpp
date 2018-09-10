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
#include "bbs/external_edit.h"

#include <algorithm>
#include <string>

#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/execexternal.h"
#include "bbs/make_abs_cmd.h"
#include "bbs/message_editor_data.h"
#include "bbs/pause.h"
#include "bbs/utility.h"
#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"

#include "bbs/stuffin.h"
#include "local_io/wconstants.h"
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
  File file(FilePath(a()->temp_directory(), filename));
  file.SetFilePermissions(File::permReadWrite);
  file.Delete();
}

//
// QBBS
//

const std::string ExternalQBBSMessageEditor::editor_filename() const { return MSGTMP; }

ExternalQBBSMessageEditor ::~ExternalQBBSMessageEditor() { this->CleanupControlFiles(); }

void ExternalQBBSMessageEditor::CleanupControlFiles() {
  RemoveEditorFileFromTemp(MSGINF);
  RemoveEditorFileFromTemp(MSGTMP);
}

/**
 * Creates a MSGINF file for QBBS style editors.
 *
 * MSGINF. - A text file containing message information:
 *
 * line 1: Who the message is FROM
 * line 2: Who the message is TO
 * line 3: Message subject
 * line 4: Message number }   Where message is being
 * line 5: Message area   }   posted. (not used in editor)
 * line 6: Private flag ("YES" or "NO")
 */
static bool WriteMsgInf(const string& title, const string& sub_name, bool is_email,
                        const string& to_name) {
  TextFile file(FilePath(a()->temp_directory(), MSGINF), "wt");
  if (!file.IsOpen()) {
    return false;
  }

  // line 1: Who the message is FROM
  file.WriteLine(a()->user()->GetName());
  if (!to_name.empty()) {
    // line 2: Who the message is TO
    file.WriteLine(to_name);
  } else {
    // Since we don't know who this is to, make it all.
    // line 2: Who the message is TO
    file.WriteLine("All");
  }
  // line 3: Message subject
  file.WriteLine(title);
  // Message area # - We are not QBBS
  // line 4: Message number
  file.WriteLine("0");
  if (is_email) {
    // line 5: Message area
    file.WriteLine("E-mail");
    // line 6: Private flag ("YES" or "NO")
    file.WriteLine("YES");
  } else {
    // line 5: Message area
    file.WriteLine(sub_name);
    // line 6: Private flag ("YES" or "NO")
    file.WriteLine("NO");
  }
  file.Close();
  return true;
}

bool ExternalQBBSMessageEditor::Before() {
  CleanupControlFiles();
  if (File::Exists(a()->temp_directory(), QUOTES_TXT)) {
    // Copy quotes.txt to MSGTMP if it exists
    File::Copy(FilePath(a()->temp_directory(), QUOTES_TXT),
               FilePath(a()->temp_directory(), MSGTMP));
  }
  return WriteMsgInf(data_.title, data_.sub_name, data_.is_email(), data_.to_name);
}

bool ExternalQBBSMessageEditor::After() {
  // Copy MSGTMP to INPUT_MSG since that's what the rest of WWIV expectes.
  // TODO(rushfan): Let this function return an object with result and filename and anything
  // else that needs to be passed back.
  File::Copy(FilePath(temp_directory_, MSGTMP), FilePath(temp_directory_, INPUT_MSG));
  return true;
}



//
// WWIV
//

const std::string ExternalWWIVMessageEditor::editor_filename() const { return INPUT_MSG; }

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
  } else if (File::Exists(a()->temp_directory(), FEDIT_INF)) {
    fedit_data_rec fedit_data;
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
    RemoveEditorFileFromTemp(QUOTES_IND);
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

/////////////////////////////////////////////////////////////////////////////
// Actually launch the editor. This won't create any control files, etc.
static bool external_edit_internal(const string& edit_filename, const string& working_directory,
                                   const editorrec& editor, int numlines) {

  string editorCommand = (a()->context().incom()) ? editor.filename : editor.filenamecon;
  if (editorCommand.empty()) {
    bout << "You can't use that full screen editor. (eti)" << wwiv::endl << wwiv::endl;
    pausescr();
    return false;
  }

  if (File::Exists(edit_filename)) {
    File file(edit_filename);
    if (file.IsDirectory()) {
      bout.nl();
      bout << "|#6You can't edit a directory." << wwiv::endl << wwiv::endl;
      pausescr();
      return false;
    }
  }

  make_abs_cmd(a()->bbsdir(), &editorCommand);

  string strippedFileName{stripfn(edit_filename.c_str())};
  ScopeExit on_exit;
  if (!working_directory.empty()) {
    const auto original_directory = File::current_directory();
    File::set_current_directory(working_directory);
    on_exit.swap([=] { File::set_current_directory(original_directory); });
  }

  File fileTempForTime(FilePath(File::current_directory(), strippedFileName));
  time_t tFileTime = fileTempForTime.Exists() ? fileTempForTime.last_write_time() : 0;

  auto num_screen_lines = a()->user()->GetScreenLines();
  if (!a()->using_modem) {
    int newtl = (a()->screenlinest > a()->defscreenbottom - a()->localIO()->GetTopLine())
                    ? 0
                    : a()->localIO()->GetTopLine();
    num_screen_lines = a()->defscreenbottom + 1 - newtl;
  }

  const auto cmdLine = stuff_in(editorCommand, fileTempForTime.full_pathname(),
                                std::to_string(a()->user()->GetScreenChars()),
                                std::to_string(num_screen_lines), std::to_string(numlines), "");

  ExecuteExternalProgram(cmdLine, ansir_to_flags(editor.ansir));
  bout.clear_lines_listed();

  time_t tFileTime1 = fileTempForTime.Exists() ? fileTempForTime.last_write_time() : 0;
  return fileTempForTime.Exists() && (tFileTime != tFileTime1);
}

static std::unique_ptr<ExternalMessageEditor>
CreateExternalMessageEditor(const editorrec& editor, MessageEditorData& data, int maxli,
                            int* setanon, const std::string& temp_directory) {
  if (editor.bbs_type == EDITORREC_EDITOR_TYPE_QBBS) {
    return std::make_unique<ExternalQBBSMessageEditor>(editor, data, maxli, setanon,
                                                       a()->temp_directory());
  }
  return std::make_unique<ExternalWWIVMessageEditor>(editor, data, maxli, setanon,
                                                     a()->temp_directory());
}

bool ExternalMessageEditor::Run() {
  if (!Before()) {
    return false;
  }
  if (!external_edit_internal(editor_filename(), temp_directory_, editor_, maxli_)) {
    return false;
  }
  return After();
}

bool DoExternalMessageEditor(MessageEditorData& data, int maxli, int* setanon) {
  const size_t editor_number = a()->user()->GetDefaultEditor() - 1;
  if (editor_number >= a()->editors.size() || !okansi()) {
    bout << "\r\nYou can't use that full screen editor (EME).\r\n\n";
    return false;
  }

  const auto& editor = a()->editors[editor_number];
  auto eme = CreateExternalMessageEditor(editor, data, maxli, setanon, a()->temp_directory());
  return eme->Run();
}

bool external_text_edit(const string& edit_filename, const string& working_directory, int numlines,
                        int flags) {
  bout.nl();
  const auto editor_number = a()->user()->GetDefaultEditor() - 1;
  if (editor_number >= wwiv::stl::size_int(a()->editors) || !okansi()) {
    bout << "You can't use that full screen editor. (ete1)" << wwiv::endl << wwiv::endl;
    pausescr();
    return false;
  }

  const auto& editor = a()->editors[editor_number];
  MessageEditorData data{};
  data.msged_flags = flags;
  auto eme = CreateExternalMessageEditor(editor, data, numlines, false, a()->temp_directory());
  if (!eme->Before()) {
    return false;
  }
  
  return external_edit_internal(edit_filename, working_directory, editor, numlines);
}
