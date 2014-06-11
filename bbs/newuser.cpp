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
#include <algorithm>

#include "ini.h"
#include "printfile.h"
#include "wtextfile.h"

//
// Local function prototypes
//

void input_phone();
bool check_name(const std::string userName);
void input_callsign();
int  find_new_usernum(const WUser *pUser, unsigned long *qsc);
void cln_nu();

void CreateNewUserRecord();
bool CanCreateNewUserAccountHere();
bool UseMinimalNewUserInfo();
void noabort(const char *pszFileName);
bool check_dupes(const char *pszPhoneNumber);
void DoMinimalNewUser();
bool check_zip(const char *pszZipCode, int mode);
void DoFullNewUser();
void VerifyNewUserFullInfo();
void VerifyNewUserPassword();
void SendNewUserFeedbackIfRequired();
void ExecNewUserCommand();
void new_mail();
bool CheckPasswordComplexity(WUser *pUser, std::string& password);


void input_phone() {
  bool ok = true;
  std::string phoneNumber;
  do {
    GetSession()->bout.NewLine();
    GetSession()->bout << "|#3Enter your VOICE phone no. in the form:\r\n|#3 ###-###-####\r\n|#2:";
    Input1(phoneNumber,  GetSession()->GetCurrentUser()->GetVoicePhoneNumber(), 12, true, INPUT_MODE_PHONE);

    ok = valid_phone(phoneNumber.c_str());
    if (!ok) {
      GetSession()->bout.NewLine();
      GetSession()->bout << "|#6Please enter a valid phone number in the correct format.\r\n";
    }
  } while (!ok && !hangup);
  if (!hangup) {
    GetSession()->GetCurrentUser()->SetVoicePhoneNumber(phoneNumber.c_str());
  }
}


void input_dataphone() {
  bool ok = true;
  char szTempDataPhoneNum[ 21 ];
  do {
    GetSession()->bout.NewLine();
    GetSession()->bout << "|#3Enter your DATA phone no. in the form. \r\n";
    GetSession()->bout << "|#3 ###-###-#### - Press Enter to use [" << GetSession()->GetCurrentUser()->GetVoicePhoneNumber()
                       << "].\r\n";
    Input1(szTempDataPhoneNum, GetSession()->GetCurrentUser()->GetDataPhoneNumber(), 12, true, INPUT_MODE_PHONE);
    if (szTempDataPhoneNum[0] == '\0') {
      GetSession()->GetCurrentUser()->SetDataPhoneNumber(GetSession()->GetCurrentUser()->GetVoicePhoneNumber());
    }
    ok = valid_phone(szTempDataPhoneNum);

    if (!ok) {
      GetSession()->bout.NewLine();
      GetSession()->bout << "|#6Please enter a valid phone number in the correct format.\r\n";
    }
  } while (!ok && !hangup);

  if (!hangup) {
    GetSession()->GetCurrentUser()->SetDataPhoneNumber(szTempDataPhoneNum);
  }
}


void input_language() {
  if (GetSession()->num_languages > 1) {
    GetSession()->GetCurrentUser()->SetLanguage(255);
    do {
      char ch = 0, onx[20];
      GetSession()->bout.NewLine(2);
      for (int i = 0; i < GetSession()->num_languages; i++) {
        GetSession()->bout << (i + 1) << ". " << languages[i].name << wwiv::endl;
        if (i < 9) {
          onx[i] = static_cast< char >('1' + i);
        }
      }
      GetSession()->bout.NewLine();
      GetSession()->bout << "|#2Which language? ";
      if (GetSession()->num_languages < 10) {
        onx[GetSession()->num_languages] = 0;
        ch = onek(onx);
        ch -= '1';
      } else {
        int i;
        for (i = 1; i <= GetSession()->num_languages / 10; i++) {
          odc[i - 1] = static_cast< char >('0' + i);
        }
        odc[i - 1] = 0;
        char* ss = mmkey(2);
        ch = *((ss) - 1);
      }
      if (ch >= 0 && ch < GetSession()->num_languages) {
        GetSession()->GetCurrentUser()->SetLanguage(languages[ch].num);
      }
    } while ((GetSession()->GetCurrentUser()->GetLanguage() == 255) && (!hangup));

    set_language(GetSession()->GetCurrentUser()->GetLanguage());
  }
}


bool check_name(const std::string userName) {
  // Since finduser is called with userName, it can not be const.  A better idea may be
  // to change this behaviour in the future.
  char s[255], s1[255], s2[MAX_PATH];

  if (userName.length() == 0 ||
      userName[ userName.length() - 1 ] == 32   ||
      userName[0] < 65                 ||
      finduser(userName) != 0          ||
      userName.find("@") != std::string::npos ||
      userName.find("#") != std::string::npos) {
    return false;
  }

  WFile trashFile(syscfg.gfilesdir, TRASHCAN_TXT);
  if (!trashFile.Open(WFile::modeReadOnly | WFile::modeBinary)) {
    return true;
  }

  trashFile.Seek(0L, WFile::seekBegin);
  long lTrashFileLen = trashFile.GetLength();
  long lTrashFilePointer = 0;
  sprintf(s2, " %s ", userName.c_str());
  bool ok = true;
  while (lTrashFilePointer < lTrashFileLen && ok && !hangup) {
    CheckForHangup();
    trashFile.Seek(lTrashFilePointer, WFile::seekBegin);
    trashFile.Read(s, 150);
    int i = 0;
    while ((i < 150) && (s[i])) {
      if (s[i] == '\r') {
        s[i] = '\0';
      } else {
        ++i;
      }
    }
    s[150] = '\0';
    lTrashFilePointer += static_cast<long>(i + 2);
    if (s[i - 1] == 1) {
      s[i - 1] = 0;
    }
    for (i = 0; i < static_cast<int>(strlen(s)); i++) {
      s[i] = upcase(s[i]);
    }
    sprintf(s1, " %s ", s);
    if (strstr(s2, s1) != NULL) {
      ok = false;
    }
  }
  trashFile.Close();
  if (!ok) {
    hangup = true;
    hang_it_up();
  }
  return ok;
}


void input_name() {
  int count = 0;
  bool ok = true;
  do {
    GetSession()->bout.NewLine();
    if (syscfg.sysconfig & sysconfig_no_alias) {
      GetSession()->bout << "|#3Enter your FULL REAL name.\r\n";
    } else {
      GetSession()->bout << "|#3Enter your full name, or your alias.\r\n";
    }
    char szTempLocalName[ 255 ];
    Input1(szTempLocalName, GetSession()->GetCurrentUser()->GetName(), 30, true, INPUT_MODE_FILE_UPPER);
    ok = check_name(szTempLocalName);
    if (ok) {
      GetSession()->GetCurrentUser()->SetName(szTempLocalName);
    } else {
      GetSession()->bout.NewLine();
      GetSession()->bout << "|#6I'm sorry, you can't use that name.\r\n";
      ++count;
      if (count == 3) {
        hangup = true;
        hang_it_up();
      }
    }
  } while (!ok && !hangup);
}


void input_realname() {
  if (!(syscfg.sysconfig & sysconfig_no_alias)) {
    do {
      GetSession()->bout.NewLine();
      GetSession()->bout << "|#3Enter your FULL real name.\r\n";
      char szTempLocalName[ 255 ];
      Input1(szTempLocalName,
             GetSession()->GetCurrentUser()->GetRealName(), 30, true, INPUT_MODE_FILE_PROPER);

      if (szTempLocalName[0] == '\0') {
        GetSession()->bout.NewLine();
        GetSession()->bout << "|#6Sorry, you must enter your FULL real name.\r\n";
      } else {
        GetSession()->GetCurrentUser()->SetRealName(szTempLocalName);
      }
    } while ((GetSession()->GetCurrentUser()->GetRealName()[0] == '\0') && !hangup);
  } else {
    GetSession()->GetCurrentUser()->SetRealName(GetSession()->GetCurrentUser()->GetName());
  }
}


void input_callsign() {
  GetSession()->bout.NewLine();
  GetSession()->bout << " |#3Enter your amateur radio callsign, or just hit <ENTER> if none.\r\n|#2:";
  std::string s;
  input(s, 6, true);
  GetSession()->GetCurrentUser()->SetCallsign(s.c_str());

}


bool valid_phone(const std::string phoneNumber) {
  if (syscfg.sysconfig & sysconfig_free_phone) {
    return true;
  }

  if (syscfg.sysconfig & sysconfig_extended_info) {
    if (!IsPhoneNumberUSAFormat(GetSession()->GetCurrentUser()) && GetSession()->GetCurrentUser()->GetCountry()[0]) {
      return true;
    }
  }
  if (phoneNumber.length() != 12) {
    return false;
  }
  if (phoneNumber[3] != '-' || phoneNumber[7] != '-') {
    return false;
  }
  for (int i = 0; i < 12; i++) {
    if (i != 3 && i != 7) {
      if (phoneNumber[i] < '0' || phoneNumber[i] > '9') {
        return false;
      }
    }
  }
  return true;
}


void input_street() {
  std::string street;
  do {
    GetSession()->bout.NewLine();
    GetSession()->bout << "|#3Enter your street address.\r\n";
    Input1(street, GetSession()->GetCurrentUser()->GetStreet(), 30, true, INPUT_MODE_FILE_PROPER);

    if (street.empty()) {
      GetSession()->bout.NewLine();
      GetSession()->bout << "|#6I'm sorry, you must enter your street address.\r\n";
    }
  } while (street.empty() && !hangup);
  if (!hangup) {
    GetSession()->GetCurrentUser()->SetStreet(street.c_str());
  }
}


void input_city() {
  char szCity[ 255 ];
  do {
    GetSession()->bout.NewLine();
    GetSession()->bout << "|#3Enter your city (i.e San Francisco). \r\n";
    Input1(szCity, GetSession()->GetCurrentUser()->GetCity(), 30, false, INPUT_MODE_FILE_PROPER);

    if (szCity[0] == '\0') {
      GetSession()->bout.NewLine();
      GetSession()->bout << "|#6I'm sorry, you must enter your city.\r\n";
    }
  } while (szCity[0] == '\0' && (!hangup));
  GetSession()->GetCurrentUser()->SetCity(szCity);
}


void input_state() {
  std::string state;
  do {
    GetSession()->bout.NewLine();
    if (wwiv::strings::IsEquals(GetSession()->GetCurrentUser()->GetCountry(), "CAN")) {
      GetSession()->bout << "|#3Enter your province (i.e. QC).\r\n";
    } else {
      GetSession()->bout << "|#3Enter your state (i.e. CA). \r\n";
    }
    GetSession()->bout << "|#2:";
    input(state, 2, true);

    if (state.empty()) {
      GetSession()->bout.NewLine();
      GetSession()->bout << "|#6I'm sorry, you must enter your state or province.\r\n";
    }
  } while (state.empty() && (!hangup));
  GetSession()->GetCurrentUser()->SetState(state.c_str());
}


void input_country() {
  std::string country;
  do {
    GetSession()->bout.NewLine();
    GetSession()->bout << "|#3Enter your country (i.e. USA). \r\n";
    GetSession()->bout << "|#3Hit Enter for \"USA\"\r\n";
    GetSession()->bout << "|#2:";
    input(country, 3, true);
    if (country.empty()) {
      country = "USA";
    }
  } while (country.empty() && (!hangup));
  GetSession()->GetCurrentUser()->SetCountry(country.c_str());
}


void input_zipcode() {
  std::string zipcode;
  do {
    int len = 7;
    GetSession()->bout.NewLine();
    if (wwiv::strings::IsEquals(GetSession()->GetCurrentUser()->GetCountry(), "USA")) {
      GetSession()->bout << "|#3Enter your zipcode as #####-#### \r\n";
      len = 10;
    } else {
      GetSession()->bout << "|#3Enter your postal code as L#L-#L#\r\n";
      len = 7;
    }
    GetSession()->bout << "|#2:";
    input(zipcode, len, true);

    if (zipcode.empty()) {
      GetSession()->bout.NewLine();
      GetSession()->bout << "|#6I'm sorry, you must enter your zipcode.\r\n";
    }
  } while (zipcode.empty() && (!hangup));
  GetSession()->GetCurrentUser()->SetZipcode(zipcode.c_str());
}


void input_sex() {
  GetSession()->bout.NewLine();
  GetSession()->bout << "|#2Your gender (M,F) :";
  GetSession()->GetCurrentUser()->SetGender(onek("MF"));
}


void input_age(WUser *pUser) {
  int y = 2000, m = 1, d = 1;
  char ag[10], s[81];
  time_t t;
  time(&t);
  struct tm * pTm = localtime(&t);

  bool ok = false;
  do {
    GetSession()->bout.NewLine();
    do {
      GetSession()->bout.NewLine();
      GetSession()->bout << "|#2Month you were born (1-12) : ";
      input(ag, 2, true);
      m = atoi(ag);
    } while (!hangup && (m > 12 || m < 1));

    do {
      GetSession()->bout.NewLine();
      GetSession()->bout << "|#2Day of month you were born (1-31) : ";
      input(ag, 2, true);
      d = atoi(ag);
    } while (!hangup && (d > 31 || d < 1));

    do {
      GetSession()->bout.NewLine();
      y = static_cast<int>(pTm->tm_year + 1900 - 10) / 100;
      GetSession()->bout << "|#2Year you were born: ";
      sprintf(s, "%2d", y);
      Input1(ag, s, 4, true, INPUT_MODE_FILE_MIXED);
      y = atoi(ag);
      if (y == 1919) {
        GetSession()->bout.NewLine();
        GetSession()->bout << "|#5Were you really born in 1919? ";
        if (!yesno()) {
          y = 0;
        }
      }
    } while (!hangup && y < 1905);

    ok = true;
    if (((m == 2) || (m == 9) || (m == 4) || (m == 6) || (m == 11)) && (d >= 31)) {
      ok = false;
    }
    if ((m == 2) && (((y % 4 != 0) && (d == 29)) || (d == 30))) {
      ok = false;
    }
    if (d > 31) {
      ok = false;
    }
    if (!ok) {
      GetSession()->bout.NewLine();
      GetSession()->bout << "|#6There aren't that many days in that month.\r\n";
    }
    if (years_old(m, d, (y - 1900)) < 5) {
      GetSession()->bout.NewLine();
      GetSession()->bout << "Not likely\r\n";
      ok = false;
    }
  } while (!ok && !hangup);
  pUser->SetBirthdayMonth(m);
  pUser->SetBirthdayDay(d);
  pUser->SetBirthdayYear(y - 1900);
  pUser->SetAge(years_old(pUser->GetBirthdayMonth(), pUser->GetBirthdayDay(), pUser->GetBirthdayYear()));
  GetSession()->bout.NewLine();
}


void input_comptype() {
  int ct = -1;
  char c[5];

  bool ok = true;
  do {
    GetSession()->bout.NewLine();
    GetSession()->bout << "Known computer types:\r\n\n";
    int i = 0;
    for (i = 1; ctypes(i); i++) {
      GetSession()->bout << i << ". " << ctypes(i) << wwiv::endl;
    }
    GetSession()->bout.NewLine();
    GetSession()->bout << "|#3Enter your computer type, or the closest to it (ie, Compaq -> IBM).\r\n";
    GetSession()->bout << "|#2:";
    input(c, 2, true);
    ct = atoi(c);

    ok = true;
    if (ct < 1 || ct > i) {
      ok = false;
    }
  } while (!ok && !hangup);

  GetSession()->GetCurrentUser()->SetComputerType(ct);
  if (hangup) {
    GetSession()->GetCurrentUser()->SetComputerType(-1);
  }
}

void input_screensize() {
  int x = 0, y = 0;
  char s[5];

  bool ok = true;
  do {
    GetSession()->bout.NewLine();
    GetSession()->bout << "|#3How wide is your screen (chars, <CR>=80) ?\r\n|#2:";
    input(s, 2, true);
    x = atoi(s);
    if (s[0] == '\0') {
      x = 80;
    }

    ok = ((x < 32) || (x > 80)) ? false : true;
  } while (!ok && !hangup);

  do {
    GetSession()->bout.NewLine();
    GetSession()->bout << "|#3How tall is your screen (lines, <CR>=24) ?\r\n|#2:";
    input(s, 2, true);
    y = atoi(s);
    if (s[0] == '\0') {
      y = 24;
    }

    ok = (y < 8 || y > 60) ? false : true;
  } while (!ok && !hangup);

  GetSession()->GetCurrentUser()->SetScreenChars(x);
  GetSession()->GetCurrentUser()->SetScreenLines(y);
  GetSession()->screenlinest = y;
}


bool CheckPasswordComplexity(WUser *, std::string& password) {
  if (password.length() < 3) {
    //TODO - the min length should be in wwiv.ini
    return false;
  }

  //if ( password.find( pUser->GetName() ) != std::string::npos )
  //{
  //    return false;
  //}

  //if ( password.find( realName ) != std::string::npos )
  //{
  //    return false;
  //}

  //if( password.find( pUser->GetVoicePhoneNumber() ) != std::string::npos )
  //{
  //    return false;
  //}

  //if ( password.find( pUser->GetDataPhoneNumber() ) != std::string::npos )
  //{
  //    return false;
  //}

  return true;
}


void input_pw(WUser *pUser) {
  std::string password;
  bool ok = true;
  do {
    ok = true;
    GetSession()->bout.NewLine();
    GetSession()->bout << "|#3Please enter a new password, 3-8 chars.\r\n";
    password.clear();
    Input1(password, "", 8, false, INPUT_MODE_FILE_UPPER);

    std::string realName = GetSession()->GetCurrentUser()->GetRealName();
    StringUpperCase(realName);
    if (!CheckPasswordComplexity(pUser, password)) {
      ok = false;
      GetSession()->bout.NewLine(2);
      GetSession()->bout << "Invalid password.  Try again.\r\n\n";
    }
  } while (!ok && !hangup);

  if (ok) {
    pUser->SetPassword(password.c_str());
  } else {
    GetSession()->bout << "Password not changed.\r\n";
  }
}


void input_ansistat() {
  GetSession()->GetCurrentUser()->ClearStatusFlag(WUser::ansi);
  GetSession()->GetCurrentUser()->ClearStatusFlag(WUser::color);
  GetSession()->bout.NewLine();
  if (check_ansi() == 1) {
    GetSession()->bout << "ANSI graphics support detected.  Use it? ";
  } else {
    GetSession()->bout << "[0;34;3mTEST[C";
    GetSession()->bout << "[0;30;45mTEST[C";
    GetSession()->bout << "[0;1;31;44mTEST[C";
    GetSession()->bout << "[0;32;7mTEST[C";
    GetSession()->bout << "[0;1;5;33;46mTEST[C";
    GetSession()->bout << "[0;4mTEST[0m\r\n";
    GetSession()->bout << "Is the above line colored, italicized, bold, inversed, or blinking? ";
  }
  if (noyes()) {
    GetSession()->GetCurrentUser()->SetStatusFlag(WUser::ansi);
    GetSession()->bout.NewLine();
    GetSession()->bout << "|#5Do you want color? ";
    if (noyes()) {
      GetSession()->GetCurrentUser()->SetStatusFlag(WUser::color);
      GetSession()->GetCurrentUser()->SetStatusFlag(WUser::extraColor);
    } else {
      color_list();
      GetSession()->bout.NewLine();
      GetSession()->bout << "|#2Monochrome base color (<C/R>=7)? ";
      char ch = onek("\r123456789");
      if (ch == '\r') {
        ch = '7';
      }
      int c  = ch - '0';
      int c2 = c << 4;
      for (int i = 0; i < 10; i++) {
        if ((GetSession()->GetCurrentUser()->GetBWColor(i) & 0x70) == 0) {
          GetSession()->GetCurrentUser()->SetBWColor(i, (GetSession()->GetCurrentUser()->GetBWColor(i) & 0x88) | c);
        } else {
          GetSession()->GetCurrentUser()->SetBWColor(i, (GetSession()->GetCurrentUser()->GetBWColor(i) & 0x88) | c2);
        }
      }
    }
  }
}


int find_new_usernum(const WUser* pUser, unsigned long *qsc) {
  WFile userFile(syscfg.datadir, USER_LST);
  for (int i = 0; !userFile.IsOpen() && (i < 20); i++) {
    if (!userFile.Open(WFile::modeBinary | WFile::modeReadWrite | WFile::modeCreateFile, WFile::shareUnknown,
                       WFile::permReadWrite)) {
      wait1(2);
    }
  }
  if (!userFile.IsOpen()) {
    return -1;
  }

  int nNewUserNumber = static_cast<int>((userFile.GetLength() / syscfg.userreclen) - 1);
  userFile.Seek(syscfg.userreclen, WFile::seekBegin);
  int nUserNumber = 1;

  if (nNewUserNumber == GetApplication()->GetStatusManager()->GetUserCount()) {
    nUserNumber = nNewUserNumber + 1;
  } else {
    while (nUserNumber <= nNewUserNumber) {
      if (nUserNumber % 25 == 0) {
        userFile.Close();
        for (int n = 0; !userFile.IsOpen() && (n < 20); n++) {
          if (!userFile.Open(WFile::modeBinary | WFile::modeReadWrite | WFile::modeCreateFile, WFile::shareUnknown,
                             WFile::permReadWrite)) {
            wait1(2);
          }
        }
        if (!userFile.IsOpen()) {
          return -1;
        }
        userFile.Seek(static_cast<long>(nUserNumber * syscfg.userreclen), WFile::seekBegin);
        nNewUserNumber = static_cast<int>((userFile.GetLength() / syscfg.userreclen) - 1);
      }
      WUser tu;
      userFile.Read(&tu.data, syscfg.userreclen);

      if (tu.IsUserDeleted() && tu.GetSl() != 255) {
        userFile.Seek(static_cast<long>(nUserNumber * syscfg.userreclen), WFile::seekBegin);
        userFile.Write(&pUser->data, syscfg.userreclen);
        userFile.Close();
        write_qscn(nUserNumber, qsc, false);
        InsertSmallRecord(nUserNumber, pUser->GetName());
        return nUserNumber;
      } else {
        nUserNumber++;
      }
    }
  }

  if (nUserNumber <= syscfg.maxusers) {
    userFile.Seek(static_cast<long>(nUserNumber * syscfg.userreclen), WFile::seekBegin);
    userFile.Write(&pUser->data, syscfg.userreclen);
    userFile.Close();
    write_qscn(nUserNumber, qsc, false);
    InsertSmallRecord(nUserNumber, pUser->GetName());
    return nUserNumber;
  } else {
    userFile.Close();
    return -1;
  }
}



// Clears GetSession()->GetCurrentUser()'s data and makes it ready to be a new user, also
// clears the QScan pointers
void CreateNewUserRecord() {
  GetSession()->GetCurrentUser()->ZeroUserData();
  memset(qsc, 0, syscfg.qscn_len);

  GetSession()->GetCurrentUser()->SetFirstOn(date());
  GetSession()->GetCurrentUser()->SetLastOn("Never.");
  GetSession()->GetCurrentUser()->SetMacro(0, "Wow! This is a GREAT BBS!");
  GetSession()->GetCurrentUser()->SetMacro(1, "Guess you forgot to define this one....");
  GetSession()->GetCurrentUser()->SetMacro(2, "User = Monkey + Keyboard");

  GetSession()->GetCurrentUser()->SetScreenLines(25);
  GetSession()->GetCurrentUser()->SetScreenChars(80);

  GetSession()->GetCurrentUser()->SetSl(syscfg.newusersl);
  GetSession()->GetCurrentUser()->SetDsl(syscfg.newuserdsl);

  GetSession()->GetCurrentUser()->SetTimesOnToday(1);
  GetSession()->GetCurrentUser()->SetLastOnDateNumber(0);
  GetSession()->GetCurrentUser()->SetRestriction(syscfg.newuser_restrict);

  *qsc = 999;
  memset(qsc_n, 0xff, ((GetSession()->GetMaxNumberFileAreas() + 31) / 32) * 4);
  memset(qsc_q, 0xff, ((GetSession()->GetMaxNumberMessageAreas() + 31) / 32) * 4);

  GetSession()->GetCurrentUser()->SetStatusFlag(WUser::pauseOnPage);
  GetSession()->GetCurrentUser()->ClearStatusFlag(WUser::conference);
  GetSession()->GetCurrentUser()->ClearStatusFlag(WUser::nscanFileSystem);
  GetSession()->GetCurrentUser()->SetGold(syscfg.newusergold);

  for (int nColorLoop = 0; nColorLoop <= 9; nColorLoop++) {
    GetSession()->GetCurrentUser()->SetColor(nColorLoop, GetSession()->newuser_colors[ nColorLoop ]);
    GetSession()->GetCurrentUser()->SetBWColor(nColorLoop, GetSession()->newuser_bwcolors[ nColorLoop ]);
  }

  GetSession()->ResetEffectiveSl();
  std::string randomPassword;
  for (int i = 0; i < 6; i++) {
    char ch = static_cast< char >(rand() % 36);
    if (ch < 10) {
      ch += '0';
    } else {
      ch += 'A' - 10;
    }
    randomPassword += ch;
  }
  GetSession()->GetCurrentUser()->SetPassword(randomPassword.c_str());
  GetSession()->GetCurrentUser()->SetEmailAddress("");

}


// returns true if the user is allow to create a new user account
// on here, if this function returns false, a sufficient error
// message has already been displayed to the user.
bool CanCreateNewUserAccountHere() {
  if (GetApplication()->GetStatusManager()->GetUserCount() >= syscfg.maxusers) {
    GetSession()->bout.NewLine(2);
    GetSession()->bout << "I'm sorry, but the system currently has the maximum number of users it can\r\nhandle.\r\n\n";
    return false;
  }

  if (syscfg.closedsystem) {
    GetSession()->bout.NewLine(2);
    GetSession()->bout << "I'm sorry, but the system is currently closed, and not accepting new users.\r\n\n";
    return false;
  }

  if ((syscfg.newuserpw[0] != 0) && (incom)) {
    GetSession()->bout.NewLine(2);
    bool ok = false;
    int nPasswordAttempt = 0;
    do {
      GetSession()->bout << "New User Password :";
      std::string password;
      input(password, 20);
      if (password == syscfg.newuserpw) {
        ok = true;
      } else {
        sysoplogf("Wrong newuser password: %s", password.c_str());
      }
    } while (!ok && !hangup && (nPasswordAttempt++ < 4));
    if (!ok) {
      return false;
    }
  }
  return true;
}


bool UseMinimalNewUserInfo() {
  WIniFile iniFile(WWIV_INI);
  if (iniFile.Open(INI_TAG)) {
    return iniFile.GetBooleanValue("NEWUSER_MIN");
  }
  return false;
}


void DoFullNewUser() {
  input_name();
  input_realname();
  input_phone();
  if (GetApplication()->HasConfigFlag(OP_FLAGS_CHECK_DUPE_PHONENUM)) {
    if (check_dupes(GetSession()->GetCurrentUser()->GetVoicePhoneNumber())) {
      if (GetApplication()->HasConfigFlag(OP_FLAGS_HANGUP_DUPE_PHONENUM)) {
        hangup = true;
        hang_it_up();
        return;
      }
    }
  }
  if (syscfg.sysconfig & sysconfig_extended_info) {
    input_street();
    char szZipFileName[ MAX_PATH ];
    sprintf(szZipFileName, "%s%s%czip1.dat", syscfg.datadir, ZIPCITY_DIR, WWIV_FILE_SEPERATOR_CHAR);
    if (WFile::Exists(szZipFileName)) {
      input_zipcode();
      if (!check_zip(GetSession()->GetCurrentUser()->GetZipcode(), 1)) {
        GetSession()->GetCurrentUser()->SetCity("");
        GetSession()->GetCurrentUser()->SetState("");
      }
      GetSession()->GetCurrentUser()->SetCountry("USA");
    }
    if (GetSession()->GetCurrentUser()->GetCity()[0] == '\0') {
      input_city();
    }
    if (GetSession()->GetCurrentUser()->GetState()[0] == '\0') {
      input_state();
    }
    if (GetSession()->GetCurrentUser()->GetZipcode()[0] == '\0') {
      input_zipcode();
    }
    if (GetSession()->GetCurrentUser()->GetCountry()[0] == '\0') {
      input_country();
    }
    input_dataphone();

    if (GetApplication()->HasConfigFlag(OP_FLAGS_CHECK_DUPE_PHONENUM)) {
      if (check_dupes(GetSession()->GetCurrentUser()->GetDataPhoneNumber())) {
        if (GetApplication()->HasConfigFlag(OP_FLAGS_HANGUP_DUPE_PHONENUM)) {
          hangup = true;
          hang_it_up();
          return;
        }
      }
    }
  }
  input_callsign();
  input_sex();
  input_age(GetSession()->GetCurrentUser());
  input_comptype();

  if (GetSession()->GetNumberOfEditors() && GetSession()->GetCurrentUser()->HasAnsi()) {
    GetSession()->bout.NewLine();
    GetSession()->bout << "|#5Select a fullscreen editor? ";
    if (yesno()) {
      select_editor();
    } else {
      for (int nEditor = 0; nEditor < GetSession()->GetNumberOfEditors(); nEditor++) {
        char szEditorDesc[ 121 ];
        strcpy(szEditorDesc, editors[nEditor].description);
        if (strstr(WWIV_STRUPR(szEditorDesc) , "WWIVEDIT") != NULL) {
          GetSession()->GetCurrentUser()->SetDefaultEditor(nEditor + 1);
          nEditor = GetSession()->GetNumberOfEditors();
        }
      }
    }
    GetSession()->bout.NewLine();
  }
  GetSession()->bout << "|#5Select a default transfer protocol? ";
  if (yesno()) {
    GetSession()->bout.NewLine();
    GetSession()->bout << "Enter your default protocol, or 0 for none.\r\n\n";
    int nDefProtocol = get_protocol(xf_down);
    if (nDefProtocol) {
      GetSession()->GetCurrentUser()->SetDefaultProtocol(nDefProtocol);
    }
  }
}


void DoNewUserASV() {
  if (GetApplication()->HasConfigFlag(OP_FLAGS_ADV_ASV)) {
    asv();
    return;
  }
  if (GetApplication()->HasConfigFlag(OP_FLAGS_SIMPLE_ASV) &&
      GetSession()->asv.sl > syscfg.newusersl && GetSession()->asv.sl < 90) {
    GetSession()->bout.NewLine();
    GetSession()->bout << "|#5Are you currently a WWIV SysOp? ";
    if (yesno()) {
      GetSession()->bout.NewLine();
      GetSession()->bout << "|#5Please enter your BBS name and number.\r\n";
      std::string note;
      inputl(note, 60, true);
      GetSession()->GetCurrentUser()->SetNote(note.c_str());
      GetSession()->GetCurrentUser()->SetSl(GetSession()->asv.sl);
      GetSession()->GetCurrentUser()->SetDsl(GetSession()->asv.dsl);
      GetSession()->GetCurrentUser()->SetAr(GetSession()->asv.ar);
      GetSession()->GetCurrentUser()->SetDar(GetSession()->asv.dar);
      GetSession()->GetCurrentUser()->SetExempt(GetSession()->asv.exempt);
      GetSession()->GetCurrentUser()->SetRestriction(GetSession()->asv.restrict);
      GetSession()->bout.NewLine();
      printfile(ASV_NOEXT);
      GetSession()->bout.NewLine();
      pausescr();
    }
  }
}


void VerifyNewUserFullInfo() {
  bool ok = false;
  do {
    GetSession()->bout.NewLine(2);
    GetSession()->bout << "|#91) Name          : |#2" << GetSession()->GetCurrentUser()->GetName() << wwiv::endl;
    if (!(syscfg.sysconfig & sysconfig_no_alias)) {
      GetSession()->bout << "|#92) Real Name     : |#2" << GetSession()->GetCurrentUser()->GetRealName() << wwiv::endl;
    }
    GetSession()->bout << "|#93) Callsign      : |#2" << GetSession()->GetCurrentUser()->GetCallsign() << wwiv::endl;
    GetSession()->bout << "|#94) Phone No.     : |#2" << GetSession()->GetCurrentUser()->GetVoicePhoneNumber() <<
                       wwiv::endl;
    GetSession()->bout << "|#95) Gender        : |#2" << GetSession()->GetCurrentUser()->GetGender() << wwiv::endl;
    GetSession()->bout << "|#96) Birthdate     : |#2" <<
                       static_cast<int>(GetSession()->GetCurrentUser()->GetBirthdayMonth()) << "/" <<
                       static_cast<int>(GetSession()->GetCurrentUser()->GetBirthdayDay()) << "/" <<
                       static_cast<int>(GetSession()->GetCurrentUser()->GetBirthdayYear()) << wwiv::endl;
    GetSession()->bout << "|#97) Computer type : |#2" << ctypes(GetSession()->GetCurrentUser()->GetComputerType()) <<
                       wwiv::endl;
    GetSession()->bout << "|#98) Screen size   : |#2" <<
                       GetSession()->GetCurrentUser()->GetScreenChars() << " X " <<
                       GetSession()->GetCurrentUser()->GetScreenLines() << wwiv::endl;
    GetSession()->bout << "|#99) Password      : |#2" << GetSession()->GetCurrentUser()->GetPassword() << wwiv::endl;
    if (syscfg.sysconfig & sysconfig_extended_info) {
      GetSession()->bout << "|#9A) Street Address: |#2" << GetSession()->GetCurrentUser()->GetStreet() << wwiv::endl;
      GetSession()->bout << "|#9B) City          : |#2" << GetSession()->GetCurrentUser()->GetCity() << wwiv::endl;
      GetSession()->bout << "|#9C) State         : |#2" << GetSession()->GetCurrentUser()->GetState() << wwiv::endl;
      GetSession()->bout << "|#9D) Country       : |#2" << GetSession()->GetCurrentUser()->GetCountry() << wwiv::endl;
      GetSession()->bout << "|#9E) Zipcode       : |#2" << GetSession()->GetCurrentUser()->GetZipcode() << wwiv::endl;
      GetSession()->bout << "|#9F) Dataphone     : |#2" << GetSession()->GetCurrentUser()->GetDataPhoneNumber() << wwiv::endl;
    }
    GetSession()->bout.NewLine();
    GetSession()->bout << "Q) No changes.\r\n";
    GetSession()->bout.NewLine(2);
    char ch = 0;
    if (syscfg.sysconfig & sysconfig_extended_info) {
      GetSession()->bout << "|#9Which (1-9,A-F,Q) : ";
      ch = onek("Q123456789ABCDEF");
    } else {
      GetSession()->bout << "|#9Which (1-9,Q) : ";
      ch = onek("Q123456789");
    }
    ok = false;
    switch (ch) {
    case 'Q':
      ok = true;
      break;
    case '1':
      input_name();
      break;
    case '2':
      if (!(syscfg.sysconfig & sysconfig_no_alias)) {
        input_realname();
      }
      break;
    case '3':
      input_callsign();
      break;
    case '4':
      input_phone();
      break;
    case '5':
      input_sex();
      break;
    case '6':
      input_age(GetSession()->GetCurrentUser());
      break;
    case '7':
      input_comptype();
      break;
    case '8':
      input_screensize();
      break;
    case '9':
      input_pw(GetSession()->GetCurrentUser());
      break;
    case 'A':
      input_street();
      break;
    case 'B':
      input_city();
      break;
    case 'C':
      input_state();
      break;
    case 'D':
      input_country();
      break;
    case 'E':
      input_zipcode();
      break;
    case 'F':
      input_dataphone();
      break;
    }
  } while (!ok && !hangup);
}


void WriteNewUserInfoToSysopLog() {
  sysoplog("** New User Information **");
  sysoplogf("-> %s #%ld (%s)", GetSession()->GetCurrentUser()->GetName(), GetSession()->usernum,
            GetSession()->GetCurrentUser()->GetRealName());
  if (syscfg.sysconfig & sysconfig_extended_info) {
    sysoplogf("-> %s", GetSession()->GetCurrentUser()->GetStreet());
    sysoplogf("-> %s, %s %s  (%s)", GetSession()->GetCurrentUser()->GetCity(),
              GetSession()->GetCurrentUser()->GetState(), GetSession()->GetCurrentUser()->GetZipcode(),
              GetSession()->GetCurrentUser()->GetCountry());
  }
  sysoplogf("-> %s (Voice)", GetSession()->GetCurrentUser()->GetVoicePhoneNumber());
  if (syscfg.sysconfig & sysconfig_extended_info) {
    sysoplogf("-> %s (Data)", GetSession()->GetCurrentUser()->GetDataPhoneNumber());
  }
  sysoplogf("-> %02d/%02d/%02d (%d yr old %s)",
            GetSession()->GetCurrentUser()->GetBirthdayMonth(), GetSession()->GetCurrentUser()->GetBirthdayDay(),
            GetSession()->GetCurrentUser()->GetBirthdayYear(), GetSession()->GetCurrentUser()->GetAge(),
            ((GetSession()->GetCurrentUser()->GetGender() == 'M') ? "Male" : "Female"));
  sysoplogf("-> Using a %s Computer", ctypes(GetSession()->GetCurrentUser()->GetComputerType()));
  if (GetSession()->GetCurrentUser()->GetWWIVRegNumber()) {
    sysoplogf("-> WWIV Registration # %ld", GetSession()->GetCurrentUser()->GetWWIVRegNumber());
  }
  sysoplog("********");


  if (GetSession()->GetCurrentUser()->GetVoicePhoneNumber()[0]) {
    add_phone_number(GetSession()->usernum, GetSession()->GetCurrentUser()->GetVoicePhoneNumber());
  }
  if (GetSession()->GetCurrentUser()->GetDataPhoneNumber()[0]) {
    add_phone_number(GetSession()->usernum, GetSession()->GetCurrentUser()->GetDataPhoneNumber());
  }
}


void VerifyNewUserPassword() {
  bool ok = false;
  do {
    GetSession()->bout.NewLine(2);
    GetSession()->bout << "|#9Your User Number: |#2" << GetSession()->usernum << wwiv::endl;
    GetSession()->bout << "|#9Your Password:    |#2" << GetSession()->GetCurrentUser()->GetPassword() << wwiv::endl;
    GetSession()->bout.NewLine(1);
    GetSession()->bout << "|#9Please write down this information, and enter your password for verification.\r\n";
    GetSession()->bout << "|#9You will need to know this password in order to change it to something else.\r\n\n";
    std::string password;
    input_password("|#9PW: ", password, 8);
    if (password == GetSession()->GetCurrentUser()->GetPassword()) {
      ok = true;
    }
  } while (!ok && !hangup);
}


void SendNewUserFeedbackIfRequired() {
  if (!incom) {
    return;
  }

  if (GetApplication()->HasConfigFlag(OP_FLAGS_FORCE_NEWUSER_FEEDBACK)) {
    noabort(FEEDBACK_NOEXT);
  } else if (printfile(FEEDBACK_NOEXT)) {
    sysoplog("", false);
  }
  feedback(true);
  if (GetApplication()->HasConfigFlag(OP_FLAGS_FORCE_NEWUSER_FEEDBACK)) {
    if (!GetSession()->GetCurrentUser()->GetNumEmailSent() && !GetSession()->GetCurrentUser()->GetNumFeedbackSent()) {
      printfile(NOFBACK_NOEXT);
      deluser(GetSession()->usernum);
      hangup = true;
      hang_it_up();
      return;
    }
  }
}


void ExecNewUserCommand() {
  if (!hangup && syscfg.newuser_c[0]) {
    const std::string commandLine = stuff_in(syscfg.newuser_c, create_chain_file(), "", "", "", "");

    // Log what is happening here.
    sysoplog("Executing New User Event: ", false);
    sysoplog(commandLine.c_str(), true);

    GetSession()->WriteCurrentUser();
    ExecuteExternalProgram(commandLine, GetApplication()->GetSpawnOptions(SPWANOPT_NEWUSER));
    GetSession()->ReadCurrentUser();
  }
}


void newuser() {
  get_colordata();
  GetSession()->screenlinest = 25;

  CreateNewUserRecord();

  input_language();

  sysoplog("", false);
  sysoplogfi(false, "*** NEW USER %s   %s    %s (%ld)", fulldate(), times(), GetSession()->GetCurrentSpeed().c_str(),
             GetApplication()->GetInstanceNumber());

  if (!CanCreateNewUserAccountHere() || hangup) {
    hangup = true;
    return;
  }

  if (check_ansi()) {
    GetSession()->GetCurrentUser()->SetStatusFlag(WUser::ansi);
    GetSession()->GetCurrentUser()->SetStatusFlag(WUser::color);
    GetSession()->GetCurrentUser()->SetStatusFlag(WUser::extraColor);
  }
  printfile(SYSTEM_NOEXT);
  GetSession()->bout.NewLine();
  pausescr();
  printfile(NEWUSER_NOEXT);
  GetSession()->bout.NewLine();
  pausescr();
  GetSession()->bout.ClearScreen();
  GetSession()->bout << "|#5Create a new user account on " << syscfg.systemname << "? ";
  if (!noyes()) {
    GetSession()->bout << "|#6Sorry the system does not meet your needs!\r\n";
    hangup = true;
    hang_it_up();
    return;
  }
  input_screensize();
  input_country();
  bool newuser_min = UseMinimalNewUserInfo();
  if (newuser_min == true) {
    DoMinimalNewUser();
  } else {
    DoFullNewUser();
  }


  GetSession()->bout.NewLine(4);
  GetSession()->bout << "Random password: " << GetSession()->GetCurrentUser()->GetPassword() << wwiv::endl << wwiv::endl;
  GetSession()->bout << "|#5Do you want a different password (Y/N)? ";
  if (yesno()) {
    input_pw(GetSession()->GetCurrentUser());
  }

  DoNewUserASV();

  if (hangup) {
    return;
  }

  if (!newuser_min) {
    VerifyNewUserFullInfo();
  }

  if (hangup) {
    return;
  }

  GetSession()->bout.NewLine();
  GetSession()->bout << "Please wait...\r\n\n";
  GetSession()->usernum = find_new_usernum(GetSession()->GetCurrentUser(), qsc);
  if (GetSession()->usernum <= 0) {
    GetSession()->bout.NewLine();
    GetSession()->bout << "|#6Error creating user account.\r\n\n";
    hangup = true;
    return;
  }

  WriteNewUserInfoToSysopLog();

  GetApplication()->UpdateTopScreen();

  VerifyNewUserPassword();

  SendNewUserFeedbackIfRequired();

  ExecNewUserCommand();

  GetSession()->ResetEffectiveSl();
  changedsl();
  new_mail();
}


bool check_zip(const char *pszZipCode, int mode) {
  char city[81], state[21];

  if (pszZipCode[0] == '\0') {
    return false;
  }

  bool ok = true;
  bool found = false;

  char szFileName[ MAX_PATH ];
  sprintf(szFileName, "%s%s%czip%c.dat", syscfg.datadir, ZIPCITY_DIR, WWIV_FILE_SEPERATOR_CHAR, pszZipCode[0]);

  WTextFile zip_file(szFileName, "r");
  if (!zip_file.IsOpen()) {
    ok = false;
    if (mode != 2) {
      GetSession()->bout << "\r\n|#6" << szFileName << " not found\r\n";
    }
  } else {
    char zip_buf[81];
    while ((zip_file.ReadLine(zip_buf, 80)) && !found && !hangup) {
      single_space(zip_buf);
      if (strncmp(zip_buf, pszZipCode, 5) == 0) {
        found = true;
        char* ss = strtok(zip_buf, " ");
        ss = strtok(NULL, " ");
        StringTrim(ss);
        strcpy(state, ss);
        ss = strtok(NULL, "\r\n");
        StringTrim(ss);
        strncpy(city, ss, 30);
        city[31] = '\0';
        properize(city);
      }
    }
    zip_file.Close();
  }

  if (mode != 2 && !found) {
    GetSession()->bout.NewLine();
    GetSession()->bout << "|#6No match for " << pszZipCode << ".";
    ok = false;
  }

  if (!mode && found) {
    GetSession()->bout << "\r\n|#2" << pszZipCode << " is in " <<
                       city << ", " << state << ".";
    ok = false;
  }

  if (found) {
    if (mode != 2) {
      GetSession()->bout << "\r\n|#2" << city << ", " << state << "  USA? (y/N): ";
      if (yesno()) {
        GetSession()->GetCurrentUser()->SetCity(city);
        GetSession()->GetCurrentUser()->SetState(state);
        GetSession()->GetCurrentUser()->SetCountry("USA");
      } else {
        GetSession()->bout.NewLine();
        GetSession()->bout << "|#6My records show differently.\r\n\n";
        ok = false;
      }
    } else {
      GetSession()->GetCurrentUser()->SetCity(city);
      GetSession()->GetCurrentUser()->SetState(state);
      GetSession()->GetCurrentUser()->SetCountry("USA");
    }
  }
  return ok;
}


bool check_dupes(const char *pszPhoneNumber) {
  int nUserNumber = find_phone_number(pszPhoneNumber);
  if (nUserNumber && nUserNumber != GetSession()->usernum) {
    char szBuffer[ 255 ];
    sprintf(szBuffer, "    %s entered phone # %s", GetSession()->GetCurrentUser()->GetName(), pszPhoneNumber);
    sysoplog(szBuffer, false);
    ssm(1, 0, szBuffer);

    WUser user;
    GetApplication()->GetUserManager()->ReadUser(&user, nUserNumber);
    sprintf(szBuffer, "      also entered by %s", user.GetName());
    sysoplog(szBuffer, false);
    ssm(1, 0, szBuffer);

    return true;
  }

  return false;
}



void noabort(const char *pszFileName) {
  bool oic = false;

  if (GetSession()->using_modem) {
    oic   = incom;
    incom = false;
    dump();
  }
  printfile(pszFileName);
  if (GetSession()->using_modem) {
    dump();
    incom = oic;
  }
}


void cln_nu() {
  GetSession()->bout.Color(0);
  int i1 = GetSession()->localIO()->WhereX();
  if (i1 > 28) {
    for (int i = i1; i > 28; i--) {
      GetSession()->bout.BackSpace();
    }
  }
  GetSession()->bout.ClearEOL();
}


void DoMinimalNewUser() {
  int m =  1, d =  1, y = 2000, ch =  0;
  char s[101], s1[81], m1[3], d1[3], y1[4];
  static const char *mon[12] = {
    "January",
    "February",
    "March",
    "April",
    "May",
    "June",
    "July",
    "August",
    "September",
    "October",
    "November",
    "December"
  };

  newline = false;
  s1[0] = 0;
  bool done = false;
  int nSaveTopData = GetSession()->topdata;
  GetSession()->topdata = WLocalIO::topdataNone;
  GetApplication()->UpdateTopScreen();
  do {
    GetSession()->bout.ClearScreen();
    GetSession()->bout.DisplayLiteBar("%s New User Registration", syscfg.systemname);
    GetSession()->bout << "|#1[A] Name (real or alias)  : ";
    if (GetSession()->GetCurrentUser()->GetName()[0] == '\0') {
      bool ok = true;
      char szTempName[ 81 ];
      do {
        Input1(szTempName, s1, 30, true, INPUT_MODE_FILE_UPPER);
        ok = check_name(szTempName);
        if (!ok) {
          cln_nu();
          BackPrint("I'm sorry, you can't use that name.", 6, 20, 1000);
        }
      } while (!ok && !hangup);
      GetSession()->GetCurrentUser()->SetName(szTempName);
    }
    s1[0] = '\0';
    cln_nu();
    GetSession()->bout << "|#2" << GetSession()->GetCurrentUser()->GetName();
    GetSession()->bout.NewLine();
    GetSession()->bout << "|#1[B] Birth Date (MM/DD/YY) : ";
    if (GetSession()->GetCurrentUser()->GetAge() == 0) {
      bool ok = false;
      do {
        ok = false;
        cln_nu();
        if ((Input1(s, s1, 8, false, INPUT_MODE_DATE)) == 8) {
          sprintf(m1, "%c%c", s[0], s[1]);
          sprintf(d1, "%c%c", s[3], s[4]);
          sprintf(y1, "%c%c", s[6], s[7]);
          m = atoi(m1);
          d = atoi(d1);
          y = atoi(y1) + 1900;
          ok = true;
          if ((((m == 2) || (m == 9) || (m == 4) || (m == 6) || (m == 11)) && (d >= 31)) ||
              ((m == 2) && (((!isleap(y)) && (d == 29)) || (d == 30))) ||
              (years_old(m, d, y - 1900) < 5) ||
              (d > 31) || ((m == 0) || (y == 0) || (d == 0))) {
            ok = false;
          }
          if (m > 12) {
            ok = false;
          }
        }
        if (!ok) {
          cln_nu();
          BackPrint("Invalid Birthdate.", 6, 20, 1000);
        }
      } while (!ok && !hangup);
      s1[0] = '\0';
    }
    if (hangup) {
      return;
    }
    GetSession()->GetCurrentUser()->SetBirthdayMonth(m);
    GetSession()->GetCurrentUser()->SetBirthdayDay(d);
    GetSession()->GetCurrentUser()->SetBirthdayYear(y - 1900);
    GetSession()->GetCurrentUser()->SetAge(years_old(m, d, y - 1900));
    s1[0] = '\0';
    cln_nu();
    GetSession()->bout << "|#2" <<
                       mon[ std::max<int>(0, GetSession()->GetCurrentUser()->GetBirthdayMonth() - 1) ] <<
                       " " <<
                       GetSession()->GetCurrentUser()->GetBirthdayDay() <<
                       ", 19" <<
                       GetSession()->GetCurrentUser()->GetBirthdayYear() <<
                       " (" <<
                       GetSession()->GetCurrentUser()->GetAge() <<
                       " years old)\r\n";
    GetSession()->bout << "|#1[C] Sex (Gender)          : ";
    if (GetSession()->GetCurrentUser()->GetGender() != 'M' && GetSession()->GetCurrentUser()->GetGender()  != 'F') {
      GetSession()->bout.ColorizedInputField(1);
      GetSession()->GetCurrentUser()->SetGender(onek_ncr("MF"));
    }
    s1[0] = '\0';
    cln_nu();
    GetSession()->bout << "|#2" << (GetSession()->GetCurrentUser()->GetGender() == 'M' ? "Male" : "Female") << wwiv::endl;
    GetSession()->bout <<  "|#1[D] Country               : " ;
    if (GetSession()->GetCurrentUser()->GetCountry()[0] == '\0') {
      Input1(reinterpret_cast<char*>(GetSession()->GetCurrentUser()->data.country), "", 3, false, INPUT_MODE_FILE_UPPER);
      if (GetSession()->GetCurrentUser()->GetCountry()[0] == '\0') {
        GetSession()->GetCurrentUser()->SetCountry("USA");
      }
    }
    s1[0] = '\0';
    cln_nu();
    GetSession()->bout << "|#2" << GetSession()->GetCurrentUser()->GetCountry() << wwiv::endl;
    GetSession()->bout << "|#1[E] ZIP or Postal Code    : ";
    if (GetSession()->GetCurrentUser()->GetZipcode()[0] == 0) {
      bool ok = false;
      do {
        if (wwiv::strings::IsEquals(GetSession()->GetCurrentUser()->GetCountry(), "USA")) {
          Input1(reinterpret_cast<char*>(GetSession()->GetCurrentUser()->data.zipcode), s1, 5, true, INPUT_MODE_FILE_UPPER);
          check_zip(GetSession()->GetCurrentUser()->GetZipcode(), 2);
        } else {
          Input1(reinterpret_cast<char*>(GetSession()->GetCurrentUser()->data.zipcode), s1, 7, true, INPUT_MODE_FILE_UPPER);
        }
        if (GetSession()->GetCurrentUser()->GetZipcode()[0]) {
          ok = true;
        }
      } while (!ok && !hangup);
    }
    s1[0] = '\0';
    cln_nu();
    GetSession()->bout << "|#2" << GetSession()->GetCurrentUser()->GetZipcode() << wwiv::endl;
    GetSession()->bout << "|#1[F] City/State/Province   : ";
    if (GetSession()->GetCurrentUser()->GetCity()[0] == 0) {
      bool ok = false;
      do {
        Input1(reinterpret_cast<char*>(GetSession()->GetCurrentUser()->data.city), s1, 30, true, INPUT_MODE_FILE_PROPER);
        if (GetSession()->GetCurrentUser()->GetCity()[0]) {
          ok = true;
        }
      } while (!ok && !hangup);
      GetSession()->bout << ", ";
      if (GetSession()->GetCurrentUser()->GetState()[0] == 0) {
        do {
          bool ok = false;
          Input1(reinterpret_cast<char*>(GetSession()->GetCurrentUser()->data.state), s1, 2, true, INPUT_MODE_FILE_UPPER);
          if (GetSession()->GetCurrentUser()->GetState()[0]) {
            ok = true;
          }
        } while (!ok && !hangup);
      }
    }
    s1[0] = '\0';
    cln_nu();
    properize(reinterpret_cast<char*>(GetSession()->GetCurrentUser()->data.city));
    GetSession()->bout << "|#2" << GetSession()->GetCurrentUser()->GetCity() << ", " <<
                       GetSession()->GetCurrentUser()->GetState() << wwiv::endl;
    GetSession()->bout << "|#1[G] Internet Mail Address : ";
    if (GetSession()->GetCurrentUser()->GetEmailAddress()[0] == 0) {
      std::string emailAddress;
      Input1(emailAddress, s1, 44, true, INPUT_MODE_FILE_MIXED);
      GetSession()->GetCurrentUser()->SetEmailAddress(emailAddress.c_str());
      if (!check_inet_addr(GetSession()->GetCurrentUser()->GetEmailAddress())) {
        cln_nu();
        BackPrint("Invalid address!", 6, 20, 1000);
      }
      if (GetSession()->GetCurrentUser()->GetEmailAddress()[0] == 0) {
        GetSession()->GetCurrentUser()->SetEmailAddress("None");
      }
    }
    s1[0] = '\0';
    cln_nu();
    GetSession()->bout << "|#2" << GetSession()->GetCurrentUser()->GetEmailAddress() << wwiv::endl;
    GetSession()->bout.NewLine();
    GetSession()->bout << "|#5Item to change or [Q] to Quit : |#0";
    ch = onek("QABCDEFG");
    switch (ch) {
    case 'Q':
      done = true;
      break;
    case 'A':
      strcpy(s1, GetSession()->GetCurrentUser()->GetName());
      GetSession()->GetCurrentUser()->SetName("");
      break;
    case 'B':
      GetSession()->GetCurrentUser()->SetAge(0);
      sprintf(s1, "%02d/%02d/%02d", GetSession()->GetCurrentUser()->GetBirthdayDay(),
              GetSession()->GetCurrentUser()->GetBirthdayMonth(), GetSession()->GetCurrentUser()->GetBirthdayYear());
      break;
    case 'C':
      GetSession()->GetCurrentUser()->SetGender('N');
      break;
    case 'D':
      GetSession()->GetCurrentUser()->SetCountry("");
    case 'E':
      GetSession()->GetCurrentUser()->SetZipcode("");
    case 'F':
      strcpy(s1, GetSession()->GetCurrentUser()->GetCity());
      GetSession()->GetCurrentUser()->SetCity("");
      GetSession()->GetCurrentUser()->SetState("");
      break;
    case 'G':
      strcpy(s1, GetSession()->GetCurrentUser()->GetEmailAddress());
      GetSession()->GetCurrentUser()->SetEmailAddress("");
      break;
    }
  } while (!done && !hangup);
  GetSession()->GetCurrentUser()->SetRealName(GetSession()->GetCurrentUser()->GetName());
  GetSession()->GetCurrentUser()->SetVoicePhoneNumber("999-999-9999");
  GetSession()->GetCurrentUser()->SetDataPhoneNumber(GetSession()->GetCurrentUser()->GetVoicePhoneNumber());
  GetSession()->GetCurrentUser()->SetStreet("None Requested");
  if (GetSession()->GetNumberOfEditors() && GetSession()->GetCurrentUser()->HasAnsi()) {
    GetSession()->GetCurrentUser()->SetDefaultEditor(1);
  }
  GetSession()->topdata = nSaveTopData;
  GetApplication()->UpdateTopScreen();
  newline = true;
}


void new_mail() {
  WFile file(syscfg.gfilesdir, (GetSession()->GetCurrentUser()->GetSl() > syscfg.newusersl) ? NEWSYSOP_MSG : NEWMAIL_MSG);
  if (!file.Exists()) {
    return;
  }
  GetSession()->SetNewMailWaiting(true);
  int save_ed = GetSession()->GetCurrentUser()->GetDefaultEditor();
  GetSession()->GetCurrentUser()->SetDefaultEditor(0);
  LoadFileIntoWorkspace(file.GetFullPathName().c_str(), true);
  use_workspace = true;
  std::string userName = GetSession()->GetCurrentUser()->GetUserNameAndNumber(GetSession()->usernum);
  sprintf(irt, "Welcome to %s!", syscfg.systemname);

  int nAllowAnon = 0;
  messagerec msg;
  msg.storage_type = 2;
  inmsg(&msg, irt, &nAllowAnon, false, "email", INMSG_NOFSED, userName.c_str(), MSGED_FLAG_NONE, true);
  sendout_email(irt, &msg, 0, GetSession()->usernum, 0, 1, 1, 0, 1, 0);
  GetSession()->GetCurrentUser()->SetNumMailWaiting(GetSession()->GetCurrentUser()->GetNumMailWaiting() + 1);
  GetSession()->GetCurrentUser()->SetDefaultEditor(save_ed);
  GetSession()->SetNewMailWaiting(false);
}

