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
#include "bbs/external_edit_qbbs.h"

#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "common/message_editor_data.h"
#include "bbs/utility.h"
#include "core/scope_exit.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "local_io/keycodes.h"
#include "sdk/filenames.h"
#include "sdk/fido/fido_util.h"

#include <string>

using std::string;
using wwiv::core::ScopeExit;
using namespace wwiv::bbs;
using namespace wwiv::core;
using namespace wwiv::strings;

static void RemoveEditorFileFromTemp(const string& filename) {
  File::Remove(FilePath(a()->temp_directory(), filename), true);
}

std::string ExternalQBBSMessageEditor::editor_filename() const { return MSGTMP; }

ExternalQBBSMessageEditor ::~ExternalQBBSMessageEditor() {
  this->ExternalQBBSMessageEditor::CleanupControlFiles();
}

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
                        const string& to_name, bool real_name) {
  TextFile file(FilePath(a()->temp_directory(), MSGINF), "wd");
  if (!file.IsOpen()) {
    return false;
  }

  // line 1: Who the message is FROM
  file.WriteLine(real_name ? a()->user()->GetRealName() : a()->user()->GetName());
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

static bool CreateMsgTmpFromQuotesTxt(const std::string& tmpdir) {
  const auto qfn = FilePath(tmpdir, QUOTES_TXT);
  if (!File::Exists(qfn)) {
    return false;
  }
  // Copy quotes.txt to MSGTMP if it exists
  TextFile in(qfn, "rt");
  if (!in) {
    return false; 
  }
  File out(FilePath(a()->temp_directory(), MSGTMP));
  if (!out.Open(File::modeBinary | File::modeReadWrite | File::modeCreateFile |
                File::modeTruncate)) {
    return false;
  }
  const auto lines = in.ReadFileIntoVector();
  int num_to_skip = 2;
  for (const auto& l : lines) {
    if (!l.empty()) {
      if (l.front() == CD) {
        continue;
      }
      if (num_to_skip-- > 0) {
        continue;
      }
    }
    out.Writeln(stripcolors(l));
  }
  return true;
}

bool ExternalQBBSMessageEditor::Before() {
  CleanupControlFiles();
  CreateMsgTmpFromQuotesTxt(temp_directory_);
  bool real_name = data_.anonymous_flag & anony_real_name;
  return WriteMsgInf(data_.title, data_.sub_name, data_.is_email(), data_.to_name, real_name);
}

bool ExternalQBBSMessageEditor::After() {
  // Copy MSGTMP to INPUT_MSG since that's what the rest of WWIV expects.

  TextFile from(FilePath(temp_directory_, MSGTMP), "rt");
  if (!from) {
    return false;
  }
  const auto qbbs = from.ReadFileIntoString();
  const auto wwiv = wwiv::sdk::fido::FidoToWWIVText(qbbs, true);

  TextFile to(FilePath(temp_directory_, INPUT_MSG), "wt");
  if (!to) {
    return false;
  }

  return to.Write(wwiv) != 0;
}

