/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2020, WWIV Software Services            */
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
#include "sdk/net/net.h"
#include "sdk/vardec.h"
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

constexpr int HOTKEYS_ON = 0;
constexpr int HOTKEYS_OFF = 1;

namespace wwiv::sdk {

/**
 * User Class - Represents a User record
 */
class User {
 public:
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
  static constexpr int ansi                       = 0x00000001;
  static constexpr int status_color               = 0x00000002;
  static constexpr int music                      = 0x00000004;
  static constexpr int pauseOnPage                = 0x00000008;
  static constexpr int expert                     = 0x00000010;
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

  //
  // Constructor and Destructor
  //
  User();
  ~User();

  User(const User& w);
  explicit User(const userrec& rhs);
  User& operator=(const User& rhs);

  //
  // Member Functions
  //
  void FixUp();    // was function fix_user_rec
  void ZeroUserData();

  //
  // Accessor Functions
  //

  // USERREC.inact
  void SetInactFlag(int nFlag) {
    data.inact |= nFlag;
  }
  void ToggleInactFlag(int nFlag) {
    data.inact ^= nFlag;
  }
  void ClearInactFlag(int nFlag) {
    data.inact &= ~nFlag;
  }
  [[nodiscard]] bool IsUserDeleted() const {
    return (data.inact & User::userDeleted) != 0;
  }
  [[nodiscard]] bool IsUserInactive() const {
    return (data.inact & User::userInactive) != 0;
  }

  // USERREC.sysstatus
  void SetStatusFlag(int nFlag, bool on) {
    if (on) {
      data.sysstatus |= nFlag;
    } else {
      data.sysstatus &= ~nFlag;
    }
  }
  void SetStatusFlag(int nFlag) {
    data.sysstatus |= nFlag;
  }
  void ToggleStatusFlag(int nFlag) {
    data.sysstatus ^= nFlag;
  }
  void ClearStatusFlag(int nFlag) {
    data.sysstatus &= ~nFlag;
  }
  [[nodiscard]] bool HasStatusFlag(int nFlag) const {
    return (data.sysstatus & nFlag) != 0;
  }
  [[nodiscard]] long GetStatus() const {
    return static_cast<long>(data.sysstatus);
  }
  void SetStatus(long l) {
    data.sysstatus = static_cast<uint32_t>(l);
  }
  [[nodiscard]] bool HasAnsi() const {
    return HasStatusFlag(User::ansi);
  }
  [[nodiscard]] bool HasColor() const { return HasStatusFlag(User::status_color); }
  [[nodiscard]] bool HasMusic() const {
    return HasStatusFlag(User::music);
  }
  [[nodiscard]] bool HasPause() const {
    return HasStatusFlag(User::pauseOnPage);
  }
  [[nodiscard]] bool IsExpert() const {
    return HasStatusFlag(User::expert);
  }
  [[nodiscard]] bool HasShortMessage() const {
    return HasStatusFlag(User::SMW);
  }
  [[nodiscard]] bool IsFullScreen() const {
    return HasStatusFlag(User::fullScreen);
  }
  [[nodiscard]] bool IsNewScanFiles() const {
    return HasStatusFlag(User::nscanFileSystem);
  }
  [[nodiscard]] bool IsUseExtraColor() const {
    return HasStatusFlag(User::extraColor);
  }
  [[nodiscard]] bool IsUseClearScreen() const {
    return HasStatusFlag(User::clearScreen);
  }
  [[nodiscard]] bool IsUseConference() const {
    return HasStatusFlag(User::conference);
  }
  [[nodiscard]] bool IsIgnoreChatRequests() const {
    return HasStatusFlag(User::noChat);
  }
  [[nodiscard]] bool IsIgnoreNodeMessages() const {
    return HasStatusFlag(User::noMsgs);
  }

  [[nodiscard]] bool IsUseAutoQuote() const {
    return HasStatusFlag(User::autoQuote);
  }
  [[nodiscard]] bool IsUse24HourClock() const {
    return HasStatusFlag(User::twentyFourHourClock);
  };

  // USERREC.exempt
  void SetExemptFlag(int nFlag) {
    data.exempt |= nFlag;
  }
  void ToggleExemptFlag(int nFlag) {
    data.exempt ^= nFlag;
  }
  void ClearExemptFlag(int nFlag) {
    data.exempt &= ~nFlag;
  }
  [[nodiscard]] bool HasExemptFlag(int nFlag) const {
    return (data.exempt & nFlag) != 0;
  }

  [[nodiscard]] bool IsExemptRatio() const {
    return HasExemptFlag(User::exemptRatio);
  }
  [[nodiscard]] bool IsExemptTime() const {
    return HasExemptFlag(User::exemptTime);
  }
  [[nodiscard]] bool IsExemptPost() const {
    return HasExemptFlag(User::exemptPost);
  }
  [[nodiscard]] bool IsExemptAll() const {
    return HasExemptFlag(User::exemptAll);
  }
  [[nodiscard]] bool IsExemptAutoDelete() const {
    return HasExemptFlag(User::exemptAutoDelete);
  }

  // USERREC.restrict
  void SetRestrictionFlag(int nFlag) noexcept {
    data.restrict |= nFlag;
  }
  void ToggleRestrictionFlag(int nFlag) noexcept {
    data.restrict ^= nFlag;
  }
  void ClearRestrictionFlag(int nFlag) noexcept {
    data.restrict &= ~nFlag;
  }
  [[nodiscard]] bool HasRestrictionFlag(int nFlag) const noexcept {
    return (data.restrict & nFlag) != 0;
  }
  [[nodiscard]] uint16_t GetRestriction() const {
    return data.restrict;
  }
  void SetRestriction(uint16_t n) {
    data.restrict = n;
  }

  [[nodiscard]] bool IsRestrictionLogon() const {
    return HasRestrictionFlag(User::restrictLogon);
  }
  [[nodiscard]] bool IsRestrictionChat() const {
    return HasRestrictionFlag(User::restrictChat);
  }
  [[nodiscard]] bool IsRestrictionValidate() const {
    return HasRestrictionFlag(User::restrictValidate);
  }
  [[nodiscard]] bool IsRestrictionAutomessage() const {
    return HasRestrictionFlag(User::restrictAutomessage);
  }
  [[nodiscard]] bool IsRestrictionAnonymous() const {
    return HasRestrictionFlag(User::restrictAnony);
  }
  [[nodiscard]] bool IsRestrictionPost() const {
    return HasRestrictionFlag(User::restrictPost);
  }
  [[nodiscard]] bool IsRestrictionEmail() const {
    return HasRestrictionFlag(User::restrictEmail);
  }
  [[nodiscard]] bool IsRestrictionVote() const {
    return HasRestrictionFlag(User::restrictVote);
  }
  [[nodiscard]] bool IsRestrictionMultiNodeChat() const {
    return HasRestrictionFlag(User::restrictMultiNodeChat);
  }
  [[nodiscard]] bool IsRestrictionNet() const {
    return HasRestrictionFlag(User::restrictNet);
  }
  [[nodiscard]] bool IsRestrictionUpload() const {
    return HasRestrictionFlag(User::restrictUpload);
  }

  void SetArFlag(int nFlag) {
    data.ar |= nFlag;
  }
  void ToggleArFlag(int nFlag) {
    data.ar ^= nFlag;
  }
  void ClearArFlag(int nFlag) {
    data.ar &= ~nFlag;
  }
  [[nodiscard]] bool HasArFlag(int ar) const {
    if (ar == 0) {
      // Always have the empty ar
      return true;
    }
    return (data.ar & ar) != 0;
  }
  [[nodiscard]] int GetAr() const {
    return data.ar;
  }
  void SetAr(int n) {
    data.ar = static_cast<uint16_t>(n);
  }

  void SetDarFlag(int nFlag) {
    data.dar |= nFlag;
  }
  void ToggleDarFlag(int nFlag) {
    data.dar ^= nFlag;
  }
  void ClearDarFlag(int nFlag) {
    data.dar &= ~nFlag;
  }
  [[nodiscard]] bool HasDarFlag(int nFlag) const {
    if (nFlag == 0) {
      // Always have the empty dar
      return true;
    }
    return (data.dar & nFlag) != 0;
  }
  [[nodiscard]] int GetDar() const {
    return data.dar;
  }
  void SetDar(int n) {
    data.dar = static_cast<uint16_t>(n);
  }

  [[nodiscard]] const char *GetName() const {
    return reinterpret_cast<const char*>(data.name);
  }
  void set_name(const char *s) {
    strcpy(reinterpret_cast<char*>(data.name), s);
  }
  [[nodiscard]] const char *GetRealName() const {
    return reinterpret_cast<const char*>(data.realname);
  }
  void SetRealName(const char *s) {
    strcpy(reinterpret_cast<char*>(data.realname), s);
  }
  [[nodiscard]] const char *GetCallsign() const {
    return reinterpret_cast<const char*>(data.callsign);
  }
  void SetCallsign(const char *s) {
    strcpy(reinterpret_cast<char*>(data.callsign), s);
  }
  [[nodiscard]] const char *GetVoicePhoneNumber() const {
    return reinterpret_cast<const char*>(data.phone);
  }
  void SetVoicePhoneNumber(const char *s) {
    strcpy(reinterpret_cast<char*>(data.phone), s);
  }
  [[nodiscard]] const char *GetDataPhoneNumber() const {
    return reinterpret_cast<const char*>(data.dataphone);
  }
  void SetDataPhoneNumber(const char *s) {
    strcpy(reinterpret_cast<char*>(data.dataphone), s);
  }
  [[nodiscard]] const char *GetStreet() const {
    return reinterpret_cast<const char*>(data.street);
  }
  void SetStreet(const char *s) {
    strcpy(reinterpret_cast<char*>(data.street), s);
  }
  [[nodiscard]] const char *GetCity() const {
    return reinterpret_cast<const char*>(data.city);
  }
  void SetCity(const char *s) {
    strcpy(reinterpret_cast<char*>(data.city), s);
  }
  [[nodiscard]] const char *GetState() const {
    return reinterpret_cast<const char*>(data.state);
  }
  void SetState(const char *s) {
    strcpy(reinterpret_cast<char*>(data.state), s);
  }
  [[nodiscard]] const char *GetCountry() const {
    return reinterpret_cast<const char*>(data.country);
  }
  void SetCountry(const char *s) {
    strcpy(reinterpret_cast<char*>(data.country) , s);
  }
  [[nodiscard]] const char *GetZipcode() const {
    return reinterpret_cast<const char*>(data.zipcode);
  }
  void SetZipcode(const char *s) {
    strcpy(reinterpret_cast<char*>(data.zipcode), s);
  }
  [[nodiscard]] const char *GetPassword() const {
    return reinterpret_cast<const char*>(data.pw);
  }
  void SetPassword(const std::string& s) {
    wwiv::strings::to_char_array(data.pw, s);
  }

  [[nodiscard]] std::string GetLastOn() const {
    return data.laston;
  }

  void SetLastOn(const std::string& s) {
    wwiv::strings::to_char_array(data.laston, s);
  }

  [[nodiscard]] std::string GetFirstOn() const {
    return data.firston;
  }
  
  void SetFirstOn(const std::string& s) {
    wwiv::strings::to_char_array(data.firston, s);
  }

  [[nodiscard]] std::string GetNote() const {
    return reinterpret_cast<const char*>(data.note);
  }
  void SetNote(const std::string& s) {
    strcpy(reinterpret_cast<char*>(data.note), s.c_str());
  }
  [[nodiscard]] std::string GetMacro(int line) const {
    return std::string(reinterpret_cast<const char*>(data.macros[line]));
  }
  void SetMacro(int nLine, const char *s) {
    memset(&data.macros[ nLine ][0], 0, 80);
    strcpy(reinterpret_cast<char*>(data.macros[ nLine ]), s);
  }
  [[nodiscard]] char GetGender() const {
    if (data.sex == 'N') {
      // N means unknown.  NEWUSER sets it to N to prompt the
      // user again.
      return 'N';
    }
    return data.sex == 'F' ? 'F' : 'M';
  }
  void SetGender(const char c) {
    data.sex = static_cast<uint8_t>(c);
  }
  [[nodiscard]] std::string GetEmailAddress() const {
    return data.email;
  }
  void SetEmailAddress(const char *s) {
    strcpy(data.email, s);
  }
  [[nodiscard]] uint8_t age() const;

  [[nodiscard]] int8_t GetComputerType() const {
    return data.comp_type;
  }
  void SetComputerType(int n) {
    data.comp_type = static_cast<int8_t>(n);
  }

  [[nodiscard]] int GetDefaultProtocol() const {
    return data.defprot;
  }
  void SetDefaultProtocol(int n) {
    data.defprot = static_cast<uint8_t>(n);
  }
  [[nodiscard]] int GetDefaultEditor() const {
    return data.defed;
  }
  void SetDefaultEditor(int n) {
    data.defed = static_cast<uint8_t>(n);
  }
  [[nodiscard]] int GetScreenChars() const {
    return data.screenchars;
  }
  void SetScreenChars(int n) {
    data.screenchars = static_cast<uint8_t>(n);
  }
  [[nodiscard]] int GetScreenLines() const {
    return data.screenlines;
  }
  void SetScreenLines(int n) {
    data.screenlines = static_cast<uint8_t>(n);
  }
  [[nodiscard]] int GetNumExtended() const {
    return data.num_extended;
  }
  void SetNumExtended(int n) {
    data.num_extended = static_cast<uint8_t>(n);
  }
  [[nodiscard]] int GetOptionalVal() const {
    return data.optional_val;
  }
  void SetOptionalVal(int n) {
    data.optional_val = static_cast<uint8_t>(n);
  }
  [[nodiscard]] int GetSl() const {
    return data.sl;
  }
  void SetSl(int n) {
    data.sl = static_cast<uint8_t>(n);
  }
  [[nodiscard]] int GetDsl() const {
    return data.dsl;
  }
  void SetDsl(int n) {
    data.dsl = static_cast<uint8_t>(n);
  }
  [[nodiscard]] int GetExempt() const {
    return data.exempt;
  }
  void SetExempt(int n) {
    data.exempt = static_cast<uint8_t>(n);
  }
  [[nodiscard]] int GetColor(int n) const {
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
    return static_cast<uint8_t>(HasAnsi() ? GetColor(n) : GetBWColor(n));
  }

  [[nodiscard]] std::vector<uint8_t> colors() const { 
    std::vector<uint8_t> c;
    for (int i = 0; i < 10; i++) {
      if (HasColor()) {
        c.push_back(data.colors[i]);
      } else {
        c.push_back(data.bwcolors[i]);
      }
    }
    return c;
  }
  void SetColor(int nColor, unsigned int n) {
    data.colors[nColor] = static_cast<uint8_t>(n);
  }
  [[nodiscard]] unsigned char GetBWColor(int n) const {
    if (n < 0 || n > 9) {
      return 7; // default color
    }
    return data.bwcolors[n];
  }
  void SetBWColor(int nColor, unsigned int n) {
    data.bwcolors[nColor] = static_cast<uint8_t>(n);
  }
  [[nodiscard]] int GetVote(int nVote) const {
    return data.votes[nVote];
  }
  void SetVote(int nVote, int n) {
    data.votes[nVote] = static_cast<uint8_t>(n);
  }
  [[nodiscard]] int GetNumIllegalLogons() const {
    return data.illegal;
  }
  void SetNumIllegalLogons(int n) {
    data.illegal = static_cast<uint8_t>(n);
  }
  [[nodiscard]] int GetNumMailWaiting() const {
    return data.waiting;
  }
  void SetNumMailWaiting(unsigned int n) {
    data.waiting = static_cast<uint8_t>(n);
  }
  [[nodiscard]] int GetTimesOnToday() const {
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
  [[nodiscard]] wwiv::core::DateTime birthday_dt() const;

  /** Gets the current user's birthday as a string in format "mm/dd/yy" */
  [[nodiscard]] std::string birthday_mmddyy() const;

  [[nodiscard]] int GetLanguage() const {
    return data.language;
  }
  void SetLanguage(int n) {
    data.language = static_cast<uint8_t>(n);
  }

  [[nodiscard]] int GetHomeUserNumber() const {
    return data.homeuser;
  }
  void SetHomeUserNumber(int n) {
    data.homeuser = static_cast<uint16_t>(n);
  }
  [[nodiscard]] uint16_t GetHomeSystemNumber() const {
    return data.homesys;
  }
  void SetHomeSystemNumber(uint16_t n) {
    data.homesys = n;
  }
  [[nodiscard]] uint16_t GetForwardUserNumber() const {
    return data.forwardusr;
  }
  void SetForwardUserNumber(uint16_t n) {
    data.forwardusr = n;
  }
  [[nodiscard]] uint16_t GetForwardSystemNumber() const {
    return data.forwardsys;
  }
  void SetForwardSystemNumber(uint16_t n) {
    data.forwardsys = n;
  }
  [[nodiscard]] int GetForwardNetNumber() const {
    return data.net_num;
  }
  void SetForwardNetNumber(int n) {
    data.net_num = static_cast<uint16_t>(n);
  }
  [[nodiscard]] int GetNumMessagesPosted() const {
    return data.msgpost;
  }
  void SetNumMessagesPosted(int n) {
    data.msgpost = static_cast<uint16_t>(n);
  }
  [[nodiscard]] int GetNumEmailSent() const {
    return data.emailsent;
  }
  void SetNumEmailSent(int n) {
    data.emailsent = static_cast<uint16_t>(n);
  }
  [[nodiscard]] int GetNumFeedbackSent() const {
    return data.feedbacksent;
  }
  void SetNumFeedbackSent(int n) {
    data.feedbacksent = static_cast<uint16_t>(n);
  }
  [[nodiscard]] int GetNumFeedbackSentToday() const {
    return data.fsenttoday1;
  }
  void SetNumFeedbackSentToday(int n) {
    data.fsenttoday1 = static_cast<uint16_t>(n);
  }
  [[nodiscard]] int GetNumPostsToday() const {
    return data.posttoday;
  }
  void SetNumPostsToday(int n) {
    data.posttoday = static_cast<uint16_t>(n);
  }
  [[nodiscard]] int GetNumEmailSentToday() const {
    return data.etoday;
  }
  void SetNumEmailSentToday(int n) {
    data.etoday = static_cast<uint16_t>(n);
  }
  [[nodiscard]] int GetAssPoints() const {
    return data.ass_pts;
  }
  void SetAssPoints(int n) {
    data.ass_pts = static_cast<uint16_t>(n);
  }
  void IncrementAssPoints(int n) {
    data.ass_pts = data.ass_pts + static_cast<uint16_t>(n);
  }
  [[nodiscard]] int GetFilesUploaded() const {
    return data.uploaded;
  }
  void SetFilesUploaded(int n) {
    data.uploaded = static_cast<uint16_t>(n);
  }
  [[nodiscard]] int GetFilesDownloaded() const {
    return data.downloaded;
  }
  void SetFilesDownloaded(int n) {
    data.downloaded = static_cast<uint16_t>(n);
  }
  [[nodiscard]] uint16_t GetLastBaudRate() const {
    return data.lastrate;
  }
  void SetLastBaudRate(int n) {
    data.lastrate = static_cast<uint16_t>(n);
  }
  [[nodiscard]] uint16_t GetNumLogons() const {
    return data.logons;
  }
  void SetNumLogons(int n) {
    data.logons = static_cast<uint16_t>(n);
  }
  [[nodiscard]] uint16_t GetNumNetEmailSent() const {
    return data.emailnet;
  }
  void SetNumNetEmailSent(uint16_t n) {
    data.emailnet = n;
  }
  [[nodiscard]] uint16_t GetNumNetPosts() const {
    return data.postnet;
  }
  void SetNumNetPosts(uint16_t n) {
    data.postnet = n;
  }
  [[nodiscard]] uint16_t GetNumDeletedPosts() const {
    return data.deletedposts;
  }
  void SetNumDeletedPosts(uint16_t n) {
    data.deletedposts = n;
  }
  [[nodiscard]] uint16_t GetNumChainsRun() const {
    return data.chainsrun;
  }
  void SetNumChainsRun(uint16_t n) {
    data.chainsrun = n;
  }
  [[nodiscard]] uint16_t GetNumGFilesRead() const {
    return data.gfilesread;
  }
  void SetNumGFilesRead(uint16_t n) {
    data.gfilesread = n;
  }
  [[nodiscard]] uint16_t GetTimeBankMinutes() const {
    return data.banktime;
  }
  void SetTimeBankMinutes(uint16_t n) {
    data.banktime = n;
  }
  [[nodiscard]] uint16_t GetHomeNetNumber() const {
    return data.homenet;
  }
  void SetHomeNetNumber(uint16_t n) {
    data.homenet = n;
  }
  [[nodiscard]] uint16_t GetLastSubConf() const {
    return data.subconf;
  }
  void SetLastSubConf(uint16_t n) {
    data.subconf = n;
  }
  [[nodiscard]] uint16_t GetLastDirConf() const {
    return data.dirconf;
  }
  void SetLastDirConf(uint16_t n) {
    data.dirconf = n;
  }
  [[nodiscard]] uint16_t GetLastSubNum() const {
    return data.subnum;
  }
  void SetLastSubNum(uint16_t n) {
    data.subnum = n;
  }
  [[nodiscard]] uint16_t GetLastDirNum() const {
    return data.dirnum;
  }
  void SetLastDirNum(uint16_t n) {
    data.dirnum = n;
  }

  [[nodiscard]] uint32_t GetNumMessagesRead() const {
    return data.msgread;
  }
  void SetNumMessagesRead(uint32_t l) {
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
  [[nodiscard]] daten_t GetLastOnDateNumber() const {
    return data.daten;
  }
  void SetLastOnDateNumber(daten_t l) {
    data.daten = l;
  }
  [[nodiscard]] uint32_t GetWWIVRegNumber() const {
    return data.wwiv_regnum;
  }
  void SetWWIVRegNumber(uint32_t l) {
    data.wwiv_regnum = l;
  }
  [[nodiscard]] daten_t GetNewScanDateNumber() const {
    return data.datenscan;
  }
  void SetNewScanDateNumber(daten_t l) {
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

  [[nodiscard]] bool GetFullFileDescriptions() const {
    return data.full_desc ? true : false;
  }
  void SetFullFileDescriptions(bool b) {
    data.full_desc = b ? 1 : 0;
  }

  [[nodiscard]] bool IsMailboxClosed() const { return GetForwardUserNumber() == 65535; }

  void CloseMailbox() {
    SetForwardSystemNumber(0);
    SetForwardUserNumber(65535);
  }
  [[nodiscard]] bool IsMailForwardedToInternet() const {
    return GetForwardUserNumber() == INTERNET_EMAIL_FAKE_OUTBOUND_NODE;
  }
  [[nodiscard]] bool IsMailboxForwarded() const {
    return GetForwardUserNumber() > 0 && GetForwardUserNumber() < INTERNET_EMAIL_FAKE_OUTBOUND_NODE;
  }
  void SetForwardToInternet() {
    SetForwardSystemNumber(INTERNET_EMAIL_FAKE_OUTBOUND_NODE);
  }
  void ClearMailboxForward() {
    SetForwardSystemNumber(0);
    SetForwardUserNumber(0);
  }

  /** Sets the last IP address from which this user connected. */
  void last_address(const wwiv::core::ip_address& a) { data.last_address = a; }

  /** Returns the last IP address from which this user connected. */
  wwiv::core::ip_address last_address() const { return data.last_address; }

  /**
   * Creates a random password in the user's password field.
   */
  bool CreateRandomPassword();

  /** Should this user use hotkeys? */
  [[nodiscard]] bool hotkeys() const;
  void set_hotkeys(bool enabled);

  /** The current menu set for the user */
  [[nodiscard]] std::string menu_set() const;
  
  /** Sets the current menu set */
  void set_menu_set(const std::string& menu_set);

  ///////////////////////////////////////////////////////////////////////////
  // Static Helper Methods

  /** Creates a new user record in 'u' using the default values passed */
  static bool CreateNewUserRecord(User* u,
    uint8_t sl, uint8_t dsl, uint16_t restr, float gold,
    const std::vector<uint8_t>& newuser_colors,
    const std::vector<uint8_t>& newuser_bwcolors);

};

/** Reset today's user statistics for user u */
void ResetTodayUserStats(User* u);

/** Increments both the total calls and today's call stats for user 'u' */
int AddCallToday(User* u);

/** Returns the age in years for user 'u' */
int years_old(const User* u, core::Clock& clock);

}  // namespace wwiv::sdk

#endif // __INCLUDED_PLATFORM_WUSER_H__
