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
#include "bbs/confutil.h"

#include "bbs/acs.h"
#include "bbs/bbs.h"
#include "bbs/conf.h"
#include "core/log.h"
#include "core/stl.h"
#include "sdk/subxtr.h"
#include "sdk/files/dirs.h"

using namespace wwiv::bbs;
using namespace wwiv::sdk;
using namespace wwiv::stl;

/**
 * Does user u have access to the conference
 * @return bool
 */
static bool access_conf(User * u, int sl, const confrec_430_t * c) {
  CHECK(u != nullptr) << "access_conf called with null user";
  CHECK(c != nullptr) << "access_conf called with null confrec_430_t";

  if (c->num < 1) {
    return false;
  }
  switch (c->sex) {
  case 0:
    if (u->GetGender() != 'M') {
      return false;
    }
    break;
  case 1:
    if (u->GetGender() != 'F') {
      return false;
    }
    break;
  }
  if (sl < c->minsl || (c->maxsl != 0 && sl > c->maxsl)) {
    return false;
  }
  const auto dsl = u->GetDsl();
  if (dsl < c->mindsl || (c->maxdsl != 0 && dsl > c->maxdsl)) {
    return false;
  }
  const auto age = u->age();
  if (age < c->minage || (c->maxage != 0 && age > c->maxage)) {
    return false;
  }
  if (a()->sess().incom() && a()->modem_speed_ < c->minbps) {
    return false;
  }
  if (c->ar && !u->HasArFlag(c->ar)) {
    return false;
  }
  if (c->dar && !u->HasDarFlag(c->dar)) {
    return false;
  }
  if ((c->status & conf_status_ansi) && !u->HasAnsi()) {
    return false;
  }
  if ((c->status & conf_status_wwivreg) && u->GetWWIVRegNumber() < 1) {
    return false;
  }
  if ((c->status & conf_status_offline) && sl < 100) {
    return false;
  }

  return true;
}

static bool access_sub(User& u, const subboard_t& s) {
  if (!check_acs(s.read_acs)) {
    return false;
  }

  if ((s.anony & anony_ansi_only) && !u.HasAnsi()) {
    return false;
  }

  return true;
}

static bool access_dir(User& u, int sl, wwiv::sdk::files::directory_t& d) {
  if (!check_acs(d.acs)) {
    return false;
  }

  if ((d.mask & mask_offline) && u.GetDsl() < 100) {
    return false;
  }

  if ((d.mask & mask_wwivreg) && !u.GetWWIVRegNumber()) {
    return false;
  }

  // Note: this is different than access_sub.
  if (sl == 0) {
    return false;
  }

  return true;
}

static void addusub(std::vector<usersubrec>& ss1, int ns, int sub, char key) {
  int last_num = 0, last;
  for (last = 0; last < ns; last++) {
    if (ss1[last].subnum == -1) {
      break;
    }
    if (ss1[last].subnum == sub) {
      return;
    }
    if (ss1[last].keys.empty()) {
      last_num = last + 1;
    }
  }

  if (last == ns) {
    return;
  }

  if (key) {
    ss1[last].subnum = static_cast<int16_t>(sub);
    ss1[last].keys = key;
  } else {
    for (int i = last; i > last_num; i--) {
      ss1[i] = ss1[ i - 1 ];
    }
    ss1[last_num].subnum = static_cast<int16_t>(sub);
    ss1[last_num].keys.clear();
  }
}

// returns true on success, false on failure
// used to return 0 on success, 1 on failure
static bool setconf(ConferenceType type, std::vector<usersubrec>& ss1, int which, int old_subnum) {
  int osub;
  confrec_430_t *c;

  size_t ns = 0;
  switch (type) {
  case ConferenceType::CONF_SUBS:
    ns = a()->subs().subs().size();
    if (old_subnum == -1) {
      osub = a()->current_user_sub().subnum;
    } else {
      osub = old_subnum;
    }
    if (which == -1) {
      c = nullptr;
    } else {
      if (which < 0 || which >= ssize(a()->subconfs)) {
        return false;
      }
      c = &(a()->subconfs[which]);
      if (!access_conf(a()->user(), a()->sess().effective_sl(), c)) {
        return false;
      }
    }
    break;
  case ConferenceType::CONF_DIRS:
    ns = a()->dirs().size();
    if (old_subnum == -1) {
      osub = a()->current_user_dir().subnum;
    } else {
      osub = old_subnum;
    }
    if (which == -1) {
      c = nullptr;
    } else {
      if (which < 0 || which >= ssize(a()->dirconfs)) {
        return false;
      }
      c = &(a()->dirconfs[which]);
      if (!access_conf(a()->user(), a()->sess().effective_sl(), c)) {
        return false;
      }
    }
    break;
  default:
    return false;
  }

  usersubrec s1 = {};
  s1.subnum = -1;

  for (size_t i = 0; i < ns; i++) {
    ss1[i] = s1;
  }

  if (c) {
    for (const auto sub : c->subs) {
      switch (type) {
      case ConferenceType::CONF_SUBS:
        if (access_sub(*a()->user(), a()->subs().sub(sub))) {
          addusub(ss1, ns, sub, a()->subs().sub(sub).key);
        }
        break;
      case ConferenceType::CONF_DIRS:
        if (access_dir(*a()->user(), a()->sess().effective_sl(),
                       a()->dirs()[sub])) {
          addusub(ss1, ns, sub, 0);
        }
        break;
      default:
        LOG(FATAL) << "[utility.cpp] setconf called with nConferenceType != (ConferenceType::CONF_SUBS || ConferenceType::CONF_DIRS)\r\n";
        break;
      }
    }
  } else {
    switch (type) {
    case ConferenceType::CONF_SUBS:
      for (const auto& conf : a()->subconfs) {
        if (access_conf(a()->user(), a()->sess().effective_sl(), &conf)) {
          for (const auto sub : conf.subs) {
            if (access_sub(*a()->user(), a()->subs().sub(sub))) {
              addusub(ss1, ns, sub, a()->subs().sub(sub).key);
            }
          }
        }
      }
      break;
    case ConferenceType::CONF_DIRS:
      for (const auto& conf : a()->dirconfs) {
        if (access_conf(a()->user(), a()->sess().effective_sl(), &conf)) {
          for (const auto& sub : conf.subs) {
            if (access_dir(*a()->user(), a()->sess().effective_sl(),
                           a()->dirs()[sub])) {
              addusub(ss1, ns, sub, 0);
            }
          }
        }
      }
      break;
    default:
      LOG(FATAL) << "[utility.cpp] setconf called with nConferenceType != (ConferenceType::CONF_SUBS || ConferenceType::CONF_DIRS)\r\n";
      break;

    }
  }

  uint16_t i1 = (type == ConferenceType::CONF_DIRS && ss1[0].subnum == 0) ? 0 : 1;

  for (size_t i = 0; i < ns && ss1[i].keys.empty() && ss1[i].subnum != -1; i++) {
    ss1[i].keys = std::to_string(i1++);
  }

  for (i1 = 0; (i1 < ns) && (ss1[i1].subnum != -1); i1++) {
    if (ss1[i1].subnum == osub) {
      break;
    }
  }
  if (i1 >= ns || ss1[i1].subnum == -1) {
    i1 = 0;
  }

  switch (type) {
  case ConferenceType::CONF_SUBS:
    a()->set_current_user_sub_num(i1);
    break;
  case ConferenceType::CONF_DIRS:
    a()->set_current_user_dir_num(i1);
    break;
  }
  return true;
}

void setuconf(ConferenceType nConferenceType, int num, int nOldSubNumber) {
  switch (nConferenceType) {
  case ConferenceType::CONF_SUBS:
    if (num >= 0 && num < MAX_CONFERENCES && has_userconf_to_subconf(num)) {
      a()->sess().set_current_user_sub_conf_num(num);
      setconf(nConferenceType, a()->usub, userconf_to_subconf(a()->sess().current_user_sub_conf_num()), nOldSubNumber);
    } else {
      setconf(nConferenceType, a()->usub, -1, nOldSubNumber);
    }
    break;
  case ConferenceType::CONF_DIRS:
    if (num >= 0 && num < MAX_CONFERENCES && has_userconf_to_dirconf(num)) {
      a()->sess().set_current_user_dir_conf_num(num);
      setconf(nConferenceType, a()->udir, a()->uconfdir[a()->sess().current_user_dir_conf_num()].confnum, nOldSubNumber);
    } else {
      setconf(nConferenceType, a()->udir, -1, nOldSubNumber);
    }
    break;
  default:
    LOG(FATAL) << "[utility.cpp] setuconf called with nConferenceType != (ConferenceType::CONF_SUBS || ConferenceType::CONF_DIRS)\r\n";
    break;
  }
}

void changedsl() {
  int ocurconfsub = userconf_to_subconf(a()->sess().current_user_sub_conf_num());
  int ocurconfdir = userconf_to_subconf(a()->sess().current_user_dir_conf_num());
  a()->UpdateTopScreen();

  a()->uconfsub.clear();
  a()->uconfdir.clear();

  int i = 0;
  for (const auto& c : a()->subconfs) {
    if (access_conf(a()->user(), a()->sess().effective_sl(), &c)) {
      userconfrec c1{};
      c1.confnum = static_cast<int16_t>(i);
      a()->uconfsub.emplace_back(c1);
    }
    ++i;
  }

  i = 0;
  for (const auto& c : a()->dirconfs) {
    if (access_conf(a()->user(), a()->sess().effective_sl(), &c)) {
      userconfrec c1{};
      c1.confnum = static_cast<int16_t>(i);
      a()->uconfdir.emplace_back(c1);
    }
    ++i;
  }

  // Move to first message area in new conference
  for (a()->sess().set_current_user_sub_conf_num(0);
       (a()->sess().current_user_sub_conf_num() < MAX_CONFERENCES) &&
       has_userconf_to_subconf(a()->sess().current_user_sub_conf_num());
       a()->sess().set_current_user_sub_conf_num(a()->sess().current_user_sub_conf_num() + 1)) {
    if (userconf_to_subconf(a()->sess().current_user_sub_conf_num()) == ocurconfsub) {
      break;
    }
  }

  if (a()->sess().current_user_sub_conf_num() >= MAX_CONFERENCES ||
      has_userconf_to_subconf(a()->sess().current_user_sub_conf_num())) {
    a()->sess().set_current_user_sub_conf_num(0);
  }

  for (a()->sess().set_current_user_dir_conf_num(0);
       (a()->sess().current_user_dir_conf_num() < MAX_CONFERENCES)
       && has_userconf_to_dirconf(a()->sess().current_user_dir_conf_num());
       a()->sess().set_current_user_dir_conf_num(a()->sess().current_user_dir_conf_num() + 1)) {
    if (a()->uconfdir[a()->sess().current_user_dir_conf_num()].confnum == ocurconfdir) {
      break;
    }
  }

  if (a()->sess().current_user_dir_conf_num() >= MAX_CONFERENCES ||
      !has_userconf_to_dirconf(a()->sess().current_user_dir_conf_num())) {
    a()->sess().set_current_user_dir_conf_num(0);
  }

  if (okconf(a()->user())) {
    setuconf(ConferenceType::CONF_SUBS, a()->sess().current_user_sub_conf_num(), -1);
    setuconf(ConferenceType::CONF_DIRS, a()->sess().current_user_dir_conf_num(), -1);
  } else {
    setconf(ConferenceType::CONF_SUBS, a()->usub, -1, -1);
    setconf(ConferenceType::CONF_DIRS, a()->udir, -1, -1);
  }
}

bool okconf(User* user) {
  if (a()->sess().disable_conf()) {
    return false;
  }
  return user->HasStatusFlag(User::conference);
}

bool ok_multiple_conf(User* user, const std::vector<userconfrec>& uc) {
  if (!okconf(user)) {
    return false;
  }
  return uc.size() > 1 && uc.at(1).confnum != -1;
}

int16_t userconf_to_subconf(int uc) { 
  if (uc >= ssize(a()->uconfsub)) {
    return -1;
  }
  return a()->uconfsub[uc].confnum; 
}

int16_t userconf_to_dirconf(int uc) {
  if (uc >= ssize(a()->uconfdir)) {
    return -1;
  }
  return a()->uconfdir[uc].confnum;
}

bool has_userconf_to_subconf(int uc) { return userconf_to_subconf(uc) != -1; }
bool has_userconf_to_dirconf(int uc) { return userconf_to_dirconf(uc) != -1; }
