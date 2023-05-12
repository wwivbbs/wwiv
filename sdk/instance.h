/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2021-2022, WWIV Software Services             */
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
/**************************************************************************/
#ifndef INCLUDED_SDK_INSTANCE_H
#define INCLUDED_SDK_INSTANCE_H

#include "core/datetime.h"

#include "sdk/config.h"
#include "sdk/vardec.h"
#include <filesystem>
#include <string>
#include <vector>

namespace wwiv::sdk {

  // (moved from vardec.h)

  /* Instance status flags */
  constexpr int INST_FLAGS_NONE = 0x0000;  // No flags at all
  constexpr int INST_FLAGS_ONLINE = 0x0001;  // User online
  constexpr int INST_FLAGS_MSG_AVAIL = 0x0002;  // Available for inst msgs
  constexpr int INST_FLAGS_INVIS = 0x0004;  // For invisibility

  /* Instance primary location points */
  constexpr int INST_LOC_DOWN = 0;
  constexpr int INST_LOC_INIT = 1;
  constexpr int INST_LOC_EMAIL = 2;
  constexpr int INST_LOC_MAIN = 3;
  constexpr int INST_LOC_XFER = 4;
  constexpr int INST_LOC_CHAINS = 5;
  constexpr int INST_LOC_NET = 6;
  constexpr int INST_LOC_GFILES = 7;
  constexpr int INST_LOC_BEGINDAY = 8;
  constexpr int INST_LOC_EVENT = 9;
  constexpr int INST_LOC_CHAT = 10;
  constexpr int INST_LOC_CHAT2 = 11;
  constexpr int INST_LOC_CHATROOM = 12;
  constexpr int INST_LOC_LOGON = 13;
  constexpr int INST_LOC_LOGOFF = 14;
  constexpr int INST_LOC_FSED = 15;
  constexpr int INST_LOC_UEDIT = 16;
  constexpr int INST_LOC_CHAINEDIT = 17;
  constexpr int INST_LOC_BOARDEDIT = 18;
  constexpr int INST_LOC_DIREDIT = 19;
  constexpr int INST_LOC_GFILEEDIT = 20;
  constexpr int INST_LOC_CONFEDIT = 21;
  constexpr int INST_LOC_DOS = 22;
  constexpr int INST_LOC_DEFAULTS = 23;
  constexpr int INST_LOC_REBOOT = 24;
  constexpr int INST_LOC_RELOAD = 25;
  constexpr int INST_LOC_VOTE = 26;
  constexpr int INST_LOC_BANK = 27;
  constexpr int INST_LOC_AMSG = 28;
  constexpr int INST_LOC_SUBS = 29;
  constexpr int INST_LOC_CHUSER = 30;
  constexpr int INST_LOC_TEDIT = 31;
  constexpr int INST_LOC_MAILR = 32;
  constexpr int INST_LOC_RESETQSCAN = 33;
  constexpr int INST_LOC_VOTEEDIT = 34;
  constexpr int INST_LOC_VOTEPRINT = 35;
  constexpr int INST_LOC_RESETF = 36;
  constexpr int INST_LOC_FEEDBACK = 37;
  constexpr int INST_LOC_KILLEMAIL = 38;
  constexpr int INST_LOC_POST = 39;
  constexpr int INST_LOC_NEWUSER = 40;
  constexpr int INST_LOC_RMAIL = 41;
  constexpr int INST_LOC_DOWNLOAD = 42;
  constexpr int INST_LOC_UPLOAD = 43;
  constexpr int INST_LOC_BIXFER = 44;
  constexpr int INST_LOC_NETLIST = 45;
  constexpr int INST_LOC_TERM = 46;
  //constexpr int INST_LOC_EVENTEDIT = 47;  NO LONGER USED
  constexpr int INST_LOC_GETUSER = 48;
  constexpr int INST_LOC_QWK = 49;
  constexpr int INST_LOC_CH1 = 5000;
  constexpr int INST_LOC_CH2 = 5001;
  constexpr int INST_LOC_CH3 = 5002;
  constexpr int INST_LOC_CH5 = 5004;
  constexpr int INST_LOC_CH6 = 5005;
  constexpr int INST_LOC_CH7 = 5006;
  constexpr int INST_LOC_CH8 = 5007;
  constexpr int INST_LOC_CH9 = 5008;
  constexpr int INST_LOC_CH10 = 5009;
  constexpr int INST_LOC_WFC = 65535;


  /****************************************************************************/


// Generated from constants using:
// awk '{print "constexpr char s" $3 "[] = " "\""$3"\";"}' x

constexpr char sINST_LOC_DOWN[] = "INST_LOC_DOWN";
constexpr char sINST_LOC_INIT[] = "INST_LOC_INIT";
constexpr char sINST_LOC_EMAIL[] = "INST_LOC_EMAIL";
constexpr char sINST_LOC_MAIN[] = "INST_LOC_MAIN";
constexpr char sINST_LOC_XFER[] = "INST_LOC_XFER";
constexpr char sINST_LOC_CHAINS[] = "INST_LOC_CHAINS";
constexpr char sINST_LOC_NET[] = "INST_LOC_NET";
constexpr char sINST_LOC_GFILES[] = "INST_LOC_GFILES";
constexpr char sINST_LOC_BEGINDAY[] = "INST_LOC_BEGINDAY";
constexpr char sINST_LOC_EVENT[] = "INST_LOC_EVENT";
constexpr char sINST_LOC_CHAT[] = "INST_LOC_CHAT";
constexpr char sINST_LOC_CHAT2[] = "INST_LOC_CHAT2";
constexpr char sINST_LOC_CHATROOM[] = "INST_LOC_CHATROOM";
constexpr char sINST_LOC_LOGON[] = "INST_LOC_LOGON";
constexpr char sINST_LOC_LOGOFF[] = "INST_LOC_LOGOFF";
constexpr char sINST_LOC_FSED[] = "INST_LOC_FSED";
constexpr char sINST_LOC_UEDIT[] = "INST_LOC_UEDIT";
constexpr char sINST_LOC_CHAINEDIT[] = "INST_LOC_CHAINEDIT";
constexpr char sINST_LOC_BOARDEDIT[] = "INST_LOC_BOARDEDIT";
constexpr char sINST_LOC_DIREDIT[] = "INST_LOC_DIREDIT";
constexpr char sINST_LOC_GFILEEDIT[] = "INST_LOC_GFILEEDIT";
constexpr char sINST_LOC_CONFEDIT[] = "INST_LOC_CONFEDIT";
constexpr char sINST_LOC_DOS[] = "INST_LOC_DOS";
constexpr char sINST_LOC_DEFAULTS[] = "INST_LOC_DEFAULTS";
constexpr char sINST_LOC_REBOOT[] = "INST_LOC_REBOOT";
constexpr char sINST_LOC_RELOAD[] = "INST_LOC_RELOAD";
constexpr char sINST_LOC_VOTE[] = "INST_LOC_VOTE";
constexpr char sINST_LOC_BANK[] = "INST_LOC_BANK";
constexpr char sINST_LOC_AMSG[] = "INST_LOC_AMSG";
constexpr char sINST_LOC_SUBS[] = "INST_LOC_SUBS";
constexpr char sINST_LOC_CHUSER[] = "INST_LOC_CHUSER";
constexpr char sINST_LOC_TEDIT[] = "INST_LOC_TEDIT";
constexpr char sINST_LOC_MAILR[] = "INST_LOC_MAILR";
constexpr char sINST_LOC_RESETQSCAN[] = "INST_LOC_RESETQSCAN";
constexpr char sINST_LOC_VOTEEDIT[] = "INST_LOC_VOTEEDIT";
constexpr char sINST_LOC_VOTEPRINT[] = "INST_LOC_VOTEPRINT";
constexpr char sINST_LOC_RESETF[] = "INST_LOC_RESETF";
constexpr char sINST_LOC_FEEDBACK[] = "INST_LOC_FEEDBACK";
constexpr char sINST_LOC_KILLEMAIL[] = "INST_LOC_KILLEMAIL";
constexpr char sINST_LOC_POST[] = "INST_LOC_POST";
constexpr char sINST_LOC_NEWUSER[] = "INST_LOC_NEWUSER";
constexpr char sINST_LOC_RMAIL[] = "INST_LOC_RMAIL";
constexpr char sINST_LOC_DOWNLOAD[] = "INST_LOC_DOWNLOAD";
constexpr char sINST_LOC_UPLOAD[] = "INST_LOC_UPLOAD";
constexpr char sINST_LOC_BIXFER[] = "INST_LOC_BIXFER";
constexpr char sINST_LOC_NETLIST[] = "INST_LOC_NETLIST";
constexpr char sINST_LOC_TERM[] = "INST_LOC_TERM";
constexpr char sINST_LOC_EVENTEDIT[] = "INST_LOC_EVENTEDIT";
constexpr char sINST_LOC_GETUSER[] = "INST_LOC_GETUSER";
constexpr char sINST_LOC_QWK[] = "INST_LOC_QWK";
constexpr char sINST_LOC_CH1[] = "INST_LOC_CH1";
constexpr char sINST_LOC_CH2[] = "INST_LOC_CH2";
constexpr char sINST_LOC_CH3[] = "INST_LOC_CH3";
constexpr char sINST_LOC_CH5[] = "INST_LOC_CH5";
constexpr char sINST_LOC_CH6[] = "INST_LOC_CH6";
constexpr char sINST_LOC_CH7[] = "INST_LOC_CH7";
constexpr char sINST_LOC_CH8[] = "INST_LOC_CH8";
constexpr char sINST_LOC_CH9[] = "INST_LOC_CH9";
constexpr char sINST_LOC_CH10[] = "INST_LOC_CH10";
constexpr char sINST_LOC_WFC[] = "INST_LOC_WFC";

class Instance final {
public:
  Instance(std::filesystem::path root_dir, std::filesystem::path data_dir, instancerec ir);
  Instance(std::filesystem::path root_dir, std::filesystem::path data_dir, int instance_num);
  Instance(std::filesystem::path root_dir, std::filesystem::path data_dir) : Instance(root_dir, data_dir, 0) {}
  Instance(const Instance&) = delete;
  Instance(Instance&&) noexcept;
  Instance& operator=(const Instance&);
  //Instance& operator=(Instance&&) noexcept;
  ~Instance() = default;

  /**
   * Mutable member to manipulate the instance record.
   */
  [[nodiscard]] const instancerec& ir() const;
  [[nodiscard]] instancerec& ir();

  /**
   * The root directory for this WWIV instance BBS.
   */
  [[nodiscard]] std::filesystem::path root_directory() const;

  /**
   * The node number for this instance metadata.
   */
  [[nodiscard]] int node_number() const noexcept;

  /**
   * The user number for this current or last user on this node.
   */
  [[nodiscard]] int user_number() const noexcept;

  /**
   * Is this instance available with a user online who can receive messages.
   */
  [[nodiscard]] bool available() const noexcept;

  /**
   * Is this instance available with a user online who can receive messages.
   */
  [[nodiscard]] bool online() const noexcept;

  /**
   * Is this node active with a user.
   */
  [[nodiscard]] bool invisible() const noexcept;

  /**
   * Description of the caller's location within the BBS.
   */
  [[nodiscard]] std::string location_description() const;

  /**
   * The integer code for the location.
   */
  [[nodiscard]] int loc_code() const noexcept;

  /**
   * Is the caller in a numbered channel
   */
  [[nodiscard]] bool in_channel() const noexcept;

  /**
   * The connection speed of the current or last session on this node.
   */
  [[nodiscard]] int modem_speed() const noexcept;

  /**
   * The sub-location code. This is typically a pointer into the list of
   * items referred to by location.  For example, in subs, it's the index
   * number of the sub the user is browsing.
   */
  [[nodiscard]] int subloc_code() const noexcept;

  /**
   * When was this instance started.
   */
  [[nodiscard]] core::DateTime started() const;

  /**
   * When was this instance's metadata last updated.
   */
  [[nodiscard]] core::DateTime updated() const;

private:
  std::filesystem::path root_dir_;
  std::filesystem::path data_dir_;
  instancerec ir_;
};

class Instances final {
public:
  typedef std::vector<Instance>::iterator iterator;
  typedef std::vector<Instance>::const_iterator const_iterator;
  typedef std::size_t size_type;

  [[nodiscard]] iterator begin() { return instances_.begin(); }
  [[nodiscard]] const_iterator begin() const { return instances_.begin(); }
  [[nodiscard]] const_iterator cbegin() const { return instances_.cbegin(); }
  [[nodiscard]] iterator end() { return instances_.end(); }
  [[nodiscard]] const_iterator end() const { return instances_.end(); }
  [[nodiscard]] const_iterator cend() const { return instances_.cend(); }

  Instances() = delete;
  Instances(const Instances&) = delete;
  Instances(Instances&&) = delete;
  explicit Instances(const Config& config);
  Instances& operator=(const Instances&) = delete;
  Instances& operator=(Instances&&) = delete;
  ~Instances() = default;

  [[nodiscard]] bool IsInitialized() const { return initialized_; }

  size_type size() const;
  Instance at(size_type pos);
  std::vector<Instance> all();

  bool upsert(size_type pos, const instancerec& ir);
  bool upsert(size_type pos, const Instance& ir);

  explicit operator bool() const noexcept { return IsInitialized(); }

private:
  [[nodiscard]] const std::filesystem::path& fn_path() const;

  bool initialized_;
  const std::filesystem::path path_;
  const std::filesystem::path root_dir_;
  const std::filesystem::path data_dir_;
  std::vector<Instance> instances_;
};

} // namespace wwiv::sdk

#endif
