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
#include "core/wfndfile.h"
#include "core/strings.h"
#include "bbs/printfile.h"
#include "bbs/wconstants.h"
#include "bbs/wstatus.h"

// local function prototypes
void add_list(int *pnUserNumber, int *numu, int maxu, int allowdup);
int  oneuser();

#define EMAIL_STORAGE 2

using std::string;
using std::unique_ptr;
using wwiv::strings::StringPrintf;

void multimail(int *pnUserNumber, int numu) {
  mailrec m, m1;
  char s[255], s2[81];
  WUser user;

  if (freek1(syscfg.msgsdir) < 10) {
    bout.nl();
    bout << "Sorry, not enough disk space left.\r\n\n";
    return;
  }
  bout.nl();

  int i = 0;
  if (getslrec(GetSession()->GetEffectiveSl()).ability & ability_email_anony) {
    i = anony_enable_anony;
  }
  bout << "|#5Show all recipients in mail? ";
  bool show_all = yesno();
  int j = 0;
  string s1 = StringPrintf("\003""6CC: \003""1");

  m.msg.storage_type = EMAIL_STORAGE;
  strcpy(irt, "Multi-Mail");
  irt_name[0] = 0;
  File::Remove(QUOTES_TXT);
  std::string t;
  inmsg(&m.msg, &t, &i, true, "email", INMSG_FSED, "Multi-Mail", MSGED_FLAG_NONE);
  if (m.msg.stored_as == 0xffffffff) {
    return;
  }
  strcpy(m.title, t.c_str());

  bout <<  "Mail sent to:\r\n";
  sysoplog("Multi-Mail to:");

  lineadd(&m.msg, "\003""7----", "email");

  for (int cv = 0; cv < numu; cv++) {
    if (pnUserNumber[cv] < 0) {
      continue;
    }
    GetApplication()->GetUserManager()->ReadUser(&user, pnUserNumber[cv]);
    if ((user.GetSl() == 255 && (user.GetNumMailWaiting() > (syscfg.maxwaiting * 5))) ||
        ((user.GetSl() != 255) && (user.GetNumMailWaiting() > syscfg.maxwaiting)) ||
        user.GetNumMailWaiting() > 200) {
      bout << user.GetUserNameAndNumber(pnUserNumber[cv]) << " mailbox full, not sent.";
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
    GetApplication()->GetUserManager()->WriteUser(&user, pnUserNumber[cv]);
    if (pnUserNumber[cv] == 1) {
      ++fwaiting;
    }
    strcat(s, user.GetUserNameAndNumber(pnUserNumber[cv]));
    WStatus* pStatus = GetApplication()->GetStatusManager()->BeginTransaction();
    if (pnUserNumber[cv] == 1) {
      pStatus->IncrementNumFeedbackSentToday();
      GetSession()->GetCurrentUser()->SetNumFeedbackSentToday(GetSession()->GetCurrentUser()->GetNumFeedbackSentToday() + 1);
      GetSession()->GetCurrentUser()->SetNumFeedbackSent(GetSession()->GetCurrentUser()->GetNumFeedbackSent() + 1);
      ++fsenttoday;
    } else {
      pStatus->IncrementNumEmailSentToday();
      GetSession()->GetCurrentUser()->SetNumEmailSent(GetSession()->GetCurrentUser()->GetNumEmailSent() + 1);
      GetSession()->GetCurrentUser()->SetNumEmailSentToday(GetSession()->GetCurrentUser()->GetNumEmailSentToday() + 1);
    }
    GetApplication()->GetStatusManager()->CommitTransaction(pStatus);
    sysoplog(s);
    bout << s;
    bout.nl();
    if (show_all) {
      sprintf(s2, "%-22.22s  ", user.GetUserNameAndNumber(pnUserNumber[cv]));
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

  m.anony = static_cast< unsigned char >(i);
  m.fromsys = 0;
  m.fromuser = static_cast<unsigned short>(GetSession()->usernum);
  m.tosys = 0;
  m.touser = 0;
  m.status = status_multimail;
  m.daten = static_cast<unsigned long>(time(nullptr));

  unique_ptr<File> pFileEmail(OpenEmailFile(true));
  int len = pFileEmail->GetLength() / sizeof(mailrec);
  if (len == 0) {
    i = 0;
  } else {
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
  int nUserNumber, nSystemNumber, i;
  WUser user;

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
  nUserNumber = finduser1(s);
  if (nUserNumber == 65535) {
    return -1;
  }
  if (s[0] == 0) {
    return -1;
  }
  if (nUserNumber <= 0) {
    bout.nl();
    bout << "Unknown user.\r\n\n";
    return 0;
  }
  nSystemNumber = 0;
  if (ForwardMessage(&nUserNumber, &nSystemNumber)) {
    bout.nl();
    bout << "Forwarded.\r\n\n";
    if (nSystemNumber) {
      bout << "Forwarded to another system.\r\n";
      bout << "Can't send multi-mail to another system.\r\n\n";
      return 0;
    }
  }
  if (nUserNumber == 0) {
    bout.nl();
    bout << "Unknown user.\r\n\n";
    return 0;
  }
  GetApplication()->GetUserManager()->ReadUser(&user, nUserNumber);
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
  bout << "     -> " << user.GetUserNameAndNumber(nUserNumber) << wwiv::endl;
  return nUserNumber;
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
  int nUserNumber[MAX_LIST], numu, i, i1;
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
  if (((fsenttoday >= 5) || (GetSession()->GetCurrentUser()->GetNumFeedbackSentToday() >= 10) ||
       (GetSession()->GetCurrentUser()->GetNumEmailSentToday() >= getslrec(GetSession()->GetEffectiveSl()).emails))
      && (!cs())) {
    bout << "Too much mail sent today.\r\n\n";
    return;
  }
  if (GetSession()->GetCurrentUser()->IsRestrictionEmail()) {
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
      add_list(nUserNumber, &numu, MAX_LIST, so());
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
        add_list(nUserNumber, &numu, MAX_LIST, so());
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
        multimail(nUserNumber, numu);
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
            nUserNumber[i1] = nUserNumber[i1 + 1];
          }
        }
      }
      break;
    case 'L':
      for (i = 0; i < numu; i++) {
        WUser user;
        GetApplication()->GetUserManager()->ReadUser(&user, nUserNumber[i]);
        bout << i + 1 << ". " << user.GetUserNameAndNumber(nUserNumber[i]) << wwiv::endl;
      }
      break;
    }
    CheckForHangup();
  } while (!done && !hangup);
}
