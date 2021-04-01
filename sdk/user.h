/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services            */
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

#ifndef INCLUDED_SDK_USER_H
#define INCLUDED_SDK_USER_H

#include "core/datetime.h"
#include "core/ip_address.h"
#include "core/strings.h"
#include "sdk/vardec.h"
#include "sdk/net/net.h"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

//
// ListPlus options from lp_options.
//
static constexpr uint32_t cfl_fname = 0x00000001;
static constexpr uint32_t cfl_extension = 0x00000002;
static constexpr uint32_t cfl_dloads = 0x00000004;
static constexpr uint32_t cfl_kbytes = 0x00000008;
static constexpr uint32_t cfl_date_uploaded = 0x00000010;
static constexpr uint32_t cfl_file_points = 0x00000020;
static constexpr uint32_t cfl_days_old = 0x00000040;
static constexpr uint32_t cfl_upby = 0x00000080;
static constexpr uint32_t unused_cfl_times_a_day_dloaded = 0x00000100;
static constexpr uint32_t unused_cfl_days_between_dloads = 0x00000200;
static constexpr uint32_t cfl_description = 0x00000400;
static constexpr uint32_t cfl_header = 0x80000000;

namespace wwiv::sdk {
struct validation_config_t;

/**
 * User Class - Represents a User record
 */
class User final {
 public:
  //
  // Constructor and Destructor
  //
  User();
  ~User() = default;

  User(const User& w);
  User(User&& u) noexcept;
  User(const userrec& rhs, int user_number);
  User& operator=(const User& rhs);
  User& operator=(User&& rhs) noexcept;

  // Constants

  // USERREC.inact
  static constexpr int userDeleted                = 0x01;
  static constexpr int userInactive               = 0x02;

  // USERREC.exempt
  static constexpr int exemptRatio                = 0x01;
  static constexpr int exemptTime                 = 0x02;
  static constexpr int exemptPost                 = 0x04;
  static constexpr int exemptAll                  = 0x08;
  static constexpr int exemptAutoDelete           = 0x10;

  // USERREC.restrict
  static constexpr int restrictLogon              = 0x0001;
  static constexpr int restrictChat               = 0x0002;
  static constexpr int restrictValidate           = 0x0004;
  static constexpr int restrictAutomessage        = 0x0008;
  static constexpr int restrictAnony              = 0x0010;
  static constexpr int restrictPost               = 0x0020;
  static constexpr int restrictEmail              = 0x0040;
  static constexpr int restrictVote               = 0x0080;
  static constexpr int restrictMultiNodeChat      = 0x0100;
  static constexpr int restrictNet                = 0x0200;
  static constexpr int restrictUpload             = 0x0400;

  // USERREC.sysstatus
  static constexpr int flag_ansi                  = 0x00000001;
  static constexpr int status_color               = 0x00000002;
  static constexpr int flag_music                 = 0x00000004;
  static constexpr int pauseOnPage                = 0x00000008;
  static constexpr int flag_expert                = 0x00000010;
  static constexpr int SMW                        = 0x00000020;
  static constexpr int fullScreen                 = 0x00000040;
  static constexpr int nscanFileSystem            = 0x00000080;
  static constexpr int extraColor                 = 0x00000100;
  static constexpr int clearScreen                = 0x00000200;
  static constexpr int msg_show_controlcodes      = 0x00000400;
  static constexpr int unused_noTag               = 0x00000800;
  static constexpr int conference                 = 0x00001000;
  static constexpr int noChat                     = 0x00002000;
  static constexpr int noMsgs                     = 0x00004000;
  static constexpr int fullScreenReader           = 0x00008000;
  static constexpr int unused_listPlus            = 0x00010000;
  static constexpr int autoQuote                  = 0x00020000;
  static constexpr int twentyFourHourClock        = 0x00040000;
  static constexpr int msgPriority                = 0x00080000;  // not used?

  //
  // Data
  //
  struct userrec data{};
  int user_number_{-1};

  //
  // Member Functions
  //
  void FixUp();    // was function fix_user_rec
  void ZeroUserData();

  //
  // Accessor Functions
  //

  // USERREC.inact
  void set_inact(int flag) {
    data.inact |= flag;
  }
  void clear_inact(int flag) {
    data.inact &= ~flag;
  }

  [[nodiscard]] bool deleted() const {
    return (data.inact & userDeleted) != 0;
  }
  [[nodiscard]] bool inactive() const {
    return (data.inact & userInactive) != 0;
  }

  /**
   * Is this the guest user.
   */
  [[nodiscard]] bool guest_user() const;

  // USERREC.sysstatus
  void set_flag(int flag, bool on) {
    if (on) {
      data.sysstatus |= flag;
    } else {
      data.sysstatus &= ~flag;
    }
  }
  void set_flag(int flag) {
    data.sysstatus |= flag;
  }
  void toggle_flag(int flag) {
    data.sysstatus ^= flag;
  }
  void clear_flag(int flag) {
    data.sysstatus &= ~flag;
  }

  [[nodiscard]] bool has_flag(int flag) const {
    return (data.sysstatus & flag) != 0;
  }

  [[nodiscard]] long get_status() const {
    return static_cast<long>(data.sysstatus);
  }

  void SetStatus(long l) {
    data.sysstatus = static_cast<uint32_t>(l);
  }

  [[nodiscard]] bool ansi() const {
    return has_flag(flag_ansi);
  }
  [[nodiscard]] bool color() const { return has_flag(status_color); }

  [[nodiscard]] bool music() const {
    return has_flag(flag_music);
  }

  [[nodiscard]] bool pause() const {
    return has_flag(pauseOnPage);
  }

  [[nodiscard]] bool IsExpert() const {
    return has_flag(flag_expert);
  }

  [[nodiscard]] bool ssm() const {
    return has_flag(SMW);
  }

  [[nodiscard]] bool fullscreen() const {
    return has_flag(fullScreen);
  }
  [[nodiscard]] bool newscan_files() const {
    return has_flag(nscanFileSystem);
  }
  [[nodiscard]] bool extra_color() const {
    return has_flag(extraColor);
  }
  [[nodiscard]] bool clear_screen() const {
    return has_flag(clearScreen);
  }
  [[nodiscard]] bool use_conference() const {
    return has_flag(conference);
  }
  [[nodiscard]] bool nochat() const {
    return has_flag(noChat);
  }
  [[nodiscard]] bool ignore_msgs() const {
    return has_flag(noMsgs);
  }

  [[nodiscard]] bool auto_quote() const {
    return has_flag(autoQuote);
  }
  [[nodiscard]] bool twentyfour_clock() const {
    return has_flag(twentyFourHourClock);
  }

  [[nodiscard]] bool exempt_ratio() const {
    return HasExemptFlag(exemptRatio);
  }

  [[nodiscard]] bool exempt_time() const {
    return HasExemptFlag(exemptTime);
  }

  [[nodiscard]] bool exempt_post() const {
    return HasExemptFlag(exemptPost);
  }

  [[nodiscard]] bool exempt_all() const {
    return HasExemptFlag(exemptAll);
  }

  [[nodiscard]] bool exempt_auto_delete() const {
    return HasExemptFlag(exemptAutoDelete);
  }

  // USERREC.restrict
  void set_restrict(int flag) noexcept {
    data.restrict |= flag;
  }

  void toggle_restrict(int flag) noexcept {
    data.restrict ^= flag;
  }

  void clear_restrict(int flag) noexcept {
    data.restrict &= ~flag;
  }

  [[nodiscard]] bool has_restrict(int flag) const noexcept {
    return (data.restrict & flag) != 0;
  }

  [[nodiscard]] uint16_t restriction() const {
    return data.restrict;
  }

  void restriction(uint16_t n) {
    data.restrict = n;
  }

  [[nodiscard]] bool restrict_logon() const {
    return has_restrict(restrictLogon);
  }

  [[nodiscard]] bool restrict_chat() const {
    return has_restrict(restrictChat);
  }

  [[nodiscard]] bool restrict_validate() const {
    return has_restrict(restrictValidate);
  }

  [[nodiscard]] bool restrict_automessage() const {
    return has_restrict(restrictAutomessage);
  }

  [[nodiscard]] bool restrict_anony() const {
    return has_restrict(restrictAnony);
  }
  [[nodiscard]] bool restrict_post() const {
    return has_restrict(restrictPost);
  }

  [[nodiscard]] bool restrict_email() const {
    return has_restrict(restrictEmail);
  }

  [[nodiscard]] bool restrict_vote() const {
    return has_restrict(restrictVote);
  }

  [[nodiscard]] bool restrict_iichat() const {
    return has_restrict(restrictMultiNodeChat);
  }

  [[nodiscard]] bool restrict_net() const {
    return has_restrict(restrictNet);
  }

  [[nodiscard]] bool restrict_upload() const {
    return has_restrict(restrictUpload);
  }

  void toggle_ar(int flag) {
    data.ar ^= flag;
  }

  [[nodiscard]] bool has_ar(int ar) const {
    if (ar == 0) {
      // Always have the empty ar
      return true;
    }
    return (data.ar & ar) != 0;
  }

  [[nodiscard]] int ar_int() const {
    return data.ar;
  }

  void ar_int(int n) {
    data.ar = static_cast<uint16_t>(n);
  }

  void toggle_dar(int flag) {
    data.dar ^= flag;
  }

  [[nodiscard]] bool has_dar(int flag) const {
    if (flag == 0) {
      // Always have the empty dar
      return true;
    }
    return (data.dar & flag) != 0;
  }

  [[nodiscard]] int dar_int() const {
    return data.dar;
  }

  void dar_int(int n) {
    data.dar = static_cast<uint16_t>(n);
  }

  [[nodiscard]] const char *GetName() const {
    return reinterpret_cast<const char*>(data.name);
  }

  [[nodiscard]] std::string name() const {
    return std::string(reinterpret_cast<const char*>(data.name));
  }

  /**
   * Returns the user's name and number formatted canonically like:
   * "Trader Jack #1"
   */
  [[nodiscard]] std::string name_and_number() const;

  /**
   * Returns the user's number. 
   * Note: This only works if the user record was loaded from the UserManager class,
   * otherwise will return -1.
   */
  [[nodiscard]] int usernum() const noexcept;

  void set_name(const std::string& s) {
    strcpy(reinterpret_cast<char*>(data.name), s.c_str());
  }

  /**
   * Returns the real name if exists, or handle if no real name exists.
   */
  [[nodiscard]] std::string real_name() const {
    const auto rn = std::string(reinterpret_cast<const char*>(data.realname));
    return rn.empty() ? name() : rn;
  }

  /**
   * Returns the real name if exists, or an empty string if none exists.
   */
  [[nodiscard]] std::string real_name_or_empty() const {
    return std::string(reinterpret_cast<const char*>(data.realname));
  }

  /**
   * Sets the real name for this user, use an empty string if none exists.
   */
  void real_name(const std::string& s) {
    strcpy(reinterpret_cast<char*>(data.realname), s.c_str());
  }

  [[nodiscard]] std::string callsign() const {
    return std::string(data.callsign);
  }

  void callsign(const std::string& s) {
    strcpy(data.callsign, s.c_str());
  }

  [[nodiscard]] std::string voice_phone() const {
    return data.phone;
  }
  void voice_phone(const std::string& s) {
    strings::to_char_array(data.phone, s);
  }

  [[nodiscard]] std::string data_phone() const {
    return data.dataphone;
  }
  void data_phone(const std::string& s) {
    strings::to_char_array(data.dataphone, s);
  }

  [[nodiscard]] std::string street() const {
    return data.street;
  }
  void street(const std::string& s) {
    strings::to_char_array(data.street, s);
  }
  [[nodiscard]] std::string city() const {
    return data.city;
  }
  void city(const std::string& s) {
    strings::to_char_array(data.city, s);
  }
  [[nodiscard]] std::string state() const {
    return data.state;
  }
  void state(const std::string& s) {
    strings::to_char_array(data.state, s);
  }

  [[nodiscard]] std::string country() const {
    return data.country;
  }
  void country(const std::string& s) {
    strings::to_char_array(data.country, s);
  }

  [[nodiscard]] std::string zip_code() const {
    return data.zipcode;
  }
  void zip_code(const std::string& s) {
    strings::to_char_array(data.zipcode, s);
  }

  [[nodiscard]] std::string password() const {
    return std::string(data.pw);
  }
  void password(const std::string& s) {
    strings::to_char_array(data.pw, s);
  }

  [[nodiscard]] std::string laston() const {
    return data.laston;
  }

  void laston(const std::string& s) {
    strings::to_char_array(data.laston, s);
  }

  [[nodiscard]] std::string firston() const {
    return data.firston;
  }
  
  void firston(const std::string& s) {
    strings::to_char_array(data.firston, s);
  }

  [[nodiscard]] std::string note() const {
    return reinterpret_cast<const char*>(data.note);
  }
  void note(const std::string& s) {
    strcpy(data.note, s.c_str());
  }

  [[nodiscard]] std::string macro(int line) const {
    return std::string(reinterpret_cast<const char*>(data.macros[line]));
  }
  void macro(int nLine, const char *s) {
    memset(&data.macros[ nLine ][0], 0, 80);
    strcpy(reinterpret_cast<char*>(data.macros[ nLine ]), s);
  }

  [[nodiscard]] char gender() const {
    if (data.sex == 'N') {
      // N means unknown.  NEWUSER sets it to N to prompt the
      // user again.
      return 'N';
    }
    return data.sex == 'F' ? 'F' : 'M';
  }
  void gender(const char c) {
    data.sex = static_cast<uint8_t>(c);
  }

  [[nodiscard]] std::string email_address() const {
    return data.email;
  }
  void email_address(const std::string& s) {
    strcpy(data.email, s.c_str());
  }

  [[nodiscard]] int age() const;

  [[nodiscard]] int8_t computer_type() const {
    return data.comp_type;
  }
  void computer_type(int n) {
    data.comp_type = static_cast<int8_t>(n);
  }

  [[nodiscard]] int default_protocol() const {
    return data.defprot;
  }
  void default_protocol(int n) {
    data.defprot = static_cast<uint8_t>(n);
  }

  [[nodiscard]] int default_editor() const {
    return data.defed;
  }
  void default_editor(int n) {
    data.defed = static_cast<uint8_t>(n);
  }

  [[nodiscard]] int screen_width() const {
    return data.screenchars == 0 ? 80 : data.screenchars;
  }
  void screen_width(int n) {
    data.screenchars = static_cast<uint8_t>(n);
  }

  [[nodiscard]] int screen_lines() const {
    return data.screenlines == 0 ? 25 : data.screenlines;
  }
  void screen_lines(int n) {
    data.screenlines = static_cast<uint8_t>(n);
  }

  [[nodiscard]] int GetNumExtended() const {
    return data.num_extended;
  }
  void SetNumExtended(int n) {
    data.num_extended = static_cast<uint8_t>(n);
  }

  [[nodiscard]] int optional_val() const {
    return data.optional_val;
  }
  void optional_val(int n) {
    data.optional_val = static_cast<uint8_t>(n);
  }

  [[nodiscard]] int sl() const {
    return data.sl;
  }
  void sl(int n) {
    data.sl = static_cast<uint8_t>(n);
  }

  [[nodiscard]] int dsl() const {
    return data.dsl;
  }
  void dsl(int n) {
    data.dsl = static_cast<uint8_t>(n);
  }

  [[nodiscard]] int exempt() const {
    return data.exempt;
  }
  void exempt(int n) {
    data.exempt = static_cast<uint8_t>(n);
  }

  [[nodiscard]] int raw_color(int n) const {
    if (n < 0 || n > 9) {
      return 7; // default color
    }
    return data.colors[n];
  }

  /**
   * Gets the color number n for this user.  Respects if ansi colors
   * is enabled or not.
   */
  [[nodiscard]] uint8_t color(int n) const { 
    if (n < 0 || n > 9) {
      return 7; // default color
    }
    return static_cast<uint8_t>(ansi() ? raw_color(n) : bwcolor(n));
  }

  [[nodiscard]] std::vector<uint8_t> colors() const { 
    std::vector<uint8_t> c;
    for (auto i = 0; i < 10; i++) {
      if (color()) {
        c.push_back(data.colors[i]);
      } else {
        c.push_back(data.bwcolors[i]);
      }
    }
    return c;
  }
  void color(int nColor, unsigned int n) {
    data.colors[nColor] = static_cast<uint8_t>(n);
  }

  [[nodiscard]] uint8_t bwcolor(int n) const {
    if (n < 0 || n > 9) {
      return 7; // default color
    }
    return data.bwcolors[n];
  }
  void bwcolor(int nColor, unsigned int n) {
    data.bwcolors[nColor] = static_cast<uint8_t>(n);
  }

  [[nodiscard]] int votes(int vote_num) const {
    return data.votes[vote_num];
  }
  void votes(int vote_num, int n) {
    data.votes[vote_num] = static_cast<uint8_t>(n);
  }

  [[nodiscard]] int illegal_logons() const {
    return data.illegal;
  }
  void illegal_logons(int n) {
    data.illegal = static_cast<uint8_t>(n);
  }
  void increment_illegal_logons() {
    if (data.illegal < std::numeric_limits<decltype(data.illegal)>::max()) {
      ++data.illegal;      
    }
  }

  [[nodiscard]] int email_waiting() const {
    return data.waiting;
  }
  void email_waiting(unsigned int n) {
    data.waiting = static_cast<uint8_t>(n);
  }

  [[nodiscard]] int ontoday() const {
    return data.ontoday;
  }

  /** 
   * Sets the birthday to month m (1-12), day d (1-31), 
   * year y (full 4 year date)
   */
  void birthday_mdy(int m, int d, int y);

  /** Gets the current user's birthday month (1-12) */
  [[nodiscard]] int birthday_month() const {
    return data.month;
  }

  /** Gets the current user's birthday day of the month (1-31) */
  [[nodiscard]] int birthday_mday() const {
    return data.day;
  }

  /** Gets the current user's birthday year (i.e. 1990) */
  [[nodiscard]] int birthday_year() const {
    return data.year + 1900;
  }

  /** Gets the current user's birthday as a DateTime */
  [[nodiscard]] core::DateTime birthday_dt() const;

  /** Gets the current user's birthday as a string in format "mm/dd/yy" */
  [[nodiscard]] std::string birthday_mmddyy() const;

  [[nodiscard]] int home_usernum() const {
    return data.homeuser;
  }

  void home_usernum(int n) {
    data.homeuser = static_cast<uint16_t>(n);
  }

  [[nodiscard]] uint16_t home_systemnum() const {
    return data.homesys;
  }
  void home_systemnum(uint16_t n) {
    data.homesys = n;
  }

  [[nodiscard]] uint16_t forward_usernum() const {
    return data.forwardusr;
  }
  void forward_usernum(uint16_t n) {
    data.forwardusr = n;
  }

  [[nodiscard]] uint16_t forward_systemnum() const {
    return data.forwardsys;
  }
  void forward_systemnum(uint16_t n) {
    data.forwardsys = n;
  }

  [[nodiscard]] int forward_netnum() const {
    return data.net_num;
  }
  void forward_netnum(int n) {
    data.net_num = static_cast<uint16_t>(n);
  }

  /** Gets a text description of the mailbox state */
  [[nodiscard]] std::string mailbox_state() const;

  [[nodiscard]] int messages_posted() const {
    return data.msgpost;
  }
  void messages_posted(int n) {
    data.msgpost = static_cast<uint16_t>(n);
  }

  [[nodiscard]] int email_sent() const {
    return data.emailsent;
  }
  void email_sent(int n) {
    data.emailsent = static_cast<uint16_t>(n);
  }

  [[nodiscard]] int feedback_sent() const {
    return data.feedbacksent;
  }
  void feedback_sent(int n) {
    data.feedbacksent = static_cast<uint16_t>(n);
  }
  [[nodiscard]] int feedback_today() const {
    return data.fsenttoday1;
  }

  void feedback_today(int n) {
    data.fsenttoday1 = static_cast<uint16_t>(n);
  }

  [[nodiscard]] int posts_today() const {
    return data.posttoday;
  }
  void posts_today(int n) {
    data.posttoday = static_cast<uint16_t>(n);
  }

  [[nodiscard]] int email_today() const {
    return data.etoday;
  }
  void email_today(int n) {
    data.etoday = static_cast<uint16_t>(n);
  }

  [[nodiscard]] int ass_points() const {
    return data.ass_pts;
  }
  void ass_points(int n) {
    data.ass_pts = static_cast<uint16_t>(n);
  }
  void increment_ass_points(int n) {
    data.ass_pts = data.ass_pts + static_cast<uint16_t>(n);
  }

  [[nodiscard]] int uploaded() const {
    return data.uploaded;
  }
  void uploaded(int n) {
    data.uploaded = static_cast<uint16_t>(n);
  }
  void increment_uploaded() {
    ++data.uploaded;
  }

  void decrement_uploaded() {
    --data.uploaded;
  }

  [[nodiscard]] int downloaded() const {
    return data.downloaded;
  }
  void downloaded(int n) {
    data.downloaded = static_cast<uint16_t>(n);
  }
  void increment_downloaded() {
    ++data.downloaded;
  }
  void decrement_downloaded() {
    --data.downloaded;
  }

  [[nodiscard]] uint16_t last_bps() const {
    return data.lastrate;
  }
  void last_bps(int n) {
    data.lastrate = static_cast<uint16_t>(n);
  }

  [[nodiscard]] uint16_t logons() const {
    return data.logons;
  }
  void increment_logons() { ++data.logons; }

  void logons(int n) {
    data.logons = static_cast<uint16_t>(n);
  }

  [[nodiscard]] uint16_t email_net() const {
    return data.emailnet;
  }
  void email_net(uint16_t n) {
    data.emailnet = n;
  }

  [[nodiscard]] uint16_t posts_net() const {
    return data.postnet;
  }
  void posts_net(uint16_t n) {
    data.postnet = n;
  }

  [[nodiscard]] uint16_t deleted_posts() const {
    return data.deletedposts;
  }
  void deleted_posts(uint16_t n) {
    data.deletedposts = n;
  }

  [[nodiscard]] uint16_t chains_run() const {
    return data.chainsrun;
  }
  void chains_run(uint16_t n) {
    data.chainsrun = n;
  }

  [[nodiscard]] uint16_t gfiles_read() const {
    return data.gfilesread;
  }
  void gfiles_read(uint16_t n) {
    data.gfilesread = n;
  }

  [[nodiscard]] uint16_t banktime_minutes() const {
    return data.banktime;
  }
  void banktime_minutes(uint16_t n) {
    data.banktime = n;
  }
  void add_banktime_minutes(int n) {
    data.banktime += static_cast<uint16_t>(n);
  }
  void subtract_banktime_minutes(int n) {
    data.banktime -= static_cast<uint16_t>(n);
  }

  [[nodiscard]] uint16_t home_netnum() const {
    return data.homenet;
  }
  void home_netnum(uint16_t n) {
    data.homenet = n;
  }

  [[nodiscard]] uint16_t subconf() const {
    return data.subconf;
  }
  void subconf(uint16_t n) {
    data.subconf = n;
  }

  [[nodiscard]] uint16_t dirconf() const {
    return data.dirconf;
  }
  void dirconf(uint16_t n) {
    data.dirconf = n;
  }

  [[nodiscard]] uint16_t subnum() const {
    return data.subnum;
  }
  void subnum(uint16_t n) {
    data.subnum = n;
  }

  [[nodiscard]] uint16_t dirnum() const {
    return data.dirnum;
  }
  void dirnum(uint16_t n) {
    data.dirnum = n;
  }

  [[nodiscard]] uint32_t messages_read() const {
    return data.msgread;
  }
  void messages_read(uint32_t l) {
    data.msgread = l;
  }

  [[nodiscard]] uint32_t uk() const {
    return data.uk;
  }
  void set_uk(uint32_t l) {
    data.uk = l;
  }

  [[nodiscard]] uint32_t dk() const {
    return data.dk;
  }
  void set_dk(uint32_t l) {
    data.dk = l;
  }

  [[nodiscard]] daten_t last_daten() const {
    return data.daten;
  }
  void last_daten(daten_t l) {
    data.daten = l;
  }

  [[nodiscard]] uint32_t wwiv_regnum() const {
    return data.wwiv_regnum;
  }
  void wwiv_regnum(uint32_t l) {
    data.wwiv_regnum = l;
  }

  [[nodiscard]] daten_t nscan_daten() const {
    return data.datenscan;
  }
  void nscan_daten(daten_t l) {
    data.datenscan = l;
  }

  /** Adds extra time to the user, returns the new total extra time. */
  std::chrono::seconds add_extratime(std::chrono::duration<double> extra);
  
  /** Subtracts extra time to the user, returns the new total extra time. */
  std::chrono::seconds subtract_extratime(std::chrono::duration<double> extra);

  /** Total extra time awarded to this user */
  [[nodiscard]] std::chrono::duration<double> extra_time() const noexcept;
  
  /** Time online in total, in seconds */
  [[nodiscard]] std::chrono::seconds timeon() const;
  
  /** Time online today, in seconds */
  [[nodiscard]] std::chrono::seconds timeontoday() const;
  
  /** Add 'd' to the time online in total */
  std::chrono::seconds add_timeon(std::chrono::duration<double> d);
  
  /** Add 'd' to the time online for today */
  std::chrono::seconds add_timeon_today(std::chrono::duration<double> d);

  /** Returns the time on as seconds. */
  [[nodiscard]] float gold() const {
    return data.gold;
  }
  void gold(float f) {
    data.gold = f;
  }

  [[nodiscard]] bool full_descriptions() const {
    return data.full_desc ? true : false;
  }
  void full_descriptions(bool b) {
    data.full_desc = b ? 1 : 0;
  }

  [[nodiscard]] bool mailbox_closed() const { return forward_usernum() == 65535; }

  /**
   * Close the user's email inbox to disallow receiving emails.
   */
  void close_mailbox() {
    forward_systemnum(0);
    forward_usernum(65535);
  }

  [[nodiscard]] bool forwarded_to_internet() const {
    return forward_usernum() == net::INTERNET_EMAIL_FAKE_OUTBOUND_NODE;
  }

  [[nodiscard]] bool mailbox_forwarded() const {
    return forward_usernum() > 0 && forward_usernum() < net::INTERNET_EMAIL_FAKE_OUTBOUND_NODE;
  }

  void clear_mailbox_forward() {
    forward_systemnum(0);
    forward_usernum(0);
  }

  /** Sets the last IP address from which this user connected. */
  void last_address(const core::ip_address& a) { data.last_address = a; }

  /** Returns the last IP address from which this user connected. */
  [[nodiscard]] core::ip_address last_address() const { return data.last_address; }

  /**
   * Creates a random password in the user's password field.
   */
  bool CreateRandomPassword();

  /** Should this user use hot keys? */
  [[nodiscard]] bool hotkeys() const;
  void set_hotkeys(bool enabled);

  /** The current menu set for the user */
  [[nodiscard]] std::string menu_set() const;
  
  /** Sets the current menu set */
  void set_menu_set(const std::string& menu_set);

  bool asv(const validation_config_t& v);

  /**
   * Returns the upload/download ratio.
   */
  [[nodiscard]] float ratio() const;

  ///////////////////////////////////////////////////////////////////////////
  // Static Helper Methods

  /** Creates a new user record in 'u' using the default values passed */
  static bool CreateNewUserRecord(User* u,
    uint8_t sl, uint8_t dsl, uint16_t restr, float gold,
    const std::vector<uint8_t>& newuser_colors,
    const std::vector<uint8_t>& newuser_bwcolors);

private:
  // USERREC.exempt
  [[nodiscard]] bool HasExemptFlag(int flag) const {
    return (data.exempt & flag) != 0;
  }

};

/** Reset today's user statistics for user u */
void ResetTodayUserStats(User* u);

/** Increments both the total calls and today's call stats for user 'u' */
int AddCallToday(User* u);

/** Returns the age in years for user 'u' */
int years_old(const User* u, core::Clock& clock);


}  // namespace wwiv::sdk

#endif