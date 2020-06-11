/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2014-2020, WWIV Software Services             */
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

#include <exception>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include <cereal/access.hpp>
#include <cereal/cereal.hpp>
#include <cereal/types/set.hpp>

#include "core/datafile.h"
#include "core/file.h"
#include "core/jsonfile.h"
#include "core/log.h"
#include "core/stl.h"
#include "core/strings.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/vardec.h"

using cereal::make_nvp;
using cereal::specialization;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

// We want to override how we store some enums as a string, not int.
// This has to be in the global namespace.
CEREAL_SPECIALIZE_FOR_ALL_ARCHIVES(chain_exec_mode_t, specialization::non_member_load_save_minimal);
CEREAL_SPECIALIZE_FOR_ALL_ARCHIVES(chain_exec_dir_t, specialization::non_member_load_save_minimal);

namespace cereal {

#define SERIALIZE(n, field)                                                                        \
  {                                                                                                \
    try {                                                                                          \
      ar(cereal::make_nvp(#field, n.field));                                                       \
    } catch (const cereal::Exception&) {                                                           \
      ar.setNextName(nullptr);                                                                     \
    }                                                                                              \
  }

template <typename T>
inline std::string to_enum_string(const T& t, const std::vector<std::string>& names) {
  try {
    return names.at(static_cast<int>(t));
  } catch (std::out_of_range&) {
    return names.at(0);
  }
}

template <typename T>
inline T from_enum_string(const std::string& v, const std::vector<std::string>& names) {
  try {
    for (auto i = 0; i < wwiv::stl::ssize(names); i++) {
      if (v == names.at(i)) {
        return static_cast<T>(i);
      }
    }
  } catch (std::out_of_range&) {
    // NOP
  }
  return static_cast<T>(0);
}

template <class Archive>
inline std::string save_minimal(Archive const&, const chain_exec_mode_t& t) {
  return Chains::exec_mode_to_string(t);
}
template <class Archive>
inline void load_minimal(Archive const&, chain_exec_mode_t& t, const std::string& s) {
  t = Chains::exec_mode_from_string(s);
}

template <class Archive>
inline std::string save_minimal(Archive const&, const chain_exec_dir_t& t) {
  return to_enum_string<const chain_exec_dir_t>(t, {"bbs", "temp"});
}
template <class Archive>
inline void load_minimal(Archive const&, chain_exec_dir_t& t, const std::string& v) {
  t = from_enum_string<const chain_exec_dir_t>(v, {"bbs", "temp"});
}

template <class Archive> void serialize(Archive& ar, chain_t& n) {
  SERIALIZE(n, filename);
  SERIALIZE(n, description);
  SERIALIZE(n, exec_mode);
  SERIALIZE(n, dir);
  SERIALIZE(n, ansi);
  SERIALIZE(n, local_only);
  SERIALIZE(n, multi_user);
  SERIALIZE(n, sl);
  SERIALIZE(n, ar);
  SERIALIZE(n, regby);
  SERIALIZE(n, usage);
  SERIALIZE(n, minage);
  SERIALIZE(n, maxage);
}

} // namespace cereal

namespace wwiv {
namespace sdk {

chain_exec_mode_t& operator++(chain_exec_mode_t& t) {
  using T = typename std::underlying_type<chain_exec_mode_t>::type;
  t = (t == chain_exec_mode_t::stdio ? chain_exec_mode_t::none
                                     : static_cast<chain_exec_mode_t>(static_cast<T>(t) + 1));
  return t;
}

chain_exec_mode_t operator++(chain_exec_mode_t& t, int) {
  using T = typename std::underlying_type<chain_exec_mode_t>::type;
  auto old = t;
  t = (t == chain_exec_mode_t::stdio ? chain_exec_mode_t::none
                                     : static_cast<chain_exec_mode_t>(static_cast<T>(t) + 1));
  return old;
}

chain_exec_dir_t& operator++(chain_exec_dir_t& t) {
  t = (t == chain_exec_dir_t::bbs ? chain_exec_dir_t::temp : chain_exec_dir_t::bbs);
  return t;
}

chain_exec_dir_t operator++(chain_exec_dir_t& t, int) {
  auto old = t;
  t = (t == chain_exec_dir_t::bbs ? chain_exec_dir_t::temp : chain_exec_dir_t::bbs);
  return old;
}

const int Chains::npos; // reserve space.

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

bool Chains::insert(int n, chain_t r) { return insert_at(chains_, n, r); }

bool Chains::erase(int n) { return erase_at(chains_, n); }

bool Chains::Load() {
  if (LoadFromJSON()) {
    return true;
  }
  return LoadFromDat();
}

bool Chains::LoadFromJSON() {
  chains_.clear();
  JsonFile json(FilePath(datadir_, CHAINS_JSON), "chains", chains_);
  return json.Load();
}

bool Chains::LoadFromDat() {
  DataFile<chainfilerec> old_chains(FilePath(datadir_, CHAINS_DAT),
                                    File::modeBinary | File::modeReadOnly, File::shareDenyNone);
  DataFile<chainregrec> regfile(FilePath(datadir_, CHAINS_REG),
                                File::modeBinary | File::modeReadOnly, File::shareDenyNone);
  if (!old_chains) {
    return false;
  }

  std::vector<chainfilerec> old;
  std::vector<chainregrec> reg;

  if (!old_chains.ReadVector(old)) {
    return false;
  }
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
    c.sl = o.sl;
    c.ar = o.ar;

    if (i < wwiv::stl::ssize(reg)) {
      // We have a chain.reg entry
      const auto& r = reg.at(i);
      for (int rb = 0; rb < 5; rb++) {
        auto rbc = r.regby[rb];
        if (rbc > 0) {
          c.regby.insert(rbc);
        }
      }
      c.usage = r.usage;
      c.minage = r.minage;
      c.maxage = r.maxage;
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
  JsonFile json(FilePath(datadir_, CHAINS_JSON), "chains", chains_);
  return json.Save();
}

bool Chains::SaveToDat() {
  std::vector<chainfilerec> cdisk;
  std::vector<chainregrec> rdisk;
  for (const auto& from : chains_) {
    chainfilerec c{};
    chainregrec r{};
    to_char_array(c.filename, from.filename);
    to_char_array(c.description, from.description);
    c.sl = from.sl;
    c.ansir = Chains::to_ansir(from);
    c.ar = from.ar;
    r.usage = from.usage;
    r.minage = from.minage;
    r.maxage = from.maxage;
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
  auto cwritten{false};
  auto rwritten{false};
  {
    DataFile<chainfilerec> cfile(FilePath(datadir_, CHAINS_DAT),
                                 File::modeBinary | File::modeReadWrite | File::modeCreateFile |
                                     File::modeTruncate,
                                 File::shareDenyReadWrite);
    if (!cfile) {
      return false;
    }
    cwritten = cfile.WriteVector(cdisk);
  }
  {
    DataFile<chainregrec> rfile(FilePath(datadir_, CHAINS_REG),
                                File::modeBinary | File::modeReadWrite | File::modeCreateFile |
                                    File::modeTruncate,
                                File::shareDenyReadWrite);
    if (rfile) {
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

void Chains::increment_chain_usage(int num) { chains_.at(num).usage++; }

// static
uint8_t Chains::to_ansir(chain_t c) {
  uint8_t r{ansir_no_DOS};

  if (c.ansi) {
    r |= ansir_ansi;
  }
  if (c.exec_mode == chain_exec_mode_t::dos) {
    r &= ~ansir_no_DOS;
  } else if (c.exec_mode == chain_exec_mode_t::fossil) {
    r |= ansir_emulate_fossil;
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

// static
std::string Chains::exec_mode_to_string(const chain_exec_mode_t& t) {
  return cereal::to_enum_string<chain_exec_mode_t>(t, {"none", "DOS", "FOSSIL", "STDIO"});
}

// static
chain_exec_mode_t Chains::exec_mode_from_string(const std::string& s) {
  return cereal::from_enum_string<chain_exec_mode_t>(s, {"none", "DOS", "FOSSIL", "STDIO"});
}

} // namespace sdk
} // namespace wwiv
