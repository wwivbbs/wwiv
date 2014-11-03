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
#include <limits>
#include <memory>
#include <string>

#include "bbs/instmsg.h"
#include "bbs/menusupp.h"
#include "bbs/printfile.h"
#include "bbs/stuffin.h"
#include "bbs/wcomm.h"
#include "bbs/wconstants.h"
#include "bbs/wstatus.h"
#include "core/inifile.h"
#include "core/strings.h"
#include "core/wutil.h"
#include "core/wwivassert.h"

using std::string;
using wwiv::core::IniFile;
using wwiv::core::FilePath;
using wwiv::strings::StringPrintf;

#define SECS_PER_DAY 86400L

static char g_szLastLoginDate[9];

/*
 * Enable the following line if you need remote.exe or to be able to launch
 * commands specified in remotes.dat.  This is disabled by default
 */
// #define ENABLE_REMOTE_VIA_SPECIAL_LOGINS

//
// Local functions
//

static void CleanUserInfo() {
  if (okconf(GetSession()->GetCurrentUser())) {
    setuconf(CONF_SUBS, GetSession()->GetCurrentUser()->GetLastSubConf(), 0);
    setuconf(CONF_DIRS, GetSession()->GetCurrentUser()->GetLastDirConf(), 0);
  }
  if (GetSession()->GetCurrentUser()->GetLastSubNum() > GetSession()->GetMaxNumberMessageAreas()) {
    GetSession()->GetCurrentUser()->SetLastSubNum(0);
  }
  if (GetSession()->GetCurrentUser()->GetLastDirNum() > GetSession()->GetMaxNumberFileAreas()) {
    GetSession()->GetCurrentUser()->SetLastDirNum(0);
  }
  if (usub[GetSession()->GetCurrentUser()->GetLastSubNum()].subnum != -1) {
    GetSession()->SetCurrentMessageArea(GetSession()->GetCurrentUser()->GetLastSubNum());
  }
  if (udir[GetSession()->GetCurrentUser()->GetLastDirNum()].subnum != -1) {
    GetSession()->SetCurrentFileArea(GetSession()->GetCurrentUser()->GetLastDirNum());
  }

}

static bool random_screen(const char *mpfn) {
  char szBuffer[ 255 ];
  sprintf(szBuffer, "%s%s%s", GetSession()->language_dir.c_str(), mpfn, ".0");
  if (WFile::Exists(szBuffer)) {
    int nNumberOfScreens = 0;
    for (int i = 0; i < 1000; i++) {
      sprintf(szBuffer, "%s%s.%d", GetSession()->language_dir.c_str(), mpfn, i);
      if (WFile::Exists(szBuffer)) {
        nNumberOfScreens++;
      } else {
        break;
      }
    }
    sprintf(szBuffer, "%s%s.%d", GetSession()->language_dir.c_str(), mpfn, WWIV_GetRandomNumber(nNumberOfScreens));
    printfile(szBuffer);
    return true;
  }
  return false;
}

bool IsPhoneNumberUSAFormat(WUser *pUser) {
  string country = pUser->GetCountry();
  return (country == "USA" || country == "CAN" || country == "MEX");
}

static int GetNetworkOnlyStatus() {
  int nNetworkOnly = 1;
  if (syscfg.netlowtime != syscfg.nethightime) {
    if (syscfg.nethightime > syscfg.netlowtime) {
      if ((timer() <= (syscfg.netlowtime * SECONDS_PER_MINUTE_FLOAT))
          || (timer() >= (syscfg.nethightime * SECONDS_PER_MINUTE_FLOAT))) {
        nNetworkOnly = 0;
      }
    } else {
      if ((timer() <= (syscfg.netlowtime * SECONDS_PER_MINUTE_FLOAT))
          && (timer() >= (syscfg.nethightime * SECONDS_PER_MINUTE_FLOAT))) {
        nNetworkOnly = 0;
      }
    }
  } else {
    nNetworkOnly = 0;
  }
  return nNetworkOnly;
}

static int GetAnsiStatusAndShowWelcomeScreen(int nNetworkOnly) {
  int ans = -1;
  if (!nNetworkOnly && incom) {
    if (GetSession()->GetCurrentSpeed().length() > 0) {
      char szCurrentSpeed[ 81 ];
      strcpy(szCurrentSpeed, GetSession()->GetCurrentSpeed().c_str());
      bout << "CONNECT " << strupr(szCurrentSpeed) << "\r\n\r\n";
    }
    string osVersion = WWIV_GetOSVersion();
    bout << "\r\nWWIV " << wwiv_version << "/" << osVersion << " " << beta_version << wwiv::endl;
    bout << "Copyright (c) 1998-2014 WWIV Software Services." << wwiv::endl;
    bout << "All Rights Reserved." << wwiv::endl;

    ans = check_ansi();
    char szFileName[ MAX_PATH ];
    sprintf(szFileName, "%s%s", GetSession()->language_dir.c_str(), WELCOME_ANS);
    if (WFile::Exists(szFileName)) {
      bout.nl();
      if (ans > 0) {
        GetSession()->GetCurrentUser()->SetStatusFlag(WUser::ansi);
        GetSession()->GetCurrentUser()->SetStatusFlag(WUser::color);
        if (!random_screen(WELCOME_NOEXT)) {
          printfile(WELCOME_ANS);
        }
      } else if (ans == 0) {
        printfile(WELCOME_MSG);
      }
    } else {
      if (ans) {
        sprintf(szFileName, "%s%s.0", GetSession()->language_dir.c_str(), WELCOME_NOEXT);
        if (WFile::Exists(szFileName)) {
          random_screen(WELCOME_NOEXT);
        } else {
          printfile(WELCOME_MSG);
        }
      } else {
        printfile(WELCOME_MSG);
      }
    }
  }
  if (curatr != 7) {
    bout.ResetColors();
  }
  return ans;
}


int ShowLoginAndGetUserNumber(int nNetworkOnly, char* pszUserName) {
  char szUserName[ 255 ];

  bout.nl();
  if (nNetworkOnly) {
    bout << "This time is reserved for net-mail ONLY.  Please try calling back again later.\r\n";
  } else {
    bout << "Enter number or name or 'NEW'\r\n";
  }

  memset(&szUserName, 0, 255);

  bout << "NN: ";
  input(szUserName, 30);
  StringTrim(szUserName);

  int nUserNumber = finduser(szUserName);
  if (nUserNumber == 0 && szUserName[ 0 ] != '\0') {
    bout << "Searching...";
    bool abort = false;
    for (int i = 1; i < GetApplication()->GetStatusManager()->GetUserCount() && nUserNumber == 0 && !hangup
         && !abort; i++) {
      if (i % 25 == 0) {   // changed from 15 since computers are faster now-a-days
        bout << ".";
      }
      int nTempUserNumber = smallist[i].number;
      GetSession()->ReadCurrentUser(nTempUserNumber);
      if (szUserName[ 0 ] == GetSession()->GetCurrentUser()->GetRealName()[ 0 ]) {
        char szTempUserName[ 255 ];
        strcpy(szTempUserName, GetSession()->GetCurrentUser()->GetRealName());
        if (wwiv::strings::IsEquals(szUserName, strupr(szTempUserName)) &&
            !GetSession()->GetCurrentUser()->IsUserDeleted()) {
          bout << "|#5Do you mean " << GetSession()->GetCurrentUser()->GetUserNameAndNumber(i) << "? ";
          if (yesno()) {
            nUserNumber = nTempUserNumber;
          }
        }
      }
      checka(&abort);
    }
  }
  strcpy(pszUserName, szUserName);
  return nUserNumber;
}


bool IsPhoneRequired() {
  IniFile iniFile(FilePath(GetApplication()->GetHomeDir(), WWIV_INI), INI_TAG);
  if (iniFile.IsOpen()) {
    if (iniFile.GetBooleanValue("NEWUSER_MIN")) {
      return false;
    }
    if (!iniFile.GetBooleanValue("LOGON_PHONE")) {
      return false;
    }
  }
  return true;
}

bool VerifyPhoneNumber() {
  if (IsPhoneNumberUSAFormat(GetSession()->GetCurrentUser()) || !GetSession()->GetCurrentUser()->GetCountry()[0]) {
    string phoneNumber;
    input_password("PH: ###-###-", &phoneNumber, 4);

    if (phoneNumber != &GetSession()->GetCurrentUser()->GetVoicePhoneNumber()[8]) {
      if (phoneNumber.length() == 4 && phoneNumber[3] == '-') {
        bout << "\r\n!! Enter the LAST 4 DIGITS of your phone number ONLY !!\r\n\n";
      }
      return false;
    }
  }
  return true;
}

static bool VerifyPassword() {
  GetApplication()->UpdateTopScreen();

  string password;
  input_password("PW: ", &password, 8);

  return (password == GetSession()->GetCurrentUser()->GetPassword());
}

static bool VerifySysopPassword() {
  string password;
  input_password("SY: ", &password, 20);
  return (password == syscfg.systempw) ? true : false;
}

static void DoFailedLoginAttempt() {
  GetSession()->GetCurrentUser()->SetNumIllegalLogons(GetSession()->GetCurrentUser()->GetNumIllegalLogons() + 1);
  GetSession()->WriteCurrentUser();
  bout << "\r\n\aILLEGAL LOGON\a\r\n\n";

  std::stringstream logLine;
  logLine << "### ILLEGAL LOGON for " <<
          GetSession()->GetCurrentUser()->GetUserNameAndNumber(GetSession()->usernum) << " (" <<
          ctim(timer()) << ")";
  sysoplog("", false);
  sysoplog(logLine.str(), false);
  sysoplog("", false);
  GetSession()->usernum = 0;
}

void ExecuteWWIVNetworkRequest(const char *pszUserName) {
  char szUserName[ 255 ];
  strcpy(szUserName, pszUserName);
  if (incom) {
    hangup = true;
    return;
  }

  GetApplication()->GetStatusManager()->RefreshStatusCache();
  long lTime = time(nullptr);
  switch (GetSession()->usernum) {
  case -2: {
    std::stringstream networkCommand;
    networkCommand << "network /B" << modem_speed << " /T" << lTime << " /F0";
    write_inst(INST_LOC_NET, 0, INST_FLAGS_NONE);
    ExecuteExternalProgram(networkCommand.str().c_str(), EFLAG_NONE);
    if (GetApplication()->GetInstanceNumber() != 1) {
      send_inst_cleannet();
    }
    set_net_num(0);
  }
  break;
#ifdef ENABLE_REMOTE_VIA_SPECIAL_LOGINS
  case -3: {
    std::stringstream networkCommand;
    networkCommand << "REMOTE /B" << modem_speed << " /F0";
    ExecuteExternalProgram(networkCommand.str().c_str() , EFLAG_NONE);
  }
  break;
  case -4: {
    szUserName[8] = '\0';
    if (szUserName[0]) {
      std::stringstream networkCommand;
      networkCommand << szUserName << " /B" << modem_speed << " /F0";
      std::stringstream remoteDatFileName;
      remoteDatFileName << syscfg.datadir << REMOTES_DAT;
      WTextFile file(remoteDatFileName.str(), "rt");
      if (file.IsOpen()) {
        bool ok = false;
        char szBuffer[ 255 ];
        while (!ok && file.ReadLine(szBuffer, 80)) {
          char* ss = strchr(szBuffer, '\n');
          if (ss) {
            *ss = 0;
          }
          if (wwiv::strings::IsEqualsIgnoreCase(szBuffer, szUserName)) {
            ok = true;
          }
        }
        file.Close();
        if (ok) {
          ExecuteExternalProgram(networkCommand.str().c_str(), EFLAG_NONE);
        }
      }
    }
  }
  break;
#endif // ENABLE_REMOTE_VIA_SPECIAL_LOGINS
  }
  GetApplication()->GetStatusManager()->RefreshStatusCache();
  hangup = true;
  GetSession()->remoteIO()->dtr(false);
  global_xx = false;
  Wait(1.0);
  GetSession()->remoteIO()->dtr(true);
  Wait(0.1);
  cleanup_net();
  hangup = true;
}

static void LeaveBadPasswordFeedback(int ans) {
  if (ans > 0) {
    GetSession()->GetCurrentUser()->SetStatusFlag(WUser::ansi);
  } else {
    GetSession()->GetCurrentUser()->ClearStatusFlag(WUser::ansi);
  }
  bout << "|#6Too many logon attempts!!\r\n\n";
  bout << "|#9Would you like to leave Feedback to " << syscfg.sysopname << "? ";
  if (yesno()) {
    bout.nl();
    bout << "What is your NAME or HANDLE? ";
    string tempName;
    input1(&tempName, 31, wwiv::bbs::InputMode::PROPER, true);
    if (!tempName.empty()) {
      bout.nl();
      GetSession()->usernum = 1;
      sprintf(irt, "** Illegal logon feedback from %s", tempName.c_str());
      GetSession()->GetCurrentUser()->SetName(tempName.c_str());
      GetSession()->GetCurrentUser()->SetMacro(0, "");
      GetSession()->GetCurrentUser()->SetMacro(1, "");
      GetSession()->GetCurrentUser()->SetMacro(2, "");
      GetSession()->GetCurrentUser()->SetSl(syscfg.newusersl);
      GetSession()->GetCurrentUser()->SetScreenChars(80);
      if (ans > 0) {
        select_editor();
      } else {
        GetSession()->GetCurrentUser()->SetDefaultEditor(0);
      }
      GetSession()->GetCurrentUser()->SetNumEmailSent(0);
      bool bSaveAllowCC = GetSession()->IsCarbonCopyEnabled();
      GetSession()->SetCarbonCopyEnabled(false);
      email(1, 0, true, 0, true);
      GetSession()->SetCarbonCopyEnabled(bSaveAllowCC);
      if (GetSession()->GetCurrentUser()->GetNumEmailSent() > 0) {
        ssm(1, 0, "Check your mailbox.  Someone forgot their password again!");
      }
    }
  }
  GetSession()->usernum = 0;
  hangup = 1;
}

static void CheckCallRestrictions() {
  if (!hangup && GetSession()->usernum > 0 && GetSession()->GetCurrentUser()->IsRestrictionLogon() &&
      wwiv::strings::IsEquals(date(), GetSession()->GetCurrentUser()->GetLastOn()) &&
      GetSession()->GetCurrentUser()->GetTimesOnToday() > 0) {
    bout.nl();
    bout << "|#6Sorry, you can only logon once per day.\r\n";
    hangup = true;
  }
}

static void DoCallBackVerification() {
  // TODO(rushfan): This is where we would do internet email validation.
}

void getuser() {
  char szUserName[ 255 ];
  write_inst(INST_LOC_GETUSER, 0, INST_FLAGS_NONE);

  int nNetworkOnly = GetNetworkOnlyStatus();
  int count = 0;
  bool ok = false;

  okmacro = false;
  GetSession()->SetCurrentConferenceMessageArea(0);
  GetSession()->SetCurrentConferenceFileArea(0);
  GetSession()->SetEffectiveSl(syscfg.newusersl);
  GetSession()->GetCurrentUser()->SetStatus(0);

  int ans = GetAnsiStatusAndShowWelcomeScreen(nNetworkOnly);

  do {
    GetSession()->usernum = ShowLoginAndGetUserNumber(nNetworkOnly, szUserName);

    if (nNetworkOnly && GetSession()->usernum != -2) {
      if (GetSession()->usernum != -4 ||
          !wwiv::strings::IsEquals(szUserName, "DNM")) {
        GetSession()->usernum = 0;
      }
    }
    if (GetSession()->usernum > 0) {
      GetSession()->ReadCurrentUser();
      read_qscn(GetSession()->usernum, qsc, false);
      if (!set_language(GetSession()->GetCurrentUser()->GetLanguage())) {
        GetSession()->GetCurrentUser()->SetLanguage(0);
        set_language(0);
      }
      int nInstanceNumber;
      if (GetSession()->GetCurrentUser()->GetSl() < 255 && user_online(GetSession()->usernum, &nInstanceNumber)) {
        bout << "\r\n|#6You are already online on instance " << nInstanceNumber << "!\r\n\n";
        continue;
      }
      ok = true;
      if (guest_user) {
        logon_guest();
      } else {
        GetSession()->SetEffectiveSl(syscfg.newusersl);
        if (!VerifyPassword()) {
          ok = false;
        }

        if ((syscfg.sysconfig & sysconfig_free_phone) == 0 && IsPhoneRequired()) {
          if (!VerifyPhoneNumber()) {
            ok = false;
          }
        }
        if (GetSession()->GetCurrentUser()->GetSl() == 255 && incom && ok) {
          if (!VerifySysopPassword()) {
            ok = false;
          }
        }
      }
      if (ok) {
        GetSession()->ResetEffectiveSl();
        changedsl();
      } else {
        DoFailedLoginAttempt();
      }
    } else if (GetSession()->usernum == 0) {
      bout.nl();
      if (!nNetworkOnly) {
        bout << "|#6Unknown user.\r\n";
      }
    } else if (GetSession()->usernum == -1) {
      write_inst(INST_LOC_NEWUSER, 0, INST_FLAGS_NONE);
      play_sdf(NEWUSER_NOEXT, false);
      newuser();
      ok = true;
    } else if (GetSession()->usernum == -2 || GetSession()->usernum == -3 || GetSession()->usernum == -4) {
      ExecuteWWIVNetworkRequest(szUserName);
    }
  } while (!hangup && !ok && ++count < 3);

  if (count >= 3) {
    LeaveBadPasswordFeedback(ans);
  }

  okmacro = true;

  CheckCallRestrictions();

  if (GetApplication()->HasConfigFlag(OP_FLAGS_CALLBACK) && (GetSession()->GetCurrentUser()->GetCbv() & 1) == 0) {
    DoCallBackVerification();
  }
}

static void FixUserLinesAndColors() {
  if (GetSession()->GetCurrentUser()->GetNumExtended() > GetSession()->max_extend_lines) {
    GetSession()->GetCurrentUser()->SetNumExtended(GetSession()->max_extend_lines);
  }
  if (GetSession()->GetCurrentUser()->GetColor(8) == 0 || GetSession()->GetCurrentUser()->GetColor(9) == 0) {
    GetSession()->GetCurrentUser()->SetColor(8, GetSession()->newuser_colors[8]);
    GetSession()->GetCurrentUser()->SetColor(9, GetSession()->newuser_colors[9]);
  }
}

static void UpdateUserStatsForLogin() {
  strcpy(g_szLastLoginDate, date());
  if (wwiv::strings::IsEquals(g_szLastLoginDate, GetSession()->GetCurrentUser()->GetLastOn())) {
    GetSession()->GetCurrentUser()->SetTimesOnToday(GetSession()->GetCurrentUser()->GetTimesOnToday() + 1);
  } else {
    GetSession()->GetCurrentUser()->SetTimesOnToday(1);
    GetSession()->GetCurrentUser()->SetTimeOnToday(0.0);
    GetSession()->GetCurrentUser()->SetExtraTime(0.0);
    GetSession()->GetCurrentUser()->SetNumPostsToday(0);
    GetSession()->GetCurrentUser()->SetNumEmailSentToday(0);
    GetSession()->GetCurrentUser()->SetNumFeedbackSentToday(0);
  }
  GetSession()->GetCurrentUser()->SetNumLogons(GetSession()->GetCurrentUser()->GetNumLogons() + 1);
  GetSession()->SetCurrentMessageArea(0);
  GetSession()->SetNumMessagesReadThisLogon(0);
  if (udir[0].subnum == 0 && udir[1].subnum > 0) {
    GetSession()->SetCurrentFileArea(1);
  } else {
    GetSession()->SetCurrentFileArea(0);
  }
  GetSession()->SetMMKeyArea(WSession::mmkeyMessageAreas);
  if (GetSession()->GetEffectiveSl() != 255 && !guest_user) {
    WStatus* pStatus = GetApplication()->GetStatusManager()->BeginTransaction();
    pStatus->IncrementCallerNumber();
    pStatus->IncrementNumCallsToday();
    GetApplication()->GetStatusManager()->CommitTransaction(pStatus);
  }
}

static void PrintLogonFile() {
  if (!incom) {
    return;
  }
  play_sdf(LOGON_NOEXT, false);
  if (!printfile(LOGON_NOEXT)) {
    pausescr();
  }
}

static void PrintUserSpecificFiles() {
  const WUser* user = GetSession()->GetCurrentUser();  // not-owned

  const string sl_fn = StringPrintf("sl%d", user->GetSl());
  printfile(sl_fn.c_str());
  const string dsl_fn = StringPrintf("dsl%d", user->GetDsl());
  printfile(dsl_fn.c_str());

  const int short_size = std::numeric_limits<unsigned short>::digits - 1;
  for (int i=0; i < short_size; i++) {
    if (user->HasArFlag(1 << i)) {
      const string ar_fn = StringPrintf("ar%c", static_cast<char>('A' + i));
      printfile(ar_fn.c_str());
    }
  }

  for (int i=0; i < short_size; i++) {
    if (user->HasDarFlag(1 << i)) {
      const string ar_fn = StringPrintf("dar%c", static_cast<char>('A' + i));
      printfile(ar_fn.c_str());
    }
  }
}

static void UpdateLastOnFileAndUserLog() {
  char s1[181], szLastOnTxtFileName[ MAX_PATH ], szLogLine[ 255 ];
  long len;

  std::unique_ptr<WStatus> pStatus(GetApplication()->GetStatusManager()->GetStatus());

  sprintf(szLogLine, "%ld: %s %s %s   %s - %d (%u)",
          pStatus->GetCallerNumber(),
          GetSession()->GetCurrentUser()->GetUserNameAndNumber(GetSession()->usernum),
          times(),
          fulldate(),
          GetSession()->GetCurrentSpeed().c_str(),
          GetSession()->GetCurrentUser()->GetTimesOnToday(),
          GetApplication()->GetInstanceNumber());
  sprintf(szLastOnTxtFileName, "%s%s", syscfg.gfilesdir, LASTON_TXT);
  char *ss = get_file(szLastOnTxtFileName, &len);
  long pos = 0;
  if (ss != nullptr) {
    if (!cs()) {
      for (int i = 0; i < 4; i++) {
        copy_line(s1, ss, &pos, len);
      }
    }
    int i = 1;
    do {
      copy_line(s1, ss, &pos, len);
      if (s1[0]) {
        if (i) {
          bout << "\r\n\n|#1Last few callers|#7: |#0\r\n\n";
          if (GetApplication()->HasConfigFlag(OP_FLAGS_SHOW_CITY_ST) &&
              (syscfg.sysconfig & sysconfig_extended_info)) {
            bout << "|#2Number Name/Handle               Time  Date  City            ST Cty Modem    ##\r\n";
          } else {
            bout << "|#2Number Name/Handle               Language   Time  Date  Speed                ##\r\n";
          }
          unsigned char chLine = (okansi()) ? 205 : '=';
          bout << "|#7" << charstr(79, chLine) << wwiv::endl;
          i = 0;
        }
        bout << s1;
        bout.nl();
      }
    } while (pos < len);
    bout.nl(2);
    pausescr();
  }

  if (GetSession()->GetEffectiveSl() != 255 || incom) {
    sysoplog("", false);
    sysoplog(stripcolors(szLogLine), false);
    sysoplog("", false);
    string remoteAddress = GetSession()->remoteIO()->GetRemoteAddress();
    string remoteName = GetSession()->remoteIO()->GetRemoteName();
    if (remoteAddress.length() > 0) {
      sysoplogf("CID NUM : %s", remoteAddress.c_str());
    }
    if (remoteName.length() > 0) {
      sysoplogf("CID NAME: %s", remoteName.c_str());
    }
    if (GetApplication()->HasConfigFlag(OP_FLAGS_SHOW_CITY_ST) &&
        (syscfg.sysconfig & sysconfig_extended_info)) {
      sprintf(szLogLine, "|#1%-6ld %-25.25s %-5.5s %-5.5s %-15.15s %-2.2s %-3.3s %-8.8s %2d\r\n",
              pStatus->GetCallerNumber(),
              GetSession()->GetCurrentUser()->GetUserNameAndNumber(GetSession()->usernum),
              times(),
              fulldate(),
              GetSession()->GetCurrentUser()->GetCity(),
              GetSession()->GetCurrentUser()->GetState(),
              GetSession()->GetCurrentUser()->GetCountry(),
              GetSession()->GetCurrentSpeed().c_str(),
              GetSession()->GetCurrentUser()->GetTimesOnToday());
    } else {
      sprintf(szLogLine, "|#1%-6ld %-25.25s %-10.10s %-5.5s %-5.5s %-20.20s %2d\r\n",
              pStatus->GetCallerNumber(),
              GetSession()->GetCurrentUser()->GetUserNameAndNumber(GetSession()->usernum),
              cur_lang_name,
              times(),
              fulldate(),
              GetSession()->GetCurrentSpeed().c_str(),
              GetSession()->GetCurrentUser()->GetTimesOnToday());
    }

    if (GetSession()->GetEffectiveSl() != 255) {
      WFile userLog(syscfg.gfilesdir, USER_LOG);
      if (userLog.Open(WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile)) {
        userLog.Seek(0L, WFile::seekEnd);
        userLog.Write(szLogLine, strlen(szLogLine));
        userLog.Close();
      }
      WFile lastonFile(szLastOnTxtFileName);
      if (lastonFile.Open(WFile::modeReadWrite | WFile::modeBinary |
                          WFile::modeCreateFile | WFile::modeTruncate)) {
        if (ss != nullptr) {
          // Need to ensure ss is not null here
          pos = 0;
          copy_line(s1, ss, &pos, len);
          for (int i = 1; i < 8; i++) {
            copy_line(s1, ss, &pos, len);
            strcat(s1, "\r\n");
            lastonFile.Write(s1, strlen(s1));
          }
        }
        lastonFile.Write(szLogLine, strlen(szLogLine));
        lastonFile.Close();
      }
    }
  }
  if (ss != nullptr) {
    free(ss);
  }
}

static void CheckAndUpdateUserInfo() {
  fsenttoday = 0;
  if (GetSession()->GetCurrentUser()->GetBirthdayYear() == 0) {
    bout << "\r\nPlease enter the following information:\r\n";
    do {
      bout.nl();
      input_age(GetSession()->GetCurrentUser());
      bout.nl();
      bout.WriteFormatted("%02d/%02d/%02d -- Correct? ",
                                        GetSession()->GetCurrentUser()->GetBirthdayMonth(),
                                        GetSession()->GetCurrentUser()->GetBirthdayDay(),
                                        GetSession()->GetCurrentUser()->GetBirthdayYear());
      if (!yesno()) {
        GetSession()->GetCurrentUser()->SetBirthdayYear(0);
      }
    } while (!hangup && GetSession()->GetCurrentUser()->GetBirthdayYear() == 0);
  }

  if (!GetSession()->GetCurrentUser()->GetRealName()[0]) {
    input_realname();
  }
  if (!(syscfg.sysconfig & sysconfig_extended_info)) {
    return;
  }
  if (!GetSession()->GetCurrentUser()->GetStreet()[0]) {
    input_street();
  }
  if (!GetSession()->GetCurrentUser()->GetCity()[0]) {
    input_city();
  }
  if (!GetSession()->GetCurrentUser()->GetState()[0]) {
    input_state();
  }
  if (!GetSession()->GetCurrentUser()->GetCountry()[0]) {
    input_country();
  }
  if (!GetSession()->GetCurrentUser()->GetZipcode()[0]) {
    input_zipcode();
  }
  if (!GetSession()->GetCurrentUser()->GetDataPhoneNumber()[0]) {
    input_dataphone();
  }
  if (GetSession()->GetCurrentUser()->GetComputerType() == -1) {
    input_comptype();
  }

  if (!GetApplication()->HasConfigFlag(OP_FLAGS_USER_REGISTRATION)) {
    return;
  }

  if (GetSession()->GetCurrentUser()->GetRegisteredDateNum() == 0) {
    return;
  }

  time_t lTime = time(nullptr);
  if ((GetSession()->GetCurrentUser()->GetExpiresDateNum() < static_cast<unsigned long>(lTime + 30 * SECS_PER_DAY))
      && (GetSession()->GetCurrentUser()->GetExpiresDateNum() > static_cast<unsigned long>(lTime + 10 * SECS_PER_DAY))) {
    bout << "Your registration expires in " <<
                       static_cast<int>((GetSession()->GetCurrentUser()->GetExpiresDateNum() - lTime) / SECS_PER_DAY) <<
                       "days";
  } else if ((GetSession()->GetCurrentUser()->GetExpiresDateNum() > static_cast<unsigned long>(lTime)) &&
             (GetSession()->GetCurrentUser()->GetExpiresDateNum() < static_cast<unsigned long>(lTime + 10 * SECS_PER_DAY))) {
    if (static_cast<int>((GetSession()->GetCurrentUser()->GetExpiresDateNum() - lTime) / static_cast<unsigned long>
                         (SECS_PER_DAY)) > 1) {
      bout << "|#6Your registration expires in " <<
                         static_cast<int>((GetSession()->GetCurrentUser()->GetExpiresDateNum() - lTime) / static_cast<unsigned long>
                                          (SECS_PER_DAY))
                         << " days";
    } else {
      bout << "|#6Your registration expires in " <<
                         static_cast<int>((GetSession()->GetCurrentUser()->GetExpiresDateNum() - lTime) / static_cast<unsigned long>(3600L))
                         << " hours.";
    }
    bout.nl(2);
    pausescr();
  }
  if (GetSession()->GetCurrentUser()->GetExpiresDateNum() < static_cast<unsigned long>(lTime)) {
    if (!so()) {
      if (GetSession()->GetCurrentUser()->GetSl() > syscfg.newusersl ||
          GetSession()->GetCurrentUser()->GetDsl() > syscfg.newuserdsl) {
        GetSession()->GetCurrentUser()->SetSl(syscfg.newusersl);
        GetSession()->GetCurrentUser()->SetDsl(syscfg.newuserdsl);
        GetSession()->GetCurrentUser()->SetExempt(0);
        ssm(1, 0, "%s%s", GetSession()->GetCurrentUser()->GetUserNameAndNumber(GetSession()->usernum),
            "'s registration has expired.");
        GetSession()->WriteCurrentUser();
        GetSession()->ResetEffectiveSl();
        changedsl();
      }
    }
    bout << "|#6Your registration has expired.\r\n\n";
    pausescr();
  }
}

static void DisplayUserLoginInformation() {
  char s1[255];
  bout.nl();

  bout << "|#9Name/Handle|#0....... |#2" << GetSession()->GetCurrentUser()->GetUserNameAndNumber(
                       GetSession()->usernum) << wwiv::endl;
  bout << "|#9Internet Address|#0.. |#2";
  if (check_inet_addr(GetSession()->GetCurrentUser()->GetEmailAddress())) {
    bout << GetSession()->GetCurrentUser()->GetEmailAddress() << wwiv::endl;
  } else if (!GetSession()->internetEmailName.empty()) {
    bout << (GetSession()->IsInternetUseRealNames() ? GetSession()->GetCurrentUser()->GetRealName() :
                           GetSession()->GetCurrentUser()->GetName())
                       << "<" << GetSession()->internetFullEmailAddress << ">\r\n";
  } else {
    bout << "None.\r\n";
  }
  bout << "|#9Time allowed on|#0... |#2" << static_cast<int>((nsl() + 30) / SECONDS_PER_MINUTE_FLOAT) <<
                     wwiv::endl;
  if (GetSession()->GetCurrentUser()->GetNumIllegalLogons() > 0) {
    bout << "|#9Illegal logons|#0.... |#2" << GetSession()->GetCurrentUser()->GetNumIllegalLogons() <<
                       wwiv::endl;
  }
  if (GetSession()->GetCurrentUser()->GetNumMailWaiting() > 0) {
    bout << "|#9Mail waiting|#0...... |#2" << GetSession()->GetCurrentUser()->GetNumMailWaiting() <<
                       wwiv::endl;
  }
  if (GetSession()->GetCurrentUser()->GetTimesOnToday() == 1) {
    bout << "|#9Date last on|#0...... |#2" << GetSession()->GetCurrentUser()->GetLastOn() << wwiv::endl;
  } else {
    bout << "|#9Times on today|#0.... |#2" << GetSession()->GetCurrentUser()->GetTimesOnToday() << wwiv::endl;
  }
  bout << "|#9Sysop currently|#0... |#2";
  if (sysop2()) {
    bout << "Available\r\n";
  } else {
    bout << "NOT Available\r\n";
  }

  bout << "|#9System is|#0......... |#2WWIV " << wwiv_version << beta_version << "  " << wwiv::endl;

  /////////////////////////////////////////////////////////////////////////
  GetApplication()->GetStatusManager()->RefreshStatusCache();
  for (int i = 0; i < GetSession()->GetMaxNetworkNumber(); i++) {
    if (net_networks[i].sysnum) {
      sprintf(s1, "|#9%s node|#0%s|#2 @%u", net_networks[i].name, charstr(13 - strlen(net_networks[i].name), '.'),
              net_networks[i].sysnum);
      if (i) {
        bout << s1;
        bout.nl();
      } else {
        int i1;
        for (i1 = strlen(s1); i1 < 26; i1++) {
          s1[i1] = ' ';
        }
        s1[i1] = '\0';
        std::unique_ptr<WStatus> pStatus(GetApplication()->GetStatusManager()->GetStatus());
        bout << s1 << "(net" << pStatus->GetNetworkVersion() << ")\r\n";
      }
    }
  }

  bout << "|#9OS|#0................ |#2" << WWIV_GetOSVersion() << wwiv::endl;
  bout << "|#9Instance|#0.......... |#2" << GetApplication()->GetInstanceNumber() << "\r\n\n";
  if (GetSession()->GetCurrentUser()->GetForwardUserNumber()) {
    if (GetSession()->GetCurrentUser()->GetForwardSystemNumber() != 0) {
      set_net_num(GetSession()->GetCurrentUser()->GetForwardNetNumber());
      if (!valid_system(GetSession()->GetCurrentUser()->GetForwardSystemNumber())) {
        GetSession()->GetCurrentUser()->ClearMailboxForward();
        bout << "Forwarded to unknown system; forwarding reset.\r\n";
      } else {
        bout << "Mail set to be forwarded to ";
        if (GetSession()->GetMaxNetworkNumber() > 1) {
          bout << "#" << GetSession()->GetCurrentUser()->GetForwardUserNumber()
                             << " @"
                             << GetSession()->GetCurrentUser()->GetForwardSystemNumber()
                             << "." << GetSession()->GetNetworkName() << "."
                             << wwiv::endl;
        } else {
          bout << "#" << GetSession()->GetCurrentUser()->GetForwardUserNumber() << " @"
                             << GetSession()->GetCurrentUser()->GetForwardSystemNumber() << "." << wwiv::endl;
        }
      }
    } else {
      if (GetSession()->GetCurrentUser()->IsMailboxClosed()) {
        bout << "Your mailbox is closed.\r\n\n";
      } else {
        bout << "Mail set to be forwarded to #" << GetSession()->GetCurrentUser()->GetForwardUserNumber() <<
                           wwiv::endl;
      }
    }
  } else if (GetSession()->GetCurrentUser()->GetForwardSystemNumber() != 0) {
    char szInternetEmailAddress[ 255 ];
    read_inet_addr(szInternetEmailAddress, GetSession()->usernum);
    bout << "Mail forwarded to Internet " << szInternetEmailAddress << ".\r\n";
  }
  if (GetSession()->IsTimeOnlineLimited()) {
    bout << "\r\n|#3Your on-line time is limited by an external event.\r\n\n";
  }
}

static void LoginCheckForNewMail() {
  bout << "|#9Scanning for new mail... ";
  if (GetSession()->GetCurrentUser()->GetNumMailWaiting() > 0) {
    int nNumNewMessages = check_new_mail(GetSession()->usernum);
    if (nNumNewMessages) {
      bout << "|#9You have |#2" << nNumNewMessages << "|#9 new message(s).\r\n\r\n" <<
                         "|#9Read your mail now? ";
      if (noyes()) {
        readmail(1);
      }
    } else {
      bout << "|#9You have |#2" << GetSession()->GetCurrentUser()->GetNumMailWaiting() <<
                         "|#9 old message(s) in your mailbox.\r\n";
    }
  } else {
    bout << " |#9No mail found.\r\n";
  }
}

static void CheckUserForVotingBooth() {
  if (!GetSession()->GetCurrentUser()->IsRestrictionVote() && GetSession()->GetEffectiveSl() > syscfg.newusersl) {
    for (int i = 0; i < 20; i++) {
      if (questused[i] && GetSession()->GetCurrentUser()->GetVote(i) == 0) {
        bout.nl();
        bout << "|#9You haven't voted yet.\r\n";
        return;
      }
    }
  }
}

void logon() {
  if (GetSession()->usernum < 1) {
    hangup = true;
    return;
  }
  GetSession()->SetUserOnline(true);
  write_inst(INST_LOC_LOGON, 0, INST_FLAGS_NONE);
  get_user_ppp_addr();
  get_next_forced_event();
  bout.ResetColors();
  bout.Color(0);
  bout.ClearScreen();

  FixUserLinesAndColors();
  UpdateUserStatsForLogin();
  PrintLogonFile();
  UpdateLastOnFileAndUserLog();
  PrintUserSpecificFiles();

  read_automessage();
  timeon = timer();
  GetApplication()->UpdateTopScreen();
  bout.nl(2);
  pausescr();
  if (!syscfg.logon_cmd.empty()) {
    bout.nl();
    const string command = stuff_in(syscfg.logon_cmd, create_chain_file(), "", "", "", "");
    ExecuteExternalProgram(command, GetApplication()->GetSpawnOptions(SPAWNOPT_LOGON));
    bout.nl(2);
  }

  DisplayUserLoginInformation();

  CheckAndUpdateUserInfo();

  GetApplication()->UpdateTopScreen();
  GetApplication()->read_subs();
  rsm(GetSession()->usernum, GetSession()->GetCurrentUser(), true);

  LoginCheckForNewMail();

  if (GetSession()->GetCurrentUser()->GetNewScanDateNumber()) {
    nscandate = GetSession()->GetCurrentUser()->GetNewScanDateNumber();
  } else {
    nscandate = GetSession()->GetCurrentUser()->GetLastOnDateNumber();
  }
  batchtime = 0.0;
  GetSession()->numbatchdl = GetSession()->numbatch = 0;

  CheckUserForVotingBooth();

  if ((incom || sysop1()) && GetSession()->GetCurrentUser()->GetSl() < 255) {
    broadcast("%s Just logged on!", GetSession()->GetCurrentUser()->GetName());
  }
  setiia(90);

  // New Message Scan
  if (GetSession()->IsNewScanAtLogin()) {
    bout << "\r\n|#5Scan All Message Areas For New Messages? ";
    if (yesno()) {
      NewMsgsAllConfs();
    }
  }

  // Handle case of first conf with no subs avail
  if (usub[0].subnum == -1 && okconf(GetSession()->GetCurrentUser())) {
    for (GetSession()->SetCurrentConferenceMessageArea(0); (GetSession()->GetCurrentConferenceMessageArea() < subconfnum)
         && (uconfsub[GetSession()->GetCurrentConferenceMessageArea()].confnum != -1);
         GetSession()->SetCurrentConferenceMessageArea(GetSession()->GetCurrentConferenceMessageArea() + 1)) {
      setuconf(CONF_SUBS, GetSession()->GetCurrentConferenceMessageArea(), -1);
      if (usub[0].subnum != -1) {
        break;
      }
    }
    if (usub[0].subnum == -1) {
      GetSession()->SetCurrentConferenceMessageArea(0);
      setuconf(CONF_SUBS, GetSession()->GetCurrentConferenceMessageArea(), -1);
    }
  }
  g_preloaded = false;

  if (GetApplication()->HasConfigFlag(OP_FLAGS_USE_FORCESCAN)) {
    int nNextSubNumber = 0;
    if (GetSession()->GetCurrentUser()->GetSl() < 255) {
      forcescansub = true;
      qscan(GetSession()->GetForcedReadSubNumber(), &nNextSubNumber);
      forcescansub = false;
    } else {
      qscan(GetSession()->GetForcedReadSubNumber(), &nNextSubNumber);
    }
  }
  CleanUserInfo();
}

void logoff() {
  mailrec m;

  if (incom) {
    play_sdf(LOGOFF_NOEXT, false);
  }

  if (GetSession()->usernum > 0) {
    if ((incom || sysop1()) && GetSession()->GetCurrentUser()->GetSl() < 255) {
      broadcast("%s Just logged off!", GetSession()->GetCurrentUser()->GetName());
    }
  }
  setiia(90);
  GetSession()->remoteIO()->dtr(false);
  hangup = true;
  if (GetSession()->usernum < 1) {
    return;
  }

  string text = "  Logged Off At ";
  text += times();
  if (GetSession()->GetEffectiveSl() != 255 || incom) {
    sysoplog("", false);
    sysoplog(stripcolors(text.c_str()), false);
  }
  GetSession()->GetCurrentUser()->SetLastBaudRate(modem_speed);

  // put this back here where it belongs... (not sure why it te
  GetSession()->GetCurrentUser()->SetLastOn(g_szLastLoginDate);

  GetSession()->GetCurrentUser()->SetNumIllegalLogons(0);
  if ((timer() - timeon) < -30.0) {
    timeon -= HOURS_PER_DAY_FLOAT * SECONDS_PER_DAY_FLOAT;
  }
  double dTimeOnNow = timer() - timeon;
  GetSession()->GetCurrentUser()->SetTimeOn(GetSession()->GetCurrentUser()->GetTimeOn() + static_cast<float>(dTimeOnNow));
  GetSession()->GetCurrentUser()->SetTimeOnToday(GetSession()->GetCurrentUser()->GetTimeOnToday() + static_cast<float>
      (dTimeOnNow - extratimecall));
  WStatus* pStatus = GetApplication()->GetStatusManager()->BeginTransaction();
  int nActiveToday = pStatus->GetMinutesActiveToday();
  pStatus->SetMinutesActiveToday(nActiveToday + static_cast<unsigned short>(dTimeOnNow / MINUTES_PER_HOUR_FLOAT));
  GetApplication()->GetStatusManager()->CommitTransaction(pStatus);
  if (g_flags & g_flag_scanned_files) {
    GetSession()->GetCurrentUser()->SetNewScanDateNumber(GetSession()->GetCurrentUser()->GetLastOnDateNumber());
  }
  long lTime = time(nullptr);
  GetSession()->GetCurrentUser()->SetLastOnDateNumber(lTime);
  sysoplogfi(false, "Read: %lu   Time on: %u", GetSession()->GetNumMessagesReadThisLogon(),
             static_cast<int>((timer() - timeon) / MINUTES_PER_HOUR_FLOAT));
  if (mailcheck) {
    WFile* pFileEmail = OpenEmailFile(true);
    WWIV_ASSERT(pFileEmail);
    if (pFileEmail->IsOpen()) {
      GetSession()->GetCurrentUser()->SetNumMailWaiting(0);
      int t = static_cast< int >(pFileEmail->GetLength() / sizeof(mailrec));
      int r = 0;
      int w = 0;
      while (r < t) {
        pFileEmail->Seek(static_cast<long>(sizeof(mailrec)) * static_cast<long>(r), WFile::seekBegin);
        pFileEmail->Read(&m, sizeof(mailrec));
        if (m.tosys != 0 || m.touser != 0) {
          if (m.tosys == 0 && m.touser == GetSession()->usernum) {
            if (GetSession()->GetCurrentUser()->GetNumMailWaiting() != 255) {
              GetSession()->GetCurrentUser()->SetNumMailWaiting(GetSession()->GetCurrentUser()->GetNumMailWaiting() + 1);
            }
          }
          if (r != w) {
            pFileEmail->Seek(static_cast<long>(sizeof(mailrec)) * static_cast<long>(w), WFile::seekBegin);
            pFileEmail->Write(&m, sizeof(mailrec));
          }
          ++w;
        }
        ++r;
      }
      if (r != w) {
        m.tosys = 0;
        m.touser = 0;
        for (int w1 = w; w1 < r; w1++) {
          pFileEmail->Seek(static_cast<long>(sizeof(mailrec)) * static_cast<long>(w1), WFile::seekBegin);
          pFileEmail->Write(&m, sizeof(mailrec));
        }
      }
      pFileEmail->SetLength(static_cast<long>(sizeof(mailrec)) * static_cast<long>(w));
      WStatus *pStatus = GetApplication()->GetStatusManager()->BeginTransaction();
      pStatus->IncrementFileChangedFlag(WStatus::fileChangeEmail);
      GetApplication()->GetStatusManager()->CommitTransaction(pStatus);
      pFileEmail->Close();
      delete pFileEmail;
    }
  } else {
    // re-calculate mail waiting'
    WFile *pFileEmail = OpenEmailFile(false);
    WWIV_ASSERT(pFileEmail);
    if (pFileEmail->IsOpen()) {
      int nTotalEmailMessages = static_cast<int>(pFileEmail->GetLength() / sizeof(mailrec));
      GetSession()->GetCurrentUser()->SetNumMailWaiting(0);
      for (int i = 0; i < nTotalEmailMessages; i++) {
        pFileEmail->Read(&m, sizeof(mailrec));
        if (m.tosys == 0 && m.touser == GetSession()->usernum) {
          if (GetSession()->GetCurrentUser()->GetNumMailWaiting() != 255) {
            GetSession()->GetCurrentUser()->SetNumMailWaiting(GetSession()->GetCurrentUser()->GetNumMailWaiting() + 1);
          }
        }
      }
      pFileEmail->Close();
      delete pFileEmail;
    }
  }
  if (smwcheck) {
    WFile smwFile(syscfg.datadir, SMW_DAT);
    if (smwFile.Open(WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile)) {
      int t = static_cast<int>(smwFile.GetLength() / sizeof(shortmsgrec));
      int r = 0;
      int w = 0;
      while (r < t) {
        shortmsgrec sm;
        smwFile.Seek(r * sizeof(shortmsgrec), WFile::seekBegin);
        smwFile.Read(&sm, sizeof(shortmsgrec));
        if (sm.tosys != 0 || sm.touser != 0) {
          if (sm.tosys == 0 && sm.touser == GetSession()->usernum) {
            GetSession()->GetCurrentUser()->SetStatusFlag(WUser::SMW);
          }
          if (r != w) {
            smwFile.Seek(w * sizeof(shortmsgrec), WFile::seekBegin);
            smwFile.Write(&sm, sizeof(shortmsgrec));
          }
          ++w;
        }
        ++r;
      }
      smwFile.SetLength(w * sizeof(shortmsgrec));
      smwFile.Close();
    }
  }
  if (GetSession()->usernum == 1) {
    fwaiting = GetSession()->GetCurrentUser()->GetNumMailWaiting();
  }
  GetSession()->WriteCurrentUser();
  write_qscn(GetSession()->usernum, qsc, false);
  remove_from_temp("*.*", syscfgovr.tempdir, false);
  remove_from_temp("*.*", syscfgovr.batchdir, false);
  if (GetSession()->numbatch && (GetSession()->numbatch != GetSession()->numbatchdl)) {
    for (int i = 0; i < GetSession()->numbatch; i++) {
      if (!batch[i].sending) {
        didnt_upload(i);
      }
    }
  }
  GetSession()->numbatch = GetSession()->numbatchdl = 0;
}


void logon_guest() {
  bout.nl(2);
  input_ansistat();

  printfile(GUEST_NOEXT);
  pausescr();

  string userName, reason;
  int count = 0;
  do {
    bout << "\r\n|#5Enter your real name : ";
    input(&userName, 25, true);
    bout << "\r\n|#5Purpose of your call?\r\n";
    input(&reason, 79, true);
    if (!userName.empty() && !reason.empty()) {
      break;
    }
    count++;
  } while (!hangup && count < 3);

  if (count >= 3) {
    printfile(REJECT_NOEXT);
    ssm(1, 0, "Guest Account failed to enter name and purpose");
    hangup = true;
  } else {
    ssm(1, 0, "Guest Account accessed by %s on %s for", userName.c_str(), times());
    ssm(1, 0, reason.c_str());
  }
}

