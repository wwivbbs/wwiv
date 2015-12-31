/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2015, WWIV Software Services             */
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

#include <string>

#include "bbs/bbs.h"
#include "bbs/email.h"
#include "bbs/fcns.h"
#include "bbs/vars.h"
#include "bbs/inmsg.h"
#include "bbs/input.h"
#include "bbs/message_file.h"
#include "core/wfndfile.h"
#include "core/strings.h"
#include "bbs/printfile.h"
#include "bbs/wconstants.h"
#include "bbs/wstatus.h"
#include "sdk/filenames.h"
#include "sdk/user.h"

// local function prototypes
void add_list(int *pnUserNumber, int *numu, int maxu, int allowdup);
int  oneuser();

using std::string;
using std::unique_ptr;
using namespace wwiv::sdk;
using namespace wwiv::strings;

void multimail(int *pnUserNumber, int numu) {
  mailrec m, m1;
  char s[255], s2[81];
  User user;
  memset(&m, 0, sizeof(mailrec));

  if (freek1(syscfg.msgsdir) < 10) {
    bout.nl();
    bout << "Sorry, not enough disk space left.\r\n\n";
    return;
  }
  bout.nl();

  MessageEditorData data;
  data.need_title = true;
  if (getslrec(session()->GetEffectiveSl()).ability & ability_email_anony) {
    data.anonymous_flag = anony_enable_anony;
  }
  bout << "|#5Show all recipients in mail? ";
  bool show_all = yesno();
  int j = 0;
  string s1 = StringPrintf("\003""6CC: \003""1");

  m.msg.storage_type = EMAIL_STORAGE;
  strcpy(irt, "Multi-Mail");
  irt_name[0] = 0;
  File::Remove(QUOTES_TXT);
  data.aux = "email";
  data.fsed_flags = INMSG_NOFSED;
  data.to_name = "Multi-Mail";
  data.msged_flags = MSGED_FLAG_NONE;
  if (!inmsg(data)) {
    return;
  }
  savefile(data.text, &m.msg, data.aux);
  strcpy(m.title, data.title.c_str());

  bout <<  "Mail sent to:\r\n";
  sysoplog("Multi-Mail to:");

  lineadd(&m.msg, "\003""7----", "email");

  for (int cv = 0; cv < numu; cv++) {
    if (pnUserNumber[cv] < 0) {
      continue;
    }
    session()->users()->ReadUser(&user, pnUserNumber[cv]);
    if ((user.GetSl() == 255 && (user.GetNumMailWaiting() > (syscfg.maxwaiting * 5))) ||
        ((user.GetSl() != 255) && (user.GetNumMailWaiting() > syscfg.maxwaiting)) ||
        user.GetNumMailWaiting() > 200) {
      bout << session()->names()->UserName(pnUserNumber[cv]) << " mailbox full, not sent.";
      pnUserNumber[cv] = -1;
      continue;
    }
    if (user.IsUserDeleted()) {
      bout << "User deleted, not sent.\r\n";
      pnUserNumber[cv] = -1;
      continue;
    }
    strcpy(s, "  ");
    user.SetNumMailWaiting(user.GetNumMailWaiting() + 1);
    session()->users()->WriteUser(&user, pnUserNumber[cv]);
    if (pnUserNumber[cv] == 1) {
      ++fwaiting;
    }
    const string pnunn = session()->names()->UserName(pnUserNumber[cv]);
    strcat(s, pnunn.c_str());
    WStatus* pStatus = session()->status_manager()->BeginTransaction();
    if (pnUserNumber[cv] == 1) {
      pStatus->IncrementNumFeedbackSentToday();
      session()->user()->SetNumFeedbackSentToday(session()->user()->GetNumFeedbackSentToday() + 1);
      session()->user()->SetNumFeedbackSent(session()->user()->GetNumFeedbackSent() + 1);
      ++fsenttoday;
    } else {
      pStatus->IncrementNumEmailSentToday();
      session()->user()->SetNumEmailSent(session()->user()->GetNumEmailSent() + 1);
      session()->user()->SetNumEmailSentToday(session()->user()->GetNumEmailSentToday() + 1);
    }
    session()->status_manager()->CommitTransaction(pStatus);
    sysoplog(s);
    bout << s;
    bout.nl();
    if (show_all) {
      const string pnunn2 = session()->names()->UserName(pnUserNumber[cv]);
      sprintf(s2, "%-22.22s  ", pnunn2.c_str());
      s1.assign(s2);
      j++;
      if (j >= 3) {
        lineadd(&m.msg, s1, "email");
        j = 0;
        s1 = "\003""1    ";
      }
    }
  }
  if (show_all) {
    if (j) {
      lineadd(&m.msg, s1, "email");
    }
  }
  s1 = StringPrintf("\003""2Mail Sent to %d Addresses!", numu);
  lineadd(&m.msg, "\003""7----", "email");
  lineadd(&m.msg, s1, "email");

  m.anony = static_cast<unsigned char>(data.anonymous_flag);
  m.fromsys = 0;
  m.fromuser = static_cast<unsigned short>(session()->usernum);
  m.tosys = 0;
  m.touser = 0;
  m.status = status_multimail;
  m.daten = static_cast<uint32_t>(time(nullptr));

  unique_ptr<File> pFileEmail(OpenEmailFile(true));
  int len = pFileEmail->GetLength() / sizeof(mailrec);
  int i = 0;
  if (len != 0) {
    i = len - 1;
    pFileEmail->Seek(static_cast<long>(i) * sizeof(mailrec), File::seekBegin);
    pFileEmail->Read(&m1, sizeof(mailrec));
    while ((i > 0) && (m1.tosys == 0) && (m1.touser == 0)) {
      --i;
      pFileEmail->Seek(static_cast<long>(i) * sizeof(mailrec), File::seekBegin);
      int i1 = pFileEmail->Read(&m1, sizeof(mailrec));
      if (i1 == -1) {
        bout << "|#6DIDN'T READ WRITE!\r\n";
      }
    }
    if ((m1.tosys) || (m1.touser)) {
      ++i;
    }
  }
  pFileEmail->Seek(static_cast<long>(i) * sizeof(mailrec), File::seekBegin);
  for (int cv = 0; cv < numu; cv++) {
    if (pnUserNumber[cv] > 0) {
      m.touser = static_cast<unsigned short>(pnUserNumber[cv]);
      pFileEmail->Write(&m, sizeof(mailrec));
    }
  }
  pFileEmail->Close();
}

static char *mml_s;
static int mml_started;

int oneuser() {
  char s[81], *ss;
  int user_number, system_number, i;
  User user;

  if (mml_s) {
    if (mml_started) {
      ss = strtok(nullptr, "\r\n");
    } else {
      ss = strtok(mml_s, "\r\n");
    }
    mml_started = 1;
    if (ss == nullptr) {
      free(mml_s);
      mml_s = nullptr;
      return -1;
    }
    strcpy(s, ss);
    for (i = 0; s[i] != 0; i++) {
      s[i] = upcase(s[i]);
    }
  } else {
    bout << "|#2>";
    input(s, 40);
  }
  user_number = finduser1(s);
  if (user_number == 65535) {
    return -1;
  }
  if (s[0] == 0) {
    return -1;
  }
  if (user_number <= 0) {
    bout.nl();
    bout << "Unknown user.\r\n\n";
    return 0;
  }
  system_number = 0;
  if (ForwardMessage(&user_number, &system_number)) {
    bout.nl();
    bout << "Forwarded.\r\n\n";
    if (system_number) {
      bout << "Forwarded to another system.\r\n";
      bout << "Can't send multi-mail to another system.\r\n\n";
      return 0;
    }
  }
  if (user_number == 0) {
    bout.nl();
    bout << "Unknown user.\r\n\n";
    return 0;
  }
  session()->users()->ReadUser(&user, user_number);
  if (((user.GetSl() == 255) && (user.GetNumMailWaiting() > (syscfg.maxwaiting * 5))) ||
      ((user.GetSl() != 255) && (user.GetNumMailWaiting() > syscfg.maxwaiting)) ||
      (user.GetNumMailWaiting() > 200)) {
    bout.nl();
    bout << "Mailbox full.\r\n\n";
    return 0;
  }
  if (user.IsUserDeleted()) {
    bout.nl();
    bout << "Deleted user.\r\n\n";
    return 0;
  }
  bout << "     -> " << session()->names()->UserName(user_number) << wwiv::endl;
  return user_number;
}


void add_list(int *pnUserNumber, int *numu, int maxu, int allowdup) {
  bool done = false;
  int mml = (mml_s != nullptr);
  mml_started = 0;
  while (!done && !hangup && (*numu < maxu)) {
    int i = oneuser();
    if (mml && (!mml_s)) {
      done = true;
    }
    if (i == -1) {
      done = true;
    } else if (i) {
      if (!allowdup) {
        for (int i1 = 0; i1 < *numu; i1++) {
          if (pnUserNumber[i1] == i) {
            bout.nl();
            bout << "Already in list, not added.\r\n\n";
            i = 0;
          }
          if (i) {
            pnUserNumber[(*numu)++] = i;
          }
        }
      }
    }
  }
  if (*numu == maxu) {
    bout.nl();
    bout << "List full.\r\n\n";
  }
}


#define MAX_LIST 40


void slash_e() {
  int user_number[MAX_LIST], numu, i, i1;
  char s[81], ch, *sss;
  bool bFound = false;
  WFindFile fnd;

  mml_s = nullptr;
  mml_started = 0;
  if (freek1(syscfg.msgsdir) < 10) {
    bout.nl();
    bout << "Sorry, not enough disk space left.\r\n\n";
    return;
  }
  if (((fsenttoday >= 5) || (session()->user()->GetNumFeedbackSentToday() >= 10) ||
       (session()->user()->GetNumEmailSentToday() >= getslrec(session()->GetEffectiveSl()).emails))
      && (!cs())) {
    bout << "Too much mail sent today.\r\n\n";
    return;
  }
  if (session()->user()->IsRestrictionEmail()) {
    bout << "You can't send mail.\r\n";
    return;
  }
  bool done = false;
  numu = 0;
  do {
    bout.nl(2);
    bout << "|#2Multi-Mail: A,M,D,L,E,Q,? : ";
    ch = onek("QAMDEL?");
    switch (ch) {
    case '?':
      printfile(MMAIL_NOEXT);
      break;
    case 'Q':
      done = true;
      break;
    case 'A':
      bout.nl();
      bout << "Enter names/numbers for users, one per line, max 20.\r\n\n";
      mml_s = nullptr;
      add_list(user_number, &numu, MAX_LIST, so());
      break;
    case 'M': {
      sprintf(s, "%s*.MML", syscfg.datadir);
      bFound = fnd.open(s, 0);
      if (bFound) {
        bout.nl();
        bout << "No mailing lists available.\r\n\n";
        break;
      }
      bout.nl();
      bout << "Available mailing lists:\r\n\n";
      while (bFound) {
        strcpy(s, fnd.GetFileName());
        sss = strchr(s, '.');
        if (sss) {
          *sss = 0;
        }
        bout << s;
        bout.nl();

        bFound = fnd.next();
      }

      bout.nl();
      bout << "|#2Which? ";
      input(s, 8);

      File fileMailList(syscfg.datadir, s);
      if (!fileMailList.Open(File::modeBinary | File::modeReadOnly)) {
        bout.nl();
        bout << "Unknown mailing list.\r\n\n";
      } else {
        i1 = fileMailList.GetLength();
        mml_s = static_cast<char *>(BbsAllocA(i1 + 10L));
        fileMailList.Read(mml_s, i1);
        mml_s[i1] = '\n';
        mml_s[i1 + 1] = 0;
        fileMailList.Close();
        mml_started = 0;
        add_list(user_number, &numu, MAX_LIST, so());
        if (mml_s) {
          free(mml_s);
          mml_s = nullptr;
        }
      }
    }
    break;
    case 'E':
      if (!numu) {
        bout.nl();
        bout << "Need to specify some users first - use A or M\r\n\n";
      } else {
        multimail(user_number, numu);
        done = true;
      }
      break;
    case 'D':
      if (numu) {
        bout.nl();
        bout << "|#2Delete which? ";
        input(s, 2);
        i = atoi(s);
        if ((i > 0) && (i <= numu)) {
          --numu;
          for (i1 = i - 1; i1 < numu; i1++) {
            user_number[i1] = user_number[i1 + 1];
          }
        }
      }
      break;
    case 'L':
      for (i = 0; i < numu; i++) {
        User user;
        session()->users()->ReadUser(&user, user_number[i]);
        bout << i + 1 << ". " << session()->names()->UserName(user_number[i]) << wwiv::endl;
      }
      break;
    }
    CheckForHangup();
  } while (!done && !hangup);
}
