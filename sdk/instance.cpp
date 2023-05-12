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
#include "sdk/instance.h"


#include "bbs/instmsg.h"
#include "core/datafile.h"
#include "core/file.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "sdk/chains.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/subxtr.h"
#include "sdk/vardec.h"
#include "sdk/files/dirs.h"

#include <algorithm>
#include <filesystem>

using namespace wwiv::core;
using namespace wwiv::strings;

namespace wwiv::sdk {

const std::filesystem::path& Instances::fn_path() const {
  return path_;
}

Instance::Instance(std::filesystem::path root_dir, std::filesystem::path data_dir, instancerec ir)
  : root_dir_(root_dir), data_dir_(data_dir), ir_(ir) {
}

Instance::Instance(std::filesystem::path root_dir, std::filesystem::path data_dir, int instance_num)
    : root_dir_(root_dir), data_dir_(data_dir), ir_() {
  ir_.number = static_cast<int16_t>(instance_num);
}

Instance::Instance(Instance&& o) noexcept : root_dir_(o.root_dir_), data_dir_(o.data_dir_), ir_(o.ir_) {}

Instance& Instance::operator=(const Instance& o) {
  data_dir_ = o.data_dir_;
  root_dir_ = o.root_dir_;
  ir_ = o.ir_;
  return *this;
}
  
//Instance& Instance::operator=(Instance&& o) noexcept {
//  data_dir_ = o.data_dir_;
//  ir_ = o.ir_;
//  return *this;
//}

const instancerec& Instance::ir() const {
  return ir_;
}

instancerec& Instance::ir() {
  return ir_;
}

std::filesystem::path Instance::root_directory() const {
  return root_dir_;
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
  if (ir_.loc >= INST_LOC_CH1 && ir_.loc <= INST_LOC_CH10) {
    return "WWIV Chatroom";
  }
  switch (ir_.loc) {
  case INST_LOC_DOWN:
    return "Offline";
  case INST_LOC_INIT:
    return "Initializing BBS";
  case INST_LOC_EMAIL:
    return "Sending Email";
  case INST_LOC_MAIN:
    return "Main Menu";
  case INST_LOC_XFER: {
    wwiv::sdk::files::Dirs dirs(data_dir_, 0);
    if (dirs.Load()) {
      if (ir_.subloc < dirs.size()) {
        return fmt::format("Transfer Area: Dir : {}", stripcolors(dirs[ir_.subloc].name));
      }
    }
    return "Transfer Area";
  }
  case INST_LOC_CHAINS: {
    Chains chains(data_dir_);
    if (chains.Load()) {
      if (ir_.subloc > 0 && ir_.subloc <= chains.size()) {
        const auto& c = chains.at(ir_.subloc - 1);
        return fmt::format("Chains: Door: {}", stripcolors(c.description));
      }
    }
  }
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
  case INST_LOC_SUBS: {
    std::vector<net::Network> nets;
    Subs subs(data_dir_, nets, 0);
    if (subs.Load()) {
      if (ir_.subloc < subs.size()) {
        return fmt::format("Reading Messages: (Sub: {})", stripcolors(subs.sub(ir_.subloc).name));
      }
    }
  }
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
  case INST_LOC_POST: {
    std::vector<net::Network> nets;
    Subs subs(data_dir_, nets, 0);
    if (subs.Load()) {
      if (ir_.subloc < subs.size()) {
        return fmt::format("Posting a Message: (Sub: {})", stripcolors(subs.sub(ir_.subloc).name));
      }
    }
  }
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
    : path_(FilePath(config.datadir(), INSTANCE_DAT)), root_dir_(config.root_directory()),
      data_dir_(config.datadir()) {
  initialized_ = File::Exists(path_);
  instances_ = all();
}

Instances::size_type Instances::size() const {
  if (const auto file = DataFile<instancerec>(path_, File::modeBinary | File::modeReadOnly)) {
    return std::max<int>(0, file.number_of_records() - 1);
  }
  return 0;
}

// ReSharper disable once CppMemberFunctionMayBeConst
Instance Instances::at(size_type pos) {
  if (auto file = DataFile<instancerec>(path_, File::modeBinary | File::modeReadOnly)) {
    instancerec ir{};
    if (file.Read(pos, &ir)) {
      return Instance(root_dir_, data_dir_, ir);
    }
  }
  return Instance(root_dir_, data_dir_, pos);
}

// ReSharper disable once CppMemberFunctionMayBeConst
std::vector<Instance> Instances::all() {
  if (auto file = DataFile<instancerec>(path_, File::modeBinary | File::modeReadOnly)) {
    std::vector<instancerec> ir;
    if (file.ReadVector(ir)) {
      std::vector<Instance> r;
      for (const auto& i : ir) {
        r.emplace_back(root_dir_, data_dir_, i);
      }
      return r;
    }
  }
  return {};
}

// ReSharper disable once CppMemberFunctionMayBeConst
bool Instances::upsert(size_type pos, const instancerec& ir) {
  instancerec mir{ ir };
  mir.last_update = daten_t_now();
  if (auto file = DataFile<instancerec>(path_, File::modeBinary | File::modeReadWrite |
                                                   File::modeCreateFile)) {
    return file.Write(pos, &mir);
  }
  return false;
}

bool Instances::upsert(size_type pos, const Instance& ir) {
  return upsert(pos, ir.ir());
}

}
