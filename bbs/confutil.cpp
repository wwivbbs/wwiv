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
#include "bbs/confutil.h"

#include "bbs/acs.h"
#include "bbs/bbs.h"
#include "bbs/conf.h"
#include "core/stl.h"
#include "sdk/subxtr.h"
#include "sdk/files/dirs.h"

using namespace wwiv::bbs;
using namespace wwiv::sdk;
using namespace wwiv::stl;

static bool access_sub(User& u, const subboard_t& s) {
  if (!check_acs(s.read_acs)) {
    return false;
  }

  if ((s.anony & anony_ansi_only) && !u.ansi()) {
    return false;
  }

  return true;
}

static void addusub(std::vector<usersubrec>& us, int sub, char key = 0) {
  for (auto& u : us) {
    if (u.subnum == sub) {
      // we already have this sub.
      return;
    }
  }
  usersubrec c{};
  c.subnum = static_cast<int16_t>(sub);
  if (key) {
    c.keys = key;
  }
  us.emplace_back(c);
}

// Populates uc with the user visible subs for the current conference.
static bool create_usersubs(Conference& conf, std::vector<usersubrec>& uc, 
                            char conf_key, int old_subnum) {
  if (auto o = conf.try_conf(conf_key)) {
    auto new_conf_subnum = -1;
    // We found the conference.
    const auto& c = o.value();
    if (!check_acs(c.acs)) {
      return false;
    }

    uc.clear();
    if (conf.type() == ConferenceType::CONF_SUBS) {
      const auto& subs = a()->subs().subs();
      for (auto subnum = 0; subnum < size_int(subs); subnum++) {
        const auto& s = at(subs, subnum);
        if (!s.conf.contains(conf_key)) {
          continue;
        }
        // This sub is in our current conference.
        if (!check_acs(s.read_acs)) {
          continue;
        }
        addusub(uc, subnum, s.key);
      }
    } else {
      const auto& dirs = a()->dirs().dirs();
      for (auto subnum = 0; subnum < size_int(dirs); subnum++) {
        const auto& s = at(dirs, subnum);
        if (!s.conf.contains(conf_key)) {
          continue;
        }
        // This sub is in our current conference.
        if (!check_acs(s.acs)) {
          continue;
        }
        addusub(uc, subnum);
      }
    }

    auto num_key = 1;
    for (auto i = 0; i < size_int(uc); i++) {
      auto& u = at(uc, i);
      if (u.keys.empty()) {
        u.keys = std::to_string(num_key++);
      }
      if (u.subnum == old_subnum) {
        // We have a new subnum position within this conference.
        new_conf_subnum = i;
      }
    }

    if (conf.type() == ConferenceType::CONF_SUBS) {
      a()->set_current_user_sub_num(new_conf_subnum != -1 ? new_conf_subnum : 0);
    } else {
      a()->set_current_user_dir_num(new_conf_subnum != -1 ? new_conf_subnum : 0);
    }
    return true;
  }
  return false;
}

bool access_at_least_one_conf(Conference& conf, const conf_set_t& e) {
  for (auto c : e.data_) {
    if (auto o = conf.try_conf(c)) {
      if (!check_acs(o.value().acs)) {
        return false;
      }
    } else {
      return false;
    }
  }
  return true;
}

// Populates uc with the user visible subs for the current conference.
bool clear_usersubs(Conference& conf, std::vector<usersubrec>& uc, int old_subnum) {
  if (conf.empty()) {
    const wwiv::sdk::key_t key('A');
    conf.add(conference_t{key, "General", ""});
  }
  auto new_conf_subnum = -1;
  uc.clear();
  // iterate through each sub and make sure we have access to at least
  // one conference for this sub.
  if (conf.type() == ConferenceType::CONF_SUBS) {
    const auto& subs = a()->subs().subs();
    for (auto subnum = 0; subnum < size_int(subs); subnum++) {
      const auto& s = at(subs, subnum);
      if (!access_at_least_one_conf(conf, s.conf)) {
        continue;
      }
      // This sub is in our current conference.
      if (!access_sub(*a()->user(), s)) {
        continue;
      }
      addusub(uc, subnum, s.key);
    }
  } else {
    const auto& dirs = a()->dirs().dirs();
    for (auto subnum = 0; subnum < size_int(dirs); subnum++) {
      const auto& s = at(dirs, subnum);
      if (!access_at_least_one_conf(conf, s.conf)) {
        continue;
      }
      // This sub is in our current conference.
      if (!check_acs(s.acs)) {
        continue;
      }
      addusub(uc, subnum);
    }
  }
  // TODO from here on down can probably be extracted and shared
  // with create_usersubs
  auto num_key = 1;
  for (auto i = 0; i < size_int(uc); i++) {
    auto& u = at(uc, i);
    if (u.keys.empty()) {
      u.keys = std::to_string(num_key++);
    }
    if (u.subnum == old_subnum) {
      // We have a new subnum position within this conference.
      new_conf_subnum = i;
    }
  }

  if (conf.type() == ConferenceType::CONF_SUBS) {
    a()->set_current_user_sub_num(new_conf_subnum != -1 ? new_conf_subnum : 0);
  } else {
    a()->set_current_user_dir_num(new_conf_subnum != -1 ? new_conf_subnum : 0);
  }
  return true;
}

void setuconf(Conference& conf, char key, int old_subnum) {
  auto& usd = conf.type() == ConferenceType::CONF_SUBS ? a()->usub : a()->udir;
  if (conf.exists(key)) {
    create_usersubs(conf, usd, key, old_subnum);
  } else {
    clear_usersubs(conf, usd, old_subnum);
  }
}

void setuconf(ConferenceType conf_type, int num, int old_subnum) {
  if (conf_type == ConferenceType::CONF_DIRS) {
    auto& conf = a()->all_confs().dirs_conf();
    const auto key = at(a()->uconfdir, num).key.key();
    setuconf(conf, key, old_subnum);
    return;
  }
  auto& conf = a()->all_confs().subs_conf();
  const auto key = at(a()->uconfsub, num).key.key();
  setuconf(conf, key, old_subnum);
}

void changedsl() {
  const auto subconfkey = optional_at(a()->uconfsub, a()->sess().current_user_sub_conf_num());
  const auto dirconfkey = optional_at(a()->uconfdir, a()->sess().current_user_dir_conf_num());

  a()->UpdateTopScreen();
  a()->uconfsub.clear();
  a()->uconfdir.clear();

  for (const auto& c : a()->all_confs().subs_conf().confs()) {
    if (check_acs(c.acs)) {
      a()->uconfsub.emplace_back(c);
    }
  }

  for (const auto& c : a()->all_confs().dirs_conf().confs()) {
    if (check_acs(c.acs)) {
      a()->uconfdir.emplace_back(c);
    }
  }

  // Move to first message area in new conference
  a()->sess().set_current_user_sub_conf_num(0);
  if (subconfkey) {
    for (auto i = 0; i < size_int(a()->uconfsub); i++) {
      if (at(a()->uconfsub, i).key.key() == subconfkey.value().key.key()) {
        a()->sess().set_current_user_sub_conf_num(i);
        break;
      }
    }
  }
  if (dirconfkey) {
    a()->sess().set_current_user_dir_conf_num(0);
    for (auto i = 0; i < size_int(a()->uconfdir); i++) {
      if (at(a()->uconfdir, i).key.key() == dirconfkey.value().key.key()) {
        a()->sess().set_current_user_dir_conf_num(i);
        break;
      }
    }
  }

  if (okconf(a()->user()) && !a()->uconfsub.empty()) {
    // Only set the subs if our conf list is not empty.
    const auto& s = at(a()->uconfsub, a()->sess().current_user_sub_conf_num());
    setuconf(a()->all_confs().subs_conf(), s.key.key(), -1);
  } else {
    clear_usersubs(a()->all_confs().subs_conf(), a()->usub, -1);
    a()->uconfsub.emplace_back(a()->all_confs().subs_conf().front());
  }

  if (okconf(a()->user()) && !a()->uconfdir.empty()) {
    // Only set the dirs if our conf list is not empty.
    const auto& d = at(a()->uconfdir, a()->sess().current_user_dir_conf_num());
    setuconf(a()->all_confs().dirs_conf(), d.key.key(), -1);
  } else {
    clear_usersubs(a()->all_confs().dirs_conf(), a()->udir, -1);
    a()->uconfdir.emplace_back(a()->all_confs().dirs_conf().front());
  }
}

bool okconf(User* user) {
  if (a()->sess().disable_conf()) {
    return false;
  }
  return user->has_flag(User::conference);
}

bool ok_multiple_conf(User* user, const std::vector<wwiv::sdk::conference_t>& uc) {
  if (!okconf(user)) {
    return false;
  }
  return uc.size() > 1;
}

