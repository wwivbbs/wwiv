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
#include "bbs/uedit.h"

#include <memory>
#include <string>

#include "bbs/callback.h"
#include "bbs/datetime.h"
#include "bbs/input.h"
#include "bbs/newuser.h"
#include "bbs/bbs.h"
#include "bbs/fcns.h"
#include "bbs/vars.h"
#include "bbs/printfile.h"
#include "bbs/wconstants.h"
#include "core/strings.h"
#include "core/wwivassert.h"
#include "bbs/keycodes.h"
#include "sdk/filenames.h"

using namespace wwiv::bbs;
using std::string;
using std::unique_ptr;
using wwiv::strings::IsEquals;

static uint32_t *u_qsc = 0;
static char *sp = nullptr;
static char search_pattern[81];
char *daten_to_date(time_t dt);

int  matchuser(int nUserNumber);
int  matchuser(WUser * pUser);
void changeopt();

void deluser(int nUserNumber) {
  WUser user;
  session()->users()->ReadUser(&user, nUserNumber);

  if (!user.IsUserDeleted()) {
    rsm(nUserNumber, &user, false);
    DeleteSmallRecord(user.GetName());
    user.SetInactFlag(WUser::userDeleted);
    user.SetNumMailWaiting(0);
    session()->users()->WriteUser(&user, nUserNumber);
    unique_ptr<File> pFileEmail(OpenEmailFile(true));
    if (pFileEmail->IsOpen()) {
      long lEmailFileLen = pFileEmail->GetLength() / sizeof(mailrec);
      for (int nMailRecord = 0; nMailRecord < lEmailFileLen; nMailRecord++) {
        mailrec m;

        pFileEmail->Seek(nMailRecord * sizeof(mailrec), File::seekBegin);
        pFileEmail->Read(&m, sizeof(mailrec));
        if ((m.tosys == 0 && m.touser == nUserNumber) ||
            (m.fromsys == 0 && m.fromuser == nUserNumber)) {
          delmail(pFileEmail.get(), nMailRecord);
        }
      }
      pFileEmail->Close();
    }
    File voteFile(syscfg.datadir, VOTING_DAT);
    voteFile.Open(File::modeReadWrite | File::modeBinary);
    long nNumVoteRecords = static_cast<int>(voteFile.GetLength() / sizeof(votingrec)) - 1;
    for (long lCurVoteRecord = 0; lCurVoteRecord < 20; lCurVoteRecord++) {
      if (user.GetVote(lCurVoteRecord)) {
        if (lCurVoteRecord <= nNumVoteRecords) {
          votingrec v;
          voting_response vr;

          voteFile.Seek(static_cast<long>(lCurVoteRecord * sizeof(votingrec)), File::seekBegin);
          voteFile.Read(&v, sizeof(votingrec));
          vr = v.responses[ user.GetVote(lCurVoteRecord) - 1 ];
          vr.numresponses--;
          v.responses[ user.GetVote(lCurVoteRecord) - 1 ] = vr;
          voteFile.Seek(static_cast<long>(lCurVoteRecord * sizeof(votingrec)), File::seekBegin);
          voteFile.Write(&v, sizeof(votingrec));
        }
        user.SetVote(lCurVoteRecord, 0);
      }
    }
    voteFile.Close();
    session()->users()->WriteUser(&user, nUserNumber);
    delete_phone_number(nUserNumber, user.GetVoicePhoneNumber());   // dupphone addition
    delete_phone_number(nUserNumber, user.GetDataPhoneNumber());    // dupphone addition
  }
}

void print_data(int nUserNumber, WUser *pUser, bool bLongFormat, bool bClearScreen) {
  char s[81], s1[81], s2[81], s3[81];

  if (bClearScreen) {
    bout.cls();
  }
  if (pUser->IsUserDeleted()) {
    bout << "|#6>>> This User Is DELETED - Use 'R' to Restore <<<\r\n\n";
  }
  bout << "|#2N|#9) Name/Alias   : |#1" << pUser->GetName() << " #" << nUserNumber << wwiv::endl;
  bout << "|#2L|#9) Real Name    : |#1" << pUser->GetRealName() << wwiv::endl;
  if (bLongFormat) {
    if (pUser->GetStreet()[0]) {
      bout << "|#2%|#9) Address      : |#1" << pUser->GetStreet() << wwiv::endl;
    }
    if (pUser->GetCity()[0] || pUser->GetState()[0] ||
        pUser->GetCountry()[0] || pUser->GetZipcode()[0]) {
      bout << "|#9   City         : |#1" << pUser->GetCity() << ", " <<
                         pUser->GetState() << "  " << pUser->GetZipcode() << " |#9(|#1" <<
                         pUser->GetCountry() << "|#9)\r\n";
    }
  }

  if (pUser->GetRegisteredDateNum() != 0) {
    bout << "|#2X|#9) Registration : |#1" << daten_to_date(pUser->GetRegisteredDateNum()) <<
                       "       Expires on   : " << daten_to_date(pUser->GetExpiresDateNum()) << wwiv::endl;
  }

  if (pUser->GetCallsign()[0] != '\0') {
    bout << "|#2C|#9) Call         : |#1" << pUser->GetCallsign() << wwiv::endl;
  }

  bout << "|#2P|#9) Voice Phone #: |#1" << pUser->GetVoicePhoneNumber() << wwiv::endl;
  if (pUser->GetDataPhoneNumber()[0] != '\0') {
    bout << "|#9   Data Phone # : |#1" << pUser->GetDataPhoneNumber() << wwiv::endl;
  }

  bout.bprintf("|#2G|#9) Birthday/Age : |#1(%02d/%02d/%02d) (Age: %d) (Gender: %c) \r\n",
                                    pUser->GetBirthdayMonth(), pUser->GetBirthdayDay(),
                                    pUser->GetBirthdayYear(), pUser->GetAge(), pUser->GetGender());

  bout << "|#2M|#9) Comp         : |#1"
       << (pUser->GetComputerType() == -1 ? "Waiting for answer" : ctypes(pUser->GetComputerType()))
       << wwiv::endl;
  if (pUser->GetForwardUserNumber() != 0) {
    bout << "|#9   Forwarded To : |#1";
    if (pUser->GetForwardSystemNumber() != 0) {
      if (session()->GetMaxNetworkNumber() > 1) {
        bout << net_networks[ pUser->GetForwardNetNumber() ].name <<
                           " #" << pUser->GetForwardUserNumber() <<
                           " @" << pUser->GetForwardSystemNumber() << wwiv::endl;
      } else {
        bout << "#" << pUser->GetForwardUserNumber() << " @" << pUser->GetForwardSystemNumber() << wwiv::endl;
      }
    } else {
      bout << "#" << pUser->GetForwardUserNumber() << wwiv::endl;
    }
  }
  if (bLongFormat) {

    bout << "|#2H|#9) Password     : |#1";
    if (AllowLocalSysop()) {
      session()->localIO()->LocalPuts(pUser->GetPassword());
    }

    if (incom && session()->user()->GetSl() == 255) {
      rputs(pUser->GetPassword());
    } else {
      rputs("(not shown remotely)");
    }
    bout.nl();

    bout << "|#9   First/Last On: |#9(Last: |#1" << pUser->GetLastOn() << "|#9)   (First: |#1" <<
                       pUser->GetFirstOn() << "|#9)\r\n";

    bout.bprintf("|#9   Message Stats: |#9(Post:|#1%u|#9) (Email:|#1%u|#9) (Fd:|#1%u|#9) (Wt:|#1%u|#9) (Net:|#1%u|#9) (Del:|#1%u|#9)\r\n",
                                      pUser->GetNumMessagesPosted(), pUser->GetNumEmailSent(),
                                      pUser->GetNumFeedbackSent(), pUser->GetNumMailWaiting(), pUser->GetNumNetEmailSent(), pUser->GetNumDeletedPosts());

    bout.bprintf("|#9   Call Stats   : |#9(Total: |#1%u|#9) (Today: |#1%d|#9) (Illegal: |#6%d|#9)\r\n",
                                      pUser->GetNumLogons(), (!IsEquals(pUser->GetLastOn(), date())) ? 0 : pUser->GetTimesOnToday(),
                                      pUser->GetNumIllegalLogons());

    bout.bprintf("|#9   Up/Dnld Stats: |#9(Up: |#1%u |#9files in |#1%lu|#9k)  (Dn: |#1%u |#9files in |#1%lu|#9k)\r\n",
                                      pUser->GetFilesUploaded(), pUser->GetUploadK(), pUser->GetFilesDownloaded(), pUser->GetDownloadK());

    bout << "|#9   Last Baud    : |#1" << pUser->GetLastBaudRate() << wwiv::endl;
  }
  if (pUser->GetNote()[0] != '\0') {
    bout << "|#2O|#9) Sysop Note   : |#1" << pUser->GetNote() << wwiv::endl;
  }
  if (pUser->GetAssPoints()) {
    bout << "|#9   Ass Points   : |#1" << pUser->GetAssPoints() << wwiv::endl;
  }
  bout << "|#2S|#9) SL           : |#1" << pUser->GetSl() << wwiv::endl;
  bout << "|#2T|#9) DSL          : |#1" << pUser->GetDsl() << wwiv::endl;
  if (u_qsc) {
    if ((*u_qsc) != 999) {
      bout << "|#9    Sysop Sub #: |#1" << *u_qsc << wwiv::endl;
    }
  }
  if (pUser->GetExempt() != 0) {
    bout.bprintf("|#9   Exemptions   : |#1%s %s %s %s %s (%d)\r\n",
                                      pUser->IsExemptRatio() ? "XFER" : "    ",
                                      pUser->IsExemptTime() ? "TIME" : "    ",
                                      pUser->IsExemptPost() ? "POST" : "    ",
                                      pUser->IsExemptAll() ? "ALL " : "    ",
                                      pUser->IsExemptAutoDelete() ? "ADEL" : "    ",
                                      pUser->GetExempt());
  }
  strcpy(s3, restrict_string);
  for (int i = 0; i <= 15; i++) {
    if (pUser->HasArFlag(1 << i)) {
      s[i] = static_cast<char>('A' + i);
    } else {
      s[i] = SPACE;
    }
    if (pUser->HasDarFlag(1 << i)) {
      s1[i] = static_cast<char>('A' + i);
    } else {
      s1[i] = SPACE;
    }
    if (pUser->GetRestriction() & (1 << i)) {
      s2[i] = s3[i];
    } else {
      s2[i] = SPACE;
    }
  }
  s[16]   = '\0';
  s1[16]  = '\0';
  s2[16]  = '\0';
  if (pUser->GetAr() != 0) {
    bout << "|#2A|#9) AR Flags     : |#1" << s << wwiv::endl;
  }
  if (pUser->GetDar() != 0) {
    bout << "|#2I|#9) DAR Flags    : |#1" << s1 << wwiv::endl;
  }
  if (pUser->GetRestriction() != 0) {
    bout << "|#2Z|#9) Restrictions : |#1" << s2 << wwiv::endl;
  }
  if (pUser->GetWWIVRegNumber()) {
    bout << "|#9   WWIV Reg Num : |#1" << pUser->GetWWIVRegNumber() << wwiv::endl;
  }
  // begin callback changes

  if (bLongFormat) {
    print_affil(pUser);
    if (session()->HasConfigFlag(OP_FLAGS_CALLBACK)) {
      bout.bprintf("|#1User has%s been callback verified.  ",
                                        (pUser->GetCbv() & 1) == 0 ? " |#6not" : "");
    }
    if (session()->HasConfigFlag(OP_FLAGS_VOICE_VAL)) {
      bout.bprintf("|#1User has%s been voice verified.",
                                        (pUser->GetCbv() & 2) == 0 ? " |#6not" : "");
    }
    bout.nl(2);
  }
  // end callback changes
}

int matchuser(int nUserNumber) {
  WUser user;
  session()->users()->ReadUser(&user, nUserNumber);
  sp = search_pattern;
  return matchuser(&user);
}


int matchuser(WUser *pUser) {
  int ok = 1, _not = 0, less = 0, cpf = 0, cpp = 0;
  int  _and = 1, gotfcn = 0, evalit = 0, tmp, tmp1, tmp2;
  char fcn[20], parm[80], ts[40];
  time_t l;

  bool done = false;
  do {
    if (*sp == 0) {
      done = true;
    } else {
      if (strchr("()|&!<>", *sp)) {
        switch (*sp++) {
        case '(':
          evalit = 2;
          break;
        case ')':
          done = true;
          break;
        case '|':
          _and = 0;
          break;
        case '&':
          _and = 1;
          break;
        case '!':
          _not = 1;
          break;
        case '<':
          less = 1;
          break;
        case '>':
          less = 0;
          break;
        }
      } else if (*sp == '[') {
        gotfcn = 1;
        sp++;
      } else if (*sp == ']') {
        evalit = 1;
        ++sp;
      } else if (*sp != ' ' || gotfcn) {
        if (gotfcn) {
          if (cpp < 22) {
            parm[cpp++] = *sp++;
          } else {
            sp++;
          }
        } else {
          if (cpf < static_cast<signed int>(sizeof(fcn)) - 1) {
            fcn[ cpf++ ] = *sp++;
          } else {
            sp++;
          }
        }
      } else {
        ++sp;
      }
      if (evalit) {
        if (evalit == 1) {
          fcn[cpf] = 0;
          parm[cpp] = 0;
          tmp = 1;
          tmp1 = atoi(parm);

          if (IsEquals(fcn, "SL")) {
            if (less) {
              tmp = (tmp1 > pUser->GetSl());
            } else {
              tmp = (tmp1 < pUser->GetSl());
            }
          } else if (IsEquals(fcn, "DSL")) {
            if (less) {
              tmp = (tmp1 > pUser->GetDsl());
            } else {
              tmp = (tmp1 < pUser->GetDsl());
            }
          } else if (IsEquals(fcn, "AR")) {
            if (parm[0] >= 'A' && parm[0] <= 'P') {
              tmp1 = 1 << (parm[0] - 'A');
              tmp = pUser->HasArFlag(tmp1) ? 1 : 0;
            } else {
              tmp = 0;
            }
          } else if (IsEquals(fcn, "DAR")) {
            if ((parm[0] >= 'A') && (parm[0] <= 'P')) {
              tmp1 = 1 << (parm[0] - 'A');
              tmp = pUser->HasDarFlag(tmp1) ? 1 : 0;
            } else {
              tmp = 0;
            }
          } else if (IsEquals(fcn, "SEX")) {
            tmp = parm[0] == pUser->GetGender();
          } else if (IsEquals(fcn, "AGE")) {
            if (less) {
              tmp = (tmp1 > pUser->GetAge());
            } else {
              tmp = (tmp1 < pUser->GetAge());
            }
          } else if (IsEquals(fcn, "LASTON")) {
            time(&l);
            tmp2 = static_cast<unsigned int>((l - pUser->GetLastOnDateNumber()) / HOURS_PER_DAY_FLOAT / SECONDS_PER_HOUR_FLOAT);
            if (less) {
              tmp = tmp2 < tmp1;
            } else {
              tmp = tmp2 > tmp1;
            }
          } else if (IsEquals(fcn, "AREACODE")) {
            tmp = !strncmp(parm, pUser->GetVoicePhoneNumber(), 3);
          } else if (IsEquals(fcn, "RESTRICT")) {
            ;
          } else if (IsEquals(fcn, "LOGONS")) {
            if (less) {
              tmp = pUser->GetNumLogons() < tmp1;
            } else {
              tmp = pUser->GetNumLogons() > tmp1;
            }
          } else if (IsEquals(fcn, "REALNAME")) {
            strcpy(ts, pUser->GetRealName());
            strupr(ts);
            tmp = (strstr(ts, parm) != nullptr);
          } else if (IsEquals(fcn, "BAUD")) {
            if (less) {
              tmp = pUser->GetLastBaudRate() < tmp1;
            } else {
              tmp = pUser->GetLastBaudRate() > tmp1;
            }

            // begin callback additions

          } else if (IsEquals(fcn, "CBV")) {
            if (less) {
              tmp = pUser->GetCbv() < tmp1;
            } else {
              tmp = pUser->GetCbv() > tmp1;
            }

            // end callback additions

          } else if (IsEquals(fcn, "COMP_TYPE")) {
            tmp = pUser->GetComputerType() == tmp1;
          }
        } else {
          tmp = matchuser(pUser);
        }

        if (_not) {
          tmp = !tmp;
        }
        if (_and) {
          ok = ok && tmp;
        } else {
          ok = ok || tmp;
        }

        _not = less = cpf = cpp = gotfcn = evalit = 0;
        _and = 1;
      }
    }
  } while (!done);
  return ok;
}


void changeopt() {
  bout.cls();
  bout << "Current search string:\r\n";
  bout << ((search_pattern[0]) ? search_pattern : "-NONE-");
  bout.nl(3);
  bout << "|#9Change it? ";
  if (yesno()) {
    bout << "Enter new search pattern:\r\n|#7:";
    input(search_pattern, 75);
  }
}


void auto_val(int n, WUser *pUser) {
  if (pUser->GetSl() == 255) {
    return;
  }
  pUser->SetSl(syscfg.autoval[n].sl);
  pUser->SetDsl(syscfg.autoval[n].dsl);
  pUser->SetAr(syscfg.autoval[n].ar);
  pUser->SetDar(syscfg.autoval[n].dar);
  pUser->SetRestriction(syscfg.autoval[n].restrict);
}


/**
 * Online User Editor.
 *
 * @param usern User Number to edit
 * @param other UEDIT_NONE, UEDIT_FULLINFO, UEDIT_CLEARSCREEN
 */
void uedit(int usern, int other) {
  char s[81];
  bool bClearScreen = true;
  WUser user;

  u_qsc = static_cast<uint32_t *>(BbsAllocA(syscfg.qscn_len));

  bool full = (incom) ? false : true;
  if (other & 1) {
    full = false;
  }
  if (other & 2) {
    bClearScreen = false;
  }
  int nUserNumber = usern;
  bool bDoneWithUEdit = false;
  session()->users()->ReadUser(&user, nUserNumber);
  int nNumUserRecords = session()->users()->GetNumberOfUserRecords();
  do {
    session()->users()->ReadUser(&user, nUserNumber);
    read_qscn(nUserNumber, u_qsc, false);
    bool bDoneWithUser = false;
    bool temp_full = false;
    do {
      print_data(nUserNumber, &user, (full || temp_full) ? true : false, bClearScreen);
      bout.nl();
      bout << "|#9(|#2Q|#9=|#1Quit, |#2?|#9=|#1Help|#9) User Editor Command: ";
      char ch = 0;
      if (session()->user()->GetSl() == 255 || session()->GetWfcStatus()) {
        ch = onek("ACDEFGHILMNOPQRSTUVWXYZ0123456789[]{}/,.?~%:", true);
      } else {
        ch = onek("ACDEFGHILMNOPQRSTUWYZ0123456789[]{}/,.?%", true);
      }
      switch (ch) {
      case 'A': {
        bout.nl();
        bout << "|#7Toggle which AR? ";
        char ch1 = onek("\rABCDEFGHIJKLMNOP");
        if (ch1 != RETURN) {
          ch1 -= 'A';
          if (session()->GetWfcStatus() || (session()->user()->HasArFlag(1 << ch1))) {
            user.ToggleArFlag(1 << ch1);
            session()->users()->WriteUser(&user, nUserNumber);
          }
        }
      }
      break;
      case 'C':
        bout.nl();
        bout << "|#7New callsign? ";
        input(s, 6);
        if (s[0]) {
          user.SetCallsign(s);
          session()->users()->WriteUser(&user, nUserNumber);
        } else {
          bout << "|#5Delete callsign? ";
          if (yesno()) {
            user.SetCallsign("");
            session()->users()->WriteUser(&user, nUserNumber);
          }
        }
        break;
      case 'D':
        if (nUserNumber != 1) {
          if (!user.IsUserDeleted() && session()->GetEffectiveSl() > user.GetSl()) {
            bout << "|#5Delete? ";
            if (yesno()) {
              deluser(nUserNumber);
              session()->users()->ReadUser(&user, nUserNumber);
            }
          }
        }
        break;
      case 'E': {
        bout.nl();
        bout << "|#7New Exemption? ";
        input(s, 3);
        int nExemption = atoi(s);
        if (nExemption >= 0 && nExemption <= 255 && s[0]) {
          user.SetExempt(nExemption);
          session()->users()->WriteUser(&user, nUserNumber);
        }
      }
      break;
      case 'F': {
        int nNetworkNumber = getnetnum("FILEnet");
        session()->SetNetworkNumber(nNetworkNumber);
        if (nNetworkNumber != -1) {
          set_net_num(session()->GetNetworkNumber());
          bout << "Current Internet Address\r\n:" << user.GetEmailAddress() << wwiv::endl;
          bout << "New Address\r\n:";
          inputl(s, 75);
          if (s[0] != '\0') {
            if (check_inet_addr(s)) {
              user.SetEmailAddress(s);
              write_inet_addr(s, nUserNumber);
              user.SetForwardNetNumber(session()->GetNetworkNumber());
            } else {
              bout.nl();
              bout << "Invalid format.\r\n";
            }
          } else {
            bout.nl();
            bout << "|#5Remove current address? ";
            if (yesno()) {
              user.SetEmailAddress("");
            }
          }
          session()->users()->WriteUser(&user, nUserNumber);
        }
      }
      break;
      case 'G':
        bout << "|#5Are you sure you want to re-enter the birthday? ";
        if (yesno()) {
          bout.nl();
          bout.bprintf("Current birthdate: %02d/%02d/%02d\r\n",
                                            user.GetBirthdayMonth(), user.GetBirthdayDay(), user.GetBirthdayYear());
          input_age(&user);
          session()->users()->WriteUser(&user, nUserNumber);
        }
        break;
      case 'H':
        bout << "|#5Change this user's password? ";
        if (yesno()) {
          input_pw(&user);
          session()->users()->WriteUser(&user, nUserNumber);
        }
        break;
      case 'I': {
        bout.nl();
        bout << "|#7Toggle which DAR? ";
        char ch1 = onek("\rABCDEFGHIJKLMNOP");
        if (ch1 != RETURN) {
          ch1 -= 'A';
          if (session()->GetWfcStatus() || (session()->user()->HasDarFlag(1 << ch1))) {
            user.ToggleDarFlag(1 << ch1);
            session()->users()->WriteUser(&user, nUserNumber);
          }
        }
      }
      break;
      case 'L': {
        bout.nl();
        bout << "|#7New FULL real name? ";
        string realName = Input1(user.GetRealName(), 20, true, InputMode::PROPER);
        if (!realName.empty()) {
          user.SetRealName(realName.c_str());
          session()->users()->WriteUser(&user, nUserNumber);
        }
      }
      break;
      case 'M': {
        int nNumCompTypes = 0;
        bout.nl();
        bout << "Known computer types:\r\n\n";
        for (int nCurCompType = 0; !ctypes(nCurCompType).empty(); nCurCompType++) {
          bout << nCurCompType << ". " << ctypes(nCurCompType) << wwiv::endl;
          nNumCompTypes++;
        }
        bout.nl();
        bout << "|#7Enter new computer type: ";
        input(s, 2);
        int nComputerType = atoi(s);
        if (nComputerType >= 0 && nComputerType <= nNumCompTypes) {
          user.SetComputerType(nComputerType);
          session()->users()->WriteUser(&user, nUserNumber);
        }
      }
      break;
      case 'N':
        bout.nl();
        bout << "|#7New name? ";
        input(s, 30);
        if (s[0]) {
          if (finduser(s) < 1) {
            DeleteSmallRecord(user.GetName());
            user.SetName(s);
            InsertSmallRecord(nUserNumber, user.GetName());
            session()->users()->WriteUser(&user, nUserNumber);
          }
        }
        break;
      case 'O':
        bout.nl();
        bout << "|#7New note? ";
        inputl(s, 60);
        user.SetNote(s);
        session()->users()->WriteUser(&user, nUserNumber);
        break;

      case 'P': {
        bool bWriteUser = false;
        bout.nl();
        bout << "|#7New phone number? ";
        string phoneNumber = Input1(user.GetVoicePhoneNumber(), 12, true, InputMode::PHONE);
        if (!phoneNumber.empty()) {
          if (phoneNumber != user.GetVoicePhoneNumber()) {
            delete_phone_number(nUserNumber, user.GetVoicePhoneNumber());
            add_phone_number(nUserNumber, phoneNumber.c_str());
          }
          user.SetVoicePhoneNumber(phoneNumber.c_str());
          bWriteUser = true;
        }
        bout.nl();
        bout << "|#7New DataPhone (0=none)? ";
        phoneNumber = Input1(user.GetDataPhoneNumber(), 12, true, InputMode::PHONE);
        if (!phoneNumber.empty()) {
          if (phoneNumber[0] == '0') {
            user.SetDataPhoneNumber("");
          } else {
            if (phoneNumber != user.GetDataPhoneNumber()) {
              delete_phone_number(nUserNumber, user.GetDataPhoneNumber());
              add_phone_number(nUserNumber, phoneNumber.c_str());
            }
            user.SetDataPhoneNumber(phoneNumber.c_str());
          }
          bWriteUser = true;
        }
        if (bWriteUser) {
          session()->users()->WriteUser(&user, nUserNumber);
        }
      }
      break;
      case 'V': {
        bool bWriteUser = false;
        if (session()->HasConfigFlag(OP_FLAGS_CALLBACK)) {
          bout << "|#7Toggle callback verify flag (y/N) ? ";
          if (yesno()) {
            if (user.GetCbv() & 1) {
              session()->user()->SetSl(syscfg.newusersl);
              session()->user()->SetDsl(syscfg.newuserdsl);
              session()->user()->SetRestriction(syscfg.newuser_restrict);
              user.SetExempt(0);
              user.SetAr(0);
              user.SetDar(0);
              user.SetCbv(user.GetCbv() - 1);
            } else {
              if (user.GetSl() < session()->cbv.sl) {
                user.SetSl(session()->cbv.sl);
              }
              if (user.GetDsl() < session()->cbv.dsl) {
                user.SetDsl(session()->cbv.dsl);
              }
              user.SetRestriction(user.GetRestriction() | session()->cbv.restrict);
              user.SetExempt(user.GetExempt() | session()->cbv.exempt);
              user.SetArFlag(session()->cbv.ar);
              user.SetDarFlag(session()->cbv.dar);
              user.SetCbv(user.GetCbv() | 1);
            }
            bWriteUser = true;
          }
        }
        if (session()->HasConfigFlag(OP_FLAGS_VOICE_VAL)) {
          bout << "|#7Toggle voice validated flag (y/N) ? ";
          if (yesno()) {
            if (user.GetCbv() & 2) {
              user.SetCbv(user.GetCbv() - 2);
            } else {
              user.SetCbv(user.GetCbv() | 2);
            }
            bWriteUser = true;
          }
        }
        if (bWriteUser) {
          session()->users()->WriteUser(&user, nUserNumber);
        }
      }
      break;
      case 'Q':
        bDoneWithUEdit = true;
        bDoneWithUser = true;
        break;
      case 'R':
        if (user.IsUserDeleted()) {
          user.ToggleInactFlag(WUser::userDeleted);
          InsertSmallRecord(nUserNumber, user.GetName());
          session()->users()->WriteUser(&user, nUserNumber);

          // begin dupphone additions

          if (user.GetVoicePhoneNumber()[0]) {
            add_phone_number(nUserNumber, user.GetVoicePhoneNumber());
          }
          if (user.GetDataPhoneNumber()[0] &&
              !IsEquals(user.GetVoicePhoneNumber(),  user.GetDataPhoneNumber())) {
            add_phone_number(nUserNumber, user.GetDataPhoneNumber());
          }

          // end dupphone additions

        }
        break;
      case 'S': {
        if (user.GetSl() >= session()->GetEffectiveSl()) {
          break;
        }
        bout.nl();
        bout << "|#7New SL? ";
        string sl;
        input(&sl, 3);
        int nNewSL = atoi(sl.c_str());
        if (!session()->GetWfcStatus() && nNewSL >= session()->GetEffectiveSl() && nUserNumber != 1) {
          bout << "|#6You can not assign a Security Level to a user that is higher than your own.\r\n";
          pausescr();
          nNewSL = -1;
        }
        if (nNewSL >= 0 && nNewSL < 255 && sl[0]) {
          user.SetSl(nNewSL);
          session()->users()->WriteUser(&user, nUserNumber);
          if (nUserNumber == session()->usernum) {
            session()->SetEffectiveSl(nNewSL);
          }
        }
      }
      break;
      case 'T': {
        if (user.GetDsl() >= session()->user()->GetDsl()) {
          break;
        }
        bout.nl();
        bout << "|#7New DSL? ";
        string dsl;
        input(&dsl, 3);
        int nNewDSL = atoi(dsl.c_str());
        if (!session()->GetWfcStatus() && nNewDSL >= session()->user()->GetDsl() && nUserNumber != 1) {
          bout << "|#6You can not assign a Security Level to a user that is higher than your own.\r\n";
          pausescr();
          nNewDSL = -1;
        }
        if (nNewDSL >= 0 && nNewDSL < 255 && dsl[0]) {
          user.SetDsl(nNewDSL);
          session()->users()->WriteUser(&user, nUserNumber);
        }
      }
      break;
      case 'U': {
        bout.nl();
        bout << "|#7User name/number: ";
        string name;
        input(&name, 30);
        int nFoundUserNumber = finduser1(name.c_str());
        if (nFoundUserNumber > 0) {
          nUserNumber = nFoundUserNumber;
          bDoneWithUser = true;
        }
      }
      break;
      // begin callback additions
      case 'W':
        wwivnode(&user, 1);
        session()->users()->WriteUser(&user, nUserNumber);
        break;
      // end callback additions
      case 'X': {
        string regDate, expDate;
        if (!session()->HasConfigFlag(OP_FLAGS_USER_REGISTRATION)) {
          break;
        }
        bout.nl();
        if (user.GetRegisteredDateNum() != 0) {
          regDate = daten_to_date(user.GetRegisteredDateNum());
          expDate = daten_to_date(user.GetExpiresDateNum());
          bout << "Registered on " << regDate << ", expires on " << expDate << wwiv::endl;
        } else {
          bout << "Not registered.\r\n";
        }
        string newRegDate;
        do {
          bout.nl();
          bout << "Enter registration date, <CR> for today: \r\n";
          bout << " MM/DD/YY\r\n:";
          input(&newRegDate, 8);
        } while (newRegDate.length() != 8 && !newRegDate.empty());

        if (newRegDate.empty()) {
          newRegDate = date();
        }

        int m = atoi(newRegDate.c_str());
        int dd = atoi(&(newRegDate[3]));
        if (newRegDate.length() == 8 && m > 0 && m <= 12 && dd > 0 && dd < 32) {
          user.SetRegisteredDateNum(date_to_daten(newRegDate.c_str()));
        } else {
          bout.nl();
        }
        string newExpDate;
        do {
          bout.nl();
          bout << "Enter expiration date, <CR> to clear registration fields: \r\n";
          bout << " MM/DD/YY\r\n:";
          input(&newExpDate, 8);
        } while (newExpDate.length() != 8 && !newExpDate.empty());
        if (newExpDate.length() == 8) {
          m = atoi(newExpDate.c_str());
          dd = atoi(&(newExpDate[3]));
        } else {
          user.SetRegisteredDateNum(0);
          user.SetExpiresDateNum(0);
        }
        if (newExpDate.length() == 8 && m > 0 && m <= 12 && dd > 0 && dd < 32) {
          user.SetExpiresDateNum(date_to_daten(newExpDate.c_str()));
        }
        session()->users()->WriteUser(&user, nUserNumber);
      }
      break;
      case 'Y':
        if (u_qsc) {
          bout.nl();
          bout << "|#7(999=None) New sysop sub? ";
          string sysopSubNum;
          input(&sysopSubNum, 3);
          int nSysopSubNum = atoi(sysopSubNum.c_str());
          if (nSysopSubNum >= 0 && nSysopSubNum <= 999 && !sysopSubNum.empty()) {
            *u_qsc = nSysopSubNum;
            write_qscn(nUserNumber, u_qsc, false);
          }
        }
        break;
      case 'Z': {
        char ch1;
        bout.nl();
        bout <<  "        " << restrict_string << wwiv::endl;
        do {
          bout << "|#7([Enter]=Quit, ?=Help) Enter Restriction to Toggle? ";
          s[0] = RETURN;
          s[1] = '?';
          strcpy(&(s[2]), restrict_string);
          ch1 = onek(s);
          if (ch1 == SPACE) {
            ch1 = RETURN;
          }
          if (ch1 == '?') {
            bout.cls();
            printfile(SRESTRCT_NOEXT);
          }
          if (ch1 != RETURN && ch1 != '?') {
            int nRestriction = -1;
            for (int i1 = 0; i1 < 16; i1++) {
              if (ch1 == s[i1 + 2]) {
                nRestriction = i1;
              }
            }
            if (nRestriction > -1) {
              user.ToggleRestrictionFlag(1 << nRestriction);
              session()->users()->WriteUser(&user, nUserNumber);
            }
          }
        } while (!hangup && ch1 == '?');
      }
      break;
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
      case '0': {
        int nAutoValNum = 9;
        if (ch != '0') {
          nAutoValNum = ch - '1';
        }
        if ((session()->GetEffectiveSl() >= syscfg.autoval[nAutoValNum].sl) &&
            (session()->user()->GetDsl() >= syscfg.autoval[nAutoValNum].dsl) &&
            (((~session()->user()->GetAr()) & syscfg.autoval[nAutoValNum].ar) == 0) &&
            (((~session()->user()->GetDar()) & syscfg.autoval[nAutoValNum].dar) == 0)) {
          auto_val(nAutoValNum, &user);
          session()->users()->WriteUser(&user, nUserNumber);
        }
      }
      break;
      case ']':
        ++nUserNumber;
        if (nUserNumber > nNumUserRecords) {
          nUserNumber = 1;
        }
        bDoneWithUser = true;
        break;
      case '[':
        --nUserNumber;
        if (nUserNumber == 0) {
          nUserNumber = nNumUserRecords;
        }
        bDoneWithUser = true;
        break;
      case '}': {
        int tempu = nUserNumber;
        ++nUserNumber;
        if (nUserNumber > nNumUserRecords) {
          nUserNumber = 1;
        }
        while (nUserNumber != tempu && !matchuser(nUserNumber)) {
          ++nUserNumber;
          if (nUserNumber > nNumUserRecords) {
            nUserNumber = 1;
          }
        }
        bDoneWithUser = true;
      }
      break;
      case '{': {
        int tempu = nUserNumber;
        --nUserNumber;
        if (nUserNumber < 1) {
          nUserNumber = nNumUserRecords;
        }
        while (nUserNumber != tempu && !matchuser(nUserNumber)) {
          --nUserNumber;
          if (nUserNumber < 1) {
            nUserNumber = nNumUserRecords;
          }
        }
        bDoneWithUser = true;
      }
      break;
      case '/':
        changeopt();
        break;
      case ',':
        temp_full = (!temp_full);
        break;
      case '.':
        full = (!full);
        temp_full = full;
        break;
      case '?': {
        bout.cls();
        printfile(SUEDIT_NOEXT);
        getkey();
      }
      break;
      case '~':
        user.SetAssPoints(0);
        session()->users()->WriteUser(&user, nUserNumber);
        break;
      case '%': {
        char s1[ 255];
        bout.nl();
        bout << "|#7New Street Address? ";
        inputl(s1, 30);
        if (s1[0]) {
          user.SetStreet(s1);
          session()->users()->WriteUser(&user, nUserNumber);
        }
        bout << "|#7New City? ";
        inputl(s1, 30);
        if (s1[0]) {
          user.SetCity(s1);
          session()->users()->WriteUser(&user, nUserNumber);
        }
        bout << "|#7New State? ";
        input(s1, 2);
        if (s1[0]) {
          user.SetState(s1);
          session()->users()->WriteUser(&user, nUserNumber);
        }
        bout << "|#7New Country? ";
        input(s1, 3);
        if (s1[0]) {
          user.SetCountry(s1);
          session()->users()->WriteUser(&user, nUserNumber);
        }
        bout << "|#7New Zip? ";
        input(s1, 10);
        if (s1[0]) {
          user.SetZipcode(s1);
          session()->users()->WriteUser(&user, nUserNumber);
        }
        bout << "|#7New DataPhone (0=none)? ";
        input(s1, 12);
        if (s1[0]) {
          user.SetDataPhoneNumber(s1);
          session()->users()->WriteUser(&user, nUserNumber);
        }
      }
      break;
      case ':': {
        bool done2 = false;
        do {
          bout.nl();
          bout << "Zap info...\r\n";
          bout << "1. Realname\r\n";
          bout << "2. Birthday\r\n";
          bout << "3. Street address\r\n";
          bout << "4. City\r\n";
          bout << "5. State\r\n";
          bout << "6. Country\r\n";
          bout << "7. Zip Code\r\n";
          bout << "8. DataPhone\r\n";
          bout << "9. Computer\r\n";
          bout << "Q. Quit\r\n";
          bout << "Which? ";
          char ch1 = onek("Q123456789");
          switch (ch1) {
          case '1':
            user.SetRealName("");
            break;
          case '2':
            user.SetBirthdayYear(0);
            break;
          case '3':
            user.SetStreet("");
            break;
          case '4':
            user.SetCity("");
            break;
          case '5':
            user.SetState("");
            break;
          case '6':
            user.SetCountry("");
            break;
          case '7':
            user.SetZipcode("");
            break;
          case '8':
            user.SetDataPhoneNumber("");
            break;
          case '9':
            // rf-I don't know why this is 254, it was in 4.31
            // but it probably shouldn't be. Changed to 0
            user.SetComputerType(0);
            break;
          case 'Q':
            done2 = true;
            break;
          }
        } while (!done2);
        session()->users()->WriteUser(&user, nUserNumber);
      }
      break;
      }
    } while (!bDoneWithUser && !hangup);
  } while (!bDoneWithUEdit && !hangup);

  if (u_qsc) {
    free(u_qsc);
  }

  u_qsc = nullptr;
}


char *daten_to_date(time_t dt) {
  static char s[9];
  struct tm * pTm = localtime(&dt);

  if (pTm) {
    sprintf(s, "%02d/%02d/%02d", pTm->tm_mon, pTm->tm_mday, pTm->tm_year % 100);
  } else {
    strcpy(s, "01/01/00");
  }
  return s;
}


void print_affil(WUser *pUser) {
  net_system_list_rec *csne;

  if (pUser->GetForwardNetNumber() == 0 || pUser->GetHomeSystemNumber() == 0) {
    return;
  }
  set_net_num(pUser->GetForwardNetNumber());
  csne = next_system(pUser->GetHomeSystemNumber());
  bout << "|#2   Sysp    : |#1";
  if (csne) {
    bout << "@" << pUser->GetHomeSystemNumber() << ", " << csne->name << ", on " <<
                       session()->GetNetworkName() << ".";
  } else {
    bout << "@" << pUser->GetHomeSystemNumber() << ", <UNKNOWN>, on " << session()->GetNetworkName() <<
                       ".";
  }
  bout.nl(2);
}
