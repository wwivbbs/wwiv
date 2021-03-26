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
#include "bbs/voteedit.h"

#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/utility.h"
#include "common/input.h"
#include "common/output.h"
#include "core/file.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "sdk/filenames.h"
#include "sdk/names.h"
#include "sdk/status.h"
#include "sdk/user.h"
#include "sdk/usermanager.h"

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

static void print_quests() {
  File file(FilePath(a()->config()->datadir(), VOTING_DAT));
  if (!file.Open(File::modeBinary | File::modeReadOnly)) {
    return;
  }
  bool abort = false;
  for (int i = 1; (i <= 20) && !abort; i++) {
    file.Seek(static_cast<long>(i - 1) * sizeof(votingrec), File::Whence::begin);

    votingrec v{};
    file.Read(&v, sizeof(votingrec));
    auto buffer = fmt::sprintf("|#2%2d|#7) |#1%s", i, v.numanswers ? v.question : ">>> NO QUESTION <<<");
    bout.bpla(buffer, &abort);
  }
  bout.nl();
  if (abort) {
    bout.nl();
  }
}

static void set_question(int ii) {
  votingrec v{};
  voting_response vr{};

  bout << "|#7Enter new question or just press [|#1Enter|#7] for none.\r\n: ";
  const auto question = bin.input_text(75);
  to_char_array(v.question, question);
  v.numanswers = 0;
  vr.numresponses = 0;
  vr.response[0] = 'X';
  vr.response[1] = 0;
  for (auto& response : v.responses) {
    response = vr;
  }
  if (question.empty()) {
    bout.nl();
    bout << "|#6Delete Question #" << ii + 1 << ", Are you sure? ";
    if (!bin.yesno()) {
      return;
    }
  } else {
    bout.nl();
    bout << "|#5Enter answer choices, Enter a blank line when finished.";
    bout.nl(2);
    while (v.numanswers < 19) {
      bout << "|#2" << v.numanswers + 1 << "|#7: ";
      auto response = bin.input_text(63);
      to_char_array(vr.response, response);
      vr.numresponses = 0;
      v.responses[v.numanswers] = vr;
      if (response.empty()) {
        // empty reponse means break.
        break;
      }
      ++v.numanswers;
    }
  }

  File file(FilePath(a()->config()->datadir(), VOTING_DAT));
  file.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile);
  file.Seek(ii * sizeof(votingrec), File::Whence::begin);
  file.Write(&v, sizeof(votingrec));
  file.Close();

  const int num_users = a()->users()->num_user_records();
  for (int user_number = 1; user_number <= num_users; user_number++) {
    if (auto u = a()->users()->readuser(user_number)) {
      u->votes(ii, 0);
      a()->users()->writeuser(u, user_number);
    }
  }
}


void ivotes() {
  votingrec v{};

  File votingDat(FilePath(a()->config()->datadir(), VOTING_DAT));
  votingDat.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile);
  int n = static_cast<int>((votingDat.length() / sizeof(votingrec)) - 1);
  if (n < 20) {
    v.question[0] = '\0';
    v.numanswers = 0;
    for (int i = n; i < 20; i++) {
      votingDat.Write(&v, sizeof(votingrec));
    }
  }
  votingDat.Close();
  bool done = false;
  do {
    print_quests();
    bout.nl();
    bout << "|#2Which (Q=Quit) ? ";
    std::string questionum = bin.input(2);
    if (questionum == "Q") {
      done = true;
    }
    int i = to_number<int>(questionum);
    if (i > 0 && i < 21) {
      set_question(i - 1);
    }
  } while (!done && !a()->sess().hangup());
}


void voteprint() {
  const int num_user_records = a()->users()->num_user_records();
  auto* x = static_cast<char *>(BbsAllocA(20 * (2 + num_user_records)));
  if (x == nullptr) {
    return;
  }
  for (int i = 0; i <= num_user_records; i++) {
    User u;
    a()->users()->readuser(&u, i);
    for (int i1 = 0; i1 < 20; i1++) {
      x[ i1 + i * 20 ] = static_cast<char>(u.votes(i1));
    }
  }
  File votingText(FilePath(a()->config()->gfilesdir(), VOTING_TXT));
  votingText.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile | File::modeText);
  votingText.Write(votingText.full_pathname());

  File votingDat(FilePath(a()->config()->datadir(), VOTING_DAT));
  a()->status_manager()->reload_status();

  for (int i1 = 0; i1 < 20; i1++) {
    if (!votingDat.Open(File::modeReadOnly | File::modeBinary)) {
      continue;
    }
    votingrec v{};
    votingDat.Seek(i1 * sizeof(votingrec), File::Whence::begin);
    votingDat.Read(&v, sizeof(votingrec));
    votingDat.Close();
    if (v.numanswers) {
      bout << v.question;
      bout.nl();
      std::ostringstream text;
      text << "\r\n" << v.question << "\r\n";
      votingText.Write(text.str());
      for (int i2 = 0; i2 < v.numanswers; i2++) {
        text.clear();
        text.str("     ");
        text << v.responses[i2].response << "\r\n";
        votingText.Write(text.str());
        for (const auto& n : a()->names()->names_vector()) {
          if (x[i1 + 20 * n.number] == i2 + 1) {
            text.clear();
            text.str("          ");
            text << n.name << " #" << n.number << "\r\n";
            votingText.Write(text.str());
          }
        }
      }
    }
  }
  votingText.Close();
  free(x);
}
