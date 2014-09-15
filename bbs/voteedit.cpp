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

#include "wwiv.h"
#include "bbs/wstatus.h"

void print_quests();
void set_question(int ii);

void print_quests() {
  WFile file(syscfg.datadir, VOTING_DAT);
  if (!file.Open(WFile::modeBinary | WFile::modeReadOnly)) {
    return;
  }
  bool abort = false;
  for (int i = 1; (i <= 20) && !abort; i++) {
    file.Seek(static_cast<long>(i - 1) * sizeof(votingrec), WFile::seekBegin);

    votingrec v;
    file.Read(&v, sizeof(votingrec));
    char szBuffer[ 255 ];
    sprintf(szBuffer, "|#2%2d|#7) |#1%s", i, v.numanswers ? v.question : ">>> NO QUESTION <<<");
    pla(szBuffer, &abort);
  }
  GetSession()->bout.NewLine();
  if (abort) {
    GetSession()->bout.NewLine();
  }
}


void set_question(int ii) {
  votingrec v;
  voting_response vr;

  GetSession()->bout << "|#7Enter new question or just press [|#1Enter|#7] for none.\r\n: ";
  std::string question;
  inputl(&question, 75, true);
  strcpy(v.question, question.c_str());
  v.numanswers = 0;
  vr.numresponses = 0;
  vr.response[0] = 'X';
  vr.response[1] = 0;
  for (int i = 0; i < 20; i++) {
    v.responses[i] = vr;
  }
  if (question.empty()) {
    GetSession()->bout.NewLine();
    GetSession()->bout << "|#6Delete Question #" << ii + 1 << ", Are you sure? ";
    if (!yesno()) {
      return;
    }
  } else {
    GetSession()->bout.NewLine();
    GetSession()->bout << "|#5Enter answer choices, Enter a blank line when finished.";
    GetSession()->bout.NewLine(2);
    while (v.numanswers < 19) {
      GetSession()->bout << "|#2" << v.numanswers + 1 << "|#7: ";
      std::string response;
      inputl(&response, 63, true);
      strcpy(vr.response, response.c_str());
      vr.numresponses = 0;
      v.responses[v.numanswers] = vr;
      if (response.empty()) {
        // empty reponse means break.
        break;
      }
      ++v.numanswers;
    }
  }

  WFile votingDat(syscfg.datadir, VOTING_DAT);
  votingDat.Open(WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile);
  votingDat.Seek(ii * sizeof(votingrec), WFile::seekBegin);
  votingDat.Write(&v, sizeof(votingrec));
  votingDat.Close();

  questused[ii] = (v.numanswers) ? 1 : 0;

  WUser u;
  GetApplication()->GetUserManager()->ReadUser(&u, 1);
  int nNumUsers = GetApplication()->GetUserManager()->GetNumberOfUserRecords();
  for (int i1 = 1; i1 <= nNumUsers; i1++) {
    GetApplication()->GetUserManager()->ReadUser(&u, i1);
    u.SetVote(nNumUsers, 0);
    GetApplication()->GetUserManager()->WriteUser(&u, i1);
  }
}


void ivotes() {
  votingrec v;

  WFile votingDat(syscfg.datadir, VOTING_DAT);
  votingDat.Open(WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile);
  int n = static_cast<int>((votingDat.GetLength() / sizeof(votingrec)) - 1);
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
    GetSession()->bout.NewLine();
    GetSession()->bout << "|#2Which (Q=Quit) ? ";
    std::string questionNumber;
    input(&questionNumber, 2);
    if (questionNumber == "Q") {
      done = true;
    }
    int i = atoi(questionNumber.c_str());
    if (i > 0 && i < 21) {
      set_question(i - 1);
    }
  } while (!done && !hangup);
}


void voteprint() {
  votingrec v;

  int nNumUserRecords = GetApplication()->GetUserManager()->GetNumberOfUserRecords();
  char *x = static_cast<char *>(BbsAllocA(20 * (2 + nNumUserRecords)));
  if (x == NULL) {
    return;
  }
  for (int i = 0; i <= nNumUserRecords; i++) {
    WUser u;
    GetApplication()->GetUserManager()->ReadUser(&u, i);
    for (int i1 = 0; i1 < 20; i1++) {
      x[ i1 + i * 20 ] = static_cast<char>(u.GetVote(i1));
    }
  }
  WFile votingText(syscfg.gfilesdir, VOTING_TXT);
  votingText.Open(WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile | WFile::modeText);
  votingText.Write(votingText.GetFullPathName());

  WFile votingDat(syscfg.datadir, VOTING_DAT);

  GetApplication()->GetStatusManager()->RefreshStatusCache();

  for (int i1 = 0; i1 < 20; i1++) {
    votingDat.Open(WFile::modeReadOnly | WFile::modeBinary);
    votingDat.Seek(i1 * sizeof(votingrec), WFile::seekBegin);
    votingDat.Read(&v, sizeof(votingrec));
    votingDat.Close();
    if (v.numanswers) {
      GetSession()->bout << v.question;
      GetSession()->bout.NewLine();
      std::ostringstream text;
      text << "\r\n" << v.question << "\r\n";
      votingText.Write(text.str());
      for (int i2 = 0; i2 < v.numanswers; i2++) {
        text.clear();
        text.str("     ");
        text << v.responses[i2].response << "\r\n";
        votingText.Write(text.str());
        for (int i3 = 0; i3 < GetApplication()->GetStatusManager()->GetUserCount(); i3++) {
          if (x[i1 + 20 * smallist[i3].number] == i2 + 1) {
            text.clear();
            text.str("          ");
            text << smallist[i3].name << " #" << smallist[i3].number << "\r\n";
            votingText.Write(text.str());
          }
        }
      }
    }
  }
  votingText.Close();
  free(x);
}
