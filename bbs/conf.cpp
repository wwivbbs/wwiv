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


#include "acs.h"
#include "sdk/arword.h"
#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/confutil.h"
#include "bbs/mmkey.h"
#include "common/com.h"
#include "common/input.h"
#include "common/pause.h"
#include "core/log.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "fmt/printf.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/files/dirs.h"
#include "sdk/subxtr.h"
#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

using std::string;
using namespace wwiv::core;
using namespace wwiv::stl;
using namespace wwiv::sdk;
using namespace wwiv::strings;

static int disable_conf_cnt = 0;

/* Max line length in conference files */
static const size_t MAX_CONF_LINE = 4096;

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
  for (size_t i = 0; i < info.uc.size(); i++) {
    if (ch == info.uc[i].key) {
      setuconf(conftype, i, -1);
      break;
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


static std::string create_conf_str(std::set<char>& chars) {
  std::string s;
  for (const auto& c : chars) {
    s.push_back(c);
  }
  return s;
}

/* 
 * Lists subs/dirs/whatever allocated to a specific conference.
 *
 * TODO(rushfan): PORT THIS
static void showsubconfs(ConferenceType conftype, confrec_430_t* c) {
  if (!c) {
    return;
  }

  auto info = get_conf_info(conftype);

  auto abort = false;
  bout.bpla("|#2NN  Name                                    ConfList", &abort);
  bout.bpla("|#7--- ======================================= ==========================", &abort);

  for (subconf_t i = 0; i < info.num_subs_or_dirs && !abort; i++) {
    // build conf list string
    std::set<char> confs;
    for (int i1 = 0; i1 < info.num_confs; i1++) {
      if (in_conference(i, &info.confs[i1])) {
        confs.insert(info.confs[i1].key);
      }
    }
    auto confstr = create_conf_str(confs);

    switch (conftype) {
    case ConferenceType::CONF_SUBS: {
      const auto s = fmt::sprintf("|#2%3d |#9%-39.39s |#1%s", i,
                                  stripcolors(a()->subs().sub(i).name), confstr);
      bout.bpla(s, &abort);
    } break;
    case ConferenceType::CONF_DIRS: {
      const auto s = fmt::sprintf("|#2%3d |#9%-39.39s |#1%s", i,
                                  stripcolors(a()->dirs()[i].name), confstr);
      bout.bpla(s, &abort);
    } break;
    }
  }
}
*/

/*
 * Takes a string like "100-150,201,333" and returns pointer to list of
 * numbers. Number of numbers in the list is returned in numinlist.
 */
static bool str_to_numrange(const char* pszNumbersText, std::vector<subconf_t>& list) {
  subconf_t intarray[1024];

  // init vars
  memset(intarray, 0, sizeof(intarray));
  list.clear();

  // check for bin.input( string
  if (!pszNumbersText) {
    return false;
  }

  // get num "words" in bin.input( string
  auto num_words = wordcount(pszNumbersText, ",");

  for (auto word = 1; word <= num_words; word++) {
    a()->CheckForHangup();
    if (a()->sess().hangup()) {
      return false;
    }

    const auto temp = extractword(word, pszNumbersText, ",");
    auto range_count = wordcount(temp, " -\t\r\n");
    switch (range_count) {
    case 0:
      break;
    case 1: {
      // This means there is no number in the range, it's just ,###,###
      auto num = to_number<int>(extractword(1, temp, " -\t\r\n"));
      if (num < 1024 && num >= 0) {
        intarray[num] = 1;
      }
    } break;
    default: {
      // We're dealing with a range here, so it should be "XXX-YYY"
      // convert the left and right strings to numbers
      auto low_number = to_number<int>(extractword(1, temp, " -\t\r\n,"));
      auto high_number = to_number<int>(extractword(2, temp, " -\t\r\n,"));
      // Switch them around if they were reversed
      if (low_number > high_number) {
        std::swap(low_number, high_number);
      }
      for (auto i = std::max<int>(low_number, 0); i <= std::min<int>(high_number, 1023); i++) {
        intarray[i] = 1;
      }
    } break;
    }
  }

  // allocate memory for list
  list.clear();
  for (subconf_t loop = 0; loop < 1024; loop++) {
    if (intarray[loop]) {
      list.push_back(loop);
    }
  }
  return true;
}


/*
 * Function for editing the data for one conference.
 */
static void modify_conf(Conference& conf, char key) {
  bool changed = false;
  bool done = false;

  if (!conf.exists(key)) {
    return;
  }

  do {
    auto& c = conf.conf(key);
    bout.cls();

    bout << "|#9A) Designator : |#2" << c.key.key() << wwiv::endl;
    bout << "|#9B) Conf Name  : |#2" << c.conf_name << wwiv::endl;
    bout << "|#9C) ACS        : |#2" << c.conf_name << wwiv::endl;
    bout << "|#9Q) Quit ConfEdit\r\n";
    bout.nl();

    bout << "|#7(|#2Q|#7=|#1Quit|#7) ConfEdit [|#1A|#7-|#1O|#7,|#1S|#7,|#1[|#7,|#1]|#7] : ";
    char ch = onek("QABC", true);

    switch (ch) {
    case 'A': {
      bout.nl();
      bout << "|#2New Designator: ";
      const auto ch1 = onek("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
      if (ch1 == c.key.key()) {
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
      bout << "|#2ACSL: ";
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
      ;
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

  bout.cls();
  list_confs(conf, true);

  do {
    bout.nl();
    bout << "|#2I|#7)|#1nsert, |#2D|#7)|#1elete, |#2M|#7)|#1odify, |#2Q|#7)|#1uit, |#2? |#7 : ";
    auto ch = onek("QIDM?", true);
    switch (ch) {
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
    case 'Q':
      done = true;
      break;
    case '?':
      bout.cls();
      list_confs(conf, true);
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
  bout.bpla("|#2  Des Name                    ACS",
            &abort);
  bout.bpla("|#7\xC9\xCD\xCD\xCD\xCD "
            "\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD"
            "\xCD\xCD \xCD\xCD\xCD \xCD\xCD\xCD \xCD\xCD\xCD\xCD \xCD\xCD\xCD\xCD \xCD\xCD\xCD\xCD "
            "\xCD\xCD\xCD\xCD \xCD\xCD\xCD\xCD\xCD \xCD\xCD\xCD \xCD\xCD\xCD \xCD \xCD \xCD",
            &abort);

  for (const auto& cp : conf.confs()) {
    if (abort) {
      break;
    }
    const auto l = fmt::sprintf("\xCC\xCD|#2 %c |#1 %-23.23s |#5%s", cp.key.key(), cp.conf_name, cp.acs);
    bout.Color(7);
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


/*
 * Returns number of "words" in a specified string, using a specified set
 * of characters as delimiters.
 */
int wordcount(const string& instr, const char* delimstr) {
  char szTempBuffer[MAX_CONF_LINE];
  int i = 0;

  to_char_array(szTempBuffer, instr);
  for (char* s = strtok(szTempBuffer, delimstr); s; s = strtok(nullptr, delimstr)) {
    i++;
  }
  return i;
}

/*
 * Returns pointer to string representing the nth "word" of a string, using
 * a specified set of characters as delimiters.
 */
std::string extractword(int ww, const string& instr, const char* delimstr) {
  char szTempBuffer[MAX_CONF_LINE];
  int i = 0;

  if (!ww) {
    return {};
  }

  to_char_array(szTempBuffer, instr);
  for (auto s = strtok(szTempBuffer, delimstr); s && (i++ < ww); s = strtok(nullptr, delimstr)) {
    if (i == ww) {
      return string(s);
    }
  }
  return {};
}
