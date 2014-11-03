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
#include "bbs/external_edit.h"

#include <algorithm>
#include <string>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif  // _WIN32

#include "wwiv.h"

#include "core/scope_exit.h"
#include "core/strings.h"
#include "core/wtextfile.h"

#include "bbs/stuffin.h"
#include "bbs/wconstants.h"


using std::string;
using wwiv::core::ScopeExit;
using wwiv::strings::StringPrintf;

// Local prototypes.
bool external_edit_internal(const string& edit_filename, const string& new_directory, const editorrec& editor, int numlines);

static string WWIV_GetCurrentDirectory(bool be) {
  char szDir[MAX_PATH];
  WWIV_GetDir(szDir, be);
  return string(szDir);
}

static void RemoveEditorFileFromTemp(const string& filename) {
  WFile file(syscfgovr.tempdir, filename);
  file.SetFilePermissions(WFile::permReadWrite);
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
  if (WFile::Exists(syscfgovr.tempdir, RESULT_ED)) {
    WTextFile file(syscfgovr.tempdir, RESULT_ED, "rt");
    string anon_string;
    if (file.ReadLine(&anon_string)) {
      *anon = atoi(anon_string.c_str());
      if (file.ReadLine(title)) {
        // Strip whitespace from title to avoid issues like bug #29
        StringTrim(title);
      }
    }
    file.Close();
  } else if (WFile::Exists(syscfgovr.tempdir, FEDIT_INF)) {
    fedit_data_rec fedit_data;
    memset(&fedit_data, '\0', sizeof(fedit_data_rec));
    WFile file(syscfgovr.tempdir, FEDIT_INF);
    file.Open(WFile::modeBinary | WFile::modeReadOnly);
      if (file.Read(&fedit_data, sizeof(fedit_data))) {
        title->assign(fedit_data.ttl);
        *anon = fedit_data.anon;
      }

  }
}

static bool WriteMsgInf(const string& title, const string& destination, const string& aux) {
  WTextFile file(syscfgovr.tempdir, MSGINF, "wt");
  if (!file.IsOpen()) {
    return false;
  }

  file.WriteLine(GetSession()->GetCurrentUser()->GetName());
  if (aux == "email") {
    // destination == to address for email
    file.WriteLine(destination);
  } else {
    if (strlen(irt_name) > 0) {
      file.WriteLine(irt_name);
    } else {
      // Since we don't know who this is to, make it all.
      file.WriteLine("All"); 
    }
  }
  file.WriteLine(title);
  file.WriteLine("0"); // Message area # - We are not QBBS
  if (aux == "email") {
    file.WriteLine("E-mail");
    // Is the message private [YES|NO]
    file.WriteLine("YES");
  } else {
    file.WriteLine(destination);
    // Is the message private [YES|NO]
    file.WriteLine("NO");
  }
  file.Close();
  return true;
}

static void WriteWWIVEditorControlFiles(const string& title, const string& destination, int flags) {
  WTextFile fileEditorInf(syscfgovr.tempdir, EDITOR_INF, "wt");
  if (fileEditorInf.IsOpen()) {
    if (irt_name[0]) {
      flags |= MSGED_FLAG_HAS_REPLY_NAME;
    }
    if (irt[0]) {
      flags |= MSGED_FLAG_HAS_REPLY_TITLE;
    }
    fileEditorInf.WriteFormatted(
      "%s\n%s\n%lu\n%s\n%s\n%u\n%u\n%lu\n%u\n",
      title.c_str(),
      destination.c_str(),
      GetSession()->usernum,
      GetSession()->GetCurrentUser()->GetName(),
      GetSession()->GetCurrentUser()->GetRealName(),
      GetSession()->GetCurrentUser()->GetSl(),
      flags,
      GetSession()->localIO()->GetTopLine(),
      GetSession()->GetCurrentUser()->GetLanguage());
    fileEditorInf.Close();
  }
  if (flags & MSGED_FLAG_NO_TAGLINE) {
    // disable tag lines by creating a DISABLE.TAG file
    WTextFile fileDisableTag(DISABLE_TAG, "w");
  } else {
    RemoveEditorFileFromTemp(DISABLE_TAG);
  }
  if (!irt[0]) {
    RemoveEditorFileFromTemp(QUOTES_TXT);
    RemoveEditorFileFromTemp(QUOTES_IND);
  }

  // Write FEDIT.INF
  fedit_data_rec fedit_data;
  memset(&fedit_data, '\0', sizeof(fedit_data_rec));
  fedit_data.tlen = 60;
  strcpy(fedit_data.ttl, title.c_str());
  fedit_data.anon = 0;

  WFile fileFEditInf(syscfgovr.tempdir, FEDIT_INF);
  if (fileFEditInf.Open(WFile::modeDefault | WFile::modeCreateFile | WFile::modeTruncate, WFile::shareDenyRead)) {
    fileFEditInf.Write(&fedit_data, sizeof(fedit_data));
    fileFEditInf.Close();
  }
}

bool WriteExternalEditorControlFiles(const editorrec& editor, const string& title, const string& destination, int flags, const string& aux) {
  if (editor.bbs_type == EDITORREC_EDITOR_TYPE_QBBS) {
    if (WFile::Exists(syscfgovr.tempdir, QUOTES_TXT)) {
      // Copy quotes.txt to MSGTMP if it exists
      WFile source(syscfgovr.tempdir, QUOTES_TXT);
      WFile dest(syscfgovr.tempdir, MSGTMP);
      WFile::CopyFile(source.GetFullPathName(), dest.GetFullPathName());
    }
    return WriteMsgInf(title, destination, aux);
  } 

  WriteWWIVEditorControlFiles(title, destination, flags);
  return true;
}

bool ExternalMessageEditor(int maxli, int *setanon, string *title, const string& destination, int flags, const string& aux) {
  const auto editor_number = GetSession()->GetCurrentUser()->GetDefaultEditor() - 1;
  if (editor_number >= GetSession()->GetNumberOfEditors() || !okansi()) {
    bout << "\r\nYou can't use that full screen editor.\r\n\n";
    return false;
  }

  const editorrec& editor = editors[editor_number];
  RemoveControlFiles(editor);
  ScopeExit on_exit([=] { RemoveControlFiles(editor); });

  const string editor_filenme = (editor.bbs_type == EDITORREC_EDITOR_TYPE_QBBS) ? MSGTMP : INPUT_MSG;

  WriteExternalEditorControlFiles(editor, *title, destination, flags, aux);
  bool save_message = external_edit_internal(editor_filenme, syscfgovr.tempdir, editor, maxli);

  if (!save_message) {
    return false;
  }

  if (editor.bbs_type == EDITORREC_EDITOR_TYPE_QBBS) {
    // Copy MSGTMP to INPUT_MSG since that's what the rest of WWIV expectes.
    // TODO(rushfan): Let this function return an object with result and filename and anything
    // else that needs to be passed back.
    WFile source(syscfgovr.tempdir, MSGTMP);
    WFile dest(syscfgovr.tempdir, INPUT_MSG);
    WFile::CopyFile(source.GetFullPathName(), dest.GetFullPathName());

    // TODO(rushfan): Do we need to re-read MSGINF to look for changes to title or setanon?
  } else {
    ReadWWIVResultFiles(title, setanon);
  }
  return true;
}

bool external_text_edit(const string& edit_filename, const string& new_directory, int numlines,
                        const string& destination, int flags) {
  const auto editor_number = GetSession()->GetCurrentUser()->GetDefaultEditor() - 1;
  if (editor_number >= GetSession()->GetNumberOfEditors() || !okansi()) {
    bout << "\r\nYou can't use that full screen editor.\r\n\n";
    return false;
  }

  RemoveWWIVControlFiles();
  const editorrec& editor = editors[editor_number];
  WriteExternalEditorControlFiles(editor, edit_filename, destination, flags, "");
  bool result = external_edit_internal(edit_filename, new_directory, editor, numlines);
  RemoveWWIVControlFiles();
  return result;
}

// Actually launch the editor. This won't create any control files, etc.
bool external_edit_internal(const string& edit_filename, const string& new_directory, 
                            const editorrec& editor, int numlines) {
  
  string editorCommand = (incom) ? editor.filename : editor.filenamecon;
  if (editorCommand.empty()) {
    bout << "\r\nYou can't use that full screen editor.\r\n\n";
    return false;
  }

  WWIV_make_abs_cmd(GetApplication()->GetHomeDir(), &editorCommand);
  const string current_directory = WWIV_GetCurrentDirectory(false);

  string strippedFileName(stripfn(edit_filename.c_str()));
  string full_filename;
  if (!new_directory.empty()) {
    chdir(new_directory.c_str()) ;
    full_filename = WWIV_GetCurrentDirectory(true);
  }
  full_filename += strippedFileName;

  time_t tFileTime = 0;
  WFile fileTempForTime(full_filename);
  bool bIsFileThere = fileTempForTime.Exists();
  if (bIsFileThere) {
    tFileTime = fileTempForTime.GetFileTime();
  }

  const string sx1 = StringPrintf("%d", GetSession()->GetCurrentUser()->GetScreenChars());
  int num_screen_lines = GetSession()->GetCurrentUser()->GetScreenLines();
  if (!GetSession()->using_modem) {
    int newtl = (GetSession()->screenlinest > defscreenbottom - GetSession()->localIO()->GetTopLine()) ? 0 :
                GetSession()->localIO()->GetTopLine();
    num_screen_lines = defscreenbottom + 1 - newtl;
  }
  const string sx2 = StringPrintf("%d", num_screen_lines);
  const string sx3 = StringPrintf("%d", numlines);
  const string cmdLine = stuff_in(editorCommand, full_filename, sx1, sx2, sx3, "");

  // TODO(rushfan): Make this a common function shared between here and chains.
  int flags = 0;
  if (!(editor.ansir & ansir_no_DOS)) {
    flags |= EFLAG_COMIO;
  }
  if (editor.ansir & ansir_emulate_fossil) {
    flags |= EFLAG_FOSSIL;
  }

  ExecuteExternalProgram(cmdLine, flags);
  
  // After launched FSED
  lines_listed = 0;
  chdir(new_directory.c_str());

  bool bModifiedOrExists = false;
  if (!bIsFileThere) {
    bModifiedOrExists = WFile::Exists(full_filename);
  } else {
    WFile fileTempForTime(full_filename);
    if (fileTempForTime.Exists()) {
      time_t tFileTime1 = fileTempForTime.GetFileTime();
      if (tFileTime != tFileTime1) {
        bModifiedOrExists = true;
      }
    }
  }
  chdir(current_directory.c_str());
  return bModifiedOrExists;
}
