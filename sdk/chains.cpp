/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2014-2022, WWIV Software Services             */
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
#include "sdk/chains.h"

#include "core/datafile.h"
#include "core/file.h"
#include "core/jsonfile.h"
#include "core/log.h"
#include "core/stl.h"
#include "core/strings.h"
#include "sdk/acs/expr.h"
#include "sdk/chains_cereal.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/vardec.h"
#include <type_traits>
#include <vector>

using cereal::make_nvp;
using cereal::specialization;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;


namespace wwiv::sdk {

chain_exec_mode_t& operator++(chain_exec_mode_t& t) {
  using T = std::underlying_type<chain_exec_mode_t>::type;
  t = (t == chain_exec_mode_t::sock_unix ? chain_exec_mode_t::none
                                     : static_cast<chain_exec_mode_t>(static_cast<T>(t) + 1));
  return t;
}

chain_exec_mode_t operator++(chain_exec_mode_t& t, int) {
  using T = std::underlying_type<chain_exec_mode_t>::type;
  const auto old = t;
  t = (t == chain_exec_mode_t::sock_unix ? chain_exec_mode_t::none
                                     : static_cast<chain_exec_mode_t>(static_cast<T>(t) + 1));
  return old;
}

chain_exec_dir_t& operator++(chain_exec_dir_t& t) {
  t = (t == chain_exec_dir_t::bbs ? chain_exec_dir_t::temp : chain_exec_dir_t::bbs);
  return t;
}

chain_exec_dir_t operator++(chain_exec_dir_t& t, int) {
  const auto old = t;
  t = (t == chain_exec_dir_t::bbs ? chain_exec_dir_t::temp : chain_exec_dir_t::bbs);
  return old;
}

const Chains::size_type Chains::npos; // reserve space.

Chains::Chains(const Config& config) : datadir_(config.datadir()) {
  if (!config.IsInitialized()) {
    return;
  }

  if (!File::Exists(FilePath(datadir_, CHAINS_JSON)) &&
      !File::Exists(FilePath(datadir_, CHAINS_DAT))) {
    return;
  }

  initialized_ = Load();
  if (!initialized_) {
    LOG(ERROR) << "Failed to read " << CHAINS_JSON << " or " << CHAINS_DAT;
  }
}

Chains::~Chains() = default;

bool Chains::insert(size_type n, chain_t r) { return insert_at(chains_, n, r); }

bool Chains::erase(size_type n) { return erase_at(chains_, n); }

bool Chains::Load() {
  if (LoadFromJSON()) {
    return true;
  }
  return LoadFromDat();
}

bool Chains::LoadFromJSON() {
  chains_.clear();
  JsonFile json(FilePath(datadir_, CHAINS_JSON), "chains", chains_, 1);
  return json.Load();
}

bool Chains::LoadFromDat() {
  DataFile<chainfilerec_422> old_chains(FilePath(datadir_, CHAINS_DAT),
                                    File::modeBinary | File::modeReadOnly, File::shareDenyNone);
  if (!old_chains) {
    return false;
  }

  std::vector<chainfilerec_422> old;
  std::vector<chainregrec_422> reg;

  if (!old_chains.ReadVector(old)) {
    return false;
  }
  DataFile<chainregrec_422> regfile(FilePath(datadir_, CHAINS_REG),
                                File::modeBinary | File::modeReadOnly, File::shareDenyNone);
  regfile.ReadVector(reg);

  for (auto i = 0; i < wwiv::stl::ssize(old); i++) {
    const auto& o = old.at(i);
    chain_t c{};
    c.filename = o.filename;
    c.description = o.description;
    if (o.ansir & ansir_emulate_fossil) {
      c.exec_mode = chain_exec_mode_t::fossil;
    } else if (o.ansir & ansir_stdio) {
      c.exec_mode = chain_exec_mode_t::stdio;
    } else if (!(o.ansir & ansir_no_DOS)) {
      c.exec_mode = chain_exec_mode_t::dos;
    } else {
      c.exec_mode = chain_exec_mode_t::none;
    }
    c.dir = (o.ansir & ansir_temp_dir) ? chain_exec_dir_t::temp : chain_exec_dir_t::bbs;
    c.ansi = (o.ansir & ansir_ansi);
    c.local_only = (o.ansir & ansir_local_only);
    c.multi_user = (o.ansir & ansir_multi_user);

    if (i < wwiv::stl::ssize(reg)) {
      // We have a chain.reg entry
      const auto& r = reg.at(i);
      for (auto rbc : r.regby) {
        if (rbc > 0) {
          c.regby.insert(rbc);
        }
      }
      acs::AcsExpr ae;
      c.acs = ae.min_sl(o.sl).ar_int(o.ar).min_age(r.minage).max_age(r.maxage).get();
      c.usage = r.usage;
    } else {
      acs::AcsExpr ae;
      c.acs = ae.min_sl(o.sl).ar_int(o.ar).get();
    }

    chains_.emplace_back(c);
  }
  return true;
}

bool Chains::Save() {
  const auto dat = SaveToDat();
  const auto json = SaveToJSON();

  return dat && json;
}

bool Chains::SaveToJSON() {
  JsonFile json(FilePath(datadir_, CHAINS_JSON), "chains", chains_, 1);
  return json.Save();
}

bool Chains::SaveToDat() {
  std::vector<chainfilerec_422> cdisk;
  std::vector<chainregrec_422> rdisk;
  for (const auto& from : chains_) {
    chainfilerec_422 c{};
    chainregrec_422 r{};
    to_char_array(c.filename, from.filename);
    to_char_array(c.description, from.description);
    c.ansir = static_cast<uint8_t>(to_ansir(from) & 0xff);
    //c.sl = from.sl;
    //c.ar = from.ar;
    //r.minage = from.minage;
    //r.maxage = from.maxage;
    r.usage = from.usage;
    int regbycount = 0;
    for (const auto& rbc : from.regby) {
      if (regbycount > 4) {
        break;
      }
      r.regby[regbycount] = rbc;
      regbycount++;
    }
    cdisk.emplace_back(c);
    rdisk.emplace_back(r);
  }
  bool cwritten;
  auto rwritten{false};
  {
    DataFile<chainfilerec_422> cfile(FilePath(datadir_, CHAINS_DAT),
                                 File::modeBinary | File::modeReadWrite | File::modeCreateFile |
                                     File::modeTruncate,
                                 File::shareDenyReadWrite);
    if (!cfile) {
      return false;
    }
    cwritten = cfile.WriteVector(cdisk);
  }
  {
    if (DataFile<chainregrec_422> rfile(FilePath(datadir_, CHAINS_REG),
                                        File::modeBinary | File::modeReadWrite | File::modeCreateFile |
                                        File::modeTruncate,
                                        File::shareDenyReadWrite); rfile) {
      rwritten = rfile.WriteVector(rdisk);
    }
  }
  return cwritten && rwritten;
}

bool Chains::HasRegisteredChains() const {
  for (const auto& c : chains_) {
    if (!c.regby.empty()) {
      return true;
    }
  }
  return false;
}

void Chains::increment_chain_usage(int num) { wwiv::stl::at(chains_, num).usage++;
}

// static
uint16_t Chains::to_ansir(const chain_t& c) {
  uint16_t r{ansir_no_DOS};

  if (c.ansi) {
    r |= ansir_ansi;
  }
  if (c.exec_mode == chain_exec_mode_t::dos) {
    r &= ~ansir_no_DOS;
  } else if (c.exec_mode == chain_exec_mode_t::fossil) {
    r |= ansir_emulate_fossil;
  } else if (c.exec_mode == chain_exec_mode_t::netfoss) {
    r |= ansir_netfoss;
  } else if (c.exec_mode == chain_exec_mode_t::stdio) {
    r |= ansir_stdio;
  } else if (c.exec_mode == chain_exec_mode_t::none) {
    // Do nothing
  }
  if (c.dir == chain_exec_dir_t::temp) {
    r |= ansir_temp_dir;
  }
  if (c.local_only) {
    r |= ansir_local_only;
  }
  if (c.multi_user) {
    r |= ansir_multi_user;
  }
  return r;
}

uint32_t Chains::to_exec_flags(const chain_t& c) { 
  auto r = to_exec_flags(c.exec_mode); 
  if (c.dir == chain_exec_dir_t::temp) {
    r |= EFLAG_TEMP_DIR;
  }
  return r;
}

// static
uint32_t Chains::to_exec_flags(chain_exec_mode_t m) { 
  switch (m) {
  case chain_exec_mode_t::fossil:
    return EFLAG_SYNC_FOSSIL;
  case chain_exec_mode_t::stdio:
    return EFLAG_STDIO;
  case chain_exec_mode_t::netfoss:
    // NetFoss implies temp dir too.
    return EFLAG_NETFOSS | EFLAG_TEMP_DIR;
  case chain_exec_mode_t::sock_port:
    return EFLAG_LISTEN_SOCK;
  case chain_exec_mode_t::sock_unix:
    return EFLAG_UNIX_SOCK;
  case chain_exec_mode_t::dos:
    return EFLAG_COMIO;
  case chain_exec_mode_t::none:
  default:
    return 0;
  }
}

} // namespace wwiv
