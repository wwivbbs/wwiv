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
#include "bbs/external_edit.h"

#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/execexternal.h"
#include "bbs/external_edit_qbbs.h"
#include "bbs/external_edit_wwiv.h"
#include "bbs/make_abs_cmd.h"
#include "common/message_editor_data.h"
#include "common/pause.h"
#include "bbs/stuffin.h"
#include "bbs/utility.h"
#include "fsed/fsed.h"
#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "sdk/names.h"
#include <string>

using std::string;
using wwiv::core::ScopeExit;
using namespace wwiv::bbs;
using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::strings;

/////////////////////////////////////////////////////////////////////////////
// Actually launch the editor. This won't create any control files, etc.
static bool external_edit_internal(const string& edit_filename, const string& working_directory,
                                   const editorrec& editor, int numlines) {

  string editorCommand = (a()->sess().incom()) ? editor.filename : editor.filenamecon;
  if (editorCommand.empty()) {
    bout << "You can't use that full screen editor. (eti)" << wwiv::endl << wwiv::endl;
    bout.pausescr();
    return false;
  }

  if (File::Exists(edit_filename)) {
    File file(edit_filename);
    if (File::is_directory(edit_filename)) {
      bout.nl();
      bout << "|#6You can't edit a directory." << wwiv::endl << wwiv::endl;
      bout.pausescr();
      return false;
    }
  }

  make_abs_cmd(a()->bbsdir(), &editorCommand);

  auto strippedFileName{stripfn(edit_filename)};
  ScopeExit on_exit([=] { File::set_current_directory(a()->bbspath()); });
  if (!working_directory.empty()) {
    File::set_current_directory(working_directory);
  }

  const auto tft_fn = FilePath(File::current_directory(), strippedFileName);
  File fileTempForTime(tft_fn);
  auto tFileTime = File::Exists(tft_fn) ? File::last_write_time(tft_fn) : 0;

  auto num_screen_lines = a()->user()->GetScreenLines();
  const auto cmdLine = stuff_in(editorCommand, fileTempForTime.full_pathname(),
                                std::to_string(a()->user()->GetScreenChars()),
                                std::to_string(num_screen_lines), std::to_string(numlines), "");

  ExecuteExternalProgram(cmdLine, ansir_to_flags(editor.ansir) | EFLAG_NO_CHANGE_DIR);
  bout.clear_lines_listed();

  auto tFileTime1 = File::Exists(tft_fn) ? File::last_write_time(tft_fn) : 0;
  return File::Exists(tft_fn) && tFileTime != tFileTime1;
}

static std::unique_ptr<ExternalMessageEditor>
CreateExternalMessageEditor(const editorrec& editor, MessageEditorData& data, int maxli,
                            int* setanon, const std::string& temp_directory) {
  if (editor.bbs_type == EDITORREC_EDITOR_TYPE_QBBS) {
    return std::make_unique<ExternalQBBSMessageEditor>(editor, data, maxli, setanon,
                                                       temp_directory);
  }
  return std::make_unique<ExternalWWIVMessageEditor>(editor, data, maxli, setanon, temp_directory);
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
  auto eme = CreateExternalMessageEditor(editor, data, maxli, setanon, a()->sess().dirs().temp_directory());
  return eme->Run();
}

bool external_text_edit(const string& edit_filename, const string& working_directory, int numlines,
                        int flags) {
  bout.nl();
  const auto editor_number = a()->user()->GetDefaultEditor() - 1;
  if (editor_number >= wwiv::stl::ssize(a()->editors) || !okansi()) {
    bout << "You can't use that full screen editor. (ete1)" << wwiv::endl << wwiv::endl;
    bout.pausescr();
    return false;
  }

  const auto& editor = a()->editors[editor_number];
  MessageEditorData data(a()->names()->UserName(a()->usernum));
  data.msged_flags = flags;
  int setanon = 0;
  auto eme = CreateExternalMessageEditor(editor, data, numlines, &setanon, a()->sess().dirs().temp_directory());
  if (!eme->Before()) {
    return false;
  }
  
  return external_edit_internal(edit_filename, working_directory, editor, numlines);
}

bool fsed_text_edit(const std::string& edit_filename, const std::string& new_directory,
  int numlines, int flags) {
  if (ok_external_fsed()) {
    return external_text_edit(edit_filename, new_directory, numlines, flags);
  }
  std::filesystem::path p{edit_filename};
  const auto dir = p.parent_path();
  return wwiv::bbs::fsed::fsed(p);
}
