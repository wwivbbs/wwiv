/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2016, WWIV Software Services             */
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
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "bbs/bbsovl1.h"
#include "bbs/bbsutl1.h"
#include "bbs/com.h"
#include "bbs/conf.h"
#include "bbs/connect1.h"
#include "bbs/datetime.h"
#include "bbs/email.h"
#include "bbs/extract.h"
#include "bbs/instmsg.h"
#include "bbs/input.h"
#include "bbs/message_file.h"
#include "bbs/mmkey.h"
#include "bbs/msgbase1.h"
#include "bbs/quote.h"
#include "bbs/read_message.h"
#include "sdk/subxtr.h"
#include "bbs/printfile.h"
#include "bbs/sysoplog.h"
#include "bbs/pause.h"
#include "bbs/bbs.h"
#include "bbs/fcns.h"
#include "bbs/vars.h"
#include "bbs/keycodes.h"
#include "bbs/workspace.h"
#include "sdk/status.h"
#include "core/strings.h"
#include "core/wwivassert.h"
#include "sdk/filenames.h"

using std::string;
using std::unique_ptr;
using std::vector;
using wwiv::endl;
using namespace wwiv::sdk;
using namespace wwiv::strings;

static char s_szFindString[21];

static string GetScanReadPrompts(int nMessageNumber) {
  if (forcescansub) {
    if (nMessageNumber < session()->GetNumMessagesInCurrentMessageArea()) {
      return "|#1Press |#7[|#2ENTER|#7]|#1 to go to the next message...";
    } else {
      return "|#1Press |#7[|#2ENTER|#7]|#1 to continue...";
    }
  }

  string local_network_name = "Local";
  if (!session()->current_sub().nets.empty()) {
    local_network_name = session()->network_name();
  } else {
    set_net_num(0);
  }
  const string sub_name_prompt = StringPrintf("|#7[|#1%s|#7] [|#2%s|#7]",
      local_network_name.c_str(), session()->current_sub().name.c_str());
  return StringPrintf("%s |#7(|#1Read |#2%d |#1of |#2%d|#1|#7) : ",
        sub_name_prompt.c_str(), nMessageNumber,
        session()->GetNumMessagesInCurrentMessageArea());
}

static void HandleScanReadAutoReply(int &nMessageNumber, const char *pszUserInput, int &nScanOptionType) {
  if (!lcs() && get_post(nMessageNumber)->status & (status_unvalidated | status_delete)) {
    return;
  }
  if (get_post(nMessageNumber)->ownersys && !get_post(nMessageNumber)->owneruser) {
    grab_user_name(&(get_post(nMessageNumber)->msg), session()->current_sub().filename, session()->net_num());
  }
  grab_quotes(&(get_post(nMessageNumber)->msg), session()->current_sub().filename.c_str());

  if (okfsed() && session()->user()->IsUseAutoQuote() && nMessageNumber > 0 &&
      nMessageNumber <= session()->GetNumMessagesInCurrentMessageArea() && pszUserInput[0] != 'O') {
    string b;
    readfile(&(get_post(nMessageNumber)->msg),
             (session()->current_sub().filename), &b);
    if (pszUserInput[0] == '@') {
      auto_quote(&b[0], b.size(), 1, get_post(nMessageNumber)->daten);
    } else {
      auto_quote(&b[0], b.size(), 3, get_post(nMessageNumber)->daten);
    }
  }

  if (get_post(nMessageNumber)->status & status_post_new_net) {
    set_net_num(get_post(nMessageNumber)->network.network_msg.net_number);
    if (get_post(nMessageNumber)->network.network_msg.net_number == -1) {
      bout << "|#6Deleted network.\r\n";
      return;
    }
  }

  if (pszUserInput[0] == 'O' && (so() || lcs())) {
    irt_sub[0] = 0;
    show_files("*.frm", session()->config()->gfilesdir().c_str());
    bout << "|#2Which form letter: ";
    char szFileName[MAX_PATH];
    input(szFileName, 8, true);
    if (!szFileName[0]) {
      return;
    }
    string full_pathname = StrCat(session()->config()->gfilesdir(), szFileName, ".frm");
    if (!File::Exists(full_pathname)) {
      full_pathname = StrCat(session()->config()->gfilesdir(), "form", szFileName, ".msg");
    }
    if (File::Exists(full_pathname)) {
      LoadFileIntoWorkspace(full_pathname, true);
      email(irt, get_post(nMessageNumber)->owneruser, get_post(nMessageNumber)->ownersys, false, get_post(nMessageNumber)->anony);
      grab_quotes(nullptr, nullptr);
    }
  } else if (pszUserInput[0] == '@') {
    bout.nl();
    bout << "|#21|#7]|#9 Reply to Different Address\r\n";
    bout << "|#22|#7]|#9 Forward Message\r\n";
    bout.nl();
    bout << "|#7(Q=Quit) Select [1,2] : ";
    char chReply = onek("Q12\r", true);
    switch (chReply) {
    case '\r':
    case 'Q':
      nScanOptionType = SCAN_OPTION_READ_MESSAGE;
      break;
    case '1': {
      bout.nl();
      bout << "|#9Enter user name or number:\r\n:";
      char szUserNameOrNumber[ 81 ];
      input(szUserNameOrNumber, 75, true);
      size_t nAtPos = strcspn(szUserNameOrNumber, "@");
      if (nAtPos != strlen(szUserNameOrNumber) && isalpha(szUserNameOrNumber[ nAtPos + 1 ])) {
        if (strstr(szUserNameOrNumber, "@32767") == nullptr) {
          strlwr(szUserNameOrNumber);
          strcat(szUserNameOrNumber, " @32767");
        }
      }
      int user_number, system_number;
      parse_email_info(szUserNameOrNumber, &user_number, &system_number);
      if (user_number || system_number) {
        email("", user_number, system_number, false, 0);
      }
      nScanOptionType = SCAN_OPTION_READ_MESSAGE;
    }
    break;
    case '2': {
      if (nMessageNumber > 0 && nMessageNumber <= session()->GetNumMessagesInCurrentMessageArea()) {
        string b;
        readfile(&(get_post(nMessageNumber)->msg), (session()->current_sub().filename), &b);
        string filename = "EXTRACT.TMP";
        if (File::Exists(filename)) {
          File::Remove(filename);
        }
        File fileExtract(filename);
        if (!fileExtract.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite)) {
          bout << "|#6Could not open file for writing.\r\n";
        } else {
          if (fileExtract.GetLength() > 0) {
            fileExtract.Seek(-1L, File::Whence::end);
            char chLastChar = CZ;
            fileExtract.Read(&chLastChar, 1);
            if (chLastChar == CZ) {
              fileExtract.Seek(-1L, File::Whence::end);
            }
          }
          string buffer = StringPrintf("ON: %s", session()->current_sub().name.c_str());
          fileExtract.Write(buffer);
          fileExtract.Write("\r\n\r\n", 4);
          fileExtract.Write(get_post(nMessageNumber)->title, strlen(get_post(nMessageNumber)->title));
          fileExtract.Write("\r\n", 2);
          fileExtract.Write(b);
          fileExtract.Close();
        }
        bout.nl();
        bout << "|#5Allow editing? ";
        if (yesno()) {
          bout.nl();
          LoadFileIntoWorkspace(filename, false);
        } else {
          bout.nl();
          LoadFileIntoWorkspace(filename, true);
        }
        send_email();
        filename = StrCat(session()->temp_directory(), INPUT_MSG);
        if (File::Exists(filename)) {
          File::Remove(filename);
        }
      }
      nScanOptionType = SCAN_OPTION_READ_MESSAGE;
    }
    break;
    }
  } else {
    if (lcs() || (getslrec(session()->GetEffectiveSl()).ability & ability_read_post_anony)
        || get_post(nMessageNumber)->anony == 0) {
      email("", get_post(nMessageNumber)->owneruser, get_post(nMessageNumber)->ownersys, false, 0);
    } else {
      email("", get_post(nMessageNumber)->owneruser, get_post(nMessageNumber)->ownersys, false, get_post(nMessageNumber)->anony);
    }
    irt_sub[0] = 0;
    grab_quotes(nullptr, nullptr);
  }
}

static void HandleScanReadFind(int &nMessageNumber, int &nScanOptionType) {
  bool abort = false;
  char *pszTempFindString = nullptr;
  if (!(g_flags & g_flag_made_find_str)) {
    pszTempFindString = strupr(stripcolors(get_post(nMessageNumber)->title));
    strncpy(s_szFindString, pszTempFindString, sizeof(s_szFindString) - 1);
    g_flags |= g_flag_made_find_str;
  } else {
    pszTempFindString = &s_szFindString[0];
  }
  while (strncmp(pszTempFindString, "RE:", 3) == 0 || *pszTempFindString == ' ') {
    if (*pszTempFindString == ' ') {
      pszTempFindString++;
    } else {
      pszTempFindString += 3;
    }
  }
  if (strlen(pszTempFindString) >= 20) {
    pszTempFindString[20] = 0;
  }
  strncpy(s_szFindString, pszTempFindString, sizeof(s_szFindString) - 1);
  bout.nl();
  bout << "|#7Find what? (CR=\"" << pszTempFindString << "\")|#1";
  char szFindString[ 81 ];
  input(szFindString, 20);
  if (!*szFindString) {
    strncpy(szFindString, pszTempFindString, sizeof(szFindString) - 1);
  } else {
    strncpy(s_szFindString, szFindString, sizeof(s_szFindString) - 1);
  }
  bout.nl();
  bout << "|#1Backwards or Forwards? ";
  char szBuffer[10];
  szBuffer[0] = 'Q';
  szBuffer[1] = upcase(*"Backwards");
  szBuffer[2] = upcase(*"Forwards");
  strcpy(&szBuffer[3], "+-");
  char ch = onek(szBuffer);
  if (ch == 'Q') {
    return;
  }
  bool fnd = false;
  int nTempMsgNum = nMessageNumber;
  bout.nl();
  bout << "|#1Searching -> |#2";

  // Store search direction and limit
  bool bSearchForwards = true;
  int nMsgNumLimit;
  if (ch == '-' || ch == upcase(*"Backwards")) {
    bSearchForwards = false;
    nMsgNumLimit = 1;
  } else {
    bSearchForwards = true;
    nMsgNumLimit = session()->GetNumMessagesInCurrentMessageArea();
  }

  while (nTempMsgNum != nMsgNumLimit && !abort && !hangup && !fnd) {
    if (bSearchForwards) {
      nTempMsgNum++;
    } else {
      nTempMsgNum--;
    }
    checka(&abort);
    if (!(nTempMsgNum % 5)) {
      bout.bprintf("%5.5d", nTempMsgNum);
      for (int i1 = 0; i1 < 5; i1++) {
        bout << "\b";
      }
      if (!(nTempMsgNum % 100)) {
        session()->tleft(true);
        CheckForHangup();
      }
    }
    string b;
    if (readfile(&(get_post(nTempMsgNum)->msg), session()->current_sub().filename, &b)) {
      StringUpperCase(&b);
      fnd = (strstr(strupr(stripcolors(get_post(nTempMsgNum)->title)), szFindString)
             || strstr(b.c_str(), szFindString)) ? true : false;
    }
  }
  if (fnd) {
    bout << "Found!\r\n";
    nMessageNumber = nTempMsgNum;
    nScanOptionType = SCAN_OPTION_READ_MESSAGE;
  } else {
    bout << "|#6Not found!\r\n";
    nScanOptionType = SCAN_OPTION_READ_PROMPT;
  }
}

static void HandleListTitles(int &nMessageNumber, int &nScanOptionType) {
  int i = 0;
  bool abort = false;
  if (nMessageNumber >= session()->GetNumMessagesInCurrentMessageArea()) {
    abort = true;
  } else {
    bout.nl();
  }
  int nNumTitleLines = std::max<int>(session()->screenlinest - 6, 1);
  while (!abort && !hangup && ++i <= nNumTitleLines) {
    ++nMessageNumber;
    postrec *p3 = get_post(nMessageNumber);

    char szPrompt[255];
    char szTempBuffer[255];
    if (p3->ownersys == 0 && p3->owneruser == session()->usernum) {
      sprintf(szTempBuffer, "|#7[|#1%d|#7]", nMessageNumber);
    } else if (p3->ownersys != 0) {
      sprintf(szTempBuffer, "|#7<|#1%d|#7>", nMessageNumber);
    } else {
      sprintf(szTempBuffer, "|#7(|#1%d|#7)", nMessageNumber);
    }
    for (int i1 = 0; i1 < 7; i1++) {
      szPrompt[i1] = SPACE;
    }
    if (p3->qscan > qsc_p[session()->GetCurrentReadMessageArea()]) {
      szPrompt[0] = '*';
    }
    if (p3->status & (status_pending_net | status_unvalidated)) {
      szPrompt[0] = '+';
    }
    strcpy(&szPrompt[9 - strlen(stripcolors(szTempBuffer))], szTempBuffer);
    strcat(szPrompt, "|#1 ");
    if ((get_post(nMessageNumber)->status & (status_unvalidated | status_delete)) && (!lcs())) {
      strcat(szPrompt, "<<< NOT VALIDATED YET >>>");
    } else {
      // Need the StringTrim on post title since some FSEDs
      // added \r in the title string, also gets rid of extra spaces
      string title(get_post(nMessageNumber)->title);
      StringTrim(&title);
      strncat(szPrompt, stripcolors(title).c_str(), 60);
    }

    if (session()->user()->GetScreenChars() >= 80) {
      if (strlen(stripcolors(szPrompt)) > 50) {
        while (strlen(stripcolors(szPrompt)) > 50) {
          szPrompt[strlen(szPrompt) - 1] = 0;
        }
      }
      strcat(szPrompt, charstr(51 - strlen(stripcolors(szPrompt)), ' '));
      if (okansi()) {
        strcat(szPrompt, "|#7\xB3|#1");
      } else {
        strcat(szPrompt, "|");
      }
      strcat(szPrompt, " ");
      if ((p3->anony & 0x0f) &&
          ((getslrec(session()->GetEffectiveSl()).ability & ability_read_post_anony) == 0)) {
        strcat(szPrompt, ">UNKNOWN<");
      } else {
        string b;
        if (readfile(&(p3->msg), (session()->current_sub().filename), &b)) {
          strncpy(szTempBuffer, b.c_str(), sizeof(szTempBuffer) - 1);
          szTempBuffer[sizeof(szTempBuffer) - 1] = 0;
          b += stripcolors(szTempBuffer);
          size_t lTempBufferPos = 0;
          strcpy(szTempBuffer, "");
          while (b[lTempBufferPos] != RETURN
                 && b[lTempBufferPos] && lTempBufferPos < b.length()
                 && lTempBufferPos < static_cast<size_t>(session()->user()->GetScreenChars() - 54)) {
            szTempBuffer[lTempBufferPos] = b[lTempBufferPos];
            lTempBufferPos++;
          }
          szTempBuffer[lTempBufferPos] = 0;
          strcat(szPrompt, szTempBuffer);
        }
      }
    }
    bout.Color(2);
    pla(szPrompt, &abort);
    if (nMessageNumber >= session()->GetNumMessagesInCurrentMessageArea()) {
      abort = true;
    }
  }
  nScanOptionType = SCAN_OPTION_READ_PROMPT;
}

static void HandleMessageDownload(int nMessageNumber) {
  if (nMessageNumber > 0 && nMessageNumber <= session()->GetNumMessagesInCurrentMessageArea()) {
    string b;
    readfile(&(get_post(nMessageNumber)->msg), (session()->current_sub().filename), &b);
    bout << "|#1Message Download -\r\n\n";
    bout << "|#2Filename to use? ";
    string filename = input(12);
    if (!okfn(filename)) {
      return;
    }
    File fileTemp(session()->temp_directory(), filename);
    fileTemp.Delete();
    fileTemp.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
    fileTemp.Write(b);
    fileTemp.Close();

    bool bFileAbortStatus;
    bool bStatus;
    send_file(fileTemp.full_pathname().c_str(), &bStatus, &bFileAbortStatus, fileTemp.full_pathname().c_str(), -1,
              b.length());
    bout << "|#1Message download... |#2" << (bStatus ? "successful" : "unsuccessful");
    if (bStatus) {
      sysoplog() << "Downloaded message";
    }
  }
}

void HandleMessageMove(int &nMessageNumber) {
  if ((lcs()) && (nMessageNumber > 0) && (nMessageNumber <= session()->GetNumMessagesInCurrentMessageArea())) {
    char *ss1 = nullptr;
    tmp_disable_conf(true);
    bout.nl();
    do {
      bout << "|#5Move to which sub? ";
      ss1 = mmkey(0);
      if (ss1[0] == '?') {
        old_sublist();
      }
    } while (!hangup && ss1[0] == '?');
    int nTempSubNum = -1;
    if (ss1[0] == 0) {
      tmp_disable_conf(false);
      return;
    }
    bool ok = false;
    for (size_t i1 = 0; (i1 < session()->subs().subs().size() && session()->usub[i1].subnum != -1 && !ok); i1++) {
      if (IsEquals(session()->usub[i1].keys, ss1)) {
        nTempSubNum = i1;
        bout.nl();
        bout << "|#9Copying to " << session()->subs().sub(session()->usub[nTempSubNum].subnum).name << endl;
        ok = true;
      }
    }
    if (nTempSubNum != -1) {
      if (session()->GetEffectiveSl() < session()->subs().sub(session()->usub[nTempSubNum].subnum).postsl) {
        bout.nl();
        bout << "Sorry, you don't have post access on that sub.\r\n\n";
        nTempSubNum = -1;
      }
    }
    if (nTempSubNum != -1) {
      open_sub(true);
      resynch(&nMessageNumber, nullptr);
      postrec p2 = *get_post(nMessageNumber);
      postrec p1 = p2;
      string b;
      readfile(&(p2.msg), (session()->current_sub().filename), &b);
      bout.nl();
      bout << "|#5Delete original post? ";
      if (yesno()) {
        delete_message(nMessageNumber);
        if (nMessageNumber > 1) {
          nMessageNumber--;
        }
      }
      close_sub();
      iscan(nTempSubNum);
      open_sub(true);
      p2.msg.storage_type = static_cast<unsigned char>(session()->current_sub().storage_type);
      savefile(b, &(p2.msg), (session()->current_sub().filename));
      WStatus* pStatus = session()->status_manager()->BeginTransaction();
      p2.qscan = pStatus->IncrementQScanPointer();
      session()->status_manager()->CommitTransaction(pStatus);
      if (session()->GetNumMessagesInCurrentMessageArea() >=
        session()->current_sub().maxmsgs) {
        int nTempMsgNum = 1;
        int nMsgToDelete = 0;
        while (nMsgToDelete == 0 && nTempMsgNum <= session()->GetNumMessagesInCurrentMessageArea()) {
          if ((get_post(nTempMsgNum)->status & status_no_delete) == 0) {
            nMsgToDelete = nTempMsgNum;
          }
          ++nTempMsgNum;
        }
        if (nMsgToDelete == 0) {
          nMsgToDelete = 1;
        }
        delete_message(nMsgToDelete);
      }
      if ((!(session()->current_sub().anony & anony_val_net)) ||
          (session()->current_sub().nets.empty())) {
        p2.status &= ~status_pending_net;
      }
      if (!session()->current_sub().nets.empty()) {
        p2.status |= status_pending_net;
        session()->user()->SetNumNetPosts(session()->user()->GetNumNetPosts() + 1);
        send_net_post(&p2, session()->current_sub());
      }
      add_post(&p2);
      close_sub();
      tmp_disable_conf(false);
      iscan(session()->current_user_sub_num());
      bout.nl();
      bout << "|#9Message moved.\r\n\n";
      resynch(&nMessageNumber, &p1);
    } else {
      tmp_disable_conf(false);
    }
  }
}

static void HandleMessageLoad() {
  if (!so()) {
    return;
  }
  bout.nl();
  bout << "|#2Filename: ";
  char szFileName[MAX_PATH];
  input(szFileName, 50);
  if (szFileName[0]) {
    bout.nl();
    bout << "|#5Allow editing? ";
    if (yesno()) {
      bout.nl();
      LoadFileIntoWorkspace(szFileName, false);
    } else {
      bout.nl();
      LoadFileIntoWorkspace(szFileName, true);
    }
  }
}

void HandleMessageReply(int &nMessageNumber) {
  irt_sub[0] = 0;
  if ((!lcs()) && (get_post(nMessageNumber)->status & (status_unvalidated | status_delete))) {
    return;
  }
  postrec p2 = *get_post(nMessageNumber);
  grab_quotes(&(p2.msg), session()->current_sub().filename.c_str());

  if (okfsed() && session()->user()->IsUseAutoQuote() &&
      nMessageNumber > 0 && nMessageNumber <= session()->GetNumMessagesInCurrentMessageArea()) {
    string b;
    if (readfile(&(get_post(nMessageNumber)->msg),
      session()->current_sub().filename, &b)) {
      auto_quote(&b[0], b.size(), 1, get_post(nMessageNumber)->daten);
    }
  }

  post();
  resynch(&nMessageNumber, &p2);
  grab_quotes(nullptr, nullptr);
}

static void HandleMessageDelete(int &nMessageNumber) {
  if (lcs()) {
    if (nMessageNumber) {
      open_sub(true);
      resynch(&nMessageNumber, nullptr);
      postrec p2 = *get_post(nMessageNumber);
      delete_message(nMessageNumber);
      close_sub();
      if (p2.ownersys == 0) {
        User tu;
        session()->users()->ReadUser(&tu, p2.owneruser);
        if (!tu.IsUserDeleted()) {
          if (date_to_daten(tu.GetFirstOn()) < static_cast<time_t>(p2.daten)) {
            bout.nl();
            bout << "|#2Remove how many posts credit? ";
            char szNumCredits[ 10 ];
            input(szNumCredits, 3, true);
            int nNumCredits = 1;
            if (szNumCredits[0]) {
              nNumCredits = (atoi(szNumCredits));
            }
            nNumCredits = std::min<int>(nNumCredits, tu.GetNumMessagesPosted());
            if (nNumCredits) {
              tu.SetNumMessagesPosted(tu.GetNumMessagesPosted() - static_cast<uint16_t>(nNumCredits));
            }
            bout.nl();
            bout << "|#7Post credit removed = " << nNumCredits << endl;
            tu.SetNumDeletedPosts(tu.GetNumDeletedPosts() - 1);
            session()->users()->WriteUser(&tu, p2.owneruser);
            session()->UpdateTopScreen();
          }
        }
      }
      resynch(&nMessageNumber, &p2);
    }
  }
}

static void HandleMessageExtract(int &nMessageNumber) {
  if (!so()) {
    return;
  }
  if ((nMessageNumber > 0) && (nMessageNumber <= session()->GetNumMessagesInCurrentMessageArea())) {
    string b;
    if (readfile(&(get_post(nMessageNumber)->msg), (session()->current_sub().filename), &b)) {
      extract_out(&b[0], b.size(), get_post(nMessageNumber)->title, get_post(nMessageNumber)->daten);
    }
  }
}

static void HandleMessageHelp() {
  if (forcescansub) {
    printfile(MUSTREAD_NOEXT);
  } else {
    if (lcs()) {
      printfile(SMBMAIN_NOEXT);
    } else {
      printfile(MBMAIN_NOEXT);
    }
  }
}

static void HandleScanReadPrompt(int &nMessageNumber, int &nScanOptionType, int *nextsub, bool &bTitleScan, bool &done,
  bool &quit, int &val) {
  resetnsp();
  string read_prompt = GetScanReadPrompts(nMessageNumber);
  bout.nl();
  char szUserInput[81];
  if (express) {
    szUserInput[0] = '\0';
    read_prompt.clear();
    bout.nl(2);
  } else {
    bout << read_prompt;
    input(szUserInput, 5, true);
    resynch(&nMessageNumber, nullptr);
    while (szUserInput[0] == 32) {
      char szTempBuffer[255];
      strcpy(szTempBuffer, &(szUserInput[1]));
      strcpy(szUserInput, szTempBuffer);
    }
  }
  if (bTitleScan && szUserInput[0] == 0 && nMessageNumber < session()->GetNumMessagesInCurrentMessageArea()) {
    nScanOptionType = SCAN_OPTION_LIST_TITLES;
    szUserInput[0] = 'T';
    szUserInput[1] = '\0';
  } else {
    bTitleScan = false;
    nScanOptionType = SCAN_OPTION_READ_PROMPT;
  }
  int nUserInput = atoi(szUserInput);
  if (szUserInput[0] == '\0') {
    nUserInput = nMessageNumber + 1;
    if (nUserInput >= session()->GetNumMessagesInCurrentMessageArea() + 1) {
      done = true;
    }
  }

  if (nUserInput != 0 && nUserInput <= session()->GetNumMessagesInCurrentMessageArea() && nUserInput >= 1) {
    nScanOptionType = SCAN_OPTION_READ_MESSAGE;
    nMessageNumber = nUserInput;
  } else if (szUserInput[1] == '\0') {
    if (forcescansub) {
      return;
    }
    switch (szUserInput[0]) {
    case '$':
    { // Last message in area.
      nScanOptionType = SCAN_OPTION_READ_MESSAGE;
      nMessageNumber = session()->GetNumMessagesInCurrentMessageArea();
    }
    break;
    case 'F':
    { // Find addition
      HandleScanReadFind(nMessageNumber, nScanOptionType);
    }
    break;
    // End of find addition

    case 'Q':
      quit = true;
      done = true;
      *nextsub = 0;
      break;
    case 'B':
      if (*nextsub != 0) {
        bout.nl();
        bout << "|#5Remove this sub from your Q-Scan? ";
        if (yesno()) {
          qsc_q[session()->current_user_sub().subnum / 32] ^= (1L <<
            (session()->current_user_sub().subnum % 32));
        } else {
          bout.nl();
          bout << "|#9Mark messages in "
            << session()->subs().sub(session()->current_user_sub().subnum).name
            << " as read? ";
          if (yesno()) {
            unique_ptr<WStatus> pStatus(session()->status_manager()->GetStatus());
            qsc_p[session()->current_user_sub().subnum] = pStatus->GetQScanPointer() - 1L;
          }
        }

        *nextsub = 1;
        done = true;
        quit = true;
      }
      break;
    case 'T':
      bTitleScan = true;
      nScanOptionType = SCAN_OPTION_LIST_TITLES;
      break;
    case 'R':
      nScanOptionType = SCAN_OPTION_READ_MESSAGE;
      break;
    case '@':
      to_char_array(irt_sub, session()->subs().sub(session()->current_user_sub().subnum).name);
    case 'O':
    case 'A':
    {
      HandleScanReadAutoReply(nMessageNumber, szUserInput, nScanOptionType);
    }
    break;
    case 'P':
      irt[0] = '\0';
      irt_name[0] = '\0';
      post();
      break;
    case 'W':
      HandleMessageReply(nMessageNumber);
      break;
    case 'Y':
      HandleMessageDownload(nMessageNumber);
      break;
    case '?':
      HandleMessageHelp();
      break;
    case '-':
      if (nMessageNumber > 1 && (nMessageNumber - 1 < session()->GetNumMessagesInCurrentMessageArea())) {
        --nMessageNumber;
        nScanOptionType = SCAN_OPTION_READ_MESSAGE;
      }
      break;
    case 'C':
      express = true;
      break;
    case '*':
      // This used to be threaded code.
      break;
    case '[':
      // This used to be threaded code.
      break;
    case ']':
      // This used to be threaded code.
      break;
    case '>':
      // This used to be threaded code.
      nScanOptionType = SCAN_OPTION_READ_MESSAGE;
      break;

    case 'V':
      if (cs() && (get_post(nMessageNumber)->ownersys == 0) && (nMessageNumber > 0)
        && (nMessageNumber <= session()->GetNumMessagesInCurrentMessageArea())) {
        valuser(get_post(nMessageNumber)->owneruser);
      } else if (cs() && (nMessageNumber > 0) && (nMessageNumber <= session()->GetNumMessagesInCurrentMessageArea())) {
        bout.nl();
        bout << "|#6Post from another system.\r\n\n";
      }
      break;
    case 'N':
      if ((lcs()) && (nMessageNumber > 0) && (nMessageNumber <= session()->GetNumMessagesInCurrentMessageArea())) {
        wwiv::bbs::OpenSub opened_sub(true);
        resynch(&nMessageNumber, nullptr);
        postrec *p3 = get_post(nMessageNumber);
        p3->status ^= status_no_delete;
        write_post(nMessageNumber, p3);
        bout.nl();
        if (p3->status & status_no_delete) {
          bout << "|#9Message will |#6NOT|#9 be auto-purged.\r\n";
        } else {
          bout << "|#9Message |#2CAN|#9 now be auto-purged.\r\n";
        }
        bout.nl();
      }
      break;
    case 'X':
      if (lcs() && nMessageNumber > 0 && nMessageNumber <= session()->GetNumMessagesInCurrentMessageArea() &&
        session()->current_sub().anony & anony_val_net &&
        !session()->current_sub().nets.empty()) {
        wwiv::bbs::OpenSub opened_sub(true);
        resynch(&nMessageNumber, nullptr);
        postrec *p3 = get_post(nMessageNumber);
        p3->status ^= status_pending_net;
        write_post(nMessageNumber, p3);
        close_sub();
        bout.nl();
        if (p3->status & status_pending_net) {
          val |= 2;
          bout << "|#9Will be sent out on net now.\r\n";
        } else {
          bout << "|#9Not set for net pending now.\r\n";
        }
        bout.nl();
      }
      break;
    case 'U':
      if (lcs() && nMessageNumber > 0 && nMessageNumber <= session()->GetNumMessagesInCurrentMessageArea()) {
        wwiv::bbs::OpenSub opened_sub(true);
        resynch(&nMessageNumber, nullptr);
        postrec *p3 = get_post(nMessageNumber);
        p3->anony = 0;
        write_post(nMessageNumber, p3);
        bout.nl();
        bout << "|#9Message is not anonymous now.\r\n";
      }
      break;
    case 'D':
      HandleMessageDelete(nMessageNumber);
      break;
    case 'E':
      HandleMessageExtract(nMessageNumber);
      break;
    case 'M':
      HandleMessageMove(nMessageNumber);
      break;
    case 'L':
      HandleMessageLoad();
      break;
    }
  } else if (IsEquals(szUserInput, "cls")) {
    bout.bputch('\x0c');
  }
}

void scan(int nMessageNumber, int nScanOptionType, int *nextsub, bool bTitleScan) {
  irt[0] = '\0';
  irt_name[0] = '\0';

  int val = 0;
  bool realexpress = express;
  iscan(session()->current_user_sub_num());
  if (session()->GetCurrentReadMessageArea() < 0) {
    bout.nl();
    bout << "No subs available.\r\n\n";
    return;
  }

  bool done = false;
  bool quit = false;
  do {
    session()->tleft(true);
    CheckForHangup();
    set_net_num((session()->current_sub().nets.empty()) ? 0 :
      session()->current_sub().nets[0].net_num);
    if (nScanOptionType != SCAN_OPTION_READ_PROMPT) {
      resynch(&nMessageNumber, nullptr);
    }
    write_inst(INST_LOC_SUBS, session()->current_user_sub().subnum, INST_FLAGS_NONE);

    switch (nScanOptionType) {
    case SCAN_OPTION_READ_PROMPT:
    { // Read Prompt
      HandleScanReadPrompt(nMessageNumber, nScanOptionType, nextsub, bTitleScan, done, quit, val);
    }
    break;
    case SCAN_OPTION_LIST_TITLES:
    { // List Titles
      HandleListTitles(nMessageNumber, nScanOptionType);
    }
    break;
    case SCAN_OPTION_READ_MESSAGE:
    { // Read Message
      bool next = false;
      if (nMessageNumber > 0 && nMessageNumber <= session()->GetNumMessagesInCurrentMessageArea()) {
        if (forcescansub) {
          incom = false;
        }
        read_post(nMessageNumber, &next, &val);
        if (forcescansub) {
          incom = true;
        }
      }
      bout.Color(0);
      bout.nl();
      if (next) {
        ++nMessageNumber;
        if (nMessageNumber > session()->GetNumMessagesInCurrentMessageArea()) {
          done = true;
        }
        nScanOptionType = SCAN_OPTION_READ_MESSAGE;
      } else {
        nScanOptionType = SCAN_OPTION_READ_PROMPT;
      }
      if (expressabort) {
        if (realexpress) {
          done = true;
          quit = true;
          *nextsub = 0;
        } else {
          expressabort = false;
          express = false;
          nScanOptionType = SCAN_OPTION_READ_PROMPT;
        }
      }
    }
    break;
    }
  } while (!done && !hangup);
  if (!realexpress) {
    express = false;
    expressabort = false;
  }
  // Express is true when we are just ripping through all
  // messages, probably to screen capture them all.
  if (express) {
    return;
  }
  if ((val & 1) && lcs()) {
    bout.nl();
    bout << "|#5Validate messages here? ";
    if (noyes()) {
      wwiv::bbs::OpenSub opened_sub(true);
      for (int i = 1; i <= session()->GetNumMessagesInCurrentMessageArea(); i++) {
        postrec* p3 = get_post(i);
        if (p3->status & (status_unvalidated | status_delete)) {
          p3->status &= (~(status_unvalidated | status_delete));
        }
        write_post(i, p3);
      }
    }
  }
  if ((val & 2) && lcs()) {
    bout.nl();
    bout << "|#5Network validate here? ";
    if (yesno()) {
      int nNumMsgsSent = 0;
      vector<postrec> to_validate;
      {
        wwiv::bbs::OpenSub opened_sub(true);
        for (int i = 1; i <= session()->GetNumMessagesInCurrentMessageArea(); i++) {
          postrec *p4 = get_post(i);
          if (p4->status & status_pending_net) {
            to_validate.push_back(*p4);
            p4->status &= ~status_pending_net;
            write_post(i, p4);
          }
        }
      }
      for (auto p : to_validate) {
        send_net_post(&p, session()->current_sub());
        nNumMsgsSent++;
      }

      bout.nl();
      bout << nNumMsgsSent << " messages sent.";
      bout.nl(2);
    }
  }
  bout.nl();
  if (quit) {
    return;
  }
  if (!session()->user()->IsRestrictionPost() &&
    (session()->user()->GetNumPostsToday() < getslrec(session()->GetEffectiveSl()).posts) &&
    (session()->GetEffectiveSl() >= session()->current_sub().postsl)) {
    bout << "|#5Post on " << session()->current_sub().name << "? ";
    irt[0] = '\0';
    irt_name[0] = '\0';
    grab_quotes(nullptr, nullptr);
    if (yesno()) {
      post();
    }
  }
  bout.nl();
}
