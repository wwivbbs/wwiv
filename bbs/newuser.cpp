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
#include "bbs/newuser.h"

#include <algorithm>
#include <chrono>
#include <string>

#include "bbs/asv.h"
#include "bbs/bbs.h"
#include "bbs/bbsovl1.h"
#include "bbs/bbsovl2.h"
#include "bbs/bbsovl3.h"
#include "bbs/bbsutl1.h"
#include "bbs/bbsutl2.h"
#include "bbs/com.h"
#include "bbs/confutil.h"
#include "bbs/defaults.h"
#include "bbs/dupphone.h"
#include "bbs/colors.h"
#include "bbs/defaults.h"
#include "bbs/email.h"
#include "bbs/execexternal.h"
#include "bbs/fcns.h"
#include "bbs/finduser.h"
#include "bbs/datetime.h"
#include "bbs/dropfile.h"
#include "bbs/inmsg.h"
#include "bbs/inetmsg.h"
#include "bbs/input.h"
#include "bbs/lilo.h"
#include "bbs/listplus.h"
#include "bbs/message_file.h"
#include "bbs/mmkey.h"
#include "bbs/printfile.h"
#include "bbs/stuffin.h"
#include "bbs/smallrecord.h"
#include "bbs/sysoplog.h"
#include "bbs/trashcan.h"
#include "bbs/uedit.h"
#include "bbs/vars.h"
#include "bbs/wconstants.h"
#include "bbs/workspace.h"
#include "sdk/status.h"
#include "core/inifile.h"
#include "core/os.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "sdk/filenames.h"

using std::chrono::milliseconds;
using std::string;
using wwiv::bbs::InputMode;
using namespace wwiv::core;
using namespace wwiv::os;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

// Local function prototypes

bool CanCreateNewUserAccountHere();
bool UseMinimalNewUserInfo();
void noabort(const char *file_name);
bool check_dupes(const char *pszPhoneNumber);
void DoMinimalNewUser();
bool check_zip(const char *pszZipCode, int mode);
void DoFullNewUser();
void VerifyNewUserFullInfo();
void VerifyNewUserPassword();
void SendNewUserFeedbackIfRequired();
void ExecNewUserCommand();
void new_mail();
bool CheckPasswordComplexity(User *pUser, string& password);


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
  if (session()->languages.size() > 1) {
    session()->user()->SetLanguage(255);
    do {
      char ch = 0, onx[20];
      bout.nl(2);
      for (size_t i = 0; i < session()->languages.size(); i++) {
        bout << (i + 1) << ". " << session()->languages[i].name << wwiv::endl;
        if (i < 9) {
          onx[i] = static_cast<char>('1' + i);
        }
      }
      bout.nl();
      bout << "|#2Which language? ";
      if (session()->languages.size() < 10) {
        onx[session()->languages.size()] = 0;
        ch = onek(onx);
        ch -= '1';
      } else {
        std::set<char> odc;
        for (size_t i = 1; i <= session()->languages.size() / 10; i++) {
          odc.insert(static_cast<char>('0' + i));
        }
        string ss = mmkey(odc);
        ch = ss.front() - 1;
      }
      if (ch >= 0 && ch < size_int(session()->languages)) {
        session()->user()->SetLanguage(session()->languages[ch].num);
      }
    } while ((session()->user()->GetLanguage() == 255) && (!hangup));

    set_language(session()->user()->GetLanguage());
  }
}

static bool check_name(const string& user_name) {
  if (user_name.length() == 0 ||
      user_name[ user_name.length() - 1 ] == 32   ||
      user_name[0] < 65                 ||
      finduser(user_name) != 0          ||
      user_name.find("@") != string::npos ||
      user_name.find("#") != string::npos) {
    return false;
  }

  Trashcan trashcan(*session()->config());
  if (trashcan.IsTrashName(user_name)) {
    LOG(INFO) << "Trashcan name entered from IP: " << session()->remoteIO()->remote_info().address
              << "; name: " << user_name;
    hang_it_up();
    Hangup();
    return false;
  }
  return true;
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
        hang_it_up();
        Hangup();
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
  string s = input(6, true);
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
    state = input(2, true);

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
    country = input(3, true);
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
    zipcode = input(len, true);

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

void input_age(User *pUser) {
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


bool CheckPasswordComplexity(User *, string& password) {
  if (password.length() < 3) {
    //TODO - the min length should be in wwiv.ini
    return false;
  }
  return true;
}


void input_pw(User *pUser) {
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
    pUser->SetPassword(password);
  } else {
    bout << "Password not changed.\r\n";
  }
}


void input_ansistat() {
  session()->user()->ClearStatusFlag(User::ansi);
  session()->user()->ClearStatusFlag(User::color);
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
    session()->user()->SetStatusFlag(User::ansi);
    bout.nl();
    bout << "|#5Do you want color? ";
    if (noyes()) {
      session()->user()->SetStatusFlag(User::color);
      session()->user()->SetStatusFlag(User::extraColor);
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

static int find_new_usernum(const User* pUser, uint32_t* qscn) {
  File userFile(session()->config()->datadir(), USER_LST);
  for (int i = 0; !userFile.IsOpen() && (i < 20); i++) {
    if (!userFile.Open(File::modeBinary | File::modeReadWrite | File::modeCreateFile)) {
      sleep_for(milliseconds(100));
    }
  }
  if (!userFile.IsOpen()) {
    return -1;
  }

  int nNewUserNumber = static_cast<int>((userFile.GetLength() / syscfg.userreclen) - 1);
  userFile.Seek(syscfg.userreclen, File::Whence::begin);
  int user_number = 1;

  if (nNewUserNumber == session()->status_manager()->GetUserCount()) {
    user_number = nNewUserNumber + 1;
  } else {
    while (user_number <= nNewUserNumber) {
      if (user_number % 25 == 0) {
        userFile.Close();
        for (int n = 0; !userFile.IsOpen() && (n < 20); n++) {
          if (!userFile.Open(File::modeBinary | File::modeReadWrite | File::modeCreateFile)) {
            sleep_for(milliseconds(100));
          }
        }
        if (!userFile.IsOpen()) {
          return -1;
        }
        userFile.Seek(static_cast<long>(user_number * syscfg.userreclen), File::Whence::begin);
        nNewUserNumber = static_cast<int>((userFile.GetLength() / syscfg.userreclen) - 1);
      }
      User tu;
      userFile.Read(&tu.data, syscfg.userreclen);

      if (tu.IsUserDeleted() && tu.GetSl() != 255) {
        userFile.Seek(static_cast<long>(user_number * syscfg.userreclen), File::Whence::begin);
        userFile.Write(&pUser->data, syscfg.userreclen);
        userFile.Close();
        write_qscn(user_number, qscn, false);
        InsertSmallRecord(user_number, pUser->GetName());
        return user_number;
      } else {
        user_number++;
      }
    }
  }

  if (user_number <= syscfg.maxusers) {
    userFile.Seek(static_cast<long>(user_number * syscfg.userreclen), File::Whence::begin);
    userFile.Write(&pUser->data, syscfg.userreclen);
    userFile.Close();
    write_qscn(user_number, qscn, false);
    InsertSmallRecord(user_number, pUser->GetName());
    return user_number;
  } else {
    userFile.Close();
    return -1;
  }
}

// Clears session()->user()'s data and makes it ready to be a new user, also
// clears the QScan pointers
static bool CreateNewUserRecord() {
  memset(qsc, 0, syscfg.qscn_len);
  *qsc = 999;
  memset(qsc_n, 0xff, ((syscfg.max_dirs + 31) / 32) * 4);
  memset(qsc_q, 0xff, ((syscfg.max_subs + 31) / 32) * 4);

  auto u = session()->user();
  session()->ResetEffectiveSl();
  
  return User::CreateNewUserRecord(u, syscfg.newusersl, syscfg.newuserdsl, 
    syscfg.newuser_restrict, syscfg.newusergold,
    session()->newuser_colors, session()->newuser_bwcolors);
}


// returns true if the user is allow to create a new user account
// on here, if this function returns false, a sufficient error
// message has already been displayed to the user.
bool CanCreateNewUserAccountHere() {
  if (session()->status_manager()->GetUserCount() >= syscfg.maxusers) {
    bout.nl(2);
    bout << "I'm sorry, but the system currently has the maximum number of users it can\r\nhandle.\r\n\n";
    return false;
  }

  if (syscfg.closedsystem) {
    bout.nl(2);
    bout << "I'm sorry, but the system is currently closed, and not accepting new users.\r\n\n";
    return false;
  }

  if ((syscfg.newuserpw[0] != 0) && incom) {
    bout.nl(2);
    bool ok = false;
    int nPasswordAttempt = 0;
    do {
      bout << "New User Password :";
      string password = input(20);
      if (password == syscfg.newuserpw) {
        ok = true;
      } else {
        sysoplog() << "Wrong newuser password: " << password;
      }
    } while (!ok && !hangup && (nPasswordAttempt++ < 4));
    if (!ok) {
      return false;
    }
  }
  return true;
}


bool UseMinimalNewUserInfo() {
  IniFile ini(FilePath(session()->GetHomeDir(), WWIV_INI), {StrCat("WWIV-", session()->instance_number()), INI_TAG});
  if (ini.IsOpen()) {
    return ini.value<bool>("NEWUSER_MIN");
  }
  return false;
}

static void DefaultToWWIVEditIfPossible() {
  for (size_t nEditor = 0; nEditor < session()->editors.size(); nEditor++) {
    // TODO(rushfan): Should we get rid of this favoring of WWIVEDIT?
    string editor_desc(session()->editors[nEditor].description);
    StringUpperCase(&editor_desc);
    if (editor_desc.find("WWIVEDIT") != string::npos) {
      session()->user()->SetDefaultEditor(nEditor + 1);
      return;
    }
  }
}
void DoFullNewUser() {
  const auto u = session()->user();

  input_name();
  input_realname();
  input_phone();
  if (session()->HasConfigFlag(OP_FLAGS_CHECK_DUPE_PHONENUM)) {
    if (check_dupes(u->GetVoicePhoneNumber())) {
      if (session()->HasConfigFlag(OP_FLAGS_HANGUP_DUPE_PHONENUM)) {
        hang_it_up();
        return;
      }
    }
  }
  if (syscfg.sysconfig & sysconfig_extended_info) {
    input_street();
    char szZipFileName[MAX_PATH];
    sprintf(szZipFileName, "%s%s%czip1.dat", syscfg.datadir, ZIPCITY_DIR, File::pathSeparatorChar);
    if (File::Exists(szZipFileName)) {
      input_zipcode();
      if (!check_zip(u->GetZipcode(), 1)) {
        u->SetCity("");
        u->SetState("");
      }
      u->SetCountry("USA");
    }
    if (u->GetCity()[0] == '\0') {
      input_city();
    }
    if (u->GetState()[0] == '\0') {
      input_state();
    }
    if (u->GetZipcode()[0] == '\0') {
      input_zipcode();
    }
    if (u->GetCountry()[0] == '\0') {
      input_country();
    }
    input_dataphone();

    if (session()->HasConfigFlag(OP_FLAGS_CHECK_DUPE_PHONENUM)) {
      if (check_dupes(u->GetDataPhoneNumber())) {
        if (session()->HasConfigFlag(OP_FLAGS_HANGUP_DUPE_PHONENUM)) {
          Hangup();
          return;
        }
      }
    }
  }
  input_callsign();
  input_sex();
  input_age(u);
  input_comptype();

  if (session()->editors.size() && u->HasAnsi()) {
    bout.nl();
    bout << "|#5Select a fullscreen editor? ";
    if (yesno()) {
      select_editor();
    } else {
      DefaultToWWIVEditIfPossible();
    }
    bout.nl();
  }
  bout << "|#5Select a default transfer protocol? ";
  if (yesno()) {
    bout.nl();
    bout << "Enter your default protocol, or 0 for none.\r\n\n";
    int nDefProtocol = get_protocol(xf_down);
    if (nDefProtocol) {
      u->SetDefaultProtocol(nDefProtocol);
    }
  }
}

void DoNewUserASV() {
  if (session()->HasConfigFlag(OP_FLAGS_SIMPLE_ASV) &&
      session()->asv.sl > syscfg.newusersl && session()->asv.sl < 90) {
    bout.nl();
    bout << "|#5Are you currently a WWIV SysOp? ";
    if (yesno()) {
      bout.nl();
      bout << "|#5Please enter your BBS name and number.\r\n";
      string note = inputl(60, true);
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
  const auto u = session()->user();

  do {
    bout.nl(2);
    bout << "|#91) Name          : |#2" << u->GetName() << wwiv::endl;
    if (!(syscfg.sysconfig & sysconfig_no_alias)) {
      bout << "|#92) Real Name     : |#2" << u->GetRealName() << wwiv::endl;
    }
    bout << "|#93) Callsign      : |#2" << u->GetCallsign() << wwiv::endl;
    bout << "|#94) Phone No.     : |#2" << u->GetVoicePhoneNumber() <<
                       wwiv::endl;
    bout << "|#95) Gender        : |#2" << u->GetGender() << wwiv::endl;
    bout << "|#96) Birthdate     : |#2" <<
                       static_cast<int>(u->GetBirthdayMonth()) << "/" <<
                       static_cast<int>(u->GetBirthdayDay()) << "/" <<
                       static_cast<int>(u->GetBirthdayYear()) << wwiv::endl;
    bout << "|#97) Computer type : |#2" << ctypes(u->GetComputerType()) <<
                       wwiv::endl;
    bout << "|#98) Screen size   : |#2" <<
                       u->GetScreenChars() << " X " <<
                       u->GetScreenLines() << wwiv::endl;
    bout << "|#99) Password      : |#2" << u->GetPassword() << wwiv::endl;
    if (syscfg.sysconfig & sysconfig_extended_info) {
      bout << "|#9A) Street Address: |#2" << u->GetStreet() << wwiv::endl;
      bout << "|#9B) City          : |#2" << u->GetCity() << wwiv::endl;
      bout << "|#9C) State         : |#2" << u->GetState() << wwiv::endl;
      bout << "|#9D) Country       : |#2" << u->GetCountry() << wwiv::endl;
      bout << "|#9E) Zipcode       : |#2" << u->GetZipcode() << wwiv::endl;
      bout << "|#9F) Dataphone     : |#2" << u->GetDataPhoneNumber() << wwiv::endl;
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
      input_age(u);
      break;
    case '7':
      input_comptype();
      break;
    case '8':
      input_screensize();
      break;
    case '9':
      input_pw(u);
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
  const auto u = session()->user();
  sysoplog() << "** New User Information **";
  sysoplog() << StringPrintf("-> %s #%ld (%s)", u->GetName(), session()->usernum,
            u->GetRealName());
  if (syscfg.sysconfig & sysconfig_extended_info) {
    sysoplog() << "-> " << u->GetStreet();
    sysoplog() << "-> " << u->GetCity() << ", " << u->GetState() << " " << u->GetZipcode()
      << "  (" << u->GetCountry() << " )";
              
  }
  sysoplog() << StringPrintf("-> %s (Voice)", u->GetVoicePhoneNumber());
  if (syscfg.sysconfig & sysconfig_extended_info) {
    sysoplog() << StringPrintf("-> %s (Data)", u->GetDataPhoneNumber());
  }
  sysoplog() << StringPrintf("-> %02d/%02d/%02d (%d yr old %s)",
            u->GetBirthdayMonth(), u->GetBirthdayDay(),
            u->GetBirthdayYear(), u->GetAge(),
            ((u->GetGender() == 'M') ? "Male" : "Female"));
  sysoplog() << StringPrintf("-> Using a %s Computer", ctypes(u->GetComputerType()).c_str());
  if (u->GetWWIVRegNumber()) {
    sysoplog() << StringPrintf("-> WWIV Registration # %ld", u->GetWWIVRegNumber());
  }
  sysoplog() << "********";


  if (u->GetVoicePhoneNumber()[0]) {
    add_phone_number(session()->usernum, u->GetVoicePhoneNumber());
  }
  if (u->GetDataPhoneNumber()[0]) {
    add_phone_number(session()->usernum, u->GetDataPhoneNumber());
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

  if (session()->HasConfigFlag(OP_FLAGS_FORCE_NEWUSER_FEEDBACK)) {
    noabort(FEEDBACK_NOEXT);
  } else if (printfile(FEEDBACK_NOEXT)) {
    sysoplog(false) << "";
  }
  feedback(true);
  if (session()->HasConfigFlag(OP_FLAGS_FORCE_NEWUSER_FEEDBACK)) {
    if (!session()->user()->GetNumEmailSent() && !session()->user()->GetNumFeedbackSent()) {
      printfile(NOFBACK_NOEXT);
      deluser(session()->usernum);
      Hangup();
      return;
    }
  }
}


void ExecNewUserCommand() {
  if (!hangup && !syscfg.newuser_cmd.empty()) {
    const string commandLine = stuff_in(syscfg.newuser_cmd, create_chain_file(), "", "", "", "");

    // Log what is happening here.
    sysoplog(false) << "Executing New User Event: ";
    sysoplog() << commandLine;

    session()->WriteCurrentUser();
    ExecuteExternalProgram(commandLine, session()->GetSpawnOptions(SPAWNOPT_NEWUSER));
    session()->ReadCurrentUser();
  }
}

void newuser() {
  sysoplog(false);
  sysoplog(false) << StringPrintf("*** NEW USER %s   %s    %s (%ld)", fulldate(), times(), session()->GetCurrentSpeed().c_str(),
             session()->instance_number());

  LOG(INFO) << "New User Attempt from IP Address: " << session()->remoteIO()->remote_info().address;

  get_colordata();
  session()->screenlinest = 25;

  if (!CreateNewUserRecord()) {
    return;
  }

  input_language();


  if (!CanCreateNewUserAccountHere() || hangup) {
    Hangup();
    return;
  }

  if (check_ansi()) {
    session()->user()->SetStatusFlag(User::ansi);
    session()->user()->SetStatusFlag(User::color);
    session()->user()->SetStatusFlag(User::extraColor);
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
    Hangup();
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
    Hangup();
    return;
  } else if (session()->usernum == 1) {
    // This is the #1 sysop record. Tell the sysop thank you and
    // update his user record with: s255/d255/r0
    ssm(1, 0) << "Thank you for installing WWIV! - The WWIV Development Team.";
    User* user = session()->user();
    user->SetSl(255);
    user->SetDsl(255);
    user->SetRestriction(0);
  }

  WriteNewUserInfoToSysopLog();
  ssm(1, 0) << "You have a new user: " << session()->user()->GetName() << " #" << session()->usernum;

  LOG(INFO) << "New User Created: '" << session()->user()->GetName() << "' "
            << "IP Address: " << session()->remoteIO()->remote_info().address;

  session()->UpdateTopScreen();
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
void single_space(char *text) {
  if (!text || !*text) {
    return;
  }
  char *pInputBuffer = text;
  char *pOutputBuffer = text;
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

  char szFileName[MAX_PATH];
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
  int user_number = find_phone_number(pszPhoneNumber);
  if (user_number && user_number != session()->usernum) {
    string s = StringPrintf("    %s entered phone # %s", session()->user()->GetName(), pszPhoneNumber);
    sysoplog(false) << s;
    ssm(1, 0) << s;

    User user;
    session()->users()->ReadUser(&user, user_number);
    s = StringPrintf("      also entered by %s", user.GetName());
    sysoplog(false) << s;
    ssm(1, 0) << s;

    return true;
  }

  return false;
}

void noabort(const char *file_name) {
  bool oic = false;

  if (session()->using_modem) {
    oic   = incom;
    incom = false;
    bout.dump();
  }
  printfile(file_name);
  if (session()->using_modem) {
    bout.dump();
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
  const auto u = session()->user();

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
  session()->UpdateTopScreen();
  do {
    bout.cls();
    bout.litebar("%s New User Registration", syscfg.systemname);
    bout << "|#1[A] Name (real or alias)    : ";
    if (u->GetName()[0] == '\0') {
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
      u->SetName(szTempName);
    }
    s1[0] = '\0';
    cln_nu();
    bout << "|#2" << u->GetName();
    bout.nl();
    bout << "|#1[B] Birth Date (MM/DD/YYYY) : ";
    if (u->GetAge() == 0) {
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
    u->SetBirthdayMonth(m);
    u->SetBirthdayDay(d);
    u->SetBirthdayYear(y);
    u->SetAge(years_old(m, d, y));
    s1[0] = '\0';
    cln_nu();
    bout << "|#2" << mon[ std::max<int>(0, u->GetBirthdayMonth() - 1) ] << " "
         << u->GetBirthdayDay() << ", "
         << u->GetBirthdayYear()
         << " (" << u->GetAge() << " years old)\r\n"
         << "|#1[C] Sex (Gender)            : ";
    if (u->GetGender() != 'M' && u->GetGender()  != 'F') {
      bout.mpl(1);
      u->SetGender(onek_ncr("MF"));
    }
    s1[0] = '\0';
    cln_nu();
    bout << "|#2" << (u->GetGender() == 'M' ? "Male" : "Female") << wwiv::endl;
    bout <<  "|#1[D] Country                 : " ;
    if (u->GetCountry()[0] == '\0') {
      Input1(reinterpret_cast<char*>(u->data.country), "", 3, false, InputMode::UPPER);
      if (u->GetCountry()[0] == '\0') {
        u->SetCountry("USA");
      }
    }
    s1[0] = '\0';
    cln_nu();
    bout << "|#2" << u->GetCountry() << wwiv::endl;
    bout << "|#1[E] ZIP or Postal Code      : ";
    if (u->GetZipcode()[0] == 0) {
      bool ok = false;
      do {
        if (IsEquals(u->GetCountry(), "USA")) {
          Input1(reinterpret_cast<char*>(u->data.zipcode), s1, 5, true, InputMode::UPPER);
          check_zip(u->GetZipcode(), 2);
        } else {
          Input1(reinterpret_cast<char*>(u->data.zipcode), s1, 7, true, InputMode::UPPER);
        }
        if (u->GetZipcode()[0]) {
          ok = true;
        }
      } while (!ok && !hangup);
    }
    s1[0] = '\0';
    cln_nu();
    bout << "|#2" << u->GetZipcode() << wwiv::endl;
    bout << "|#1[F] City/State/Province     : ";
    if (u->GetCity()[0] == 0) {
      bool ok = false;
      do {
        Input1(reinterpret_cast<char*>(u->data.city), s1, 30, true, InputMode::PROPER);
        if (u->GetCity()[0]) {
          ok = true;
        }
      } while (!ok && !hangup);
      bout << ", ";
      if (u->GetState()[0] == 0) {
        do {
          ok = false;
          Input1(reinterpret_cast<char*>(u->data.state), s1, 2, true, InputMode::UPPER);
          if (u->GetState()[0]) {
            ok = true;
          }
        } while (!ok && !hangup);
      }
    }
    s1[0] = '\0';
    cln_nu();
    properize(reinterpret_cast<char*>(u->data.city));
    bout << "|#2" << u->GetCity() << ", " <<
                       u->GetState() << wwiv::endl;
    bout << "|#1[G] Internet Mail Address   : ";
    if (u->GetEmailAddress()[0] == 0) {
      string emailAddress = Input1(s1, 44, true, InputMode::MIXED);
      u->SetEmailAddress(emailAddress.c_str());
      if (!check_inet_addr(u->GetEmailAddress())) {
        cln_nu();
        BackPrint("Invalid address!", 6, 20, 1000);
      }
      if (u->GetEmailAddress()[0] == 0) {
        u->SetEmailAddress("None");
      }
    }
    s1[0] = '\0';
    cln_nu();
    bout << "|#2" << u->GetEmailAddress() << wwiv::endl;
    bout.nl();
    bout << "|#5Item to change or [Q] to Quit : |#0";
    ch = onek("QABCDEFG");
    switch (ch) {
    case 'Q':
      done = true;
      break;
    case 'A':
      strcpy(s1, u->GetName());
      u->SetName("");
      break;
    case 'B':
      u->SetAge(0);
      sprintf(s1, "%02d/%02d/%02d", u->GetBirthdayDay(),
              u->GetBirthdayMonth(), u->GetBirthdayYear());
      break;
    case 'C':
      u->SetGender('N');
      break;
    case 'D':
      u->SetCountry("");
    case 'E':
      u->SetZipcode("");
    case 'F':
      strcpy(s1, u->GetCity());
      u->SetCity("");
      u->SetState("");
      break;
    case 'G':
      strcpy(s1, u->GetEmailAddress());
      u->SetEmailAddress("");
      break;
    }
  } while (!done && !hangup);
  u->SetRealName(u->GetName());
  u->SetVoicePhoneNumber("999-999-9999");
  u->SetDataPhoneNumber(u->GetVoicePhoneNumber());
  u->SetStreet("None Requested");
  if (session()->editors.size() && u->HasAnsi()) {
    u->SetDefaultEditor(1);
  }
  session()->topdata = nSaveTopData;
  session()->UpdateTopScreen();
  newline = true;
}


void new_mail() {
  File file(session()->config()->gfilesdir(), (session()->user()->GetSl() > syscfg.newusersl) ? NEWSYSOP_MSG : NEWMAIL_MSG);
  if (!file.Exists()) {
    return;
  }
  int save_ed = session()->user()->GetDefaultEditor();
  session()->user()->SetDefaultEditor(0);
  LoadFileIntoWorkspace(file.full_pathname(), true, true);
  use_workspace = true;

  MessageEditorData data;
  data.title = StringPrintf("Welcome to %s!", syscfg.systemname);
  data.need_title = true;
  data.anonymous_flag = 0;
  data.aux = "email";
  data.fsed_flags = FsedFlags::NOFSED;
  data.to_name = session()->names()->UserName(session()->usernum);
  data.msged_flags = MSGED_FLAG_NONE;
  data.silent_mode = true;
  messagerec msg;
  msg.storage_type = 2;
  if (inmsg(data)) {
    savefile(data.text, &msg, data.aux);

    EmailData email(data);
    email.msg = &msg;
    email.anony = 0;
    email.user_number = session()->usernum;
    email.system_number = 0;
    email.an = true;
    email.from_user = 1;
    email.from_system = 0;
    email.forwarded_code = 1;
    email.from_network_number = 0;
    email.silent_mode = true;
    sendout_email(email);
  }
  session()->user()->SetNumMailWaiting(session()->user()->GetNumMailWaiting() + 1);
  session()->user()->SetDefaultEditor(save_ed);
}

