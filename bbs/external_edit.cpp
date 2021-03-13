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
#include "bbs/external_edit.h"

#include "bbs/bbs.h"
#include "bbs/execexternal.h"
#include "bbs/external_edit_qbbs.h"
#include "bbs/external_edit_wwiv.h"
#include "bbs/make_abs_cmd.h"
#include "common/message_editor_data.h"
#include "common/pause.h"
#include "bbs/stuffin.h"
#include "bbs/utility.h"
#include "common/output.h"
#include "fsed/fsed.h"
#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "sdk/names.h"
#include <string>

using std::string;
using wwiv::core::ScopeExit;
using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::strings;

/////////////////////////////////////////////////////////////////////////////
// Actually launch the editor. This won't create any control files, etc.
static bool external_edit_internal(const string& edit_filename, const std::filesystem::path& working_directory,
                                   const editorrec& editor, int numlines) {

  string editorCommand = a()->sess().incom() ? editor.filename : editor.filenamecon;
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

  make_abs_cmd(a()->bbspath(), &editorCommand);

  const auto stripped_file_name{stripfn(edit_filename)};
  ScopeExit on_exit([=] { File::set_current_directory(a()->bbspath()); });
  if (!working_directory.empty()) {
    File::set_current_directory(working_directory);
  }

  const auto tft_fn = FilePath(File::current_directory(), stripped_file_name);
  const auto orig_file_time = File::Exists(tft_fn) ? File::last_write_time(tft_fn) : 0;

  const auto num_screen_lines = a()->user()->GetScreenLines();
  const auto cmdLine = stuff_in(editorCommand, tft_fn.string(),
                                std::to_string(a()->user()->GetScreenChars()),
                                std::to_string(num_screen_lines), std::to_string(numlines), "");

  ExecuteExternalProgram(cmdLine, ansir_to_flags(editor.ansir) | EFLAG_NO_CHANGE_DIR);
  bout.clear_lines_listed();

  const auto file_time_now = File::Exists(tft_fn) ? File::last_write_time(tft_fn) : 0;
  return File::Exists(tft_fn) && orig_file_time != file_time_now;
}

static std::unique_ptr<ExternalMessageEditor>
CreateExternalMessageEditor(const editorrec& editor, MessageEditorData& data, int maxli,
                            int* setanon, const std::filesystem::path& temp_directory) {
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

bool external_text_edit(const string& edit_filename, const std::filesystem::path& working_directory,
                        int numlines, int flags) {
  bout.nl();
  const auto editor_number = a()->user()->GetDefaultEditor() - 1;
  if (editor_number >= wwiv::stl::ssize(a()->editors) || !okansi()) {
    bout << "You can't use that full screen editor. (ete1)" << wwiv::endl << wwiv::endl;
    bout.pausescr();
    return false;
  }

  const auto& editor = a()->editors[editor_number];
  MessageEditorData data(a()->user()->name_and_number());
  data.msged_flags = flags;
  auto setanon = 0;
  if (auto eme = CreateExternalMessageEditor(editor, data, numlines, &setanon,
                                             a()->sess().dirs().temp_directory()); !eme->Before()) {
    return false;
  }
  
  return external_edit_internal(edit_filename, working_directory, editor, numlines);
}

bool fsed_text_edit(const std::string& edit_filename, const std::filesystem::path& new_directory,
                    int numlines, int flags) {
  if (ok_external_fsed()) {
    return external_text_edit(edit_filename, new_directory, numlines, flags);
  }
  std::filesystem::path p{edit_filename};
  const auto dir = p.parent_path();
  return wwiv::fsed::fsed(a()->context(), p);
}
