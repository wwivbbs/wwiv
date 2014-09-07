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

#include "core/strings.h"
#include "core/wtextfile.h"

using std::string;
using wwiv::strings::StringPrintf;

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

bool ExternalMessageEditor(int maxli, int *setanon, string *title, const string destination, int flags) {
  RemoveEditorFileFromTemp(FEDIT_INF);
  RemoveEditorFileFromTemp(RESULT_ED);

  fedit_data_rec fedit_data;
  memset(&fedit_data, '\0', sizeof(fedit_data_rec));
  fedit_data.tlen = 60;
  strcpy(fedit_data.ttl, title->c_str());
  fedit_data.anon = 0;

  WFile fileFEditInf(syscfgovr.tempdir, FEDIT_INF);
  if (fileFEditInf.Open(WFile::modeDefault | WFile::modeCreateFile | WFile::modeTruncate, WFile::shareDenyRead,
                        WFile::permReadWrite)) {
    fileFEditInf.Write(&fedit_data, sizeof(fedit_data));
    fileFEditInf.Close();
  }
  bool bSaveMessage = external_edit(INPUT_MSG, syscfgovr.tempdir, GetSession()->GetCurrentUser()->GetDefaultEditor() - 1,
                                    maxli, destination, *title, flags);
  if (bSaveMessage) {
    if (WFile::Exists(syscfgovr.tempdir, RESULT_ED)) {
      WTextFile file(syscfgovr.tempdir, RESULT_ED, "rt");
      string anon_string;
      if (file.ReadLine(&anon_string)) {
        *setanon = atoi(anon_string.c_str());
        if (file.ReadLine(title)) {
          // Strip whitespace from title to avoid issues like bug #29
          StringTrim(title);
        }
      }
      file.Close();
    } else if (WFile::Exists(fileFEditInf.GetFullPathName())) {
      WFile file(fileFEditInf.GetFullPathName());
      file.Open(WFile::modeBinary | WFile::modeReadOnly);
      if (file.Read(&fedit_data, sizeof(fedit_data))) {
        title->assign(fedit_data.ttl);
        *setanon = fedit_data.anon;
      }
      file.Close();
    }
  }

  RemoveEditorFileFromTemp(FEDIT_INF);
  RemoveEditorFileFromTemp(RESULT_ED);
  return bSaveMessage;
}

bool external_text_edit(const std::string& edit_filename, const std::string& new_directory, int numlines,
                        const std::string& destination, int flags) {
  const auto editor_number = GetSession()->GetCurrentUser()->GetDefaultEditor() - 1;
  return external_edit(edit_filename, new_directory, editor_number, numlines, destination, edit_filename, flags);
}

bool external_edit(const std::string& edit_filename, const std::string& new_directory, int nEditorNumber, int numlines,
                   const std::string& destination, const std::string& title, int flags) {
  if (nEditorNumber >= GetSession()->GetNumberOfEditors() || !okansi()) {
    GetSession()->bout << "\r\nYou can't use that full screen editor.\r\n\n";
    return false;
  }
  
  string editorCommand = (incom) ? editors[nEditorNumber].filename : editors[nEditorNumber].filenamecon;
  if (editorCommand.empty()) {
    GetSession()->bout << "\r\nYou can't use that full screen editor.\r\n\n";
    return false;
  }

  WWIV_make_abs_cmd(GetApplication()->GetHomeDir(), &editorCommand);
  const string current_directory = WWIV_GetCurrentDirectory(false);

  RemoveEditorFileFromTemp(EDITOR_INF);
  RemoveEditorFileFromTemp(RESULT_ED);

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
  const std::string cmdLine = stuff_in(editorCommand, full_filename, sx1, sx2, sx3, "");

  WTextFile fileEditorInf(EDITOR_INF, "wt");

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
    WFile::Remove(DISABLE_TAG);
  }
  if (!irt[0]) {
    WFile::Remove(QUOTES_TXT);
    WFile::Remove(QUOTES_IND);
  }
  ExecuteExternalProgram(cmdLine, GetApplication()->GetSpawnOptions(SPWANOPT_FSED));
  lines_listed = 0;
  chdir(new_directory.c_str());
  WFile::Remove(EDITOR_INF);
  WFile::Remove(DISABLE_TAG);
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
