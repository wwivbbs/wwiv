/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2020, WWIV Software Services             */
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

#include "bbs/acs.h"
#include "bbs/bbs.h"
#include "bbs/confutil.h"
#include "bbs/mmkey.h"
#include "common/com.h"
#include "common/input.h"
#include "common/input_range.h"
#include "common/pause.h"
#include "core/log.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "fmt/printf.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/subxtr.h"
#include "sdk/files/dirs.h"
#include <filesystem>
#include <string>
#include <vector>

using std::string;
using namespace wwiv::core;
using namespace wwiv::stl;
using namespace wwiv::sdk;
using namespace wwiv::strings;

static int disable_conf_cnt = 0;

namespace wwiv::bbs {

TempDisableConferences::TempDisableConferences()
    : wwiv::core::Transaction([] { tmp_disable_conf(false); }, nullptr) {
  tmp_disable_conf(true);
}

} // namespace wwiv

void tmp_disable_conf(bool disable) {
  static int ocs = 0, oss = 0, ocd = 0, osd = 0;

  if (disable) {
    disable_conf_cnt++;
    if (okconf(a()->user())) {
      a()->sess().disable_conf(true);
      ocs = a()->sess().current_user_sub_conf_num();
      oss = a()->current_user_sub().subnum;
      ocd = a()->sess().current_user_dir_conf_num();
      osd = a()->current_user_dir().subnum;
      clear_usersubs(a()->all_confs().subs_conf(), a()->usub, oss);
      clear_usersubs(a()->all_confs().dirs_conf(), a()->udir, oss);
    }
  } else if (disable_conf_cnt) {
    disable_conf_cnt--;
    if (disable_conf_cnt == 0 && a()->sess().disable_conf()) {
      a()->sess().disable_conf(false);
      auto key = at(a()->uconfsub, ocs).key.key();
      setuconf(a()->all_confs().subs_conf(), key, oss);
      key = at(a()->uconfdir, ocd).key.key();
      setuconf(a()->all_confs().dirs_conf(), key, osd);
    }
  }
}

void reset_disable_conf() { disable_conf_cnt = 0; }

conf_info_t get_conf_info(ConferenceType conftype) {
  if (conftype == ConferenceType::CONF_DIRS) {
    conf_info_t ret(a()->all_confs().dirs_conf(), a()->uconfdir);
    ret.file_name = FilePath(a()->config()->datadir(), DIRS_CNF).string();
    ret.num_subs_or_dirs = a()->dirs().size();
    return ret;
  }

  conf_info_t ret(a()->all_confs().subs_conf(), a()->uconfsub);
  ret.file_name = FilePath(a()->config()->datadir(), SUBS_CNF).string();
  ret.num_subs_or_dirs = wwiv::stl::size_int(a()->subs().subs());
  return ret;
}

/**
 * Select a free (unused) conf key
 */
 static std::optional<char> select_free_conf_key(Conference& conf, bool listconfs) {
  if (listconfs) {
    bout.nl();
    list_confs(conf, false);
  }
  std::set<char> current;
  for (const auto& cp : conf.confs()) {
    current.insert(cp.key.key());
  }
  std::string allowed(" \r\n");
  for (auto c = 'A'; c <= 'Z'; c++) {
    if (!contains(current, c)) {
      allowed.push_back(c);
    }
  }
  bout.format("|#9Select a free conference key (|#1{}|#9): ", allowed);
  const auto key = onek(allowed, true);
  if (key == ' ' || key == '\r' || key == '\n') {
    return std::nullopt;
  }
  return { key };
}


/*
 * Presents user with selection of conferences, gets selection, and changes
 * conference.
 */
void jump_conf(ConferenceType conftype) {
  bout.litebar(StrCat(a()->config()->system_name(), " Conference Selection"));
  auto info = get_conf_info(conftype);
  string allowable = " ";
  bout.nl();
  for (const auto& uc : info.uc) {
    if (bin.checka())
      break;
    const auto ac = uc.key.key();
    bout << "|#2" << ac << "|#7)|#1 " << stripcolors(uc.conf_name) << "\r\n";
    allowable.push_back(ac);
  }

  bout.nl();
  bout << "|#2Select [" << allowable.substr(1) << ", <space> to quit]: ";
  const auto ch = onek(allowable);
  if (ch == ' ') {
    return;
  }
  auto& conf = conftype == ConferenceType::CONF_SUBS ? a()->all_confs().subs_conf() : a()->all_confs().dirs_conf();
  auto& uc = conftype == ConferenceType::CONF_SUBS ? a()->uconfsub : a()->uconfdir;
  if (const auto o = conf.try_conf(ch)) {
    setuconf(conf, o.value().key.key(), -1);
    for (int i=0; i < size_int(uc); i++) {
      if (ch == at(uc, i).key.key()) {
        if (conftype == ConferenceType::CONF_SUBS) {
          a()->sess().set_current_user_sub_conf_num(i);
        } else {
          a()->sess().set_current_user_dir_conf_num(i);
        }
      }
    }
  }
}

/*
 * Returns true if subnum is allocated to conference c, 0 otherwise.
 */
bool in_conference(subconf_t subnum, confrec_430_t* c) {
  if (!c) {
    return false;
  }
  for (const auto& s : c->subs) {
    if (s == subnum) {
      return true;
    }
  }
  return false;
}

/*
 * Returns first available conference key, of a specified conference
 * type.
 */
char first_available_key(const Conference& conf) {
  if (conf.size() == MAX_CONFERENCES) {
    return 0;
  }
  if (conf.confs().empty()) {
    return 'A';
  }
  auto cv = conf.confs();
  const auto last = std::end(cv);
  return static_cast<char>(last->key.key() + 1);
}

static int display_conf_subs(Conference& conf) {
  auto abort = false;
  bout.cls();
  bout.bpla("|#2NN  Name                                    ConfList", &abort);
  bout.bpla("|#7--- ======================================= ==========================", &abort);

  int count = 0;
  switch (conf.type()) {
  case ConferenceType::CONF_SUBS: {
    count = size_int(a()->subs().subs());
    for (auto i = 0; i < count; i++) {
      const auto& sub = a()->subs().sub(i);
      const auto s =
          fmt::sprintf("|#2%3d |#1%-39.39s |#5%s", i, stripcolors(sub.name), sub.conf.to_string());
      bout.bpla(s, &abort);
    }
  } break;
  case ConferenceType::CONF_DIRS: {
    count = size_int(a()->dirs().dirs());
    for (auto i = 0; i < count; i++) {
      const auto& dir = a()->dirs().dir(i);
      const auto s =
          fmt::sprintf("|#2%3d |#1%-39.39s |#5%s", i, stripcolors(dir.name), dir.conf.to_string());
      bout.bpla(s, &abort);
    }
  } break;
  }
  return count;
}

static conf_set_t& get_conf_set(Conference& conf, int num) {
  if (conf.type() == ConferenceType::CONF_DIRS) {
    return a()->dirs().dir(num).conf;
  }
  return a()->subs().sub(num).conf;
}

void edit_conf_subs(Conference& conf) {
  bool changed{false};
  while (!a()->sess().hangup()) {
    const auto count = display_conf_subs(conf);
    bout.nl();
    bout << "|#9(|#2Q|#9)uit, (|#2S|#9)et, (|#2C|#9)lear, (|#2T|#9)oggle conferences: ";
    const auto cmd = onek_ncr("CSTQ");
    if (cmd == 'Q') {
      break;
    }
    bout.Left(80);
    bout.clreol();
    bout << "|#9Enter conference key: ";
    const auto key = onek_ncr(StrCat("\r\n", conf.keys_string()));
    if (key == 0 || key == '\r' || key == '\n') {
      continue;
    }

    bout.Left(80);
    bout.clreol();
    bout << "|#9Enter range (i.e. 1-10, 5, etc): ";
    std::set<int> range;
    for (auto i=0; i <count; i++) {
      range.insert(i);
    }
    if (auto o = input_range(bin, 40, range)) {
      const auto& r = o.value();
      for (const auto n : r) {
        auto& cs = get_conf_set(conf, n);
        changed = true;
        switch (cmd) {  // NOLINT(hicpp-multiway-paths-covered)
        case 'S':
          cs.insert(key);
          break;
        case 'C':
          cs.erase(key);
          break;
        case 'T':
          cs.toggle(key);
          break;
        }
      }
    }
  }

  if (changed) {
    if (conf.type() == ConferenceType::CONF_DIRS) {
      a()->dirs().Save();      
    } else if (conf.type() == ConferenceType::CONF_SUBS) {
      a()->subs().Save();
    }
  }
}


/*
 * Function for editing the data for one conference.
 */
static void modify_conf(Conference& conf, char key) {
  auto changed = false;
  auto done = false;

  if (!conf.exists(key)) {
    return;
  }

  do {
    auto& c = conf.conf(key);
    bout.cls();
    bout.litebar("Edit Conference");

    bout << "|#9A) Key  : |#2" << c.key.key() << wwiv::endl;
    bout << "|#9B) Name : |#2" << c.conf_name << wwiv::endl;
    bout << "|#9C) ACS  : |#2" << c.acs << wwiv::endl;
    bout.nl();

    bout << "|#7(|#2Q|#7=|#1Quit|#7) Conference Edit [|#1A|#7-|#1C|#7] : ";
    const auto ch = onek("QABC", true);

    switch (ch) { // NOLINT(hicpp-multiway-paths-covered)
    case 'A': {
      bout.nl();
      bout << "|#2New Key: ";
      const auto ch1 = onek("\rABCDEFGHIJKLMNOPQRSTUVWXYZ");
      if (ch1 == c.key.key() || ch1 == '\r') {
        break;
      }
      if (conf.exists(ch1)) {
        bout.nl();
        bout << "|#6That key already in use!\r\n";
        bout.pausescr();
        break;
      }
      c.key.key(ch1);
      changed = true;
    } break;
    case 'B': {
      bout.nl();
      bout << "|#2Conference Name: ";
      auto cname = bin.input_text(60);
      if (!cname.empty()) {
        c.conf_name = cname;
        changed = true;
      }
    } break;
    case 'C': {
      bout.nl();
      bout << "|#2New ACS: ";
      c.acs = wwiv::bbs::input_acs(bin, bout, c.acs, 60);
      changed = true;
    } break;
    case 'Q':
      done = true;
      break;
    }
  } while (!done && !a()->sess().hangup());

  if (changed) {
    a()->all_confs().Save();
  }
}

/*
 * Function for inserting one conference.
 */
static void insert_conf(Conference& conf, char key) {

  conference_t c{};
  c.key.key(key);
  c.conf_name = StrCat("Conference ", c.key);
  c.acs = "";

  conf.add(c);

  if (!a()->all_confs().Save()) {
    LOG(ERROR) << "Unable to save conferences.";
  }

  modify_conf(conf, key);
}

/**
 * Function for deleting one conference.
 */
static void delete_conf(Conference& conf, char key) {
  conf.erase(key);
  a()->all_confs().Save();
}

/**
 * Function for editing conferences.
 */
void conf_edit(Conference& conf) {
  bool done = false;

  do {
    bout.cls();
    list_confs(conf, false);
    bout.nl();
    bout << "|#2I|#7)|#1nsert, |#2D|#7)|#1elete, |#2M|#7)|#1odify, |#2Q|#7)|#1uit, |#2S|#7)|#1ubs Configuration|#2? |#7 : ";
    const auto ch = onek("QIDMS?", true);
    switch (ch) { // NOLINT(hicpp-multiway-paths-covered)
    case 'D':
      if (conf.size() == 1) {
        bout << "\r\n|#6Cannot delete all conferences!\r\n";
      } else {
        if (auto ec = select_conf("Delete which conference? ", conf, false)) {
          delete_conf(conf, ec.value());
        }
      }
      break;
    case 'I':
      if (conf.size() == MAX_CONFERENCES) {
        bout << "\r\n|#6Cannot insert any more conferences!\r\n";
      } else {
        if (auto ec = select_free_conf_key(conf, false)){
          insert_conf(conf, ec.value());
        }
      }
      break;
    case 'M': {
      if (auto ec = select_conf("Modify which conference? ", conf, false)) {
        modify_conf(conf, ec.value());
      }
    } break;
    case 'S':
      edit_conf_subs(conf);
      break;
    case 'Q':
      done = true;
      break;
    case '?':
      bout.cls();
      list_confs(conf, false);
      break;
    }
  } while (!done && !a()->sess().hangup());
  if (!a()->at_wfc()) {
    changedsl();
  }
}

/*
 * Lists conferences of a specified type.
 */
void list_confs(ConferenceType conftype, bool list_subs) {
  if (conftype == ConferenceType::CONF_SUBS) {
    return list_confs(a()->all_confs().subs_conf(), list_subs);
  }
  return list_confs(a()->all_confs().dirs_conf(), list_subs);
}

void list_confs(Conference& conf, bool list_subs) {
  auto abort = false;
  bout.bpla("|#2Key Name                    ACS", &abort);
  bout.bpla("|#7--- ======================= -------------------------------------------------",
            &abort);

  for (const auto& cp : conf.confs()) {
    if (abort) {
      break;
    }
    const auto l = fmt::sprintf("|#2 %c |#1 %-23.23s |#5%s", cp.key.key(), cp.conf_name, cp.acs);
    bout.bpla(l, &abort);
    if (!list_subs) {
      continue;
    }
    auto subnum = 0;
    for (const auto& sub : a()->subs().subs()) {
      if (sub.conf.contains(cp.key.key())) {
        const auto ll = fmt::format("   \xC3\xC4\xC4\xC4 |#9Sub #{:<3} : {}", subnum, sub.name);
        bout.Color(7);
        bout.bpla(ll, &abort);
      }
      ++subnum;
    }
  }
  bout.nl();
}

/*
 * Allows user to select a conference. Returns the conference selected
 * (0-based), or -1 if the user aborted. Error message is printed for
 * invalid conference selections.
 */
std::optional<char> select_conf(const std::string& prompt_text, Conference& conf, bool listconfs) {
  if (listconfs) {
    bout.nl();
    list_confs(conf, false);
  }
  std::string allowed(" \r\n");
  for (const auto& cp : conf.confs()) {
    allowed.push_back(cp.key.key());
  }
  while (!a()->sess().hangup()) {
    if (!prompt_text.empty()) {
      bout.nl();
      bout << "|#1" << prompt_text;
    }
    auto key = onek(allowed, true);
    if (key == ' ' || key == '\r' || key == '\n') {
      return std::nullopt;
    }
    if (key == '?') {
      bout.nl();
      list_confs(conf, false);
      continue;
    }
    if (conf.exists(key)) {
      return {key};
    }
    bout << "\r\n|#6Invalid conference key!\r\n";
  }
  return std::nullopt;
}
