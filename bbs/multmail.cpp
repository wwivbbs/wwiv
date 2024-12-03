/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2022, WWIV Software Services             */
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
#include "bbs/multmail.h"

#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/email.h"
#include "bbs/finduser.h"
#include "bbs/inmsg.h"
#include "bbs/message_file.h"
#include "bbs/sysoplog.h"
#include "common/com.h"
#include "common/input.h"
#include "common/output.h"
#include "core/datetime.h"
#include "core/findfiles.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/names.h"
#include "sdk/status.h"
#include "sdk/user.h"
#include "sdk/usermanager.h"
#include <string>

// local function prototypes
void add_list(int *pnUserNumber, int *numu, int maxu, int allowdup);
int  oneuser();

using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

void multimail(int *pnUserNumber, int numu) {
  mailrec m, m1;
  char s[255];
  User user;
  memset(&m, 0, sizeof(mailrec));

  if (File::freespace_for_path(a()->config()->msgsdir()) < 10) {
    bout.nl();
    bout.outstr("Sorry, not enough disk space left.\r\n\n");
    return;
  }
  bout.nl();
  MessageEditorData data(a()->user()->name_and_number());
  data.need_title = true;
  if (a()->config()->sl(a()->sess().effective_sl()).ability & ability_email_anony) {
    data.anonymous_flag = anony_enable_anony;
  }
  bout.outstr("|#5Show all recipients in mail? ");
  bool show_all = bin.yesno();
  int j = 0;
  auto s1 = fmt::sprintf("\003""6CC: \003""1");

  m.msg.storage_type = EMAIL_STORAGE;
  a()->sess().irt("Multi-Mail");
  File::Remove(QUOTES_TXT);
  data.aux = "email";
  data.fsed_flags = FsedFlags::NOFSED;
  data.to_name = "Multi-Mail";
  data.msged_flags = MSGED_FLAG_NONE;
  if (!inmsg(data)) {
    return;
  }
  savefile(data.text, &m.msg, data.aux);
  strcpy(m.title, data.title.c_str());

  bout.outstr("Mail sent to:\r\n");
  sysoplog("Multi-Mail to:");

  lineadd(&m.msg, "\003""7----", "email");

  for (int cv = 0; cv < numu; cv++) {
    if (pnUserNumber[cv] < 0) {
      continue;
    }
    a()->users()->readuser(&user, pnUserNumber[cv]);
    if ((user.sl() == 255 && (user.email_waiting() > a()->config()->max_waiting() * 5)) ||
        ((user.sl() != 255) && (user.email_waiting() > a()->config()->max_waiting())) ||
        user.email_waiting() > 200) {
      bout.print("{} mailbox full, not sent.", a()->names()->UserName(pnUserNumber[cv]));
      pnUserNumber[cv] = -1;
      continue;
    }
    if (user.deleted()) {
      bout.outstr("User deleted, not sent.\r\n");
      pnUserNumber[cv] = -1;
      continue;
    }
    strcpy(s, "  ");
    user.email_waiting(user.email_waiting() + 1);
    a()->users()->writeuser(&user, pnUserNumber[cv]);
    const auto pnunn = a()->names()->UserName(pnUserNumber[cv]);
    strcat(s, pnunn.c_str());
    a()->status_manager()->Run([&](Status& status) {
      if (pnUserNumber[cv] == 1) {
        status.increment_feedback_today();
        a()->user()->feedback_today(a()->user()->feedback_today() + 1);
        a()->user()->feedback_sent(a()->user()->feedback_sent() + 1);
      } else {
        status.increment_email_today();
        a()->user()->email_sent(a()->user()->email_sent() + 1);
        a()->user()->email_today(a()->user()->email_today() + 1);
      }
    });
    sysoplog(s);
    bout.outstr(s);
    bout.nl();
    if (show_all) {
      const std::string pnunn2 = a()->names()->UserName(pnUserNumber[cv]);
      s1 = fmt::sprintf("%-22.22s  ", pnunn2);
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
  s1 = fmt::sprintf("\003""2Mail Sent to %d Addresses!", numu);
  lineadd(&m.msg, "\003""7----", "email");
  lineadd(&m.msg, s1, "email");

  m.anony = static_cast<unsigned char>(data.anonymous_flag);
  m.fromsys = 0;
  m.fromuser = static_cast<uint16_t>(a()->sess().user_num());
  m.tosys = 0;
  m.touser = 0;
  m.status = status_multimail;
  m.daten = daten_t_now();

  auto pFileEmail(OpenEmailFile(true));
  auto len = pFileEmail->length() / sizeof(mailrec);
  File::size_type i = 0;
  if (len != 0) {
    i = len - 1;
    pFileEmail->Seek(static_cast<long>(i) * sizeof(mailrec), File::Whence::begin);
    pFileEmail->Read(&m1, sizeof(mailrec));
    while ((i > 0) && (m1.tosys == 0) && (m1.touser == 0)) {
      --i;
      pFileEmail->Seek(static_cast<long>(i) * sizeof(mailrec), File::Whence::begin);
      auto i1 = pFileEmail->Read(&m1, sizeof(mailrec));
      if (i1 == -1) {
        bout.outstr("|#6DIDN'T READ WRITE!\r\n");
      }
    }
    if ((m1.tosys) || (m1.touser)) {
      ++i;
    }
  }
  pFileEmail->Seek(static_cast<long>(i) * sizeof(mailrec), File::Whence::begin);
  for (auto cv = 0; cv < numu; cv++) {
    if (pnUserNumber[cv] > 0) {
      m.touser = static_cast<uint16_t>(pnUserNumber[cv]);
      pFileEmail->Write(&m, sizeof(mailrec));
    }
  }
  pFileEmail->Close();
}

static char *mml_s;
static int mml_started;

int oneuser() {
  char s[81], *ss;
  int i;
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
    bout.outstr("|#2>");
    bin.input(s, 40);
  }
  auto user_number_int = finduser1(s);
  if (user_number_int == 65535) {
    return -1;
  }
  if (s[0] == 0) {
    return -1;
  }
  if (user_number_int <= 0) {
    bout.nl();
    bout.outstr("Unknown user.\r\n\n");
    return 0;
  }
  uint16_t user_number = static_cast<uint16_t>(user_number_int);
  uint16_t system_number = 0;
  if (ForwardMessage(&user_number, &system_number)) {
    bout.nl();
    bout.outstr("Forwarded.\r\n\n");
    if (system_number) {
      bout.outstr("Forwarded to another system.\r\n");
      bout.outstr("Can't send multi-mail to another system.\r\n\n");
      return 0;
    }
  }
  if (user_number == 0) {
    bout.nl();
    bout.outstr("Unknown user.\r\n\n");
    return 0;
  }
  a()->users()->readuser(&user, user_number);
  if (((user.sl() == 255) && (user.email_waiting() > (a()->config()->max_waiting() * 5))) ||
      ((user.sl() != 255) && (user.email_waiting() > a()->config()->max_waiting())) ||
      (user.email_waiting() > 200)) {
    bout.nl();
    bout.outstr("Mailbox full.\r\n\n");
    return 0;
  }
  if (user.deleted()) {
    bout.nl();
    bout.outstr("Deleted user.\r\n\n");
    return 0;
  }
  bout.print("     -> {}\r\n", a()->names()->UserName(user_number));
  return user_number;
}


void add_list(int *pnUserNumber, int *numu, int maxu, int allowdup) {
  bool done = false;
  int mml = (mml_s != nullptr);
  mml_started = 0;
  while (!done && (*numu < maxu)) {
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
            bout.outstr("Already in list, not added.\r\n\n");
            i = 0;
	    break;
          }
	}
	  if (i) {
	    pnUserNumber[(*numu)++] = i;
	  }
      } else {
	if (i) {
	    pnUserNumber[(*numu)++] = i;
	}
      }
    }
  }
  if (*numu == maxu) {
    bout.nl();
    bout.outstr("List full.\r\n\n");
  }
}

#define MAX_LIST 40

void slash_e() {
  int user_number[MAX_LIST]={}, numu, i;
  char s[81],*sss;

  mml_s = nullptr;
  mml_started = 0;
  if (File::freespace_for_path(a()->config()->msgsdir()) < 10) {
    bout.nl();
    bout.outstr("Sorry, not enough disk space left.\r\n\n");
    return;
  }
  if (((a()->user()->feedback_today() >= 10) ||
       (a()->user()->email_today() >= a()->config()->sl(a()->sess().effective_sl()).emails))
      && (!cs())) {
    bout.outstr("Too much mail sent today.\r\n\n");
    return;
  }
  if (a()->user()->restrict_email()) {
    bout.outstr("You can't send mail.\r\n");
    return;
  }
  auto done = false;
  numu = 0;
  do {
    bout.nl(2);
    bout.print("|#2Multi-Mail: List: {} : A,M,D,L,E,Q,? : ",numu);
    switch (char ch = onek("QAMDEL?"); ch) {
    case '?':
      bout.printfile(MMAIL_NOEXT);
      break;
    case 'Q':
      done = true;
      break;
    case 'A':
      bout.nl();
      bout.print("Enter names/numbers for users, one per line, max {}.\r\n\n",MAX_LIST);
      mml_s = nullptr;
      //add_list(user_number, &numu, MAX_LIST, so());
      add_list(user_number, &numu, MAX_LIST, 0); // do we really want sysops
                                           // to be able to add dupe entries?
      break;
    case 'M': {
      FindFiles ff(FilePath(a()->config()->datadir(), "*.mml"), FindFiles::FindFilesType::any);
      if (ff.empty()) {
        bout.nl();
        bout.outstr("No mailing lists available.\r\n\n");
        break;
      }
      bout.nl();
      bout.outstr("Available mailing lists:\r\n\n");
      for (const auto& f : ff) {
        to_char_array(s, f.name);
        sss = strchr(s, '.');
        if (sss) {
          *sss = 0;
        }
        bout.outstr(s);
        bout.nl();
      }

      bout.nl();
      bout.outstr("|#2Which? ");
      bin.input(s, 8);
      File fileMailList(FilePath(a()->config()->datadir(), s));
      if (!fileMailList.Open(File::modeBinary | File::modeReadOnly)) {
        bout.nl();
        bout.outstr("Unknown mailing list.\r\n\n");
      } else {
        auto i1 = fileMailList.length();
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
        bout.outstr("Need to specify some users first - use A or M\r\n\n");
      } else {
        multimail(user_number, numu);
        done = true;
      }
      break;
    case 'D':
      if (numu) {
	bout.nl();
	for (i = 0; i < numu; i++) {
	  User user;
	  a()->users()->readuser(&user, user_number[i]);
	  bout.print("{}. {}\r\n", i + 1, a()->names()->UserName(user_number[i]));
	}
        bout.nl();
        bout.outstr("|#2Delete which? ");
        bin.input(s, 2);
        i = to_number<int>(s);
        if ((i > 0) && (i <= numu)) {
          --numu;
          for (auto i1 = i - 1; i1 < numu; i1++) {
            user_number[i1] = user_number[i1 + 1];
          }
        }
      }
      break;
    case 'L':
      bout.nl();
      for (i = 0; i < numu; i++) {
        User user;
        a()->users()->readuser(&user, user_number[i]);
        bout.print("{}. {}\r\n", i + 1, a()->names()->UserName(user_number[i]));
      }
      break;
    }
    a()->CheckForHangup();
  } while (!done);
}
