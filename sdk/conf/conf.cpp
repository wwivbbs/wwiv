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
#include "bbs/conf.h"

#include "sdk/acs/expr.h"
#include "core/jsonfile.h"
#include "core/log.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "fmt/printf.h"
#include "sdk/arword.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/files/dirs.h"
#include "sdk/conf/conf_cereal.h"
#include "sdk/subxtr.h"
#include <filesystem>
#include <string>
#include <vector>

using std::string;
using namespace wwiv::core;
using namespace wwiv::stl;
using namespace wwiv::strings;

namespace wwiv::sdk {

static std::filesystem::path get_conf_430_path(const std::filesystem::path& datadir,
                                               ConferenceType conftype) {
  if (conftype == ConferenceType::CONF_DIRS) {
    return FilePath(datadir, DIRS_CNF);
  }
  return FilePath(datadir, SUBS_CNF);
}

/*
 * Saves conferences of a specified conference-type.
 */
bool save_confs_430(const Config& config, std::vector<confrec_430_t> confs,
                    ConferenceType conftype) {
  const auto path = get_conf_430_path(config.datadir(), conftype);
  // No longer need to back this up since it is not source of truth anymore.

  TextFile f(path, "wt");
  if (!f.IsOpen()) {
    LOG(ERROR) << "Couldn't write to conference file: " << path.filename().string() << std::endl;
    return false;
  }

  f.Write(fmt::sprintf("%s\n\n",
                       "/* !!!-!!! key not edit this file - use WWIV's conf editor! !!!-!!! */"));
  for (const auto& cp : confs) {
    const auto s =
        fmt::sprintf("~%c %s\n!%d %d %d %d %d %d %d %u %d %s %s\n@", cp.key, cp.conf_name,
                     cp.status, cp.minsl, cp.maxsl, cp.mindsl, cp.maxdsl, cp.minage, cp.maxage,
                     cp.minbps, cp.sex, word_to_arstr(cp.ar, "-"), word_to_arstr(cp.dar, "-"));
    f.Write(s);
    for (const auto sub : cp.subs) {
      f.Write(StrCat(sub, " "));
    }
    f.Write(fmt::sprintf("\n\n"));
  }
  f.Close();
  return true;
}

Conference::Conference(ConferenceType type, const std::set<conference_t>& confs) : type_(type) {
  for (const auto& c : confs) {
    confs_.try_emplace(c.key.key(), c);
  }
}

const conference_t& Conference::conf(char key) const {
  return at(confs_, key);
}

std::optional<conference_t> Conference::try_conf(char key) const {
  if (!contains(confs_, key)) {
    return std::nullopt;
  }
  return {at(confs_, key)};
}

conference_t& Conference::conf(char key) {
  return at(confs_, key);
}

conference_t& Conference::front() {
  if (confs_.empty()) {
    DLOG(FATAL) << "Conference::front() called on empty conference";
  }
  auto it = std::begin(confs_);
  return it->second;
}

bool Conference::exists(char key) const {
  return contains(confs_, key);
}

std::set<conference_t> Conference::confs() const {
  std::set<conference_t> r{};
  for (const auto& [k, v] : confs_) {
    r.insert(v);
  }
  return r;
}

std::string Conference::keys_string() const {
  std::string out;
  for (const auto& [k, _] : confs_) {
    out.push_back(k);
  }
  return out;
}

bool Conference::add(conference_t r) {
  auto [_, happened] = confs_.try_emplace(r.key.key(), r);
  return happened;
}

bool Conference::erase(char key) {
  return confs_.erase(key) > 0;
}

Conferences::Conferences(const std::string& datadir, Subs& subs, files::Dirs& dirs, int max_backups)
  : datadir_(datadir), subs_(subs), dirs_(dirs), max_backups_(max_backups) {
  if (auto o = Load()) {
    LoadFromFile(o.value());
  } else {
    std::set<conference_t> empty_set;
    subs_conf_ = std::make_unique<Conference>(ConferenceType::CONF_SUBS, empty_set);
    dirs_conf_ = std::make_unique<Conference>(ConferenceType::CONF_DIRS, empty_set);
  }
}

std::optional<conference_file_t> Conferences::Load() const {
  conference_file_t c{};
  const auto path = FilePath(datadir_, "conference.json");
  JsonFile f(path, "conf", c, 1);
  if (!f.Load()) {
    return std::nullopt;
  }
  return { c};
}

bool Conferences::LoadFromFile(const conference_file_t& f) {
  subs_conf_ = std::make_unique<Conference>(ConferenceType::CONF_SUBS, f.subs);
  dirs_conf_ = std::make_unique<Conference>(ConferenceType::CONF_DIRS, f.dirs);
  return true;
}

bool Conferences::Save() {
  conference_file_t t{};
  t.subs = subs_conf().confs();
  t.dirs = dirs_conf().confs();
  const auto path = FilePath(datadir_, "conference.json");
  JsonFile f(path, "conf", t, 1);
  return f.Save();

}


/////////////////////////////////////////////////////////////////////////////
//
// 4.30 support and upgrading code
//
std::vector<confrec_430_t> read_conferences_430(const std::filesystem::path& path) {
  if (!File::Exists(path)) {
    return {};
  }

  TextFile f(path, "rt");
  if (!f.IsOpen()) {
    return {};
  }

  std::vector<confrec_430_t> result;
  string ls;
  confrec_430_t c{};
  while (f.ReadLine(&ls) && result.size() < MAX_CONFERENCES) {
    StringTrim(&ls);
    if (ls.empty()) {
      continue;
    }
    const auto id = ls.front();
    ls = ls.substr(1);
    switch (id) {
    case '~': {
      if (ls.size() > 2 && ls[1] == ' ') {
        c.key = ls.front();
        auto name = ls.substr(2);
        StringTrim(&name);
        c.conf_name = name;
      }
    } break;
    case '!': {
      // data about the conference
      auto words = SplitString(ls, DELIMS_WHITE);
      if (!c.key || words.size() < 10) {
        LOG(ERROR) << "Invalid conf line: '" << ls << "'";
        c.maxsl = 255;
        c.maxdsl = 255;
        c.maxage = 255;
        c.sex = 2;
        break;
      }
      auto it = words.begin();
      auto status = *it++;
      c.status = to_number<uint16_t>(status.substr(1));
      c.minsl = to_number<uint8_t>(*it++);
      c.maxsl = to_number<uint8_t>(*it++);
      c.mindsl = to_number<uint8_t>(*it++);
      c.maxdsl = to_number<uint8_t>(*it++);
      c.minage = to_number<uint8_t>(*it++);
      c.maxage = to_number<uint8_t>(*it++);
      c.minbps = to_number<uint16_t>(*it++);
      c.sex = to_number<uint8_t>(*it++);
      c.ar = static_cast<uint16_t>(str_to_arword(*it++));
      c.dar = static_cast<uint16_t>(str_to_arword(*it++));
    } break;
    case '@':
      // Sub numbers.
      if (!c.key) {
        break;
      }
      auto words = SplitString(ls, DELIMS_WHITE);
      for (auto& word : words) {
        StringTrim(&word);
        if (word.empty())
          continue;
        c.subs.insert(to_number<uint16_t>(word));
      }
      c.num = static_cast<subconf_t>(c.subs.size());
      c.maxnum = c.num;
      result.push_back(c);
      c = {};
      break;
    }
  }
  f.Close();
  return result;
}

conference_t to_conference_t(const confrec_430_t& c4) {
  conference_t c56{};
  c56.key.key(c4.key);
  c56.conf_name = c4.conf_name;
  // N.B. There is no longer any offline, bps, nor ansi status check.
  acs::AcsExpr ae;
  ae.min_sl(c4.minsl).max_sl(c4.maxsl).min_dsl(c4.mindsl).max_dsl(c4.maxdsl);
  ae.min_age(c4.minage).max_age(c4.maxage).ar_int(c4.ar).dar_int(c4.dar);
  ae.regnum(c4.status & conf_status_wwivreg);
  c56.acs = ae.get();
  return c56;
}

conference_file_t UpgradeConferences(const Config& config, Subs& subs, files::Dirs& dirs) {
  auto subconf =
      read_conferences_430(get_conf_430_path(config.datadir(), ConferenceType::CONF_SUBS));
  auto dirconf =
      read_conferences_430(get_conf_430_path(config.datadir(), ConferenceType::CONF_DIRS));

  conference_file_t confs56{};

  for (const auto& c4 : subconf) {
    auto c56 = to_conference_t(c4);
    // Now to emplace this into the subs.
    for (const auto subnum : c4.subs) {
      try {
        auto& sub = subs.sub(subnum);
        LOG(INFO) << "Inserting conf: '" << c56.key << "' into sub : " << sub.name;
        sub.conf.insert(c56.key.key());
      } catch (const std::exception& e) {
        LOG(ERROR) << "Exception adding sub number: " << subnum << " in conf: '" << c56.key
                   << "': " << e.what();
      }
    }
    confs56.subs.insert(c56);
  }

  for (const auto& c4 : dirconf) {
    auto c56 = to_conference_t(c4);
    // Now to emplace this into the subs.
    for (const auto subnum : c4.subs) {
      try {
        auto& dir = dirs.dir(subnum);
        LOG(INFO) << "Inserting conf: '" << c56.key << "' into dir : " << dir.name;
        dir.conf.insert(c56.key.key());
      } catch (const std::exception& e) {
        LOG(ERROR) << "Exception adding sub number: " << subnum << " in conf: '" << c56.key
                   << "': " << e.what();
      }
    }
    confs56.dirs.insert(c56);
  }
  return confs56;
}

} // namespace wwiv::sdk
