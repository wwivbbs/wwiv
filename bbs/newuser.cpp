/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
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
#include "bbs/newuser.h"

#include <algorithm>
#include <string>

#include "bbs/asv.h"
#include "bbs/confutil.h"
#include "bbs/wwiv.h"
#include "bbs/datetime.h"
#include "bbs/dropfile.h"
#include "bbs/inmsg.h"
#include "bbs/input.h"
#include "bbs/printfile.h"
#include "bbs/stuffin.h"
#include "bbs/uedit.h"
#include "bbs/wconstants.h"
#include "bbs/wstatus.h"
#include "core/inifile.h"
#include "core/strings.h"
#include "core/textfile.h"

using std::string;
using wwiv::bbs::InputMode;
using wwiv::core::FilePath;
using wwiv::core::IniFile;
using namespace wwiv::strings;;

// Local function prototypes

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
bool CheckPasswordComplexity(WUser *pUser, string& password);


static void input_phone() {
  bool ok = true;
  string phoneNumber;
  do {
    bout.nl();
    bout << "|#3Enter your VOICE phone no. in the form:\r\n|#3 ###-###-####\r\n|#2:";
    phoneNumber = Input1(session()->user()->GetVoicePhoneNumber(), 12, true, InputMode::PHONE);

    ok = valid_phone(phoneNumber.c_str());
    if (!ok) {
      bout.nl();
      bout << "|#6Please enter a valid phone number in the correct format.\r\n";
    }
  } while (!ok && !hangup);
  if (!hangup) {
    session()->user()->SetVoicePhoneNumber(phoneNumber.c_str());
  }
}

void input_dataphone() {
  bool ok = true;
  do {
    bout.nl();
    bout << "|#3Enter your DATA phone no. in the form. \r\n";
    bout << "|#3 ###-###-#### - Press Enter to use [" << session()->user()->GetVoicePhoneNumber()
         << "].\r\n";
    string data_phone_number = Input1(session()->user()->GetDataPhoneNumber(), 12, true, InputMode::PHONE);
    if (data_phone_number[0] == '\0') {
      data_phone_number = session()->user()->GetVoicePhoneNumber();
    }
    ok = valid_phone(data_phone_number);

    if (ok) {
      session()->user()->SetDataPhoneNumber(data_phone_number.c_str());
    } else {
      bout.nl();
      bout << "|#6Please enter a valid phone number in the correct format.\r\n";
    }
  } while (!ok && !hangup);
}

void input_language() {
  if (session()->num_languages > 1) {
    session()->user()->SetLanguage(255);
    do {
      char ch = 0, onx[20];
      bout.nl(2);
      for (int i = 0; i < session()->num_languages; i++) {
        bout << (i + 1) << ". " << languages[i].name << wwiv::endl;
        if (i < 9) {
          onx[i] = static_cast< char >('1' + i);
        }
      }
      bout.nl();
      bout << "|#2Which language? ";
      if (session()->num_languages < 10) {
        onx[session()->num_languages] = 0;
        ch = onek(onx);
        ch -= '1';
      } else {
        int i;
        for (i = 1; i <= session()->num_languages / 10; i++) {
          odc[i - 1] = static_cast< char >('0' + i);
        }
        odc[i - 1] = 0;
        char* ss = mmkey(2);
        ch = *((ss) - 1);
      }
      if (ch >= 0 && ch < session()->num_languages) {
        session()->user()->SetLanguage(languages[ch].num);
      }
    } while ((session()->user()->GetLanguage() == 255) && (!hangup));

    set_language(session()->user()->GetLanguage());
  }
}

static bool check_name(const string userName) {
  // Since finduser is called with userName, it can not be const.  A better idea may be
  // to change this behaviour in the future.
  char s[255], s1[255], s2[MAX_PATH];

  if (userName.length() == 0 ||
      userName[ userName.length() - 1 ] == 32   ||
      userName[0] < 65                 ||
      finduser(userName) != 0          ||
      userName.find("@") != string::npos ||
      userName.find("#") != string::npos) {
    return false;
  }

  File trashFile(syscfg.gfilesdir, TRASHCAN_TXT);
  if (!trashFile.Open(File::modeReadOnly | File::modeBinary)) {
    return true;
  }

  trashFile.Seek(0L, File::seekBegin);
  long lTrashFileLen = trashFile.GetLength();
  long lTrashFilePointer = 0;
  sprintf(s2, " %s ", userName.c_str());
  bool ok = true;
  while (lTrashFilePointer < lTrashFileLen && ok && !hangup) {
    CheckForHangup();
    trashFile.Seek(lTrashFilePointer, File::seekBegin);
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
    if (strstr(s2, s1) != nullptr) {
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
    bout.nl();
    if (syscfg.sysconfig & sysconfig_no_alias) {
      bout << "|#3Enter your FULL REAL name.\r\n";
    } else {
      bout << "|#3Enter your full name, or your alias.\r\n";
    }
    string temp_local_name = Input1(session()->user()->GetName(), 30, true, InputMode::UPPER);
    ok = check_name(temp_local_name);
    if (ok) {
      session()->user()->SetName(temp_local_name.c_str());
    } else {
      bout.nl();
      bout << "|#6I'm sorry, you can't use that name.\r\n";
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
      bout.nl();
      bout << "|#3Enter your FULL real name.\r\n";
      string temp_local_name = Input1(session()->user()->GetRealName(), 30, true, InputMode::PROPER);
      if (temp_local_name.empty()) {
        bout.nl();
        bout << "|#6Sorry, you must enter your FULL real name.\r\n";
      } else {
        session()->user()->SetRealName(temp_local_name.c_str());
      }
    } while ((session()->user()->GetRealName()[0] == '\0') && !hangup);
  } else {
    session()->user()->SetRealName(session()->user()->GetName());
  }
}

static void input_callsign() {
  bout.nl();
  bout << " |#3Enter your amateur radio callsign, or just hit <ENTER> if none.\r\n|#2:";
  string s;
  input(&s, 6, true);
  session()->user()->SetCallsign(s.c_str());
}

bool valid_phone(const string& phoneNumber) {
  if (syscfg.sysconfig & sysconfig_free_phone) {
    return true;
  }

  if (syscfg.sysconfig & sysconfig_extended_info) {
    if (!IsPhoneNumberUSAFormat(session()->user()) && session()->user()->GetCountry()[0]) {
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
  string street;
  do {
    bout.nl();
    bout << "|#3Enter your street address.\r\n";
    street = Input1(session()->user()->GetStreet(), 30, true, InputMode::PROPER);

    if (street.empty()) {
      bout.nl();
      bout << "|#6I'm sorry, you must enter your street address.\r\n";
    }
  } while (street.empty() && !hangup);
  if (!hangup) {
    session()->user()->SetStreet(street.c_str());
  }
}

void input_city() {
  string city;
  do {
    bout.nl();
    bout << "|#3Enter your city (i.e San Francisco). \r\n";
    city = Input1(session()->user()->GetCity(), 30, false, InputMode::PROPER);

    if (city.empty()) {
      bout.nl();
      bout << "|#6I'm sorry, you must enter your city.\r\n";
    }
  } while (city.empty() && !hangup);
  session()->user()->SetCity(city.c_str());
}

void input_state() {
  string state;
  do {
    bout.nl();
    if (IsEquals(session()->user()->GetCountry(), "CAN")) {
      bout << "|#3Enter your province (i.e. QC).\r\n";
    } else {
      bout << "|#3Enter your state (i.e. CA). \r\n";
    }
    bout << "|#2:";
    input(&state, 2, true);

    if (state.empty()) {
      bout.nl();
      bout << "|#6I'm sorry, you must enter your state or province.\r\n";
    }
  } while (state.empty() && (!hangup));
  session()->user()->SetState(state.c_str());
}

void input_country() {
  string country;
  do {
    bout.nl();
    bout << "|#3Enter your country (i.e. USA). \r\n";
    bout << "|#3Hit Enter for \"USA\"\r\n";
    bout << "|#2:";
    input(&country, 3, true);
    if (country.empty()) {
      country = "USA";
    }
  } while (country.empty() && (!hangup));
  session()->user()->SetCountry(country.c_str());
}

void input_zipcode() {
  string zipcode;
  do {
    int len = 7;
    bout.nl();
    if (IsEquals(session()->user()->GetCountry(), "USA")) {
      bout << "|#3Enter your zipcode as #####-#### \r\n";
      len = 10;
    } else {
      bout << "|#3Enter your postal code as L#L-#L#\r\n";
      len = 7;
    }
    bout << "|#2:";
    input(&zipcode, len, true);

    if (zipcode.empty()) {
      bout.nl();
      bout << "|#6I'm sorry, you must enter your zipcode.\r\n";
    }
  } while (zipcode.empty() && (!hangup));
  session()->user()->SetZipcode(zipcode.c_str());
}

void input_sex() {
  bout.nl();
  bout << "|#2Your gender (M,F) :";
  session()->user()->SetGender(onek("MF"));
}

void input_age(WUser *pUser) {
  int y = 2000, m = 1, d = 1;
  char ag[10], s[81];
  time_t t;
  time(&t);
  struct tm * pTm = localtime(&t);

  bool ok = false;
  do {
    bout.nl();
    do {
      bout.nl();
      bout << "|#2Month you were born (1-12) : ";
      input(ag, 2, true);
      m = atoi(ag);
    } while (!hangup && (m > 12 || m < 1));

    do {
      bout.nl();
      bout << "|#2Day of month you were born (1-31) : ";
      input(ag, 2, true);
      d = atoi(ag);
    } while (!hangup && (d > 31 || d < 1));

    do {
      bout.nl();
      y = static_cast<int>(pTm->tm_year + 1900 - 10) / 100;
      bout << "|#2Year you were born: ";
      sprintf(s, "%2d", y);
      Input1(ag, s, 4, true, InputMode::MIXED);
      y = atoi(ag);
      if (y == 1919) {
        bout.nl();
        bout << "|#5Were you really born in 1919? ";
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
      bout.nl();
      bout << "|#6There aren't that many days in that month.\r\n";
    }
    if (years_old(m, d, y) < 5) {
      bout.nl();
      bout << "Not likely\r\n";
      ok = false;
    }
  } while (!ok && !hangup);
  pUser->SetBirthdayMonth(m);
  pUser->SetBirthdayDay(d);
  pUser->SetBirthdayYear(y);
  pUser->SetAge(years_old(pUser->GetBirthdayMonth(), pUser->GetBirthdayDay(), pUser->GetBirthdayYear()));
  bout.nl();
}


void input_comptype() {
  int ct = -1;
  char c[5];

  bool ok = true;
  do {
    bout.nl();
    bout << "Known computer types:\r\n\n";
    int i = 0;
    for (i = 1; !ctypes(i).empty(); i++) {
      bout << i << ". " << ctypes(i) << wwiv::endl;
    }
    bout.nl();
    bout << "|#3Enter your computer type, or the closest to it (ie, Compaq -> IBM).\r\n";
    bout << "|#2:";
    input(c, 2, true);
    ct = atoi(c);

    ok = true;
    if (ct < 1 || ct > i) {
      ok = false;
    }
  } while (!ok && !hangup);

  session()->user()->SetComputerType(ct);
  if (hangup) {
    session()->user()->SetComputerType(-1);
  }
}

void input_screensize() {
  int x = 0, y = 0;
  char s[5];

  bool ok = true;
  do {
    bout.nl();
    bout << "|#3How wide is your screen (chars, <CR>=80) ?\r\n|#2:";
    input(s, 2, true);
    x = atoi(s);
    if (s[0] == '\0') {
      x = 80;
    }

    ok = ((x < 32) || (x > 80)) ? false : true;
  } while (!ok && !hangup);

  do {
    bout.nl();
    bout << "|#3How tall is your screen (lines, <CR>=24) ?\r\n|#2:";
    input(s, 2, true);
    y = atoi(s);
    if (s[0] == '\0') {
      y = 24;
    }

    ok = (y < 8 || y > 60) ? false : true;
  } while (!ok && !hangup);

  session()->user()->SetScreenChars(x);
  session()->user()->SetScreenLines(y);
  session()->screenlinest = y;
}


bool CheckPasswordComplexity(WUser *, string& password) {
  if (password.length() < 3) {
    //TODO - the min length should be in wwiv.ini
    return false;
  }

  //if ( password.find( pUser->GetName() ) != string::npos )
  //{
  //    return false;
  //}

  //if ( password.find( realName ) != string::npos )
  //{
  //    return false;
  //}

  //if( password.find( pUser->GetVoicePhoneNumber() ) != string::npos )
  //{
  //    return false;
  //}

  //if ( password.find( pUser->GetDataPhoneNumber() ) != string::npos )
  //{
  //    return false;
  //}

  return true;
}


void input_pw(WUser *pUser) {
  string password;
  bool ok = true;
  do {
    ok = true;
    bout.nl();
    bout << "|#3Please enter a new password, 3-8 chars.\r\n";
    password.clear();
    password = Input1("", 8, false, InputMode::UPPER);

    string realName = session()->user()->GetRealName();
    StringUpperCase(&realName);
    if (!CheckPasswordComplexity(pUser, password)) {
      ok = false;
      bout.nl(2);
      bout << "Invalid password.  Try again.\r\n\n";
    }
  } while (!ok && !hangup);

  if (ok) {
    pUser->SetPassword(password.c_str());
  } else {
    bout << "Password not changed.\r\n";
  }
}


void input_ansistat() {
  session()->user()->ClearStatusFlag(WUser::ansi);
  session()->user()->ClearStatusFlag(WUser::color);
  bout.nl();
  if (check_ansi() == 1) {
    bout << "ANSI graphics support detected.  Use it? ";
  } else {
    bout << "[0;34;3mTEST[C";
    bout << "[0;30;45mTEST[C";
    bout << "[0;1;31;44mTEST[C";
    bout << "[0;32;7mTEST[C";
    bout << "[0;1;5;33;46mTEST[C";
    bout << "[0;4mTEST[0m\r\n";
    bout << "Is the above line colored, italicized, bold, inversed, or blinking? ";
  }
  if (noyes()) {
    session()->user()->SetStatusFlag(WUser::ansi);
    bout.nl();
    bout << "|#5Do you want color? ";
    if (noyes()) {
      session()->user()->SetStatusFlag(WUser::color);
      session()->user()->SetStatusFlag(WUser::extraColor);
    } else {
      color_list();
      bout.nl();
      bout << "|#2Monochrome base color (<C/R>=7)? ";
      char ch = onek("\r123456789");
      if (ch == '\r') {
        ch = '7';
      }
      int c  = ch - '0';
      int c2 = c << 4;
      for (int i = 0; i < 10; i++) {
        if ((session()->user()->GetBWColor(i) & 0x70) == 0) {
          session()->user()->SetBWColor(i, (session()->user()->GetBWColor(i) & 0x88) | c);
        } else {
          session()->user()->SetBWColor(i, (session()->user()->GetBWColor(i) & 0x88) | c2);
        }
      }
    }
  }
}

static int find_new_usernum(const WUser* pUser, uint32_t* qscn) {
  File userFile(syscfg.datadir, USER_LST);
  for (int i = 0; !userFile.IsOpen() && (i < 20); i++) {
    if (!userFile.Open(File::modeBinary | File::modeReadWrite | File::modeCreateFile)) {
      Wait(0.1);
    }
  }
  if (!userFile.IsOpen()) {
    return -1;
  }

  int nNewUserNumber = static_cast<int>((userFile.GetLength() / syscfg.userreclen) - 1);
  userFile.Seek(syscfg.userreclen, File::seekBegin);
  int nUserNumber = 1;

  if (nNewUserNumber == application()->GetStatusManager()->GetUserCount()) {
    nUserNumber = nNewUserNumber + 1;
  } else {
    while (nUserNumber <= nNewUserNumber) {
      if (nUserNumber % 25 == 0) {
        userFile.Close();
        for (int n = 0; !userFile.IsOpen() && (n < 20); n++) {
          if (!userFile.Open(File::modeBinary | File::modeReadWrite | File::modeCreateFile)) {
            Wait(0.1);
          }
        }
        if (!userFile.IsOpen()) {
          return -1;
        }
        userFile.Seek(static_cast<long>(nUserNumber * syscfg.userreclen), File::seekBegin);
        nNewUserNumber = static_cast<int>((userFile.GetLength() / syscfg.userreclen) - 1);
      }
      WUser tu;
      userFile.Read(&tu.data, syscfg.userreclen);

      if (tu.IsUserDeleted() && tu.GetSl() != 255) {
        userFile.Seek(static_cast<long>(nUserNumber * syscfg.userreclen), File::seekBegin);
        userFile.Write(&pUser->data, syscfg.userreclen);
        userFile.Close();
        write_qscn(nUserNumber, qscn, false);
        InsertSmallRecord(nUserNumber, pUser->GetName());
        return nUserNumber;
      } else {
        nUserNumber++;
      }
    }
  }

  if (nUserNumber <= syscfg.maxusers) {
    userFile.Seek(static_cast<long>(nUserNumber * syscfg.userreclen), File::seekBegin);
    userFile.Write(&pUser->data, syscfg.userreclen);
    userFile.Close();
    write_qscn(nUserNumber, qscn, false);
    InsertSmallRecord(nUserNumber, pUser->GetName());
    return nUserNumber;
  } else {
    userFile.Close();
    return -1;
  }
}

// Clears session()->user()'s data and makes it ready to be a new user, also
// clears the QScan pointers
void CreateNewUserRecord() {
  session()->user()->ZeroUserData();
  memset(qsc, 0, syscfg.qscn_len);

  session()->user()->SetFirstOn(date());
  session()->user()->SetLastOn("Never.");
  session()->user()->SetMacro(0, "Wow! This is a GREAT BBS!");
  session()->user()->SetMacro(1, "Guess you forgot to define this one....");
  session()->user()->SetMacro(2, "User = Monkey + Keyboard");

  session()->user()->SetScreenLines(25);
  session()->user()->SetScreenChars(80);

  session()->user()->SetSl(syscfg.newusersl);
  session()->user()->SetDsl(syscfg.newuserdsl);

  session()->user()->SetTimesOnToday(1);
  session()->user()->SetLastOnDateNumber(0);
  session()->user()->SetRestriction(syscfg.newuser_restrict);

  *qsc = 999;
  memset(qsc_n, 0xff, ((session()->GetMaxNumberFileAreas() + 31) / 32) * 4);
  memset(qsc_q, 0xff, ((session()->GetMaxNumberMessageAreas() + 31) / 32) * 4);

  session()->user()->SetStatusFlag(WUser::pauseOnPage);
  session()->user()->ClearStatusFlag(WUser::conference);
  session()->user()->ClearStatusFlag(WUser::nscanFileSystem);
  session()->user()->SetGold(syscfg.newusergold);

  for (int nColorLoop = 0; nColorLoop <= 9; nColorLoop++) {
    session()->user()->SetColor(nColorLoop, session()->newuser_colors[ nColorLoop ]);
    session()->user()->SetBWColor(nColorLoop, session()->newuser_bwcolors[ nColorLoop ]);
  }

  session()->ResetEffectiveSl();
  string randomPassword;
  for (int i = 0; i < 6; i++) {
    char ch = static_cast< char >(rand() % 36);
    if (ch < 10) {
      ch += '0';
    } else {
      ch += 'A' - 10;
    }
    randomPassword += ch;
  }
  session()->user()->SetPassword(randomPassword.c_str());
  session()->user()->SetEmailAddress("");

}


// returns true if the user is allow to create a new user account
// on here, if this function returns false, a sufficient error
// message has already been displayed to the user.
bool CanCreateNewUserAccountHere() {
  if (application()->GetStatusManager()->GetUserCount() >= syscfg.maxusers) {
    bout.nl(2);
    bout << "I'm sorry, but the system currently has the maximum number of users it can\r\nhandle.\r\n\n";
    return false;
  }

  if (syscfg.closedsystem) {
    bout.nl(2);
    bout << "I'm sorry, but the system is currently closed, and not accepting new users.\r\n\n";
    return false;
  }

  if ((syscfg.newuserpw[0] != 0) && (incom)) {
    bout.nl(2);
    bool ok = false;
    int nPasswordAttempt = 0;
    do {
      bout << "New User Password :";
      string password;
      input(&password, 20);
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
  IniFile iniFile(FilePath(application()->GetHomeDir(), WWIV_INI), INI_TAG);
  if (iniFile.IsOpen()) {
    return iniFile.GetBooleanValue("NEWUSER_MIN");
  }
  return false;
}


void DoFullNewUser() {
  input_name();
  input_realname();
  input_phone();
  if (application()->HasConfigFlag(OP_FLAGS_CHECK_DUPE_PHONENUM)) {
    if (check_dupes(session()->user()->GetVoicePhoneNumber())) {
      if (application()->HasConfigFlag(OP_FLAGS_HANGUP_DUPE_PHONENUM)) {
        hangup = true;
        hang_it_up();
        return;
      }
    }
  }
  if (syscfg.sysconfig & sysconfig_extended_info) {
    input_street();
    char szZipFileName[ MAX_PATH ];
    sprintf(szZipFileName, "%s%s%czip1.dat", syscfg.datadir, ZIPCITY_DIR, File::pathSeparatorChar);
    if (File::Exists(szZipFileName)) {
      input_zipcode();
      if (!check_zip(session()->user()->GetZipcode(), 1)) {
        session()->user()->SetCity("");
        session()->user()->SetState("");
      }
      session()->user()->SetCountry("USA");
    }
    if (session()->user()->GetCity()[0] == '\0') {
      input_city();
    }
    if (session()->user()->GetState()[0] == '\0') {
      input_state();
    }
    if (session()->user()->GetZipcode()[0] == '\0') {
      input_zipcode();
    }
    if (session()->user()->GetCountry()[0] == '\0') {
      input_country();
    }
    input_dataphone();

    if (application()->HasConfigFlag(OP_FLAGS_CHECK_DUPE_PHONENUM)) {
      if (check_dupes(session()->user()->GetDataPhoneNumber())) {
        if (application()->HasConfigFlag(OP_FLAGS_HANGUP_DUPE_PHONENUM)) {
          hangup = true;
          hang_it_up();
          return;
        }
      }
    }
  }
  input_callsign();
  input_sex();
  input_age(session()->user());
  input_comptype();

  if (session()->GetNumberOfEditors() && session()->user()->HasAnsi()) {
    bout.nl();
    bout << "|#5Select a fullscreen editor? ";
    if (yesno()) {
      select_editor();
    } else {
      for (int nEditor = 0; nEditor < session()->GetNumberOfEditors(); nEditor++) {
        char szEditorDesc[ 121 ];
        strcpy(szEditorDesc, editors[nEditor].description);
        if (strstr(strupr(szEditorDesc) , "WWIVEDIT") != nullptr) {
          session()->user()->SetDefaultEditor(nEditor + 1);
          nEditor = session()->GetNumberOfEditors();
        }
      }
    }
    bout.nl();
  }
  bout << "|#5Select a default transfer protocol? ";
  if (yesno()) {
    bout.nl();
    bout << "Enter your default protocol, or 0 for none.\r\n\n";
    int nDefProtocol = get_protocol(xf_down);
    if (nDefProtocol) {
      session()->user()->SetDefaultProtocol(nDefProtocol);
    }
  }
}


void DoNewUserASV() {
  if (application()->HasConfigFlag(OP_FLAGS_ADV_ASV)) {
    asv();
    return;
  }
  if (application()->HasConfigFlag(OP_FLAGS_SIMPLE_ASV) &&
      session()->asv.sl > syscfg.newusersl && session()->asv.sl < 90) {
    bout.nl();
    bout << "|#5Are you currently a WWIV SysOp? ";
    if (yesno()) {
      bout.nl();
      bout << "|#5Please enter your BBS name and number.\r\n";
      string note;
      inputl(&note, 60, true);
      session()->user()->SetNote(note.c_str());
      session()->user()->SetSl(session()->asv.sl);
      session()->user()->SetDsl(session()->asv.dsl);
      session()->user()->SetAr(session()->asv.ar);
      session()->user()->SetDar(session()->asv.dar);
      session()->user()->SetExempt(session()->asv.exempt);
      session()->user()->SetRestriction(session()->asv.restrict);
      bout.nl();
      printfile(ASV_NOEXT);
      bout.nl();
      pausescr();
    }
  }
}


void VerifyNewUserFullInfo() {
  bool ok = false;
  do {
    bout.nl(2);
    bout << "|#91) Name          : |#2" << session()->user()->GetName() << wwiv::endl;
    if (!(syscfg.sysconfig & sysconfig_no_alias)) {
      bout << "|#92) Real Name     : |#2" << session()->user()->GetRealName() << wwiv::endl;
    }
    bout << "|#93) Callsign      : |#2" << session()->user()->GetCallsign() << wwiv::endl;
    bout << "|#94) Phone No.     : |#2" << session()->user()->GetVoicePhoneNumber() <<
                       wwiv::endl;
    bout << "|#95) Gender        : |#2" << session()->user()->GetGender() << wwiv::endl;
    bout << "|#96) Birthdate     : |#2" <<
                       static_cast<int>(session()->user()->GetBirthdayMonth()) << "/" <<
                       static_cast<int>(session()->user()->GetBirthdayDay()) << "/" <<
                       static_cast<int>(session()->user()->GetBirthdayYear()) << wwiv::endl;
    bout << "|#97) Computer type : |#2" << ctypes(session()->user()->GetComputerType()) <<
                       wwiv::endl;
    bout << "|#98) Screen size   : |#2" <<
                       session()->user()->GetScreenChars() << " X " <<
                       session()->user()->GetScreenLines() << wwiv::endl;
    bout << "|#99) Password      : |#2" << session()->user()->GetPassword() << wwiv::endl;
    if (syscfg.sysconfig & sysconfig_extended_info) {
      bout << "|#9A) Street Address: |#2" << session()->user()->GetStreet() << wwiv::endl;
      bout << "|#9B) City          : |#2" << session()->user()->GetCity() << wwiv::endl;
      bout << "|#9C) State         : |#2" << session()->user()->GetState() << wwiv::endl;
      bout << "|#9D) Country       : |#2" << session()->user()->GetCountry() << wwiv::endl;
      bout << "|#9E) Zipcode       : |#2" << session()->user()->GetZipcode() << wwiv::endl;
      bout << "|#9F) Dataphone     : |#2" << session()->user()->GetDataPhoneNumber() << wwiv::endl;
    }
    bout.nl();
    bout << "Q) No changes.\r\n";
    bout.nl(2);
    char ch = 0;
    if (syscfg.sysconfig & sysconfig_extended_info) {
      bout << "|#9Which (1-9,A-F,Q) : ";
      ch = onek("Q123456789ABCDEF");
    } else {
      bout << "|#9Which (1-9,Q) : ";
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
      input_age(session()->user());
      break;
    case '7':
      input_comptype();
      break;
    case '8':
      input_screensize();
      break;
    case '9':
      input_pw(session()->user());
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
  sysoplogf("-> %s #%ld (%s)", session()->user()->GetName(), session()->usernum,
            session()->user()->GetRealName());
  if (syscfg.sysconfig & sysconfig_extended_info) {
    sysoplogf("-> %s", session()->user()->GetStreet());
    sysoplogf("-> %s, %s %s  (%s)", session()->user()->GetCity(),
              session()->user()->GetState(), session()->user()->GetZipcode(),
              session()->user()->GetCountry());
  }
  sysoplogf("-> %s (Voice)", session()->user()->GetVoicePhoneNumber());
  if (syscfg.sysconfig & sysconfig_extended_info) {
    sysoplogf("-> %s (Data)", session()->user()->GetDataPhoneNumber());
  }
  sysoplogf("-> %02d/%02d/%02d (%d yr old %s)",
            session()->user()->GetBirthdayMonth(), session()->user()->GetBirthdayDay(),
            session()->user()->GetBirthdayYear(), session()->user()->GetAge(),
            ((session()->user()->GetGender() == 'M') ? "Male" : "Female"));
  sysoplogf("-> Using a %s Computer", ctypes(session()->user()->GetComputerType()).c_str());
  if (session()->user()->GetWWIVRegNumber()) {
    sysoplogf("-> WWIV Registration # %ld", session()->user()->GetWWIVRegNumber());
  }
  sysoplog("********");


  if (session()->user()->GetVoicePhoneNumber()[0]) {
    add_phone_number(session()->usernum, session()->user()->GetVoicePhoneNumber());
  }
  if (session()->user()->GetDataPhoneNumber()[0]) {
    add_phone_number(session()->usernum, session()->user()->GetDataPhoneNumber());
  }
}


void VerifyNewUserPassword() {
  bool ok = false;
  do {
    bout.nl(2);
    bout << "|#9Your User Number: |#2" << session()->usernum << wwiv::endl;
    bout << "|#9Your Password:    |#2" << session()->user()->GetPassword() << wwiv::endl;
    bout.nl(1);
    bout << "|#9Please write down this information, and enter your password for verification.\r\n";
    bout << "|#9You will need to know this password in order to change it to something else.\r\n\n";
    string password = input_password("|#9PW: ", 8);
    if (password == session()->user()->GetPassword()) {
      ok = true;
    }
  } while (!ok && !hangup);
}


void SendNewUserFeedbackIfRequired() {
  if (!incom) {
    return;
  }

  if (syscfg.sysconfig & sysconfig_no_newuser_feedback) {
    // If NEWUSER_FEEDBACK=N don't attempt to send newuser feedback.
    return;
  }

  if (application()->HasConfigFlag(OP_FLAGS_FORCE_NEWUSER_FEEDBACK)) {
    noabort(FEEDBACK_NOEXT);
  } else if (printfile(FEEDBACK_NOEXT)) {
    sysoplog("", false);
  }
  feedback(true);
  if (application()->HasConfigFlag(OP_FLAGS_FORCE_NEWUSER_FEEDBACK)) {
    if (!session()->user()->GetNumEmailSent() && !session()->user()->GetNumFeedbackSent()) {
      printfile(NOFBACK_NOEXT);
      deluser(session()->usernum);
      hangup = true;
      hang_it_up();
      return;
    }
  }
}


void ExecNewUserCommand() {
  if (!hangup && !syscfg.newuser_cmd.empty()) {
    const string commandLine = stuff_in(syscfg.newuser_cmd, create_chain_file(), "", "", "", "");

    // Log what is happening here.
    sysoplog("Executing New User Event: ", false);
    sysoplog(commandLine.c_str(), true);

    session()->WriteCurrentUser();
    ExecuteExternalProgram(commandLine, application()->GetSpawnOptions(SPAWNOPT_NEWUSER));
    session()->ReadCurrentUser();
  }
}


void newuser() {
  get_colordata();
  session()->screenlinest = 25;

  CreateNewUserRecord();

  input_language();

  sysoplog("", false);
  sysoplogfi(false, "*** NEW USER %s   %s    %s (%ld)", fulldate(), times(), session()->GetCurrentSpeed().c_str(),
             application()->GetInstanceNumber());

  if (!CanCreateNewUserAccountHere() || hangup) {
    hangup = true;
    return;
  }

  if (check_ansi()) {
    session()->user()->SetStatusFlag(WUser::ansi);
    session()->user()->SetStatusFlag(WUser::color);
    session()->user()->SetStatusFlag(WUser::extraColor);
  }
  printfile(SYSTEM_NOEXT);
  bout.nl();
  pausescr();
  printfile(NEWUSER_NOEXT);
  bout.nl();
  pausescr();
  bout.cls();
  bout << "|#5Create a new user account on " << syscfg.systemname << "? ";
  if (!noyes()) {
    bout << "|#6Sorry the system does not meet your needs!\r\n";
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


  bout.nl(4);
  bout << "Random password: " << session()->user()->GetPassword() << wwiv::endl << wwiv::endl;
  bout << "|#5Do you want a different password (Y/N)? ";
  if (yesno()) {
    input_pw(session()->user());
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

  bout.nl();
  bout << "Please wait...\r\n\n";
  session()->usernum = find_new_usernum(session()->user(), qsc);
  if (session()->usernum <= 0) {
    bout.nl();
    bout << "|#6Error creating user account.\r\n\n";
    hangup = true;
    return;
  }

  WriteNewUserInfoToSysopLog();
  ssm(1, 0, "You have a new user:  %s #%ld", session()->user()->GetName(), session()->usernum);
  application()->UpdateTopScreen();
  VerifyNewUserPassword();
  SendNewUserFeedbackIfRequired();
  ExecNewUserCommand();
  session()->ResetEffectiveSl();
  changedsl();
  new_mail();
}

/**
 * Takes an input string and reduces repeated spaces in the string to one space.
 */
void single_space(char *pszText) {
  if (!pszText || !*pszText) {
    return;
  }
  char *pInputBuffer = pszText;
  char *pOutputBuffer = pszText;
  int i = 0;
  int cnt = 0;

  while (*pInputBuffer) {
    if (isspace(*pInputBuffer) && cnt) {
      pInputBuffer++;
    } else {
      if (!isspace(*pInputBuffer)) {
        cnt = 0;
      } else {
        *pInputBuffer = ' ';
        cnt = 1;
      }
      pOutputBuffer[i++] = *pInputBuffer++;
    }
  }
  pOutputBuffer[i] = '\0';
}

bool check_zip(const char *pszZipCode, int mode) {
  char city[81], state[21];

  if (pszZipCode[0] == '\0') {
    return false;
  }

  bool ok = true;
  bool found = false;

  char szFileName[ MAX_PATH ];
  sprintf(szFileName, "%s%s%czip%c.dat", syscfg.datadir, ZIPCITY_DIR, File::pathSeparatorChar, pszZipCode[0]);

  TextFile zip_file(szFileName, "r");
  if (!zip_file.IsOpen()) {
    ok = false;
    if (mode != 2) {
      bout << "\r\n|#6" << szFileName << " not found\r\n";
    }
  } else {
    char zip_buf[81];
    while ((zip_file.ReadLine(zip_buf, 80)) && !found && !hangup) {
      single_space(zip_buf);
      if (strncmp(zip_buf, pszZipCode, 5) == 0) {
        found = true;
        char* ss = strtok(zip_buf, " ");
        ss = strtok(nullptr, " ");
        StringTrim(ss);
        strcpy(state, ss);
        ss = strtok(nullptr, "\r\n");
        StringTrim(ss);
        strncpy(city, ss, 30);
        city[31] = '\0';
        properize(city);
      }
    }
    zip_file.Close();
  }

  if (mode != 2 && !found) {
    bout.nl();
    bout << "|#6No match for " << pszZipCode << ".";
    ok = false;
  }

  if (!mode && found) {
    bout << "\r\n|#2" << pszZipCode << " is in " <<
                       city << ", " << state << ".";
    ok = false;
  }

  if (found) {
    if (mode != 2) {
      bout << "\r\n|#2" << city << ", " << state << "  USA? (y/N): ";
      if (yesno()) {
        session()->user()->SetCity(city);
        session()->user()->SetState(state);
        session()->user()->SetCountry("USA");
      } else {
        bout.nl();
        bout << "|#6My records show differently.\r\n\n";
        ok = false;
      }
    } else {
      session()->user()->SetCity(city);
      session()->user()->SetState(state);
      session()->user()->SetCountry("USA");
    }
  }
  return ok;
}


bool check_dupes(const char *pszPhoneNumber) {
  int nUserNumber = find_phone_number(pszPhoneNumber);
  if (nUserNumber && nUserNumber != session()->usernum) {
    char szBuffer[ 255 ];
    sprintf(szBuffer, "    %s entered phone # %s", session()->user()->GetName(), pszPhoneNumber);
    sysoplog(szBuffer, false);
    ssm(1, 0, szBuffer);

    WUser user;
    application()->users()->ReadUser(&user, nUserNumber);
    sprintf(szBuffer, "      also entered by %s", user.GetName());
    sysoplog(szBuffer, false);
    ssm(1, 0, szBuffer);

    return true;
  }

  return false;
}

void noabort(const char *pszFileName) {
  bool oic = false;

  if (session()->using_modem) {
    oic   = incom;
    incom = false;
    dump();
  }
  printfile(pszFileName);
  if (session()->using_modem) {
    dump();
    incom = oic;
  }
}

static void cln_nu() {
  bout.Color(0);
  int i1 = session()->localIO()->WhereX();
  if (i1 > 28) {
    for (int i = i1; i > 28; i--) {
      bout.bs();
    }
  }
  bout.clreol();
}


void DoMinimalNewUser() {
  int m =  1, d =  1, y = 2000, ch =  0;
  char s[101], s1[81], m1[3], d1[3], y1[5];
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
  int nSaveTopData = session()->topdata;
  session()->topdata = LocalIO::topdataNone;
  application()->UpdateTopScreen();
  do {
    bout.cls();
    bout.litebar("%s New User Registration", syscfg.systemname);
    bout << "|#1[A] Name (real or alias)    : ";
    if (session()->user()->GetName()[0] == '\0') {
      bool ok = true;
      char szTempName[ 81 ];
      do {
        Input1(szTempName, s1, 30, true, InputMode::UPPER);
        ok = check_name(szTempName);
        if (!ok) {
          cln_nu();
          BackPrint("I'm sorry, you can't use that name.", 6, 20, 1000);
        }
      } while (!ok && !hangup);
      session()->user()->SetName(szTempName);
    }
    s1[0] = '\0';
    cln_nu();
    bout << "|#2" << session()->user()->GetName();
    bout.nl();
    bout << "|#1[B] Birth Date (MM/DD/YYYY) : ";
    if (session()->user()->GetAge() == 0) {
      bool ok = false;
      do {
        ok = false;
        cln_nu();
        Input1(s, s1, 10, false, InputMode::DATE);
        if (strlen(s) == 10) {
          sprintf(m1, "%c%c", s[0], s[1]);
          sprintf(d1, "%c%c", s[3], s[4]);
          sprintf(y1, "%c%c%c%c", s[6], s[7], s[8], s[9]);
          m = atoi(m1);
          d = atoi(d1);
          y = atoi(y1);
          ok = true;
          if ((((m == 2) || (m == 9) || (m == 4) || (m == 6) || (m == 11)) && (d >= 31)) ||
              ((m == 2) && (((!isleap(y)) && (d == 29)) || (d == 30))) ||
              (years_old(m, d, y) < 5) ||
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
    session()->user()->SetBirthdayMonth(m);
    session()->user()->SetBirthdayDay(d);
    session()->user()->SetBirthdayYear(y);
    session()->user()->SetAge(years_old(m, d, y));
    s1[0] = '\0';
    cln_nu();
    bout << "|#2" << mon[ std::max<int>(0, session()->user()->GetBirthdayMonth() - 1) ] << " "
         << session()->user()->GetBirthdayDay() << ", "
         << session()->user()->GetBirthdayYear()
         << " (" << session()->user()->GetAge() << " years old)\r\n"
         << "|#1[C] Sex (Gender)            : ";
    if (session()->user()->GetGender() != 'M' && session()->user()->GetGender()  != 'F') {
      bout.mpl(1);
      session()->user()->SetGender(onek_ncr("MF"));
    }
    s1[0] = '\0';
    cln_nu();
    bout << "|#2" << (session()->user()->GetGender() == 'M' ? "Male" : "Female") << wwiv::endl;
    bout <<  "|#1[D] Country                 : " ;
    if (session()->user()->GetCountry()[0] == '\0') {
      Input1(reinterpret_cast<char*>(session()->user()->data.country), "", 3, false, InputMode::UPPER);
      if (session()->user()->GetCountry()[0] == '\0') {
        session()->user()->SetCountry("USA");
      }
    }
    s1[0] = '\0';
    cln_nu();
    bout << "|#2" << session()->user()->GetCountry() << wwiv::endl;
    bout << "|#1[E] ZIP or Postal Code      : ";
    if (session()->user()->GetZipcode()[0] == 0) {
      bool ok = false;
      do {
        if (IsEquals(session()->user()->GetCountry(), "USA")) {
          Input1(reinterpret_cast<char*>(session()->user()->data.zipcode), s1, 5, true, InputMode::UPPER);
          check_zip(session()->user()->GetZipcode(), 2);
        } else {
          Input1(reinterpret_cast<char*>(session()->user()->data.zipcode), s1, 7, true, InputMode::UPPER);
        }
        if (session()->user()->GetZipcode()[0]) {
          ok = true;
        }
      } while (!ok && !hangup);
    }
    s1[0] = '\0';
    cln_nu();
    bout << "|#2" << session()->user()->GetZipcode() << wwiv::endl;
    bout << "|#1[F] City/State/Province     : ";
    if (session()->user()->GetCity()[0] == 0) {
      bool ok = false;
      do {
        Input1(reinterpret_cast<char*>(session()->user()->data.city), s1, 30, true, InputMode::PROPER);
        if (session()->user()->GetCity()[0]) {
          ok = true;
        }
      } while (!ok && !hangup);
      bout << ", ";
      if (session()->user()->GetState()[0] == 0) {
        do {
          ok = false;
          Input1(reinterpret_cast<char*>(session()->user()->data.state), s1, 2, true, InputMode::UPPER);
          if (session()->user()->GetState()[0]) {
            ok = true;
          }
        } while (!ok && !hangup);
      }
    }
    s1[0] = '\0';
    cln_nu();
    properize(reinterpret_cast<char*>(session()->user()->data.city));
    bout << "|#2" << session()->user()->GetCity() << ", " <<
                       session()->user()->GetState() << wwiv::endl;
    bout << "|#1[G] Internet Mail Address   : ";
    if (session()->user()->GetEmailAddress()[0] == 0) {
      string emailAddress = Input1(s1, 44, true, InputMode::MIXED);
      session()->user()->SetEmailAddress(emailAddress.c_str());
      if (!check_inet_addr(session()->user()->GetEmailAddress())) {
        cln_nu();
        BackPrint("Invalid address!", 6, 20, 1000);
      }
      if (session()->user()->GetEmailAddress()[0] == 0) {
        session()->user()->SetEmailAddress("None");
      }
    }
    s1[0] = '\0';
    cln_nu();
    bout << "|#2" << session()->user()->GetEmailAddress() << wwiv::endl;
    bout.nl();
    bout << "|#5Item to change or [Q] to Quit : |#0";
    ch = onek("QABCDEFG");
    switch (ch) {
    case 'Q':
      done = true;
      break;
    case 'A':
      strcpy(s1, session()->user()->GetName());
      session()->user()->SetName("");
      break;
    case 'B':
      session()->user()->SetAge(0);
      sprintf(s1, "%02d/%02d/%02d", session()->user()->GetBirthdayDay(),
              session()->user()->GetBirthdayMonth(), session()->user()->GetBirthdayYear());
      break;
    case 'C':
      session()->user()->SetGender('N');
      break;
    case 'D':
      session()->user()->SetCountry("");
    case 'E':
      session()->user()->SetZipcode("");
    case 'F':
      strcpy(s1, session()->user()->GetCity());
      session()->user()->SetCity("");
      session()->user()->SetState("");
      break;
    case 'G':
      strcpy(s1, session()->user()->GetEmailAddress());
      session()->user()->SetEmailAddress("");
      break;
    }
  } while (!done && !hangup);
  session()->user()->SetRealName(session()->user()->GetName());
  session()->user()->SetVoicePhoneNumber("999-999-9999");
  session()->user()->SetDataPhoneNumber(session()->user()->GetVoicePhoneNumber());
  session()->user()->SetStreet("None Requested");
  if (session()->GetNumberOfEditors() && session()->user()->HasAnsi()) {
    session()->user()->SetDefaultEditor(1);
  }
  session()->topdata = nSaveTopData;
  application()->UpdateTopScreen();
  newline = true;
}


void new_mail() {
  File file(syscfg.gfilesdir, (session()->user()->GetSl() > syscfg.newusersl) ? NEWSYSOP_MSG : NEWMAIL_MSG);
  if (!file.Exists()) {
    return;
  }
  session()->SetNewMailWaiting(true);
  int save_ed = session()->user()->GetDefaultEditor();
  session()->user()->SetDefaultEditor(0);
  LoadFileIntoWorkspace(file.full_pathname().c_str(), true);
  use_workspace = true;
  string userName = session()->user()->GetUserNameAndNumber(session()->usernum);
  string title = StringPrintf("Welcome to %s!", syscfg.systemname);

  int nAllowAnon = 0;
  messagerec msg;
  msg.storage_type = 2;
  inmsg(&msg, &title, &nAllowAnon, false, "email", INMSG_NOFSED, userName.c_str(), MSGED_FLAG_NONE, true);
  sendout_email(title, &msg, 0, session()->usernum, 0, 1, 1, 0, 1, 0);
  session()->user()->SetNumMailWaiting(session()->user()->GetNumMailWaiting() + 1);
  session()->user()->SetDefaultEditor(save_ed);
  session()->SetNewMailWaiting(false);
}

