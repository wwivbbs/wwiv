/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2019, WWIV Software Services             */
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
#include "bbs/bbsutl.h"
#include "bbs/bbsutl1.h"
#include "bbs/bbsutl2.h"
#include "bbs/com.h"
#include "bbs/confutil.h"
#include "bbs/datetime.h"
#include "bbs/defaults.h"
#include "bbs/dropfile.h"
#include "bbs/email.h"
#include "bbs/execexternal.h"
#include "bbs/finduser.h"
#include "bbs/inetmsg.h"
#include "bbs/inmsg.h"
#include "bbs/input.h"
#include "bbs/lilo.h"
#include "bbs/listplus.h"
#include "bbs/message_file.h"
#include "bbs/mmkey.h"
#include "bbs/pause.h"
#include "bbs/printfile.h"
#include "bbs/shortmsg.h"
#include "bbs/sr.h"
#include "bbs/stuffin.h"
#include "bbs/sysoplog.h"
#include "bbs/trashcan.h"
#include "bbs/uedit.h"

#include "bbs/workspace.h"
#include "bbs/wqscn.h"
#include "core/inifile.h"
#include "core/os.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "local_io/wconstants.h"
#include "sdk/filenames.h"
#include "sdk/phone_numbers.h"
#include "sdk/status.h"

using std::string;
using std::chrono::milliseconds;
using wwiv::bbs::InputMode;
using namespace wwiv::bbs;
using namespace wwiv::core;
using namespace wwiv::os;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

// Local function prototypes

static bool CreateNewUserRecord();
bool CreateRandomPassword();

bool CanCreateNewUserAccountHere();
bool UseMinimalNewUserInfo();
void noabort(const char* file_name);
bool check_dupes(const char* pszPhoneNumber);
void DoMinimalNewUser();
bool check_zip(const std::string& zipcode, int mode);
void DoFullNewUser();
void VerifyNewUserFullInfo();
void VerifyNewUserPassword();
void SendNewUserFeedbackIfRequired();
void ExecNewUserCommand();
void new_mail();
bool CheckPasswordComplexity(User* pUser, string& password);

static void input_phone() {
  bool ok = true;
  string phoneNumber;
  do {
    bout.nl();
    bout << "|#3Enter your VOICE phone no. in the form:\r\n|#3 ###-###-####\r\n|#2:";
    phoneNumber = input_phonenumber(a()->user()->GetVoicePhoneNumber(), 12);

    ok = valid_phone(phoneNumber.c_str());
    if (!ok) {
      bout.nl();
      bout << "|#6Please enter a valid phone number in the correct format.\r\n";
    }
  } while (!ok && !a()->hangup_);
  if (!a()->hangup_) {
    a()->user()->SetVoicePhoneNumber(phoneNumber.c_str());
  }
}

void input_dataphone() {
  bool ok = true;
  do {
    bout.nl();
    bout << "|#3Enter your DATA phone no. in the form. \r\n";
    bout << "|#3 ###-###-#### - Press Enter to use [" << a()->user()->GetVoicePhoneNumber()
         << "].\r\n";
    string data_phone_number = input_phonenumber(a()->user()->GetDataPhoneNumber(), 12);
    if (data_phone_number[0] == '\0') {
      data_phone_number = a()->user()->GetVoicePhoneNumber();
    }
    ok = valid_phone(data_phone_number);

    if (ok) {
      a()->user()->SetDataPhoneNumber(data_phone_number.c_str());
    } else {
      bout.nl();
      bout << "|#6Please enter a valid phone number in the correct format.\r\n";
    }
  } while (!ok && !a()->hangup_);
}

void input_language() {
  if (a()->languages.size() > 1) {
    a()->user()->SetLanguage(255);
    do {
      char ch = 0, onx[20];
      bout.nl(2);
      for (size_t i = 0; i < a()->languages.size(); i++) {
        bout << (i + 1) << ". " << a()->languages[i].name << wwiv::endl;
        if (i < 9) {
          onx[i] = static_cast<char>('1' + i);
        }
      }
      bout.nl();
      bout << "|#2Which language? ";
      if (a()->languages.size() < 10) {
        onx[a()->languages.size()] = 0;
        ch = onek(onx);
        ch -= '1';
      } else {
        std::set<char> odc;
        for (size_t i = 1; i <= a()->languages.size() / 10; i++) {
          odc.insert(static_cast<char>('0' + i));
        }
        string ss = mmkey(odc);
        ch = ss.front() - 1;
      }
      if (ch >= 0 && ch < size_int(a()->languages)) {
        a()->user()->SetLanguage(a()->languages[ch].num);
      }
    } while ((a()->user()->GetLanguage() == 255) && (!a()->hangup_));

    set_language(a()->user()->GetLanguage());
  }
}

static bool check_name(const string& user_name) {
  if (user_name.length() == 0 || user_name[user_name.length() - 1] == 32 || user_name[0] < 65 ||
      finduser(user_name) != 0 || user_name.find("@") != string::npos ||
      user_name.find("#") != string::npos) {
    return false;
  }

  Trashcan trashcan(*a()->config());
  if (trashcan.IsTrashName(user_name)) {
    LOG(INFO) << "Trashcan name entered from IP: " << a()->remoteIO()->remote_info().address
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
    if (!(a()->config()->sysconfig_flags() & sysconfig_allow_alias)) {
      bout << "|#3Enter your FULL REAL name.\r\n";
    } else {
      bout << "|#3Enter your full name, or your alias.\r\n";
    }
    string temp_local_name = input_upper(a()->user()->GetName(), 30);
    ok = check_name(temp_local_name);
    if (ok) {
      a()->user()->set_name(temp_local_name.c_str());
    } else {
      bout.nl();
      bout << "|#6I'm sorry, you can't use that name.\r\n";
      ++count;
      if (count == 3) {
        hang_it_up();
        Hangup();
      }
    }
  } while (!ok && !a()->hangup_);
}

void input_realname() {
  if (a()->config()->sysconfig_flags() & sysconfig_allow_alias) {
    do {
      bout.nl();
      bout << "|#3Enter your FULL real name.\r\n";
      string temp_local_name = input_proper(a()->user()->GetRealName(), 30);
      if (temp_local_name.empty()) {
        bout.nl();
        bout << "|#6Sorry, you must enter your FULL real name.\r\n";
      } else {
        a()->user()->SetRealName(temp_local_name.c_str());
      }
    } while ((a()->user()->GetRealName()[0] == '\0') && !a()->hangup_);
  } else {
    a()->user()->SetRealName(a()->user()->GetName());
  }
}

static void input_callsign() {
  bout.nl();
  bout << " |#3Enter your amateur radio callsign, or just hit <ENTER> if none.\r\n|#2:";
  string s = input_upper(6);
  a()->user()->SetCallsign(s.c_str());
}

bool valid_phone(const string& phoneNumber) {
  if (a()->config()->sysconfig_flags() & sysconfig_free_phone) {
    return true;
  }

  if (a()->config()->sysconfig_flags() & sysconfig_extended_info) {
    if (!IsPhoneNumberUSAFormat(a()->user()) && a()->user()->GetCountry()[0]) {
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
    street = input_proper(a()->user()->GetStreet(), 30);

    if (street.empty()) {
      bout.nl();
      bout << "|#6I'm sorry, you must enter your street address.\r\n";
    }
  } while (street.empty() && !a()->hangup_);
  if (!a()->hangup_) {
    a()->user()->SetStreet(street.c_str());
  }
}

void input_city() {
  string city;
  do {
    bout.nl();
    bout << "|#3Enter your city (i.e San Francisco). \r\n";
    city = input_proper(a()->user()->GetCity(), 30);

    if (city.empty()) {
      bout.nl();
      bout << "|#6I'm sorry, you must enter your city.\r\n";
    }
  } while (city.empty() && !a()->hangup_);
  a()->user()->SetCity(city.c_str());
}

void input_state() {
  string state;
  do {
    bout.nl();
    if (IsEquals(a()->user()->GetCountry(), "CAN")) {
      bout << "|#3Enter your province (i.e. QC).\r\n";
    } else {
      bout << "|#3Enter your state (i.e. CA). \r\n";
    }
    bout << "|#2:";
    state = input_upper(2);

    if (state.empty()) {
      bout.nl();
      bout << "|#6I'm sorry, you must enter your state or province.\r\n";
    }
  } while (state.empty() && (!a()->hangup_));
  a()->user()->SetState(state.c_str());
}

void input_country() {
  string country;
  do {
    bout.nl();
    bout << "|#3Enter your country (i.e. USA). \r\n";
    bout << "|#3Hit Enter for \"USA\"\r\n";
    bout << "|#2:";
    country = input_upper(3);
    if (country.empty()) {
      country = "USA";
    }
  } while (country.empty() && (!a()->hangup_));
  a()->user()->SetCountry(country.c_str());
}

void input_zipcode() {
  string zipcode;
  do {
    int len = 7;
    bout.nl();
    if (IsEquals(a()->user()->GetCountry(), "USA")) {
      bout << "|#3Enter your zipcode as #####-#### \r\n";
      len = 10;
    } else {
      bout << "|#3Enter your postal code as L#L-#L#\r\n";
      len = 7;
    }
    bout << "|#2:";
    zipcode = input_upper(len);

    if (zipcode.empty()) {
      bout.nl();
      bout << "|#6I'm sorry, you must enter your zipcode.\r\n";
    }
  } while (zipcode.empty() && (!a()->hangup_));
  a()->user()->SetZipcode(zipcode.c_str());
}

void input_sex() {
  bout.nl();
  bout << "|#2Your gender (M,F) :";
  a()->user()->SetGender(onek("MF"));
}

void input_age(User* pUser) {
  int y = 2000, m = 1, d = 1;
  auto dt = DateTime::now();

  bout.nl();
  do {
    bout.nl();
    y = static_cast<int>(dt.year() - 30) / 100;
    bout << "|#2Year you were born: ";
    y = input_number<int>(y, 1900, dt.year() - 30);
  } while (!a()->hangup_ && y < 1905);

  do {
    bout.nl();
    bout << "|#2Month you were born (1-12) : ";
    m = input_number<int>(pUser->GetBirthdayMonth(), 1, 12);
  } while (!a()->hangup_ && (m > 12 || m < 1));

  do {
    std::map<int, int> days_in_month = {
        {1, 31}, {3, 31}, {4, 30},  {5, 31},  {6, 30},  {7, 31},
        {8, 31}, {9, 30}, {10, 31}, {11, 30}, {12, 31},
    };
    if (isleap(y)) {
      days_in_month[2] = 29;
    } else {
      days_in_month[2] = 28;
    }
    bout.nl();
    bout << "|#2Day of month you were born (1-31) : ";
    d = input_number<int>(pUser->GetBirthdayDay(), 1, days_in_month.at(m));
  } while (!a()->hangup_ && (d > 31 || d < 1));
  pUser->SetBirthdayMonth(m);
  pUser->SetBirthdayDay(d);
  pUser->SetBirthdayYear(y);
  pUser->SetAge(
      years_old(pUser->GetBirthdayMonth(), pUser->GetBirthdayDay(), pUser->GetBirthdayYear()));
  bout.nl();
}

void input_comptype() {
  int ct = -1;

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
    ct = input_number(1, 1, i, false);
    ok = true;
    if (ct < 1 || ct > i) {
      ok = false;
    }
  } while (!ok && !a()->hangup_);

  a()->user()->SetComputerType(ct);
  if (a()->hangup_) {
    a()->user()->SetComputerType(-1);
  }
}

void input_screensize() {
  int x = 0, y = 0;

  bool ok = true;
  do {
    bout.nl();
    bout << "|#3How wide is your screen (chars, <CR>=80) ?\r\n|#2:";
    x = input_number(80, 32, 80, true);
    ok = true;
  } while (!ok && !a()->hangup_);

  do {
    bout.nl();
    bout << "|#3How tall is your screen (lines, <CR>=24) ?\r\n|#2:";
    y = input_number(24, 8, 60, true);
    ok = true;
  } while (!ok && !a()->hangup_);

  a()->user()->SetScreenChars(x);
  a()->user()->SetScreenLines(y);
  a()->screenlinest = y;
}

bool CheckPasswordComplexity(User*, string& password) {
  if (password.length() < 3) {
    // TODO - the min length should be in wwiv.ini
    return false;
  }
  return true;
}

void input_pw(User* pUser) {
  string password;
  bool ok = true;
  do {
    ok = true;
    password.clear();
    bout.nl();
    password = input_password("|#3Please enter a new password, 3-8 chars.\r\n", 8);

    if (!CheckPasswordComplexity(pUser, password)) {
      ok = false;
      bout.nl(2);
      bout << "Invalid password.  Try again.\r\n\n";
    }
  } while (!ok && !a()->hangup_);

  if (ok) {
    pUser->SetPassword(password);
  } else {
    bout << "Password not changed.\r\n";
  }
}

void input_ansistat() {
  a()->user()->ClearStatusFlag(User::ansi);
  a()->user()->ClearStatusFlag(User::status_color);
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
    a()->user()->SetStatusFlag(User::ansi);
    bout.nl();
    bout << "|#5Do you want color? ";
    if (noyes()) {
      a()->user()->SetStatusFlag(User::status_color);
      a()->user()->SetStatusFlag(User::extraColor);
    } else {
      color_list();
      bout.nl();
      bout << "|#2Monochrome base color (<C/R>=7)? ";
      char ch = onek("\r123456789");
      if (ch == '\r') {
        ch = '7';
      }
      int c = ch - '0';
      int c2 = c << 4;
      for (int i = 0; i < 10; i++) {
        if ((a()->user()->GetBWColor(i) & 0x70) == 0) {
          a()->user()->SetBWColor(i, (a()->user()->GetBWColor(i) & 0x88) | c);
        } else {
          a()->user()->SetBWColor(i, (a()->user()->GetBWColor(i) & 0x88) | c2);
        }
      }
    }
  }
}

// Inserts a record into NAMES.LST
static void InsertSmallRecord(StatusMgr& sm, Names& names, int user_number, const char* name) {
  sm.Run([&](WStatus& s) {
    names.Add(name, user_number);
    names.Save();
    s.IncrementNumUsers();
    s.IncrementFileChangedFlag(WStatus::fileChangeNames);
  });
}

static int find_new_usernum(const User* pUser, uint32_t* qscn) {
  File userFile(FilePath(a()->config()->datadir(), USER_LST));
  for (int i = 0; !userFile.IsOpen() && (i < 20); i++) {
    if (!userFile.Open(File::modeBinary | File::modeReadWrite | File::modeCreateFile)) {
      sleep_for(milliseconds(100));
    }
  }
  if (!userFile.IsOpen()) {
    return -1;
  }

  int nNewUserNumber =
      static_cast<int>((userFile.length() / a()->config()->userrec_length()) - 1);
  userFile.Seek(a()->config()->userrec_length(), File::Whence::begin);
  int user_number = 1;

  if (nNewUserNumber == a()->status_manager()->GetUserCount()) {
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
        userFile.Seek(static_cast<long>(user_number * a()->config()->userrec_length()),
                      File::Whence::begin);
        nNewUserNumber =
            static_cast<int>((userFile.length() / a()->config()->userrec_length()) - 1);
      }
      User tu;
      userFile.Read(&tu.data, a()->config()->userrec_length());

      if (tu.IsUserDeleted() && tu.GetSl() != 255) {
        userFile.Seek(static_cast<long>(user_number * a()->config()->userrec_length()),
                      File::Whence::begin);
        userFile.Write(&pUser->data, a()->config()->userrec_length());
        userFile.Close();
        write_qscn(user_number, qscn, false);
        InsertSmallRecord(*a()->status_manager(), *a()->names(), user_number, pUser->GetName());
        return user_number;
      } else {
        user_number++;
      }
    }
  }

  if (user_number <= a()->config()->max_users()) {
    userFile.Seek(static_cast<long>(user_number * a()->config()->userrec_length()),
                  File::Whence::begin);
    userFile.Write(&pUser->data, a()->config()->userrec_length());
    userFile.Close();
    write_qscn(user_number, qscn, false);
    InsertSmallRecord(*a()->status_manager(), *a()->names(), user_number, pUser->GetName());
    return user_number;
  } else {
    userFile.Close();
    return -1;
  }
}

// Clears a()->user()'s data and makes it ready to be a new user, also
// clears the QScan pointers
static bool CreateNewUserRecord() {
  a()->context().ResetQScanPointers();

  auto u = a()->user();
  a()->reset_effective_sl();

  bool ok = User::CreateNewUserRecord(
      u, a()->config()->newuser_sl(), a()->config()->newuser_dsl(),
      a()->config()->newuser_restrict(), a()->config()->newuser_gold(),
      a()->newuser_colors, a()->newuser_bwcolors);
  u->CreateRandomPassword();
  return ok;
}

// returns true if the user is allow to create a new user account
// on here, if this function returns false, a sufficient error
// message has already been displayed to the user.
bool CanCreateNewUserAccountHere() {
  if (a()->status_manager()->GetUserCount() >= a()->config()->max_users()) {
    bout.nl(2);
    bout << "I'm sorry, but the system currently has the maximum number of users it "
            "can\r\nhandle.\r\n\n";
    return false;
  }

  if (a()->config()->closed_system()) {
    bout.nl(2);
    bout << "I'm sorry, but the system is currently closed, and not accepting new users.\r\n\n";
    return false;
  }

  if (!a()->config()->newuser_password().empty() && a()->context().incom()) {
    bout.nl(2);
    bool ok = false;
    int nPasswordAttempt = 0;
    do {
      auto password = input_password("New User Password :" , 20);
      if (password == a()->config()->newuser_password()) {
        ok = true;
      } else {
        sysoplog() << "Wrong newuser password: " << password;
      }
    } while (!ok && !a()->hangup_ && (nPasswordAttempt++ < 4));
    if (!ok) {
      return false;
    }
  }
  return true;
}

bool UseMinimalNewUserInfo() {
  IniFile ini(FilePath(a()->bbsdir(), WWIV_INI),
              {StrCat("WWIV-", a()->instance_number()), INI_TAG});
  if (ini.IsOpen()) {
    return ini.value<bool>("NEWUSER_MIN");
  }
  return false;
}

static void DefaultToWWIVEditIfPossible() {
  for (size_t nEditor = 0; nEditor < a()->editors.size(); nEditor++) {
    // TODO(rushfan): Should we get rid of this favoring of WWIVEDIT?
    string editor_desc(a()->editors[nEditor].description);
    StringUpperCase(&editor_desc);
    if (editor_desc.find("WWIVEDIT") != string::npos) {
      a()->user()->SetDefaultEditor(nEditor + 1);
      return;
    }
  }
}
void DoFullNewUser() {
  const auto u = a()->user();

  input_name();
  input_realname();
  input_phone();
  if (a()->HasConfigFlag(OP_FLAGS_CHECK_DUPE_PHONENUM)) {
    if (check_dupes(u->GetVoicePhoneNumber())) {
      if (a()->HasConfigFlag(OP_FLAGS_HANGUP_DUPE_PHONENUM)) {
        hang_it_up();
        return;
      }
    }
  }
  if (a()->config()->sysconfig_flags() & sysconfig_extended_info) {
    input_street();
    const auto zip_city_dir = FilePath(a()->config()->datadir(), ZIPCITY_DIR);
    if (File::Exists(FilePath(zip_city_dir, "zip1.dat"))) {
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

    if (a()->HasConfigFlag(OP_FLAGS_CHECK_DUPE_PHONENUM)) {
      if (check_dupes(u->GetDataPhoneNumber())) {
        if (a()->HasConfigFlag(OP_FLAGS_HANGUP_DUPE_PHONENUM)) {
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

  if (a()->editors.size() && u->HasAnsi()) {
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
  IniFile ini(FilePath(a()->bbsdir(), WWIV_INI),
              {StrCat("WWIV-", a()->instance_number()), INI_TAG});
  if (!ini.IsOpen()) {
    return;
  }
  const auto asv_num = ini.value<int>("SIMPLE_ASV_NUM", 0);
  if (a()->HasConfigFlag(OP_FLAGS_SIMPLE_ASV) && a()->asv.sl > a()->config()->newuser_sl() &&
      a()->asv.sl < 90) {
    bout.nl();
    bout << "|#5Are you currently a WWIV SysOp? ";
    if (yesno()) {
      bout.nl();
      bout << "|#5Please enter your BBS name and number.\r\n";
      auto note = input_text(60);
      a()->user()->SetNote(note.c_str());
      a()->user()->SetSl(a()->asv.sl);
      a()->user()->SetDsl(a()->asv.dsl);
      a()->user()->SetAr(a()->asv.ar);
      a()->user()->SetDar(a()->asv.dar);
      a()->user()->SetExempt(a()->asv.exempt);
      a()->user()->SetRestriction(a()->asv.restrict);
      bout.nl();
      printfile(ASV_NOEXT);
      bout.nl();
      pausescr();
    }
  }
}

void VerifyNewUserFullInfo() {
  bool ok = false;
  const auto u = a()->user();

  do {
    bout.nl(2);
    bout << "|#91) Name          : |#2" << u->GetName() << wwiv::endl;
    if (a()->config()->sysconfig_flags() & sysconfig_allow_alias) {
      bout << "|#92) Real Name     : |#2" << u->GetRealName() << wwiv::endl;
    }
    bout << "|#93) Callsign      : |#2" << u->GetCallsign() << wwiv::endl;
    bout << "|#94) Phone No.     : |#2" << u->GetVoicePhoneNumber() << wwiv::endl;
    bout << "|#95) Gender        : |#2" << u->GetGender() << wwiv::endl;
    bout << "|#96) Birthdate     : |#2" << static_cast<int>(u->GetBirthdayMonth()) << "/"
         << static_cast<int>(u->GetBirthdayDay()) << "/" << static_cast<int>(u->GetBirthdayYear())
         << wwiv::endl;
    bout << "|#97) Computer type : |#2" << ctypes(u->GetComputerType()) << wwiv::endl;
    bout << "|#98) Screen size   : |#2" << u->GetScreenChars() << " X " << u->GetScreenLines()
         << wwiv::endl;
    bout << "|#99) Password      : |#2" << u->GetPassword() << wwiv::endl;
    if (a()->config()->sysconfig_flags() & sysconfig_extended_info) {
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
    if (a()->config()->sysconfig_flags() & sysconfig_extended_info) {
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
      if (a()->config()->sysconfig_flags() & sysconfig_allow_alias) {
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
  } while (!ok && !a()->hangup_);
}

static void add_phone_number(int usernum, const char* phone) {
  if (strstr(phone, "000-")) {
    return;
  }

  PhoneNumbers pn(*a()->config());
  if (!pn.IsInitialized()) {
    return;
  }

  pn.insert(usernum, phone);
}

void WriteNewUserInfoToSysopLog() {
  const auto u = a()->user();
  sysoplog() << "** New User Information **";
  sysoplog() << StringPrintf("-> %s #%ld (%s)", u->GetName(), a()->usernum, u->GetRealName());
  if (a()->config()->sysconfig_flags() & sysconfig_extended_info) {
    sysoplog() << "-> " << u->GetStreet();
    sysoplog() << "-> " << u->GetCity() << ", " << u->GetState() << " " << u->GetZipcode() << "  ("
               << u->GetCountry() << " )";
  }
  sysoplog() << StringPrintf("-> %s (Voice)", u->GetVoicePhoneNumber());
  if (a()->config()->sysconfig_flags() & sysconfig_extended_info) {
    sysoplog() << StringPrintf("-> %s (Data)", u->GetDataPhoneNumber());
  }
  sysoplog() << StringPrintf("-> %02d/%02d/%02d (%d yr old %s)", u->GetBirthdayMonth(),
                             u->GetBirthdayDay(), u->GetBirthdayYear(), u->GetAge(),
                             ((u->GetGender() == 'M') ? "Male" : "Female"));
  sysoplog() << StringPrintf("-> Using a %s Computer", ctypes(u->GetComputerType()).c_str());
  if (u->GetWWIVRegNumber()) {
    sysoplog() << StringPrintf("-> WWIV Registration # %ld", u->GetWWIVRegNumber());
  }
  sysoplog() << "********";

  if (u->GetVoicePhoneNumber()[0]) {
    add_phone_number(a()->usernum, u->GetVoicePhoneNumber());
  }
  if (u->GetDataPhoneNumber()[0]) {
    add_phone_number(a()->usernum, u->GetDataPhoneNumber());
  }
}

void VerifyNewUserPassword() {
  bool ok = false;
  do {
    bout.nl(2);
    bout << "|#9Your User Number: |#2" << a()->usernum << wwiv::endl;
    bout << "|#9Your Password:    |#2" << a()->user()->GetPassword() << wwiv::endl;
    bout.nl(1);
    bout << "|#9Please write down this information, and enter your password for verification.\r\n";
    bout << "|#9You will need to know this password in order to change it to something else.\r\n\n";
    string password = input_password("|#9PW: ", 8);
    if (password == a()->user()->GetPassword()) {
      ok = true;
    }
  } while (!ok && !a()->hangup_);
}

void SendNewUserFeedbackIfRequired() {
  if (!a()->context().incom()) {
    return;
  }

  if (a()->config()->sysconfig_flags() & sysconfig_no_newuser_feedback) {
    // If NEWUSER_FEEDBACK=N don't attempt to send newuser feedback.
    return;
  }

  if (a()->HasConfigFlag(OP_FLAGS_FORCE_NEWUSER_FEEDBACK)) {
    noabort(FEEDBACK_NOEXT);
  } else if (printfile(FEEDBACK_NOEXT)) {
    sysoplog(false) << "";
  }
  feedback(true);
  if (a()->HasConfigFlag(OP_FLAGS_FORCE_NEWUSER_FEEDBACK)) {
    if (!a()->user()->GetNumEmailSent() && !a()->user()->GetNumFeedbackSent()) {
      printfile(NOFBACK_NOEXT);
      a()->users()->delete_user(a()->usernum);
      Hangup();
      return;
    }
  }
}

void ExecNewUserCommand() {
  if (!a()->hangup_ && !a()->newuser_cmd.empty()) {
    const auto commandLine = stuff_in(a()->newuser_cmd, create_chain_file(), "", "", "", "");

    // Log what is happening here.
    sysoplog(false) << "Executing New User Event: ";
    sysoplog() << commandLine;

    a()->WriteCurrentUser();
    ExecuteExternalProgram(commandLine, a()->spawn_option(SPAWNOPT_NEWUSER));
    a()->ReadCurrentUser();
  }
}

void newuser() {
  sysoplog(false);
  string t = times();
  string f = fulldate();
  sysoplog(false) << StringPrintf("*** NEW USER %s   %s    %s (%ld)", f.c_str(), t.c_str(),
                                  a()->GetCurrentSpeed().c_str(), a()->instance_number());

  LOG(INFO) << "New User Attempt from IP Address: " << a()->remoteIO()->remote_info().address;
  a()->screenlinest = 25;

  if (!CreateNewUserRecord()) {
    return;
  }

  input_language();
  if (!CanCreateNewUserAccountHere() || a()->hangup_) {
    Hangup();
    return;
  }

  if (check_ansi()) {
    a()->user()->SetStatusFlag(User::ansi);
    a()->user()->SetStatusFlag(User::status_color);
    a()->user()->SetStatusFlag(User::extraColor);
  }
  printfile(SYSTEM_NOEXT);
  bout.nl();
  pausescr();
  printfile(NEWUSER_NOEXT);
  bout.nl();
  pausescr();
  bout.cls();
  bout << "|#5Create a new user account on " << a()->config()->system_name() << "? ";
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
  bout << "Random password: " << a()->user()->GetPassword() << wwiv::endl << wwiv::endl;
  bout << "|#5Do you want a different password (Y/N)? ";
  if (yesno()) {
    input_pw(a()->user());
  }

  DoNewUserASV();

  if (a()->hangup_) {
    return;
  }

  if (!newuser_min) {
    VerifyNewUserFullInfo();
  }

  if (a()->hangup_) {
    return;
  }

  bout.nl();
  bout << "Please wait...\r\n\n";
  auto usernum = find_new_usernum(a()->user(), a()->context().qsc);
  if (usernum <= 0) {
    bout.nl();
    bout << "|#6Error creating user account.\r\n\n";
    Hangup();
    return;
  } else if (usernum == 1) {
    a()->usernum = static_cast<uint16_t>(usernum);

    // This is the #1 sysop record. Tell the sysop thank you and
    // update his user record with: s255/d255/r0
    ssm(1) << "Thank you for installing WWIV! - The WWIV Development Team.";
    User* user = a()->user();
    user->SetSl(255);
    user->SetDsl(255);
    user->SetRestriction(0);
  }
  a()->usernum = static_cast<uint16_t>(usernum);

  WriteNewUserInfoToSysopLog();
  ssm(1) << "You have a new user: " << a()->user()->GetName() << " #" << a()->usernum;

  LOG(INFO) << "New User Created: '" << a()->user()->GetName() << "' "
            << "IP Address: " << a()->remoteIO()->remote_info().address;

  a()->UpdateTopScreen();
  VerifyNewUserPassword();
  SendNewUserFeedbackIfRequired();
  ExecNewUserCommand();
  a()->reset_effective_sl();
  changedsl();
  new_mail();
}

/**
 * Takes an input string and reduces repeated spaces in the string to one space.
 */
void single_space(char* text) {
  if (!text || !*text) {
    return;
  }
  char* pInputBuffer = text;
  char* pOutputBuffer = text;
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

bool check_zip(const std::string& zipcode, int mode) {
  char city[81], state[21];

  if (zipcode.empty()) {
    return false;
  }

  bool ok = true;
  bool found = false;

  const auto zipcity_dir = FilePath(a()->config()->datadir(), ZIPCITY_DIR);
  const auto fn = StringPrintf("zip%c.dat", zipcode.front());

  TextFile zip_file(FilePath(zipcity_dir, fn), "r");
  if (!zip_file.IsOpen()) {
    ok = false;
    if (mode != 2) {
      bout << "\r\n|#6" << FilePath(zipcity_dir, fn) << " not found\r\n";
    }
  } else {
    char zip_buf[81];
    while ((zip_file.ReadLine(zip_buf, 80)) && !found && !a()->hangup_) {
      single_space(zip_buf);
      if (strncmp(zip_buf, zipcode.c_str(), 5) == 0) {
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
    bout << "|#6No match for " << zipcode << ".";
    ok = false;
  }

  if (!mode && found) {
    bout << "\r\n|#2" << zipcode << " is in " << city << ", " << state << ".";
    ok = false;
  }

  if (found) {
    if (mode != 2) {
      bout << "\r\n|#2" << city << ", " << state << "  USA? (y/N): ";
      if (yesno()) {
        a()->user()->SetCity(city);
        a()->user()->SetState(state);
        a()->user()->SetCountry("USA");
      } else {
        bout.nl();
        bout << "|#6My records show differently.\r\n\n";
        ok = false;
      }
    } else {
      a()->user()->SetCity(city);
      a()->user()->SetState(state);
      a()->user()->SetCountry("USA");
    }
  }
  return ok;
}

static int find_phone_number(const char* phone) {
  PhoneNumbers pn(*a()->config());
  if (!pn.IsInitialized()) {
    return 0;
  }

  auto user_number = pn.find(phone);
  User user{};
  if (!a()->users()->readuser(&user, user_number)) {
    return 0;
  }
  if (user.IsUserDeleted()) {
    return 0;
  }
  return user_number;
}

bool check_dupes(const char* pszPhoneNumber) {
  int user_number = find_phone_number(pszPhoneNumber);
  if (user_number && user_number != a()->usernum) {
    string s = StringPrintf("    %s entered phone # %s", a()->user()->GetName(), pszPhoneNumber);
    sysoplog(false) << s;
    ssm(1) << s;

    User user;
    a()->users()->readuser(&user, user_number);
    s = StringPrintf("      also entered by %s", user.GetName());
    sysoplog(false) << s;
    ssm(1) << s;

    return true;
  }

  return false;
}

void noabort(const char* file_name) {
  bool oic = false;

  if (a()->using_modem) {
    oic = a()->context().incom();
    a()->context().incom(false);
    bout.dump();
  }
  printfile(file_name);
  if (a()->using_modem) {
    bout.dump();
    a()->context().incom(oic);
  }
}

static void cln_nu() {
  bout.RestorePosition();
  bout.Color(0);
  bout.clreol();
}

void DoMinimalNewUser() {
  const auto u = a()->user();

  int m = 1, d = 1, y = 2000, ch = 0;
  char m1[3], d1[3], y1[5];
  static const char* mon[12] = {"January",   "February", "March",    "April",
                                "May",       "June",     "July",     "August",
                                "September", "October",  "November", "December"};

  bout.newline = false;
  bool done = false;
  int nSaveTopData = a()->topdata;
  a()->topdata = LocalIO::topdataNone;
  a()->UpdateTopScreen();
  do {
    bout.cls();
    bout.litebar(StrCat(a()->config()->system_name(), " New User Registration"));
    bout << "|#1[A] Name (real or alias)    : ";
    if (u->GetName()[0] == '\0') {
      bool ok = true;
      std::string temp_name;
      do {
        bout.SavePosition();
        temp_name = input_upper("", 30);
        ok = check_name(temp_name);
        if (!ok) {
          cln_nu();
          BackPrint("I'm sorry, you can't use that name.", 6, 20, 1000);
        }
      } while (!ok && !a()->hangup_);
      u->set_name(temp_name.c_str());
    }
    cln_nu();
    bout << "|#2" << u->GetName();
    bout.nl();
    bout << "|#1[B] Birth Date (MM/DD/YYYY) : ";
    bout.SavePosition();
    if (u->GetAge() == 0) {
      bool ok = false;
      bout.SavePosition();
      do {
        ok = false;
        cln_nu();
        auto s = input_date_mmddyyyy("");
        if (s.size() == 10) {
          sprintf(m1, "%c%c", s[0], s[1]);
          sprintf(d1, "%c%c", s[3], s[4]);
          sprintf(y1, "%c%c%c%c", s[6], s[7], s[8], s[9]);
          m = to_number<int>(m1);
          d = to_number<int>(d1);
          y = to_number<int>(y1);
          ok = true;
          if ((((m == 2) || (m == 9) || (m == 4) || (m == 6) || (m == 11)) && (d >= 31)) ||
              ((m == 2) && (((!isleap(y)) && (d == 29)) || (d == 30))) ||
              (years_old(m, d, y) < 5) || (d > 31) || ((m == 0) || (y == 0) || (d == 0))) {
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
      } while (!ok && !a()->hangup_);
    }
    if (a()->hangup_) {
      return;
    }
    u->SetBirthdayMonth(m);
    u->SetBirthdayDay(d);
    u->SetBirthdayYear(y);
    u->SetAge(years_old(m, d, y));
    cln_nu();
    bout << "|#2" << mon[std::max<int>(0, u->GetBirthdayMonth() - 1)] << " " << u->GetBirthdayDay()
         << ", " << u->GetBirthdayYear() << " (" << u->GetAge() << " years old)\r\n"
         << "|#1[C] Sex (Gender)            : ";
    bout.SavePosition();
    if (u->GetGender() != 'M' && u->GetGender() != 'F') {
      bout.mpl(1);
      u->SetGender(onek_ncr("MF"));
    }
    cln_nu();
    bout << "|#2" << (u->GetGender() == 'M' ? "Male" : "Female") << wwiv::endl;
    bout << "|#1[D] Country                 : ";
    bout.SavePosition();
    if (u->GetCountry()[0] == '\0') {
      auto country = input_upper("", 3);
      if (!country.empty()) {
        u->SetCountry(country.c_str());
      } else {
        u->SetCountry("USA");
      }
    }
    cln_nu();
    bout << "|#2" << u->GetCountry() << wwiv::endl;
    bout << "|#1[E] ZIP or Postal Code      : ";
    bout.SavePosition();
    if (u->GetZipcode()[0] == 0) {
      bool ok = false;
      do {
        if (IsEquals(u->GetCountry(), "USA")) {
          auto zip = input_upper(u->GetZipcode(), 5);
          check_zip(zip, 2);
          u->SetZipcode(zip.c_str());
        } else {
          auto zip = input_upper(u->GetZipcode(), 7);
          u->SetZipcode(zip.c_str());
        }
        if (u->GetZipcode()[0]) {
          ok = true;
        }
      } while (!ok && !a()->hangup_);
    }
    cln_nu();
    bout << "|#2" << u->GetZipcode() << wwiv::endl;
    bout << "|#1[F] City/State/Province     : ";
    bout.SavePosition();
    if (u->GetCity()[0] == 0) {
      bool ok = false;
      do {
        auto city = input_proper(u->GetCity(), 30);
        u->SetCity(city.c_str());
        if (u->GetCity()[0]) {
          ok = true;
        }
      } while (!ok && !a()->hangup_);
      bout << ", ";
      if (u->GetState()[0] == 0) {
        do {
          ok = false;
          auto state = input_upper(u->GetState(), 2);
          u->SetState(state.c_str());
          if (u->GetState()[0]) {
            ok = true;
          }
        } while (!ok && !a()->hangup_);
      }
    }
    cln_nu();
    properize(reinterpret_cast<char*>(u->data.city));
    bout << "|#2" << u->GetCity() << ", " << u->GetState() << wwiv::endl;
    bout << "|#1[G] Internet Mail Address   : ";
    bout.SavePosition();
    if (u->GetEmailAddress().empty()) {
      auto emailAddress = input_text("", 44);
      u->SetEmailAddress(emailAddress.c_str());
      if (!check_inet_addr(u->GetEmailAddress())) {
        cln_nu();
        BackPrint("Invalid address!", 6, 20, 1000);
      }
      if (u->GetEmailAddress().empty()) {
        u->SetEmailAddress("None");
      }
    }
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
      u->set_name("");
      break;
    case 'B':
      u->SetAge(0);
      //sprintf(s1, "%02d/%02d/%02d", u->GetBirthdayDay(), u->GetBirthdayMonth(),
      //        u->GetBirthdayYear());
      break;
    case 'C':
      u->SetGender('N');
      break;
    case 'D':
      u->SetCountry("");
    case 'E':
      u->SetZipcode("");
    case 'F':
      u->SetCity("");
      u->SetState("");
      break;
    case 'G':
      u->SetEmailAddress("");
      break;
    }
  } while (!done && !a()->hangup_);
  u->SetRealName(u->GetName());
  u->SetVoicePhoneNumber("999-999-9999");
  u->SetDataPhoneNumber(u->GetVoicePhoneNumber());
  u->SetStreet("None Requested");
  if (a()->editors.size() && u->HasAnsi()) {
    u->SetDefaultEditor(1);
  }
  a()->topdata = nSaveTopData;
  a()->UpdateTopScreen();
  bout.newline = true;
}

void new_mail() {
  auto file =
      FilePath(a()->config()->gfilesdir(),
               (a()->user()->GetSl() > a()->config()->newuser_sl()) ? NEWSYSOP_MSG : NEWMAIL_MSG);

  if (!File::Exists(file)) {
    return;
  }
  int save_ed = a()->user()->GetDefaultEditor();
  a()->user()->SetDefaultEditor(0);
  LoadFileIntoWorkspace(file, true, true);
  use_workspace = true;

  MessageEditorData data;
  data.title = StrCat("Welcome to ", a()->config()->system_name(), "!");
  data.need_title = true;
  data.anonymous_flag = 0;
  data.aux = "email";
  data.fsed_flags = FsedFlags::NOFSED;
  data.to_name = a()->names()->UserName(a()->usernum);
  data.msged_flags = MSGED_FLAG_NONE;
  data.silent_mode = true;
  messagerec msg;
  msg.storage_type = 2;
  if (inmsg(data)) {
    savefile(data.text, &msg, data.aux);

    EmailData email(data);
    email.msg = &msg;
    email.anony = 0;
    email.user_number = a()->usernum;
    email.system_number = 0;
    email.an = true;
    email.from_user = 1;
    email.from_system = 0;
    email.forwarded_code = 1;
    email.from_network_number = 0;
    email.silent_mode = true;
    sendout_email(email);
  }
  a()->user()->SetNumMailWaiting(a()->user()->GetNumMailWaiting() + 1);
  a()->user()->SetDefaultEditor(save_ed);
}
