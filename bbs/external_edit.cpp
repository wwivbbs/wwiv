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
#include "bbs/execexternal.h"
#include "bbs/bbsutl.h"
#include "bbs/utility.h"
#include "bbs/vars.h"
#include "bbs/pause.h"
#include "bbs/platform/platformfcns.h"
#include "core/scope_exit.h"
#include "core/strings.h"
#include "core/textfile.h"

#include "bbs/stuffin.h"
#include "bbs/wconstants.h"
#include "sdk/filenames.h"

using std::string;
using wwiv::core::ScopeExit;
using namespace wwiv::strings;

// Local prototypes.
bool external_edit_internal(const string& edit_filename, const string& new_directory, const editorrec& editor, int numlines);

static void RemoveEditorFileFromTemp(const string& filename) {
  File file(a()->temp_directory(), filename);
  file.SetFilePermissions(File::permReadWrite);
  file.Delete();
}

static void RemoveWWIVControlFiles() {
  RemoveEditorFileFromTemp(FEDIT_INF);
  RemoveEditorFileFromTemp(RESULT_ED);
  RemoveEditorFileFromTemp(EDITOR_INF);
}

static void RemoveQBBSControlFiles() {
  RemoveEditorFileFromTemp(MSGINF);
  RemoveEditorFileFromTemp(MSGTMP);
}

static void RemoveControlFiles(const editorrec& editor) {
  if (editor.bbs_type == EDITORREC_EDITOR_TYPE_QBBS) {
    RemoveQBBSControlFiles();
  } else {
    RemoveWWIVControlFiles();
  }
}

static void ReadWWIVResultFiles(string* title, int* anon) {
  if (File::Exists(a()->temp_directory(), RESULT_ED)) {
    TextFile file(a()->temp_directory(), RESULT_ED, "rt");
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
    File file(a()->temp_directory(), FEDIT_INF);
    file.Open(File::modeBinary | File::modeReadOnly);
      if (file.Read(&fedit_data, sizeof(fedit_data))) {
        title->assign(fedit_data.ttl);
        *anon = fedit_data.anon;
      }

  }
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
static bool WriteMsgInf(const string& title, const string& sub_name, bool is_email, const string& to_name) {
  TextFile file(a()->temp_directory(), MSGINF, "wt");
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

static void WriteWWIVEditorControlFiles(const string& title, const string& sub_name, const string& to_name, int flags) {
  TextFile fileEditorInf(a()->temp_directory(), EDITOR_INF, "wt");
  if (fileEditorInf.IsOpen()) {
    if (!to_name.empty()) {
      flags |= MSGED_FLAG_HAS_REPLY_NAME;
    }
    if (!title.empty()) {
      flags |= MSGED_FLAG_HAS_REPLY_TITLE;
    }
    fileEditorInf.WriteFormatted(
      "%s\n%s\n%lu\n%s\n%s\n%u\n%u\n%lu\n%u\n",
      title.c_str(),
      sub_name.c_str(),
      a()->usernum,
      a()->user()->GetName(),
      a()->user()->GetRealName(),
      a()->user()->GetSl(),
      flags,
      a()->localIO()->GetTopLine(),
      a()->user()->GetLanguage());
    fileEditorInf.Close();
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
  fedit_data_rec fedit_data;
  memset(&fedit_data, '\0', sizeof(fedit_data_rec));
  fedit_data.tlen = 60;
  to_char_array(fedit_data.ttl, title);
  fedit_data.anon = 0;

  File fileFEditInf(a()->temp_directory(), FEDIT_INF);
  if (fileFEditInf.Open(File::modeDefault | File::modeCreateFile | File::modeTruncate, File::shareDenyReadWrite)) {
    fileFEditInf.Write(&fedit_data, sizeof(fedit_data));
    fileFEditInf.Close();
  }
}

static bool WriteExternalEditorControlFiles(const editorrec& editor, const string& title, const string& sub_name, int flags, bool is_email, const string& to_name) {
  if (editor.bbs_type == EDITORREC_EDITOR_TYPE_QBBS) {
    if (File::Exists(a()->temp_directory(), QUOTES_TXT)) {
      // Copy quotes.txt to MSGTMP if it exists
      File source(a()->temp_directory(), QUOTES_TXT);
      File dest(a()->temp_directory(), MSGTMP);
      File::Copy(source.full_pathname(), dest.full_pathname());
    }
    return WriteMsgInf(title, sub_name, is_email, to_name);
  } 

  WriteWWIVEditorControlFiles(title, sub_name, to_name, flags);
  return true;
}

bool ExternalMessageEditor(int maxli, int *setanon, string *title, const string& to_name, const std::string& sub_name, int flags, bool is_email) {
  const size_t editor_number = a()->user()->GetDefaultEditor() - 1;
  if (editor_number >= a()->editors.size() || !okansi()) {
    bout << "\r\nYou can't use that full screen editor (EME).\r\n\n";
    return false;
  }

  const editorrec& editor = a()->editors[editor_number];
  RemoveControlFiles(editor);
  ScopeExit on_exit([=] { RemoveControlFiles(editor); });

  const string editor_filenme = (editor.bbs_type == EDITORREC_EDITOR_TYPE_QBBS) ? MSGTMP : INPUT_MSG;

  WriteExternalEditorControlFiles(editor, *title, sub_name, flags, is_email, to_name);
  bool save_message = external_edit_internal(editor_filenme, a()->temp_directory(), editor, maxli);

  if (!save_message) {
    return false;
  }

  if (editor.bbs_type == EDITORREC_EDITOR_TYPE_QBBS) {
    // Copy MSGTMP to INPUT_MSG since that's what the rest of WWIV expectes.
    // TODO(rushfan): Let this function return an object with result and filename and anything
    // else that needs to be passed back.
    File source(a()->temp_directory(), MSGTMP);
    File dest(a()->temp_directory(), INPUT_MSG);
    File::Copy(source.full_pathname(), dest.full_pathname());

    // TODO(rushfan): Do we need to re-read MSGINF to look for changes to title or setanon?
  } else {
    ReadWWIVResultFiles(title, setanon);
  }
  return true;
}

bool external_text_edit(const string& edit_filename, const string& new_directory, int numlines,
                        int flags) {
  bout.nl();
  const auto editor_number = a()->user()->GetDefaultEditor() - 1;
  if (editor_number >= a()->editors.size() || !okansi()) {
    bout << "You can't use that full screen editor. (ete1)" << wwiv::endl << wwiv::endl;
    pausescr();
    return false;
  }

  RemoveWWIVControlFiles();
  const auto& editor = a()->editors[editor_number];
  WriteExternalEditorControlFiles(editor, edit_filename, "", flags, false, "" /* to_name */);
  auto result = external_edit_internal(edit_filename, new_directory, editor, numlines);
  RemoveWWIVControlFiles();
  return result;
}

// Actually launch the editor. This won't create any control files, etc.
bool external_edit_internal(const string& edit_filename, const string& new_directory, 
                            const editorrec& editor, int numlines) {
  
  string editorCommand = (incom) ? editor.filename : editor.filenamecon;
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

  WWIV_make_abs_cmd(a()->GetHomeDir(), &editorCommand);
  const auto original_directory = File::current_directory();

  string strippedFileName(stripfn(edit_filename.c_str()));
  if (!new_directory.empty()) {
    File::set_current_directory(new_directory);
  }

  time_t tFileTime = 0;
  File fileTempForTime(File::current_directory(), strippedFileName);
  auto bIsFileThere = fileTempForTime.Exists();
  if (bIsFileThere) {
    tFileTime = fileTempForTime.last_write_time();
  }

  int num_screen_lines = a()->user()->GetScreenLines();
  if (!a()->using_modem) {
    int newtl = (a()->screenlinest > a()->defscreenbottom - a()->localIO()->GetTopLine()) ? 0 :
                a()->localIO()->GetTopLine();
    num_screen_lines = a()->defscreenbottom + 1 - newtl;
  }

  const auto cmdLine = stuff_in(editorCommand, fileTempForTime.full_pathname(), 
    std::to_string(a()->user()->GetScreenChars()), 
    std::to_string(num_screen_lines), 
    std::to_string(numlines), "");

  // TODO(rushfan): Make this a common function shared between here and chains.
  int flags = 0;
  if (!(editor.ansir & ansir_no_DOS)) {
    flags |= EFLAG_COMIO;
  }
  if (editor.ansir & ansir_emulate_fossil) {
    flags |= EFLAG_FOSSIL;
  }
  if (editor.ansir & ansir_temp_dir) {
    flags |= EFLAG_TEMP_DIR;
  }
  if (editor.ansir & ansir_stdio) {
    flags |= EFLAG_STDIO;
  }
  
  ExecuteExternalProgram(cmdLine, flags);
  
  // After launched FSED
  bout.clear_lines_listed();
  File::set_current_directory(new_directory);

  auto bModifiedOrExists = false;
  const auto full_filename = fileTempForTime.full_pathname();
  if (!bIsFileThere) {
    bModifiedOrExists = File::Exists(full_filename);
  } else {
    File fileTempForTime2(full_filename);
    if (fileTempForTime2.Exists()) {
      time_t tFileTime1 = fileTempForTime2.last_write_time();
      if (tFileTime != tFileTime1) {
        bModifiedOrExists = true;
      }
    }
  }
  File::set_current_directory(original_directory);
  return bModifiedOrExists;
}
