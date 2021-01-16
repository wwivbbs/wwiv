/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*                 Copyright (C)2021, WWIV Software Services              */
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
#include "sdk/instance.h"


#include "bbs/instmsg.h"
#include "core/datafile.h"
#include "core/file.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "sdk/config.h"
#include "sdk/filenames.h"

#include <algorithm>
#include <filesystem>

using namespace wwiv::core;
using namespace wwiv::strings;

namespace wwiv::sdk {

const std::filesystem::path& Instances::fn_path() const {
  return path_;
}

Instance::Instance(instancerec ir)
  : ir_(ir) {
}

Instance::Instance(int instance_num) : ir_() {
  ir_.number = static_cast<int16_t>(instance_num);
}

Instance::Instance(Instance&& o) noexcept : ir_(o.ir_) {}

Instance& Instance::operator=(const Instance& o)  = default;
  
Instance& Instance::operator=(Instance&& o) noexcept {
  ir_ = o.ir_;
  return *this;
}

const instancerec& Instance::ir() const {
  return ir_;
}

instancerec& Instance::ir() {
  return ir_;
}

int Instance::node_number() const noexcept {
  return ir_.number;
}

int Instance::user_number() const noexcept {
  return ir_.user;
}

bool Instance::available() const noexcept {
  return online() && ir_.flags & INST_FLAGS_MSG_AVAIL;
}

bool Instance::online() const noexcept {
 return ir_.flags & INST_FLAGS_ONLINE;
}

bool Instance::invisible() const noexcept {
 return ir_.flags & INST_FLAGS_INVIS;
}

std::string Instance::location_description() const {
  return instance_location(ir_);
}

int Instance::loc_code() const noexcept {
  return ir_.loc;
}

bool Instance::in_channel() const noexcept {
  return ir_.loc >= INST_LOC_CH1 && ir_.loc <= INST_LOC_CH10;
}

int Instance::modem_speed() const noexcept {
  return ir_.modem_speed;
}

int Instance::subloc_code() const noexcept {
  return ir_.subloc;
}

DateTime Instance::started() const {
  return DateTime::from_daten(ir_.inst_started);
}

DateTime Instance::updated() const {
  return DateTime::from_daten(ir_.last_update);
}

Instances::Instances(const Config& config)
    : datadir_(config.datadir()), path_(FilePath(datadir_, INSTANCE_DAT)) {
  initialized_ = File::Exists(path_);
}

// ReSharper disable once CppMemberFunctionMayBeConst
Instances::size_type Instances::size() {
  if (const auto file = DataFile<instancerec>(path_, File::modeBinary | File::modeReadOnly)) {
    return std::max(0, file.number_of_records() - 1);
  }
  return 0;
}

// ReSharper disable once CppMemberFunctionMayBeConst
Instance Instances::at(size_type pos) {
  if (auto file = DataFile<instancerec>(path_, File::modeBinary | File::modeReadOnly)) {
    instancerec ir{};
    if (file.Read(pos, &ir)) {
      return Instance(ir);
    }
  }
  return Instance(pos);
}

// ReSharper disable once CppMemberFunctionMayBeConst
std::vector<Instance> Instances::all() {
  if (auto file = DataFile<instancerec>(path_, File::modeBinary | File::modeReadOnly)) {
    std::vector<instancerec> ir;
    if (file.ReadVector(ir)) {
      std::vector<Instance> r;
      for (const auto& i : ir) {
        r.emplace_back(i);
      }
      return r;
    }
  }
  return {};
}

// ReSharper disable once CppMemberFunctionMayBeConst
bool Instances::upsert(size_type pos, const instancerec& ir) {
  if (auto file = DataFile<instancerec>(path_, File::modeBinary | File::modeReadWrite |
                                                   File::modeCreateFile)) {
    return file.Write(pos, &ir);
  }
  return false;
}

bool Instances::upsert(size_type pos, const Instance& ir) {
  return upsert(pos, ir.ir());
}

std::string instance_location(const instancerec& ir) {
  if (ir.loc >= INST_LOC_CH1 && ir.loc <= INST_LOC_CH10) {
    return "WWIV Chatroom";
  }
  switch (ir.loc) {
  case INST_LOC_DOWN:
    return "Offline";
  case INST_LOC_INIT:
    return "Initializing BBS";
  case INST_LOC_EMAIL:
    return "Sending Email";
  case INST_LOC_MAIN:
    return "Main Menu";
  case INST_LOC_XFER:
    return "Transfer Area";
  case INST_LOC_CHAINS:
    return "Chains";
  case INST_LOC_NET:
    return "Network Transmission";
  case INST_LOC_GFILES:
    return "GFiles";
  case INST_LOC_BEGINDAY:
    return "Running BeginDay";
  case INST_LOC_EVENT:
    return "Executing Event";
  case INST_LOC_CHAT:
    return "Normal Chat";
  case INST_LOC_CHAT2:
    return "SplitScreen Chat";
  case INST_LOC_CHATROOM:
    return "ChatRoom";
  case INST_LOC_LOGON:
    return "Logging On";
  case INST_LOC_LOGOFF:
    return "Logging off";
  case INST_LOC_FSED:
    return "FullScreen Editor";
  case INST_LOC_UEDIT:
    return "In UEDIT";
  case INST_LOC_CHAINEDIT:
    return "In CHAINEDIT";
  case INST_LOC_BOARDEDIT:
    return "In BOARDEDIT";
  case INST_LOC_DIREDIT:
    return "In DIREDIT";
  case INST_LOC_GFILEEDIT:
    return "In GFILEEDIT";
  case INST_LOC_CONFEDIT:
    return "In CONFEDIT";
  case INST_LOC_DOS:
    return "In DOS";
  case INST_LOC_DEFAULTS:
    return "In Defaults";
  case INST_LOC_REBOOT:
    return "Rebooting";
  case INST_LOC_RELOAD:
    return "Reloading BBS data";
  case INST_LOC_VOTE:
    return "Voting";
  case INST_LOC_BANK:
    return "In TimeBank";
  case INST_LOC_AMSG:
    return "AutoMessage";
  case INST_LOC_SUBS:
    return "Reading Messages";
  case INST_LOC_CHUSER:
    return "Changing User";
  case INST_LOC_TEDIT:
    return "In TEDIT";
  case INST_LOC_MAILR:
    return "Reading All Mail";
  case INST_LOC_RESETQSCAN:
    return "Resetting QSCAN pointers";
  case INST_LOC_VOTEEDIT:
    return "In VOTEEDIT";
  case INST_LOC_VOTEPRINT:
    return "Printing Voting Data";
  case INST_LOC_RESETF:
    return "Resetting NAMES.LST";
  case INST_LOC_FEEDBACK:
    return "Leaving Feedback";
  case INST_LOC_KILLEMAIL:
    return "Viewing Old Email";
  case INST_LOC_POST:
    return "Posting a Message";
  case INST_LOC_NEWUSER:
    return "Registering a Newuser";
  case INST_LOC_RMAIL:
    return "Reading Email";
  case INST_LOC_DOWNLOAD:
    return "Downloading";
  case INST_LOC_UPLOAD:
    return "Uploading";
  case INST_LOC_BIXFER:
    return "Bi-directional Transfer";
  case INST_LOC_NETLIST:
    return "Listing Net Info";
  case INST_LOC_TERM:
    return "In a terminal program";
  case INST_LOC_GETUSER:
    return "Getting User ID";
  case INST_LOC_WFC:
    return "Waiting for Call";
  case INST_LOC_QWK:
    return "In QWK";
  }
  return "Unknown BBS Location!";
}

}
