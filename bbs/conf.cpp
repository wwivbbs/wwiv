/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2017, WWIV Software Services             */
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

#include <algorithm>
#include <vector>
#include <string>

#include "bbs/arword.h"
#include "bbs/confutil.h"
#include "bbs/input.h"
#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/com.h"
#include "bbs/mmkey.h"
#include "bbs/pause.h"
#include "core/log.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "core/wwivassert.h"
#include "sdk/filenames.h"

using std::string;
using namespace wwiv::core;
using namespace wwiv::stl;
using namespace wwiv::strings;

static int disable_conf_cnt = 0;

/* Max line length in conference files */
#define MAX_CONF_LINE 4096

/* To prevent heap fragmentation, allocate confrec.subs in multiples. */
#define CONF_MULTIPLE ( a()->config()->config()->max_subs / 5 )

// Locals
char* GetGenderAllowed(int nGender, char *pszGenderAllowed);
int  modify_conf(ConferenceType conftype, int which);
void insert_conf(ConferenceType conftype, int n);
void delete_conf(ConferenceType conftype, int n);
bool create_conf_file(ConferenceType conftype);

namespace wwiv {
namespace bbs {

TempDisableConferences::TempDisableConferences() : wwiv::core::Transaction([] {
  tmp_disable_conf(false);
  }, nullptr) {
  tmp_disable_conf(true);
}

}  // namespace bbs
}  // namespace wwiv

void tmp_disable_conf(bool disable) {
  static int ocs = 0, oss = 0, ocd = 0, osd = 0;

  if (disable) {
    disable_conf_cnt++;
    if (okconf(a()->user())) {
      a()->context().disable_conf(true);
      ocs = a()->GetCurrentConferenceMessageArea();
      oss = a()->current_user_sub().subnum;
      ocd = a()->GetCurrentConferenceFileArea();
      osd = a()->current_user_dir().subnum;
      setuconf(ConferenceType::CONF_SUBS, -1, oss);
      setuconf(ConferenceType::CONF_DIRS, -1, osd);
    }
  } else if (disable_conf_cnt) {
    disable_conf_cnt--;
    if ((disable_conf_cnt == 0) && a()->context().disable_conf()) {
      a()->context().disable_conf(false);
      setuconf(ConferenceType::CONF_SUBS, ocs, oss);
      setuconf(ConferenceType::CONF_DIRS, ocd, osd);
    }
  }
}

void reset_disable_conf() {
  disable_conf_cnt = 0;
}

conf_info_t get_conf_info(ConferenceType conftype) {
  if (conftype == ConferenceType::CONF_DIRS) {
    conf_info_t ret(a()->dirconfs, a()->uconfdir);
    ret.file_name = FilePath(a()->config()->datadir(), DIRS_CNF);
    ret.num_subs_or_dirs = a()->directories.size();
    return ret;
  }

  conf_info_t ret(a()->subconfs, a()->uconfsub);
  ret.file_name = FilePath(a()->config()->datadir(), SUBS_CNF);
  ret.num_subs_or_dirs = a()->subs().subs().size();
  return ret;
}

static string get_conf_filename(ConferenceType conftype) {
  if (conftype == ConferenceType::CONF_SUBS) {
    return FilePath(a()->config()->datadir(), SUBS_CNF);
  }
  else if (conftype == ConferenceType::CONF_DIRS) {
    return FilePath(a()->config()->datadir(), DIRS_CNF);
  }
  return{};
}

/*
 * Presents user with selection of conferences, gets selection, and changes
 * conference.
 */
void jump_conf(ConferenceType conftype) {
  bout.litebar(StrCat(a()->config()->system_name(), " Conference Selection"));
  conf_info_t info = get_conf_info(conftype);
  string allowable = " ";
  bout.nl();
  for (const auto& uc : info.uc) {
    if (uc.confnum == -1 || checka()) break;
    const auto ac = static_cast<char>(info.confs[uc.confnum].designator);
    bout << "|#2" << ac << "|#7)|#1 "
      << stripcolors(info.confs[uc.confnum].conf_name)
      << "\r\n";
    allowable.push_back(ac);
  }

  bout.nl();
  bout << "|#2Select [" << allowable.substr(1) << ", <space> to quit]: ";
  char ch = onek(allowable);
  if (ch == ' ') {
    return;
  }
  for (size_t i = 0; i < info.uc.size() && info.uc[i].confnum != -1; i++) {
    if (ch == info.confs[info.uc[i].confnum].designator) {
      setuconf(conftype, i, -1);
      break;
    }
  }
}

/*
 * Removes, inserts, or swaps subs/dirs in all conferences of a specified
 * type.
 */
void update_conf(ConferenceType conftype, subconf_t * sub1, subconf_t * sub2, int action) {
  auto info = get_conf_info(conftype);

  switch (action) {
  case CONF_UPDATE_INSERT:
    if (!sub1) {
      return;
    }
    for (auto& c : info.confs) {
      c.subs.insert(*sub1);
    }
    save_confs(conftype);
    break;
  case CONF_UPDATE_DELETE:
    if (!sub1) {
      return;
    }
    for (auto& c : info.confs) {
      c.subs.erase(*sub1);
    }
    save_confs(conftype);
    break;
  case CONF_UPDATE_SWAP:
    if (!sub1 || !sub2) {
      return;
    }
    for (auto& c : info.confs) {
      bool has1 = in_conference(*sub1, &c);
      bool has2 = in_conference(*sub2, &c);
      if (has1) {
        c.subs.erase(*sub1);
        c.subs.insert(*sub2);
      }
      if (has2) {
        c.subs.erase(*sub2);
        c.subs.insert(*sub1);
      }
    }
    save_confs(conftype);
    break;
  }
}

/*
 * Returns first available conference designator, of a specified conference
 * type.
 */
char first_available_designator(ConferenceType conftype) {
  auto info = get_conf_info(conftype);
  if (info.confs.size() == MAX_CONFERENCES) {
    return 0;
  }
  if (info.confs.empty()) {
    return 'A';
  }
  auto last = info.confs.back();
  return last.designator + 1;
}

/*
 * Returns 1 if subnum is allocated to conference c, 0 otherwise.
 */
bool in_conference(subconf_t subnum, confrec* c) {
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
 * Saves conferences of a specified conference-type.
 */
bool save_confs(ConferenceType conftype) {
  auto info = get_conf_info(conftype);

  TextFile f(info.file_name, "wt");
  if (!f.IsOpen()) {
    bout.nl();
    bout << "|#6Couldn't write to conference file: " << info.file_name << wwiv::endl;
    return false;
  }

  f.WriteFormatted("%s\n\n", "/* !!!-!!! Do not edit this file - use WWIV's conf editor! !!!-!!! */");
  for (const auto& cp : info.confs) {
    f.WriteFormatted("~%c %s\n!%d %d %d %d %d %d %d %u %d %s %s\n@", cp.designator,
                     cp.conf_name.c_str(), cp.status, cp.minsl, cp.maxsl, cp.mindsl, cp.maxdsl,
                     cp.minage, cp.maxage, cp.minbps, cp.sex, word_to_arstr(cp.ar).c_str(),
                     word_to_arstr(cp.dar).c_str());
    for (const auto sub : cp.subs) {
      f.Write(StrCat(sub, " "));
    }
    f.WriteFormatted("\n\n");
  }
  f.Close();
  return true;
}

static std::string create_conf_str(std::set<char> chars) {
  std::string s;
  for (const auto& c : chars) {
    s.push_back(c);
  }
  return s;
}

/*
 * Lists subs/dirs/whatever allocated to a specific conference.
 */
void showsubconfs(ConferenceType conftype, confrec * c) {
  char s[120];

  if (!c) {
    return;
  }

  auto info = get_conf_info(conftype);

  bool abort = false;
  bout.bpla("|#2NN  Name                                    ConfList", &abort);
  bout.bpla("|#7--- ======================================= ==========================", &abort);

  for (subconf_t i = 0; i < info.num_subs_or_dirs && !abort; i++) {
    // build conf list string
    std::set<char> confs;
    for (int i1 = 0; i1 < info.num_confs; i1++) {
      if (in_conference(i, &info.confs[i1])) {
        confs.insert(info.confs[i1].designator);
      }
    }
    auto confstr = create_conf_str(confs);

    switch (conftype) {
    case ConferenceType::CONF_SUBS:
      sprintf(s, "|#2%3d |#9%-39.39s |#1%s", i, 
        stripcolors(a()->subs().sub(i).name.c_str()), confstr.c_str());
      break;
    case ConferenceType::CONF_DIRS:
      sprintf(s, "|#2%3d |#9%-39.39s |#1%s", i, stripcolors(a()->directories[i].name),
              confstr.c_str());
      break;
    }
    bout.bpla(s, &abort);
  }
}

/*
 * Takes a string like "100-150,201,333" and returns pointer to list of
 * numbers. Number of numbers in the list is returned in numinlist.
 */
bool str_to_numrange(const char *pszNumbersText, std::vector<subconf_t>& list) {
  subconf_t intarray[1024];

  // init vars
  memset(intarray, 0, sizeof(intarray));
  list.clear();

  // check for input string
  if (!pszNumbersText) {
    return false;
  }

  // get num "words" in input string
  int nNumWords = wordcount(pszNumbersText, ",");

  for (int word = 1; word <= nNumWords; word++) {
    CheckForHangup();
    if (a()->hangup_) {
      return false;
    }

    string temp = extractword(word, pszNumbersText, ",");
    int nRangeCount = wordcount(temp, " -\t\r\n");
    switch (nRangeCount) {
    case 0:
      break;
    case 1: {
      //This means there is no number in the range, it's just ,###,###
      int num = to_number<int>(extractword(1, temp, " -\t\r\n"));
      if (num < 1024 && num >= 0) {
        intarray[ num ] = 1;
      }
    }
    break;
    default: {
      // We're dealing with a range here, so it should be "XXX-YYY"
      // convert the left and right strings to numbers
      int nLowNumber = to_number<int>(extractword(1, temp, " -\t\r\n,"));
      int nHighNumber = to_number<int>(extractword(2, temp, " -\t\r\n,"));
      // Switch them around if they were reversed
      if (nLowNumber > nHighNumber) {
        std::swap(nLowNumber, nHighNumber);
      }
      for (int i = std::max<int>(nLowNumber, 0); i <= std::min<int>(nHighNumber, 1023); i++) {
        intarray[i] = 1;
      }
    }
    break;
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
 * Function to add one subconf (sub/dir/whatever) to a conference.
 */
void addsubconf(ConferenceType conftype, confrec* c, subconf_t* which) {
  std::vector<subconf_t> intlist;

  if (!c) {
    return;
  }

  auto info = get_conf_info(conftype);

  if (info.num_subs_or_dirs < 1) {
    return;
  }

  if (c->num >= 1023) {
    bout << "Maximum number of subconfs already in that conference.\r\n";
    return;
  }
  if (which == nullptr) {
    bout.nl();
    bout << "|#2Add: ";
    string text = input(60, true);
    if (text.empty()) {
      return;
    }
    str_to_numrange(text.c_str(), intlist);
  } else {
    intlist.clear();
    intlist.push_back(*which);
  }

  // add the subconfs now
  for (auto cn : intlist) {
    if (cn >= info.num_subs_or_dirs) {
      break;
    }
    if (!in_conference(cn, c)) {
      c->subs.insert(cn);
      c->num = static_cast<subconf_t>(c->subs.size());
    }
  }
}

/*
 * Function to delete one subconf (sub/dir/whatever) from a conference.
 */
static void delsubconf(confrec* c, subconf_t* which) {
  if (!c || c->subs.empty()) {
    return;
  }

  std::vector<subconf_t> intlist;
  if (which == nullptr) {
    bout.nl();
    bout << "|#2Remove: ";
    string text = input(60, true);
    if (text.empty()) {
      return;
    }
    str_to_numrange(text.c_str(), intlist);
  } else {
    intlist.push_back(*which);
  }

  for (auto cn : intlist) {
    if (in_conference(cn, c)) {
      c->subs.erase(cn);
      c->num = static_cast<subconf_t>(c->subs.size());
    }
  }
}

char* GetGenderAllowed(int nGender, char *pszGenderAllowed) {
  switch (nGender) {
  case 0:
    strcpy(pszGenderAllowed, "Male");
    break;
  case 1:
    strcpy(pszGenderAllowed, "Female");
    break;
  case 2:
  default:
    strcpy(pszGenderAllowed, "Anyone");
    break;
  }
  return pszGenderAllowed;
}


/*
 * Function for editing the data for one conference.
 */
int modify_conf(ConferenceType conftype,  int which) {
  bool changed = false;
  bool ok   = false;
  bool done = false;

  int n = which;

  auto info = get_conf_info(conftype);

  do {
    confrec& c = info.confs[n];
    char szGenderAllowed[ 21 ];
    bout.cls();

    bout << "|#9A) Designator           : |#2" << c.designator << wwiv::endl;
    bout << "|#9B) Conf Name            : |#2" << c.conf_name << wwiv::endl;
    bout << "|#9C) Min SL               : |#2" << static_cast<int>(c.minsl) << wwiv::endl;
    bout << "|#9D) Max SL               : |#2" << static_cast<int>(c.maxsl) << wwiv::endl;
    bout << "|#9E) Min DSL              : |#2" << static_cast<int>(c.mindsl) << wwiv::endl;
    bout << "|#9F) Max DSL              : |#2" << static_cast<int>(c.maxdsl) << wwiv::endl;
    bout << "|#9G) Min Age              : |#2" << static_cast<int>(c.minage) << wwiv::endl;
    bout << "|#9H) Max Age              : |#2" << static_cast<int>(c.maxage) << wwiv::endl;
    bout << "|#9I) ARs Required         : |#2" << word_to_arstr(c.ar) << wwiv::endl;
    bout << "|#9J) DARs Required        : |#2" << word_to_arstr(c.dar) << wwiv::endl;
    bout << "|#9K) Min BPS Required     : |#2" << c.minbps << wwiv::endl;
    bout << "|#9L) Gender Allowed       : |#2" << GetGenderAllowed(c.sex, szGenderAllowed) << wwiv::endl;
    bout << "|#9M) Ansi Required        : |#2" << YesNoString((c.status & conf_status_ansi) ? true : false) <<
                       wwiv::endl;
    bout << "|#9N) WWIV RegNum Required : |#2" << YesNoString((c.status & conf_status_wwivreg) ? true : false)
                       << wwiv::endl;
    bout << "|#9O) Available            : |#2" << YesNoString((c.status & conf_status_offline) ? true : false)
                       << wwiv::endl;
    bout << "|#9S) Edit SubConferences\r\n";
    bout << "|#9Q) Quit ConfEdit\r\n";
    bout.nl();

    bout << "|#7(|#2Q|#7=|#1Quit|#7) ConfEdit [|#1A|#7-|#1O|#7,|#1S|#7,|#1[|#7,|#1]|#7] : ";
    char ch = onek("QSABCDEFGHIJKLMNO[]", true);

    switch (ch) {
    case '[': {
      n--;
      if (n < 1) {
        n = std::max<int>(0, info.confs.size() - 1);
      }
    } break;
    case ']':
      n++;
      if (n >= size_int(info.confs)) {
        n = 0;
      }
      break;
    case 'A': {
      bout.nl();
      bout << "|#2New Designator: ";
      char ch1 = onek("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
      ok = true;
      for (int i = 0; i < size_int(info.confs); i++) {
        if (i == n) {
          i++;
        }
        if (ok && (info.confs[i].designator == ch1)) {
          ok = false;
        }
      }
      if (!ok) {
        bout.nl();
        bout << "|#6That designator already in use!\r\n";
        pausescr();
        break;
      }
      c.designator = ch1;
      changed = true;
    } break;
    case 'B': {
      bout.nl();
      bout << "|#2Conference Name: ";
      auto cname = input_text(60);
      if (!cname.empty()) {
        c.conf_name = cname;
        changed = true;
      }
    }
    break;
    case 'C': {
      bout.nl();
      bout << "|#2Min SL: ";
      c.minsl = input_number(c.minsl);
      changed = true;
    } break;
    case 'D':
      bout.nl();
      bout << "|#2Max SL: ";
      c.maxsl = input_number(c.maxsl);
      changed = true;
      break;
    case 'E':
      bout.nl();
      bout << "|#2Min DSL: ";
      c.mindsl = input_number(c.mindsl);
      changed = true;
      break;
    case 'F':
      bout.nl();
      bout << "|#2Max DSL";
      c.maxdsl = input_number(c.maxdsl);
      changed = true;
      break;
    case 'G':
      bout.nl();
      bout << "|#2Min Age: ";
      c.minage = input_number<uint8_t>(c.minage, 0, 255, true);
      changed = true;
      break;
    case 'H':
      bout.nl();
      bout << "|#2Max Age: ";
      c.maxage = input_number(c.maxage);
      changed = true;
      break;
    case 'I': {
      bout.nl();
      bout << "|#2Toggle which AR requirement? ";
      char ch1 = onek("\rABCDEFGHIJKLMNOP ");
      switch (ch1) {
      case ' ':
      case '\r':
        break;
      default:
        c.ar ^= (1 << (ch1 - 'A'));
        changed = true;
        break;
      }
    } break;
    case 'J': {
      bout.nl();
      bout << "|#2Toggle which DAR requirement? ";
      char ch1 = onek("\rABCDEFGHIJKLMNOP ");
      switch (ch1) {
      case ' ':
      case '\r':
        break;
      default:
        c.dar ^= (1 << (ch1 - 'A'));
        changed = true;
        break;
      }
    } break;
    case 'K':
      bout.nl();
      bout << "|#2Min BPS Rate: ";
      c.minbps = input_number(c.minbps);
      changed = true;
      break;
    case 'L': {
      bout.nl();
      changed = true;
      bout << "|#5(Q=Quit) Gender Allowed: (M)ale, (F)emale, (A)ll: ";
      char ch1 = onek("MFAQ");
      switch (ch1) {
      case 'M':
        c.sex = 0;
        break;
      case 'F':
        c.sex = 1;
        break;
      case 'A':
        c.sex = 2;
        break;
      case 'Q':
        break;
      }
    } break;
    case 'M':
      bout.nl();
      changed = true;
      c.status ^= conf_status_ansi;
      break;
    case 'N':
      bout.nl();
      changed = true;
      c.status ^= conf_status_wwivreg;
      break;
    case 'O':
      bout.nl();
      changed = true;
      c.status ^= conf_status_offline;
      break;
    case 'S':
      ok = false;
      do {
        bout.nl();
        bout << "|#2A)dd, R)emove, C)lear, F)ill, Q)uit, S)tatus: ";
        char ch1 = onek("QARCFS");
        switch (ch1) {
        case 'A':
          addsubconf(conftype, &c, nullptr);
          changed = true;
          break;
        case 'R':
          delsubconf(&c, nullptr);
          changed = true;
          break;
        case 'C':
          c.subs.clear();
          changed = true;
          break;
        case 'F':
          for (subconf_t i = 0; i < info.num_subs_or_dirs; i++) {
            c.subs.insert(i);
            c.num = static_cast<subconf_t>(c.subs.size());
            changed = true;
          }
          break;
        case 'Q':
          ok = true;
          break;
        case 'S':
          bout.nl();
          showsubconfs(conftype, &c);
          break;
        }
      } while (!ok);
      break;
    case 'Q':
      done = true;
      break;
    }
  } while (!done && !a()->hangup_);

  if (changed) {
    save_confs(conftype);
  }
  return changed;
}


/*
 * Function for inserting one conference.
 */
void insert_conf(ConferenceType conftype,  int n) {

  auto info = get_conf_info(conftype);
  confrec c{};

  c.designator = first_available_designator(conftype);

  if (c.designator == 0) {
    return;
  }

  c.conf_name = StrCat("Conference ", c.designator);
  c.minsl = 0;
  c.maxsl = 255;
  c.mindsl = 0;
  c.maxdsl = 255;
  c.minage = 0;
  c.maxage = 255;
  c.status = 0;
  c.minbps = 0;
  c.sex = 2;
  c.ar = 0;
  c.dar = 0;
  c.num = 0;

  if (!insert_at(info.confs, n, c)) {
    LOG(ERROR) << "Error inserting conference.";
  }

  if (!save_confs(conftype)) {
    LOG(ERROR) << "Unable to save conferences.";
  }

  read_in_conferences(conftype);

  if (modify_conf(conftype, n)) {
    save_confs(conftype);
  }
}

/**
 * Function for deleting one conference.
 */
void delete_conf(ConferenceType conftype,  int n) {
  auto info = get_conf_info(conftype);
  erase_at(info.confs, n);
  save_confs(conftype);
  read_in_conferences(conftype);
}

/**
 * Function for editing conferences.
 */
void conf_edit(ConferenceType conftype) {
  bool done = false;

  bout.cls();
  list_confs(conftype, 1);

  do {
    auto info = get_conf_info(conftype);
    bout.nl();
    bout << "|#2I|#7)|#1nsert, |#2D|#7)|#1elete, |#2M|#7)|#1odify, |#2Q|#7)|#1uit, |#2? |#7 : ";
    char ch = onek("QIDM?", true);
    switch (ch) {
    case 'D':
      if (info.confs.size() == 1) {
        bout << "\r\n|#6Cannot delete all conferences!\r\n";
      } else {
        int ec = select_conf("Delete which conference? ", conftype, 0);
        if (ec >= 0) {
          delete_conf(conftype, ec);
        }
      }
      break;
    case 'I':
      if (info.confs.size() == MAX_CONFERENCES) {
        bout << "\r\n|#6Cannot insert any more conferences!\r\n";
      } else {
        int ec = select_conf("Insert before which conference ('$'=at end)? ", conftype, 0);
        if (ec != -1) {
          if (ec == -2) {
            ec = info.confs.size();
          }
          insert_conf(conftype, ec);
        }
      }
      break;
    case 'M': {
      int ec = select_conf("Modify which conference? ", conftype, 0);
      if (ec >= 0) {
        if (modify_conf(conftype, ec)) {
          save_confs(conftype);
        }
      }
    }
    break;
    case 'Q':
      done = true;
      break;
    case '?':
      bout.cls();
      list_confs(conftype, 1);
      break;
    }
  } while (!done && !a()->hangup_);
  if (!a()->at_wfc()) {
    changedsl();
  }
}


/*
 * Lists conferences of a specified type. If OP_FLAGS_SHOW_HIER is set,
 * then the subs (dirs, whatever) in each conference are also shown.
 */
void list_confs(ConferenceType conftype, int ssc) {
  char s[121], s1[121];
  bool abort = false;
  auto ret = get_conf_info(conftype);

  bout.bpla("|#2  Des Name                    LSL HSL LDSL HDSL LAge HAge LoBPS AR  DAR S A W", &abort);
  bout.bpla("|#7\xC9\xCD\xCD\xCD\xCD \xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD \xCD\xCD\xCD \xCD\xCD\xCD \xCD\xCD\xCD\xCD \xCD\xCD\xCD\xCD \xCD\xCD\xCD\xCD \xCD\xCD\xCD\xCD \xCD\xCD\xCD\xCD\xCD \xCD\xCD\xCD \xCD\xCD\xCD \xCD \xCD \xCD", &abort);

  for (const auto& cp : ret.confs) {
    if (abort) break;
    const string ar = word_to_arstr(cp.ar);
    sprintf(s, "%c\xCD|17|15 %c |16|#1 %-23.23s %3d %3d %4d %4d %4d %4d %5u %-3.3s ",
            '\xCC', 
            cp.designator, cp.conf_name.c_str(), cp.minsl,
            cp.maxsl, cp.mindsl, cp.maxdsl, cp.minage, cp.maxage,
            cp.minbps, ar.c_str());
    sprintf(s1, "%-3.3s %c %1.1s %1.1s",
            word_to_arstr(cp.dar).c_str(),
            (cp.sex) ? ((cp.sex == 2) ? 'A' : 'F') : 'M',
            YesNoString((cp.status & conf_status_ansi) ? true : false),
            YesNoString((cp.status & conf_status_wwivreg)  ? true : false));
    strcat(s, s1);
    bout.Color(7);
    bout.bpla(s, &abort);
    if (a()->HasConfigFlag(OP_FLAGS_SHOW_HIER)) {
      if (cp.num > 0 && ssc) {
        for (const auto sub : cp.subs) {
          if (sub < ret.num_subs_or_dirs) {
            bout.Color(7);
            sprintf(s, "%c  %c\xC4\xC4\xC4 |#9", ' ', '\xC3');
            switch (conftype) {
            case ConferenceType::CONF_SUBS:
              sprintf(s1, "%s%-3d : %s", "Sub #", sub,
                      stripcolors(a()->subs().sub(sub).name.c_str()));
              break;
            case ConferenceType::CONF_DIRS:
              sprintf(s1, "%s%-3d : %s", "Dir #", sub,
                      stripcolors(a()->directories[sub].name));
              break;
            }
            strcat(s, s1);
            bout.bpla(s, &abort);
          }
        }
      }
    }
  }
  bout.nl();
}

/*
 * Allows user to select a conference. Returns the conference selected
 * (0-based), or -1 if the user aborted. Error message is printed for
 * invalid conference selections.
 */
int select_conf(const char *prompt_text, ConferenceType conftype, int listconfs) {
  int i = 0, sl = 0;
  bool ok = false;
  string mmk;

  do {
    if (listconfs || sl) {
      bout.nl();
      list_confs(conftype, 0);
      sl = 0;
    }
    if (prompt_text && prompt_text[0]) {
      bout.nl();
      bout <<  "|#1" << prompt_text;
    }
    mmk = mmkey(MMKeyAreaType::subs);
    if (mmk.empty()) {
      i = -1;
      ok = true;
    } else {
      switch (mmk.front()) {
      case '?':
        sl = 1;
        break;
      default:
        switch (conftype) {
        case ConferenceType::CONF_SUBS:
          for (size_t i1 = 0; i1 < a()->subconfs.size(); i1++) {
            if (mmk[0] == a()->subconfs[i1].designator) {
              ok = true;
              i = i1;
              break;
            }
          }
          break;
        case ConferenceType::CONF_DIRS:
          for (size_t i1 = 0; i1 < a()->dirconfs.size(); i1++) {
            if (mmk[0] == a()->dirconfs[i1].designator) {
              ok = true;
              i = i1;
              break;
            }
          }
          break;
        }
        if (mmk[0] == '$') {
          i = -2;
          ok = true;
        }
        break;
      }
      if (!ok && !sl) {
        bout << "\r\n|#6Invalid conference designator!\r\n";
      }
    }
  } while (!ok && !a()->hangup_);
  return i;
}

/*
 * Creates a default conference file. Should only be called if no conference
 * file for that conference type already exists.
 */
bool create_conf_file(ConferenceType conftype) {
  conf_info_t info = get_conf_info(conftype);
  TextFile f(info.file_name, "wt");
  if (!f.IsOpen()) {
    return false;
  }

  f.WriteLine("/* !!!-!!! Do not edit this file - use WWIV's conf editor! !!!-!!! */");
  f.WriteLine("");
  f.WriteLine("~A General");
  f.WriteLine("!0 0 255 0 255 0 255 0 2 - -");
  std::ostringstream ss;
  ss << "@";
  for (int i = 0; i < info.num_subs_or_dirs; i++) {
    ss << i << " ";
  }
  f.WriteLine(ss.str());
  f.WriteLine("");
  f.Close();
  return true;
}

/*
 * Reads in conferences and returns pointer to conference data. Out-of-memory
 * messages are shown if applicable.
 */
static std::vector<confrec> read_conferences(const std::string& file_name) {
  if (!File::Exists(file_name)) {
    return{};
  }

  TextFile f(file_name, "rt");
  if (!f.IsOpen()) {
    return{};
  }

  std::vector<confrec> result;
  string ls;
  confrec c{};
  while (f.ReadLine(&ls) && result.size() < MAX_CONFERENCES) {
    StringTrim(&ls);
    if (ls.size() < 1) {
      continue;
    }
    const char id = ls.front();
    ls = ls.substr(1);
    switch (id) {
    case '~': {
      if (ls.size() > 2 && ls[1] == ' ') {
        c.designator = ls.front();
        auto name = ls.substr(2);
        StringTrim(&name);
        c.conf_name = name;
      }
    } break;
    case '!': {
      // data about the conference
      std::vector<string> words = SplitString(ls, DELIMS_WHITE);
      if (!c.designator || words.size() < 10) {
        LOG(ERROR) << "Invalid conf line: '" << ls << "'";
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
      if (!c.designator) {
        break;
      }
      std::vector<string> words = SplitString(ls, DELIMS_WHITE);
      for (auto& word : words) {
        StringTrim(&word);
        if (word.empty()) continue;
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

/*
 * Reads in a conference type. Creates default conference file if it is
 * necessary. If conferences cannot be read, then BBS aborts.
 */
void read_in_conferences(ConferenceType conftype) {
  const auto fn = get_conf_filename(conftype);
  if (!File::Exists(fn)) {
    if (!create_conf_file(conftype)) {
      LOG(FATAL) << "Problem creating conferences.";
    }
  }

  switch (conftype) {
  case ConferenceType::CONF_SUBS: {
    a()->subconfs = read_conferences(fn);
  } break;
  case ConferenceType::CONF_DIRS: {
    a()->dirconfs = read_conferences(fn);
  } break;
  }
}

/*
 * Reads all conferences into memory. Creates default conference files if
 * none exist. If called when conferences already in memory, then memory
 * for "old" conference data is deallocated first.
 */
void read_all_conferences() {
  read_in_conferences(ConferenceType::CONF_SUBS);
  read_in_conferences(ConferenceType::CONF_DIRS);
}

/*
 * Returns number of "words" in a specified string, using a specified set
 * of characters as delimiters.
 */
int wordcount(const string& instr, const char *delimstr) {
  char szTempBuffer[MAX_CONF_LINE];
  int i = 0;

  strcpy(szTempBuffer, instr.c_str());
  for (char *s = strtok(szTempBuffer, delimstr); s; s = strtok(nullptr, delimstr)) {
    i++;
  }
  return i;
}


/*
 * Returns pointer to string representing the nth "word" of a string, using
 * a specified set of characters as delimiters.
 */
std::string extractword(int ww, const string& instr, const char *delimstr) {
  char szTempBuffer[MAX_CONF_LINE];
  int i = 0;

  if (!ww) {
    return {};
  }

  strcpy(szTempBuffer, instr.c_str());
  for (char *s = strtok(szTempBuffer, delimstr); s && (i++ < ww); s = strtok(nullptr, delimstr)) {
    if (i == ww) {
      return string(s);
    }
  }
  return {};
}

