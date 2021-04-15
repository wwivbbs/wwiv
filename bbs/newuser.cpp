/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services             */
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

#include "bbs/bbs.h"
#include "bbs/bbsovl1.h"
#include "bbs/bbsovl2.h"
#include "bbs/bbsutl.h"
#include "bbs/bbsutl1.h"
#include "bbs/bbsutl2.h"
#include "bbs/confutil.h"
#include "bbs/defaults.h"
#include "bbs/dropfile.h"
#include "bbs/email.h"
#include "bbs/execexternal.h"
#include "bbs/finduser.h"
#include "bbs/inetmsg.h"
#include "bbs/inmsg.h"
#include "bbs/lilo.h"
#include "bbs/message_file.h"
#include "bbs/mmkey.h"
#include "bbs/shortmsg.h"
#include "bbs/sr.h"
#include "bbs/stuffin.h"
#include "bbs/sysoplog.h"
#include "bbs/trashcan.h"
#include "bbs/wqscn.h"
#include "common/com.h"
#include "common/datetime.h"
#include "common/input.h"
#include "common/output.h"
#include "common/workspace.h"
#include "core/clock.h"
#include "core/inifile.h"
#include "core/os.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "fmt/printf.h"
#include "local_io/local_io.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/names.h"
#include "sdk/phone_numbers.h"
#include "sdk/status.h"
#include "sdk/user.h"
#include "sdk/usermanager.h"
#include <chrono>
#include <string>

using std::chrono::milliseconds;
using wwiv::common::InputMode;
using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::os;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

// Local function prototypes

static bool CreateNewUserRecord();

bool CanCreateNewUserAccountHere();
void noabort(const char* file_name);
bool check_zip(const std::string& zipcode, int mode);
void VerifyNewUserPassword();
void SendNewUserFeedbackIfRequired();
void ExecNewUserCommand();
void new_mail();
bool CheckPasswordComplexity(User* pUser, std::string& password);

void input_phone() {
  bool ok = true;
  std::string phoneNumber;
  do {
    bout.nl();
    bout << "|#3Enter your VOICE phone no. in the form:\r\n|#3 ###-###-####\r\n|#2:";
    phoneNumber = bin.input_phonenumber(a()->user()->voice_phone(), 12);
    
    ok = valid_phone(phoneNumber);
    if (!ok) {
      bout.nl();
      bout << "|#6Please enter a valid phone number in the correct format.\r\n";
    }
  } while (!ok && !a()->sess().hangup());
  if (!a()->sess().hangup()) {
    a()->user()->voice_phone(phoneNumber);
  }
}

void input_dataphone() {
  bool ok = true;
  do {
    bout.nl();
    bout << "|#9Enter your DATA phone no. in the form. \r\n";
    bout << "|#9 ###-###-#### - Press Enter to use [" << a()->user()->voice_phone() << "].\r\n";
    std::string data_phone_number = bin.input_phonenumber(a()->user()->data_phone(), 12);
    if (data_phone_number[0] == '\0') {
      data_phone_number = a()->user()->voice_phone();
    }
    ok = valid_phone(data_phone_number);

    if (ok) {
      a()->user()->data_phone(data_phone_number);
    } else {
      bout.nl();
      bout << "|#6Please enter a valid phone number in the correct format.\r\n";
    }
  } while (!ok && !a()->sess().hangup());
}

static bool check_name(const std::string& user_name) {
  if (user_name.length() == 0 || user_name[user_name.length() - 1] == 32 || user_name[0] < 65 ||
      finduser(user_name) != 0 || user_name.find("@") != std::string::npos ||
      user_name.find("#") != std::string::npos) {
    return false;
  }

  Trashcan trashcan(*a()->config());
  if (trashcan.IsTrashName(user_name)) {
    LOG(INFO) << "Trashcan name entered from IP: " << bout.remoteIO()->remote_info().address
              << "; name: " << user_name;
    hang_it_up();
    a()->Hangup();
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
    std::string temp_local_name = bin.input_upper(a()->user()->name(), 30);
    ok = check_name(temp_local_name);
    if (ok) {
      a()->user()->set_name(temp_local_name);
    } else {
      bout.nl();
      bout << "|#6I'm sorry, you can't use that name.\r\n";
      ++count;
      if (count == 3) {
        hang_it_up();
        a()->Hangup();
      }
    }
  } while (!ok && !a()->sess().hangup());
}

void input_realname() {
  if (a()->config()->sysconfig_flags() & sysconfig_allow_alias) {
    do {
      bout.nl();
      bout << "|#3Enter your FULL real name.\r\n";
      std::string temp_local_name = bin.input_proper(a()->user()->real_name(), 30);
      if (temp_local_name.empty()) {
        bout.nl();
        bout << "|#6Sorry, you must enter your FULL real name.\r\n";
      } else {
        a()->user()->real_name(temp_local_name);
      }
    } while (a()->user()->real_name_or_empty().empty() && !a()->sess().hangup());
  } 
}

void input_callsign() {
  bout.nl();
  bout << " |#3Enter your amateur radio callsign, or just hit <ENTER> if none.\r\n|#2:";
  const auto s = bin.input_upper(a()->user()->callsign(), 6);
  a()->user()->callsign(s);
}

bool valid_phone(const std::string& phoneNumber) {
  if (a()->config()->sysconfig_flags() & sysconfig_free_phone) {
    return true;
  }

  if (!IsPhoneNumberUSAFormat(a()->user()) && !a()->user()->country().empty()) {
    return true;
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
    bout.nl();
    bout << "|#3Enter your street address.\r\n";
    street = bin.input_proper(a()->user()->street(), 30);

    if (street.empty()) {
      bout.nl();
      bout << "|#6I'm sorry, you must enter your street address.\r\n";
    }
  } while (street.empty() && !a()->sess().hangup());
  if (!a()->sess().hangup()) {
    a()->user()->street(street);
  }
}

void input_city() {
  std::string city;
  do {
    bout.nl();
    bout << "|#3Enter your city (i.e San Francisco). \r\n";
    city = bin.input_proper(a()->user()->city(), 30);

    if (city.empty()) {
      bout.nl();
      bout << "|#6I'm sorry, you must enter your city.\r\n";
    }
  } while (city.empty() && !a()->sess().hangup());
  a()->user()->city(city);
}

void input_state() {
  std::string state;
  do {
    bout.nl();
    if (iequals(a()->user()->country(), "CAN")) {
      bout << "|#3Enter your province (i.e. QC).\r\n";
    } else {
      bout << "|#3Enter your state (i.e. CA). \r\n";
    }
    bout << "|#2:";
    state = bin.input_upper(2);

    if (state.empty()) {
      bout.nl();
      bout << "|#6I'm sorry, you must enter your state or province.\r\n";
    }
  } while (state.empty() && (!a()->sess().hangup()));
  a()->user()->state(state);
}

void input_country() {
  std::string country;
  do {
    bout.nl();
    bout << "|#9Enter your country.  Hit Enter for \"|#1USA|#9\"\r\n";
    bout << "|#7: ";
    country = bin.input_upper(3);
    if (country.empty()) {
      country = "USA";
    }
  } while (country.empty() && (!a()->sess().hangup()));
  a()->user()->country(country);
}

void input_zipcode() {
  std::string zipcode;
  do {
    int len = 7;
    bout.nl();
    if (iequals(a()->user()->country(), "USA")) {
      bout << "|#3Enter your zipcode as #####-#### \r\n";
      len = 10;
    } else {
      bout << "|#3Enter your postal code as L#L-#L#\r\n";
      len = 7;
    }
    bout << "|#2:";
    zipcode = bin.input_upper(len);

    if (zipcode.empty()) {
      bout.nl();
      bout << "|#6I'm sorry, you must enter your zipcode.\r\n";
    }
  } while (zipcode.empty() && (!a()->sess().hangup()));
  a()->user()->zip_code(zipcode);
}

void input_sex() {
  bout.nl();
  bout << "|#2Your gender (M,F) :";
  a()->user()->gender(onek("MF"));
}

void input_age(User* u) {
  int y = 2000, m = 1, d = 1;
  const auto dt = DateTime::now();

  bout.nl();
  do {
    bout.nl();
    y = (dt.year() - 30) / 100;
    bout << "|#2Year you were born: ";
    y = bin.input_number<int>(y, 1900, static_cast<int>(dt.year() - 30));
  } while (!a()->sess().hangup() && y < 1905);

  do {
    bout.nl();
    bout << "|#2Month you were born (1-12) : ";
    m = bin.input_number<int>(u->birthday_month(), 1, 12);
  } while (!a()->sess().hangup() && (m > 12 || m < 1));

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
    d = bin.input_number<int>(u->birthday_mday(), 1, days_in_month.at(m));
  } while (!a()->sess().hangup() && (d > 31 || d < 1));
  u->birthday_mdy(m, d, y);
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
    ct = bin.input_number(1, 1, i, false);
    ok = true;
    if (ct < 1 || ct > i) {
      ok = false;
    }
  } while (!ok && !a()->sess().hangup());

  a()->user()->computer_type(ct);
  if (a()->sess().hangup()) {
    a()->user()->computer_type(-1);
  }
}

bool detect_screensize() { 
  if (const auto sso = bin.screen_size()) {
    const auto& ss = sso.value();
    if (ss.x != a()->user()->screen_width() || ss.y != a()->user()->screen_lines()) {
      bout.format("|#9Screen size of |#2{}|#9x|#2{} |#9detected.  Use it?", ss.x, ss.y);
      if (bin.noyes()) {
        a()->user()->screen_width(ss.x);
        a()->user()->screen_lines(ss.y);
        a()->sess().num_screen_lines(ss.y);
        return true;
      }
    }
  }
  return false;
}

void input_screensize() {
  if (detect_screensize()) {
    return;
  }

  bout.nl();
  bout << "|#9How wide is your screen : ";
  const auto x = bin.input_number(a()->user()->screen_width(), 40, 160, true);
  bout << "|#9How tall is your screen : ";
  const auto y = bin.input_number(a()->user()->screen_lines(), 20, 100, true);
  a()->user()->screen_width(x);
  a()->user()->screen_lines(y);
  a()->sess().num_screen_lines(y);
}

bool CheckPasswordComplexity(User*, std::string& password) {
  if (password.length() < 3) {
    // TODO - the min length should be in wwiv.ini
    return false;
  }
  return true;
}

void input_pw(User* pUser) {
  std::string password;
  bool ok = true;
  do {
    ok = true;
    password.clear();
    bout.nl();
    password = bin.input_password("|#3Please enter a new password, 3-8 chars.\r\n", 8);

    if (!CheckPasswordComplexity(pUser, password)) {
      ok = false;
      bout.nl(2);
      bout << "Invalid password.  Try again.\r\n\n";
    }
  } while (!ok && !a()->sess().hangup());

  if (ok) {
    pUser->password(password);
  } else {
    bout << "Password not changed.\r\n";
  }
}

void input_ansistat() {
  a()->user()->clear_flag(User::flag_ansi);
  a()->user()->clear_flag(User::status_color);
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
  if (bin.noyes()) {
    a()->user()->set_flag(User::flag_ansi);
    bout.nl();
    bout << "|#5Do you want color? ";
    if (bin.noyes()) {
      a()->user()->set_flag(User::status_color);
      a()->user()->set_flag(User::extraColor);
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
        if ((a()->user()->bwcolor(i) & 0x70) == 0) {
          a()->user()->bwcolor(i, (a()->user()->bwcolor(i) & 0x88) | c);
        } else {
          a()->user()->bwcolor(i, (a()->user()->bwcolor(i) & 0x88) | c2);
        }
      }
    }
  }
}

// Inserts a record into NAMES.LST
static void InsertSmallRecord(StatusMgr& sm, Names& names, int user_number, const char* name) {
  sm.Run([&](Status& s) {
    names.Add(name, user_number);
    names.Save();
    s.increment_num_users();
    s.increment_filechanged(Status::file_change_names);
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

  int nNewUserNumber = static_cast<int>((userFile.length() / a()->config()->userrec_length()) - 1);
  userFile.Seek(a()->config()->userrec_length(), File::Whence::begin);
  int user_number = 1;

  if (nNewUserNumber == a()->status_manager()->user_count()) {
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
        userFile.Seek(static_cast<File::size_type>(user_number * a()->config()->userrec_length()),
                      File::Whence::begin);
        nNewUserNumber =
            static_cast<int>((userFile.length() / a()->config()->userrec_length()) - 1);
      }
      User tu;
      userFile.Read(&tu.data, a()->config()->userrec_length());

      if (tu.deleted() && tu.sl() != 255) {
        userFile.Seek(static_cast<File::size_type>(user_number * a()->config()->userrec_length()),
                      File::Whence::begin);
        userFile.Write(&pUser->data, a()->config()->userrec_length());
        userFile.Close();
        write_qscn(user_number, qscn, false);
        InsertSmallRecord(*a()->status_manager(), *a()->names(), user_number, pUser->GetName());
        return user_number;
      }
      user_number++;
    }
  }

  if (user_number <= a()->config()->max_users()) {
    userFile.Seek(static_cast<File::size_type>(user_number * a()->config()->userrec_length()),
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
  a()->sess().ResetQScanPointers(*a()->config());

  auto* u = a()->user();
  a()->reset_effective_sl();

  const auto ok =
      User::CreateNewUserRecord(u, a()->config()->newuser_sl(), a()->config()->newuser_dsl(),
                                a()->config()->newuser_restrict(), a()->config()->newuser_gold(),
                                a()->newuser_colors, a()->newuser_bwcolors);
  u->CreateRandomPassword();
  return ok;
}

// returns true if the user is allow to create a new user account
// on here, if this function returns false, a sufficient error
// message has already been displayed to the user.
bool CanCreateNewUserAccountHere() {
  if (a()->status_manager()->user_count() >= a()->config()->max_users()) {
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

  if (!a()->config()->newuser_password().empty() && a()->sess().incom()) {
    bout.nl(2);
    bool ok = false;
    int nPasswordAttempt = 0;
    do {
      auto password = bin.input_password("New User Password: ", 20);
      if (password == a()->config()->newuser_password()) {
        ok = true;
      } else {
        sysoplog() << "Wrong newuser password: " << password;
      }
    } while (!ok && !a()->sess().hangup() && (nPasswordAttempt++ < 4));
    if (!ok) {
      return false;
    }
  }
  return true;
}

void DoNewUserASV() {
  IniFile ini(FilePath(a()->bbspath(), WWIV_INI),
              {StrCat("WWIV-", a()->sess().instance_number()), INI_TAG});
  if (!ini.IsOpen()) {
    return;
  }
  const auto asv_num = ini.value<int>("SIMPLE_ASV_NUM", 0);
  if (a()->HasConfigFlag(OP_FLAGS_SIMPLE_ASV) && a()->asv.sl > a()->config()->newuser_sl() &&
      a()->asv.sl < 90) {
    bout.nl();
    bout << "|#5Are you currently a WWIV SysOp? ";
    if (bin.yesno()) {
      bout.nl();
      bout << "|#5Please enter your BBS name and number.\r\n";
      const auto note = bin.input_text(60);
      a()->user()->note(note);
      a()->user()->sl(a()->asv.sl);
      a()->user()->dsl(a()->asv.dsl);
      a()->user()->ar_int(a()->asv.ar);
      a()->user()->dar_int(a()->asv.dar);
      a()->user()->exempt(a()->asv.exempt);
      a()->user()->restriction(a()->asv.restrict);
      bout.nl();
      bout.printfile(ASV_NOEXT);
      bout.nl();
      bout.pausescr();
    }
  }
}

static void add_phone_number(int usernum, const std::string& phone) {
  if (phone.find("000-") != std::string::npos) {
    return;
  }

  PhoneNumbers pn(*a()->config());
  if (!pn.IsInitialized()) {
    return;
  }

  pn.insert(usernum, phone);
}

void WriteNewUserInfoToSysopLog() {
  const auto* u = a()->user();
  sysoplog() << "** New User Information **";
  sysoplog() << fmt::sprintf("-> %s #%ld (%s)", u->name(), a()->sess().user_num(), u->real_name_or_empty());
  if (a()->config()->newuser_config().use_address_street != newuser_item_type_t::unused) {
    sysoplog() << "-> " << u->street();
  }
  if (a()->config()->newuser_config().use_address_city_state != newuser_item_type_t::unused) {
    sysoplog() << "-> " << u->city() << ", " << u->state() << " " << u->zip_code() << "  ("
               << u->country() << " )";
  }
  if (a()->config()->newuser_config().use_voice_phone != newuser_item_type_t::unused) {
    sysoplog() << fmt::format("-> {} (Voice)", u->voice_phone());
    if (!u->voice_phone().empty()) {
      add_phone_number(a()->sess().user_num(), u->voice_phone());
    }
  }
  if (a()->config()->newuser_config().use_data_phone != newuser_item_type_t::unused) {
    sysoplog() << fmt::format("-> {} (Data)", u->data_phone());
    if (!u->data_phone().empty()) {
      add_phone_number(a()->sess().user_num(), u->data_phone());
    }
  }
  if (a()->config()->newuser_config().use_birthday != newuser_item_type_t::unused) {
    sysoplog() << fmt::format("-> {} ({} yr old {})", u->birthday_mmddyy(), u->age(), u->gender());
  }
  if (a()->config()->newuser_config().use_computer_type != newuser_item_type_t::unused) {
    sysoplog() << fmt::format("-> Using a {} Computer", ctypes(u->computer_type()));
  }
  if (u->wwiv_regnum()) {
    sysoplog() << fmt::sprintf("-> WWIV Registration # %ld", u->wwiv_regnum());
  }
  sysoplog() << "********";

}

void VerifyNewUserPassword() {
  bool ok = false;
  do {
    bout.nl(2);
    bout << "|#9Your User Number: |#2" << a()->sess().user_num() << wwiv::endl;
    bout << "|#9Your Password:    |#2" << a()->user()->password() << wwiv::endl;
    bout.nl(1);
    bout << "|#9Please write down this information, and enter your password for verification.\r\n";
    bout << "|#9You will need to know this password in order to change it to something else.\r\n\n";
    const auto password = bin.input_password(bout.lang().value("PW_PROMPT"), 8);
    if (password == a()->user()->password()) {
      ok = true;
    }
  } while (!ok && !a()->sess().hangup());
}

void SendNewUserFeedbackIfRequired() {
  if (!a()->sess().incom()) {
    return;
  }

  if (a()->config()->sysconfig_flags() & sysconfig_no_newuser_feedback) {
    // If NEWUSER_FEEDBACK=N don't attempt to send newuser feedback.
    return;
  }

  bout.printfile(FEEDBACK_NOEXT);
  feedback(true);
  if (a()->HasConfigFlag(OP_FLAGS_FORCE_NEWUSER_FEEDBACK)) {
    if (!a()->user()->email_sent() && !a()->user()->feedback_sent()) {
      bout.printfile(NOFBACK_NOEXT);
      a()->users()->delete_user(a()->sess().user_num());
      a()->Hangup();
    }
  }
}

void ExecNewUserCommand() {
  if (!a()->sess().hangup() && !a()->newuser_cmd.empty()) {
    const auto commandLine = stuff_in(a()->newuser_cmd, create_chain_file(), "", "", "", "");

    // Log what is happening here.
    sysoplog(false) << "Executing New User Event: ";
    sysoplog() << commandLine;

    a()->WriteCurrentUser();
    ExecuteExternalProgram(commandLine, a()->spawn_option(SPAWNOPT_NEWUSER));
    a()->ReadCurrentUser();
  }
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

  auto ok = true;
  auto found = false;

  const auto zipcity_dir = FilePath(a()->config()->datadir(), ZIPCITY_DIR);
  const auto fn = fmt::format("zip{}.dat", zipcode.front());

  TextFile zip_file(FilePath(zipcity_dir, fn), "r");
  if (!zip_file.IsOpen()) {
    ok = false;
    if (mode != 2) {
      bout << "\r\n|#6" << FilePath(zipcity_dir, fn).string() << " not found\r\n";
    }
  } else {
    char zip_buf[81];
    while ((zip_file.ReadLine(zip_buf, 80)) && !found && !a()->sess().hangup()) {
      single_space(zip_buf);
      if (strncmp(zip_buf, zipcode.c_str(), 5) == 0) {
        found = true;
        auto ss = strtok(zip_buf, " ");
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
      if (bin.yesno()) {
        a()->user()->city(city);
        a()->user()->state(state);
        a()->user()->country("USA");
      } else {
        bout.nl();
        bout << "|#6My records show differently.\r\n\n";
        ok = false;
      }
    } else {
      a()->user()->city(city);
      a()->user()->state(state);
      a()->user()->country("USA");
    }
  }
  return ok;
}


void noabort(const char* file_name) {
  bool oic = false;

  if (a()->sess().using_modem()) {
    oic = a()->sess().incom();
    a()->sess().incom(false);
    bout.dump();
  }
  bout.printfile(file_name);
  if (a()->sess().using_modem()) {
    bout.dump();
    a()->sess().incom(oic);
  }
}

static void cln_nu() {
  bout.RestorePosition();
  bout.Color(0);
  bout.clreol();
}

/////////////////////////////////////////////////////////////////////////////

enum class NewUserItemResult { success, no_change, need_redraw };

class NewUserContext {
public:
  NewUserContext(char l, User& u, newuser_item_type_t i, bool f) : letter(l), user(u), item_type(i), first(f) {}
  char letter;
  User& user;
  newuser_item_type_t item_type;
  bool first{true};
};

typedef std::function<NewUserItemResult(NewUserContext&)> newuser_item_fn;
class NewUserItem {
public:
  NewUserItem(newuser_item_fn f, newuser_item_type_t s) : fn(std::move(f)), state(s) {}
  newuser_item_fn fn;
  newuser_item_type_t state;
};

NewUserItemResult DoNameOrHandle(NewUserContext& c) {
  bout << "|#1" << c.letter << "|#9) Name (alias or real)    : ";
  bout.SavePosition();
  while (!a()->sess().hangup() && c.first) {
    bout.SavePosition();
    const auto temp_name = bin.input_upper(c.user.name(), 30);
    if (check_name(temp_name)) {
      c.user.set_name(temp_name);
      break;
    }
    cln_nu();
    BackPrint("I'm sorry, you can't use that name.", 6, 20, 1000);
  }
  cln_nu();
  bout << "|#2" << c.user.name();
  bout.nl();
  return NewUserItemResult::success;
}

NewUserItemResult DoRealName(NewUserContext& c) {
  bout << "|#1" << c.letter << "|#9) Real Name               : ";
  bout.SavePosition();
  while (!a()->sess().hangup() && c.first) {
    bout.SavePosition();
    const auto temp_name = bin.input_proper(c.user.real_name_or_empty(), 30);
    if (!temp_name.empty() || c.item_type != newuser_item_type_t::required) {
      c.user.real_name(temp_name);
      break;
    }
    cln_nu();
    BackPrint("I'm sorry, you can't use that name.", 6, 20, 1000);
  }
  cln_nu();
  bout << "|#2" << c.user.real_name();
  bout.nl();
  return NewUserItemResult::success;
}

NewUserItemResult DoBirthDay(NewUserContext& c) {
  bout << "|#1" << c.letter << "|#9) Birth Date (MM/DD/YYYY) : ";
  bout.SavePosition();
  if (c.user.age() == 0 && c.first) {
    bool ok = false;
    bout.SavePosition();
    do {
      ok = false;
      cln_nu();
      auto s = bin.input_date_mmddyyyy("");
      if (s.size() == 10) {
        auto m = to_number<int>(s.substr(0, 2));
        auto d = to_number<int>(s.substr(3, 2));
        auto y = to_number<int>(s.substr(6, 4));
        ok = true;
        SystemClock clock{};
        if ((((m == 2) || (m == 9) || (m == 4) || (m == 6) || (m == 11)) && (d >= 31)) ||
            ((m == 2) && (((!isleap(y)) && (d == 29)) || (d == 30))) ||
            (years_old(m, d, y, clock) < 5) || (d > 31) || ((m == 0) || (y == 0) || (d == 0))) {
          ok = false;
        }
        if (m > 12) {
          ok = false;
        }
        if (ok) {
          c.user.birthday_mdy(m, d, y);
          cln_nu();
          auto dt = c.user.birthday_dt().to_string("%B %d, %Y");
          bout.format("|#2{} ({} years old)\r\n", dt, c.user.age());
          return NewUserItemResult::success;
        }
      }
      cln_nu();
      BackPrint("Invalid Birthdate.", 6, 20, 1000);
      if (c.item_type != newuser_item_type_t::required) {
        ok = true;
      }
    } while (!ok && !a()->sess().hangup());
  } 
  auto dt = c.user.birthday_dt().to_string("%B %d, %Y");
  bout.format("|#2{} ({} years old)\r\n", dt, c.user.age());
  return NewUserItemResult::no_change;
}

// TODO(rushfan): This needs updating, maybe add option to quit, system defined items?
NewUserItemResult DoGender(NewUserContext& c) {
  bout << "|#1" << c.letter << "|#9) Sex (Gender)            : ";
  bout.SavePosition();
  if (c.user.gender() != 'M' && c.user.gender() != 'F') {
    bout.mpl(1);
    c.user.gender(onek_ncr("MF"));
  }
  cln_nu();
  bout << "|#2" << (c.user.gender() == 'M' ? "Male" : "Female") << wwiv::endl;
  return NewUserItemResult::success;
}

NewUserItemResult DoCountry(NewUserContext& c) {
  bout << "|#1" << c.letter << "|#9) Country                 : ";
  bout.SavePosition();
  if (c.first) {
    auto country = bin.input_upper(c.user.country(), 3);
    if (!country.empty()) {
      c.user.country(country);
    } else {
      c.user.country("USA");
    }
  }
  cln_nu();
  bout << "|#2" << c.user.country() << wwiv::endl;
  return NewUserItemResult::success;
}

NewUserItemResult DoZipCode(NewUserContext& c) {
  bout << "|#1" << c.letter << "|#9) ZIP or Postal Code      : ";
  bout.SavePosition();
  if (c.user.zip_code().empty() && c.first) {
    bool ok = false;
    do {
      if (iequals(c.user.country(), "USA")) {
        auto zip = bin.input_upper(c.user.zip_code(), 5);
        check_zip(zip, 2);
        c.user.zip_code(zip);
      } else {
        auto zip = bin.input_upper(c.user.zip_code(), 7);
        c.user.zip_code(zip);
      }
      cln_nu();
      if (!c.user.zip_code().empty() || c.item_type != newuser_item_type_t::required) {
        ok = true;
      }
    } while (!ok && !a()->sess().hangup());
  }
  cln_nu();
  bout << "|#2" << c.user.zip_code() << wwiv::endl;
  return NewUserItemResult::success;
}

NewUserItemResult DoStreet(NewUserContext& c) {
  bout << "|#1" << c.letter << "|#9) Street Address          : ";
  bout.SavePosition();
  while (!a()->sess().hangup() && c.first) {
    bout.SavePosition();
    const auto st = bin.input_proper(c.user.street(), 30);
    if (!st.empty() || c.item_type != newuser_item_type_t::required) {
      c.user.street(st);
      break;
    }
    cln_nu();
    BackPrint("I'm sorry, you must enter your street address.", 6, 20, 1000);
  }
  cln_nu();
  bout << "|#2" << c.user.street();
  bout.nl();
  return NewUserItemResult::success;
}

NewUserItemResult DoCityState(NewUserContext& c) {
  bout << "|#1" << c.letter << "|#9) City                    : ";
  bout.SavePosition();
  if (c.user.city().empty() && c.first) {
    bool ok = false;
    do {
      auto city = bin.input_proper(c.user.city(), 30);
      if (!city.empty() || c.item_type != newuser_item_type_t::required) {
        c.user.city(city);
        ok = true;
      }
    } while (!ok && !a()->sess().hangup());
    c.user.city(properize(c.user.city()));
  }
  cln_nu();
  bout << "|#2" << c.user.city() << wwiv::endl;
  return NewUserItemResult::success;
}

NewUserItemResult DoState(NewUserContext& c) {

  bout << "|#1" << c.letter << "|#9) State                   : ";
  bout.SavePosition();
  if (c.user.state().empty() && c.first) {
    bool ok = false;
    do {
      ok = false;
      if (auto state = bin.input_upper(c.user.state(), 2); !state.empty() || c.item_type != newuser_item_type_t::required) {
         c.user.state(state);
         ok = true;
        }
        bout.RestorePosition();
      } while (!ok && !a()->sess().hangup());
    }

  
  cln_nu();
  bout << "|#2" << c.user.state() << wwiv::endl;
  return NewUserItemResult::success;
}

NewUserItemResult DoEmailAddress(NewUserContext& c) {
  bout << "|#1" << c.letter << "|#9) Internet Mail Address   : ";
  bout.SavePosition();
  bool done = false;
  while (!done && c.first) {
    c.user.email_address(bin.input_text(c.user.email_address(), 44));
    if (!check_inet_addr(c.user.email_address())) {
      cln_nu();
      BackPrint("Invalid address!", 6, 20, 1000);
      c.user.email_address("");
    }
    if (c.item_type != newuser_item_type_t::required || !c.user.email_address().empty()) {
      done = true;
    }
  }
  cln_nu();
  bout << "|#2" << c.user.email_address() << wwiv::endl;
  return NewUserItemResult::success;
}

NewUserItemResult DoVoicePhone(NewUserContext& c) {
  bout << "|#1" << c.letter << "|#9) Voice Phone             : ";
  bout.SavePosition();
  while (!a()->sess().hangup() && c.first) {
    bout.SavePosition();
    const auto phoneNumber = bin.input_phonenumber(c.user.voice_phone(), 12);
    if (valid_phone(phoneNumber)) {
      c.user.voice_phone(phoneNumber);
      break;
    }
    if (!c.user.voice_phone().empty()  || c.item_type != newuser_item_type_t::required) {
      break;
    }
    cln_nu();
    BackPrint("I'm sorry, you can't use that phone number.", 6, 20, 1000);
  }
  cln_nu();
  bout << "|#2" << c.user.voice_phone();
  bout.nl();
  return NewUserItemResult::success;
}

NewUserItemResult DoDataPhone(NewUserContext& c) {
  bout << "|#1" << c.letter << "|#9) Data Phone              : ";
  bout.SavePosition();
  while (!a()->sess().hangup() && c.first) {
    bout.SavePosition();
    const auto phoneNumber = bin.input_phonenumber(c.user.data_phone(), 12);
    if (valid_phone(phoneNumber)) {
      c.user.data_phone(phoneNumber);
      break;
    }
    if (!c.user.data_phone().empty()  || c.item_type != newuser_item_type_t::required) {
      break;
    }
    cln_nu();
    BackPrint("I'm sorry, you can't use that phone number.", 6, 20, 1000);
  }
  cln_nu();
  bout << "|#2" << c.user.data_phone();
  bout.nl();
  return NewUserItemResult::success;
}

NewUserItemResult DoCallsign(NewUserContext& c) {
  bout << "|#1" << c.letter << "|#9) Callsign                : ";
  bout.SavePosition();
  while (!a()->sess().hangup() && c.first) {
    bout.SavePosition();
    const auto s = bin.input_upper(c.user.callsign(), 6);
    if (!s.empty() || c.item_type != newuser_item_type_t::required) {
      c.user.callsign(s);
      break;
    }
    cln_nu();
  }
  cln_nu();
  bout << "|#2" << c.user.callsign() << wwiv::endl;
  return NewUserItemResult::success;
}

NewUserItemResult DoComputerType(NewUserContext& c) {
  int ct = -1;

  std::string computer_type = c.user.computer_type() >= 0 ? ctypes(c.user.computer_type()) : "Unknown";
  if (c.user.computer_type() >= 0 || !c.first) {
    bout << "|#1" << c.letter << "|#9) Computer Type           : |#2" << computer_type << wwiv::endl;
    return NewUserItemResult::no_change;
  }
  bool ok = true;
  do {
    bout.cls();
    bout.litebar("Known computer types: ");
    int i = 0;
    for (i = 1; !ctypes(i).empty(); i++) {
      bout << i << ". " << ctypes(i) << wwiv::endl;
    }
    bout.nl();
    bout << "|#9Enter your computer type, or the closest to it (ie, Compaq -> IBM).\r\n";
    bout << "|#9: ";
    ct = bin.input_number(1, 1, i, false);
    ok = true;
    if (ct < 1 || ct > i) {
      ok = false;
    }
  } while (!ok && !a()->sess().hangup());

  c.user.computer_type(ct);
  return NewUserItemResult::need_redraw;
}

void NewUserDataEntry(const newuser_config_t& nc) {
  auto& u = *a()->user();

  bout.newline = false;
  const auto saved_topdata = bout.localIO()->topdata();
  bout.localIO()->topdata(wwiv::local::io::LocalIO::topdata_t::none);
  a()->UpdateTopScreen();
  auto letter = 'A';
  std::map<char, NewUserItem> nu_items;
  std::map<char, std::function<void(User&)>> clr_items;

  nu_items.try_emplace(letter, DoNameOrHandle, newuser_item_type_t::required);
  ++letter;
  if (nc.use_real_name != newuser_item_type_t::unused) {
    nu_items.try_emplace(letter, DoRealName, nc.use_real_name);
    ++letter;
  }
  if (nc.use_birthday != newuser_item_type_t::unused) {
    nu_items.try_emplace(letter, DoBirthDay, nc.use_birthday);
    clr_items.try_emplace(letter, [](User& u) { u.birthday_mdy(0, 0, 0); });
    ++letter;
  }
  if (nc.use_voice_phone != newuser_item_type_t::unused) {
    nu_items.try_emplace(letter, DoVoicePhone, nc.use_voice_phone);
    ++letter;
  }
  if (nc.use_data_phone != newuser_item_type_t::unused) {
    nu_items.try_emplace(letter, DoDataPhone, nc.use_data_phone);
    ++letter;
  }
  if (nc.use_callsign != newuser_item_type_t::unused) {
    nu_items.try_emplace(letter, DoCallsign, nc.use_callsign);
    ++letter;
  }
  if (nc.use_gender != newuser_item_type_t::unused) {
    nu_items.try_emplace(letter, DoGender, nc.use_gender);
    clr_items.try_emplace(letter, [](User& u) { u.gender('N'); });
    ++letter;
  }
  if (nc.use_address_country != newuser_item_type_t::unused) {
    nu_items.try_emplace(letter, DoCountry, nc.use_address_country);
    clr_items.try_emplace(letter, [](User& u) { u.country(""); });
    ++letter;
  }
  if (nc.use_address_zipcode != newuser_item_type_t::unused) {
    nu_items.try_emplace(letter, DoZipCode, nc.use_address_zipcode);
    clr_items.try_emplace(letter, [](User& u) { u.zip_code(""); });
    ++letter;
  }
  if (nc.use_address_street != newuser_item_type_t::unused) {
    nu_items.try_emplace(letter, DoStreet, nc.use_address_street);
    ++letter;
  }
  if (nc.use_address_city_state != newuser_item_type_t::unused) {
    nu_items.try_emplace(letter, DoCityState, nc.use_address_city_state);
    clr_items.try_emplace(letter, [](User& u) { u.city(""); });
    ++letter;
  }
  if (nc.use_address_city_state != newuser_item_type_t::unused) {
    nu_items.try_emplace(letter, DoState, nc.use_address_city_state);
    clr_items.try_emplace(letter, [](User& u) { u.state(""); });
    ++letter;
  }
  if (nc.use_email_address != newuser_item_type_t::unused) {
    nu_items.try_emplace(letter, DoEmailAddress, nc.use_email_address);
    ++letter;
  }
  if (nc.use_computer_type != newuser_item_type_t::unused) {
    u.computer_type(-1);
    nu_items.try_emplace(letter, DoComputerType, nc.use_computer_type);
    clr_items.try_emplace(letter, [](User& u) { u.computer_type(-1); });
    ++letter;
  }
  std::set<char> letters;
  do {
    bout.cls();
    bout.litebar(StrCat(a()->config()->system_name(), " New User Registration"));
    auto need_redraw = false;
    for (auto& kv : nu_items) {
      const auto is_first = letters.find(kv.first) == std::end(letters);
      NewUserContext context(kv.first, u, kv.second.state, is_first);
      letters.insert(kv.first);
      if (const auto result = kv.second.fn(context); result == NewUserItemResult::need_redraw) {
        need_redraw = true;
        break;
      }
    }
    if (need_redraw) {
      continue;
    }
    bout.nl();
    bout << "|#5Item to change or [|#2Q|#5] to Quit : |#0";
    std::string allowed("Q");
    for (auto cc = 'A'; cc <= letter; cc++) {
      allowed.push_back(cc);
    }
    auto ch = onek(allowed);
    if (ch == 'Q') {
      break;
    }
    if (auto c = clr_items.find(ch); c != std::end(clr_items) ) {
      clr_items.at(ch)(u);
    }
    // Reset 'first' so it runs again vs. displays.
    letters.erase(ch);
  } while (!a()->sess().hangup());

  if (u.ansi()) {
    u.set_flag(User::status_color);
    u.set_flag(User::extraColor);
    // Enable the internal FSED and full screen reader for the sysop.
    u.default_editor(0xff);
    u.set_flag(User::fullScreenReader);
  }
  bout.localIO()->topdata(saved_topdata);
  a()->UpdateTopScreen();
  bout.newline = true;
}

void newuser() {
  sysoplog(false);
  const auto t = times();
  const auto f = fulldate();
  sysoplog(false) << fmt::sprintf("*** NEW USER %s   %s    %s (%ld)", f, t, a()->GetCurrentSpeed(),
                                  a()->sess().instance_number());

  LOG(INFO) << "New User Attempt from IP Address: " << bout.remoteIO()->remote_info().address;
  a()->sess().num_screen_lines(25);

  if (!CreateNewUserRecord()) {
    return;
  }

  if (!CanCreateNewUserAccountHere() || a()->sess().hangup()) {
    a()->Hangup();
    return;
  }

  if (check_ansi() == 1) {
    a()->user()->set_flag(User::flag_ansi);
    a()->user()->set_flag(User::status_color);
    a()->user()->set_flag(User::extraColor);
    a()->user()->set_flag(User::fullScreenReader);
    // 0xff is the internal FSED.
    a()->user()->default_editor(0xff);
  }
  bout.printfile(SYSTEM_NOEXT);
  bout.nl();
  bout.pausescr();
  bout.printfile(NEWUSER_NOEXT);
  bout.nl();
  bout.pausescr();
  bout.cls();
  bout << "|#5Create a new user account on |#2" << a()->config()->system_name() << "|#5? ";
  if (!bin.noyes()) {
    bout << "|#6Sorry the system does not meet your needs!\r\n";
    a()->Hangup();
    return;
  }

  input_screensize();
  NewUserDataEntry(a()->config()->newuser_config());

  bout.nl(4);
  bout << "Random password: " << a()->user()->password() << wwiv::endl << wwiv::endl;
  bout << "|#5Do you want a different password (Y/N)? ";
  if (bin.yesno()) {
    input_pw(a()->user());
  }

  DoNewUserASV();

  if (a()->sess().hangup()) {
    return;
  }
  bout.nl();
  bout << "Please wait...\r\n\n";
  const auto usernum = find_new_usernum(a()->user(), a()->sess().qsc);
  if (usernum <= 0) {
    bout.nl();
    bout << "|#6Error creating user account.\r\n\n";
    a()->Hangup();
    return;
  }
  if (usernum == 1) {
    // This is the #1 sysop record. Tell the sysop thank you and
    // update his user record with: s255/d255/r0
    ssm(1) << "Thank you for installing WWIV! - The WWIV Development Team.";
    auto user = a()->user();
    user->sl(255);
    user->dsl(255);
    user->restriction(0);
  }
  // Set the user number on the session and also on this User instance
  // since it hasn't been loaded nor saved from the UserManager yet.
  a()->sess().user_num(usernum);
  a()->user()->user_number_ = usernum;

  WriteNewUserInfoToSysopLog();
  ssm(1) << "You have a new user: " << a()->user()->name() << " #" << a()->sess().user_num();

  LOG(INFO) << "New User Created: '" << a()->user()->name() << "' "
            << "IP Address: " << bout.remoteIO()->remote_info().address;

  a()->UpdateTopScreen();
  VerifyNewUserPassword();
  SendNewUserFeedbackIfRequired();
  ExecNewUserCommand();
  a()->reset_effective_sl();
  changedsl();
  new_mail();
}

void new_mail() {
  auto file = FilePath(a()->config()->gfilesdir(),
                           (a()->user()->sl() > a()->config()->newuser_sl()) ? NEWSYSOP_MSG
                                                                                : NEWMAIL_MSG);

  if (!File::Exists(file)) {
    return;
  }
  const auto save_ed = a()->user()->default_editor();
  a()->user()->default_editor(0);
  LoadFileIntoWorkspace(a()->context(), file, true, true);
  use_workspace = true;

  MessageEditorData data(a()->user()->name_and_number());
  data.title = StrCat("Welcome to ", a()->config()->system_name(), "!");
  data.need_title = true;
  data.anonymous_flag = 0;
  data.aux = "email";
  data.fsed_flags = FsedFlags::NOFSED;
  data.to_name = a()->user()->name_and_number();
  data.msged_flags = MSGED_FLAG_NONE;
  data.silent_mode = true;
  messagerec msg;
  msg.storage_type = 2;
  if (inmsg(data)) {
    savefile(data.text, &msg, data.aux);

    EmailData email(data);
    email.msg = &msg;
    email.anony = 0;
    email.set_user_number(a()->sess().user_num());
    email.system_number = 0;
    email.an = true;
    email.from_user = 1;
    email.from_system = 0;
    email.forwarded_code = 1;
    email.from_network_number = 0;
    email.silent_mode = true;
    sendout_email(email);
  }
  a()->user()->email_waiting(a()->user()->email_waiting() + 1);
  a()->user()->default_editor(save_ed);
}
