/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2016, WWIV Software Services             */
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

#include "bbs/bbs.h"
#include "bbs/conf.h"
#include "bbs/fcns.h"
#include "bbs/mmkey.h"
#include "bbs/vars.h"
#include "core/log.h"
#include "core/strings.h"
#include "core/wwivassert.h"

using namespace wwiv::sdk;

/**
 * Does user u have access to the conference
 * @return bool
 */
static bool access_conf(User * u, int sl, confrec * c) {
  CHECK(u != nullptr) << "access_conf called with null user";
  CHECK(c != nullptr) << "access_conf called with null confrec";

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
  if ((sl < c->minsl) || (sl > c->maxsl)) {
    return false;
  }
  if ((u->GetDsl() < c->mindsl) || (u->GetDsl() > c->maxdsl)) {
    return false;
  }
  if ((u->GetAge() < c->minage) || (u->GetAge() > c->maxage)) {
    return false;
  }
  if (incom && (modem_speed < c->minbps)) {
    return false;
  }
  if (c->ar && !u->HasArFlag(c->ar)) {
    return false;
  }
  if (c->dar && !u->HasDarFlag(c->dar)) {
    return false;
  }
  if ((c->status & conf_status_ansi) && (!u->HasAnsi())) {
    return false;
  }
  if ((c->status & conf_status_wwivreg) && (u->GetWWIVRegNumber() < 1)) {
    return false;
  }
  if ((c->status & conf_status_offline) && (sl < 100)) {
    return false;
  }

  return true;
}

static bool access_sub(User& u, int sl, const subboard_t& s) {
  if (sl < s.readsl) {
    return false;
  }
  if (u.GetAge() < s.age) {
    return false;
  }
  if (s.ar != 0 && !u.HasArFlag(s.ar)) {
    return false;
  }
  if ((s.anony & anony_ansi_only) && !u.HasAnsi()) {
    return false;
  }

  return true;
}

static bool access_dir(User& u, int sl, directoryrec& d) {
  if (u.GetDsl() < d.dsl) {
    return false;
  }
  if (u.GetAge() < d.age) {
    return false;
  }
  if (d.dar && !u.HasDarFlag(d.dar)) {
    return false;
  }
  if ((d.mask & mask_offline) && u.GetDsl() < 100) {
    return false;
  }
  if ((d.mask & mask_wwivreg) && !u.GetWWIVRegNumber()) {
    return false;
  }
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
    if (ss1[last].keys[0] == 0) {
      last_num = last + 1;
    }
  }

  if (last == ns) {
    return;
  }

  if (key) {
    ss1[last].subnum = static_cast<int16_t>(sub);
    ss1[last].keys[0] = key;
  } else {
    for (int i = last; i > last_num; i--) {
      ss1[i] = ss1[ i - 1 ];
    }
    ss1[last_num].subnum = static_cast<int16_t>(sub);
    ss1[last_num].keys[0] = 0;
  }
}

// returns true on success, false on failure
// used to return 0 on success, 1 on failure
static bool setconf(ConferenceType type, std::vector<usersubrec>& ss1, int which, int old_subnum) {
  int osub;
  confrec *c;

  size_t ns = 0;
  switch (type) {
  case ConferenceType::CONF_SUBS:
    ns = session()->subs().subs().size();
    if (old_subnum == -1) {
      osub = session()->current_user_sub().subnum;
    } else {
      osub = old_subnum;
    }
    if (which == -1) {
      c = nullptr;
    } else {
      if (which < 0 || which >= static_cast<int>(subconfnum)) {
        return false;
      }
      c = &(subconfs[which]);
      if (!access_conf(session()->user(), session()->GetEffectiveSl(), c)) {
        return false;
      }
    }
    break;
  case ConferenceType::CONF_DIRS:
    ns = session()->directories.size();
    if (old_subnum == -1) {
      osub = session()->current_user_dir().subnum;
    } else {
      osub = old_subnum;
    }
    if (which == -1) {
      c = nullptr;
    } else {
      if (which < 0 || which >= static_cast<int>(dirconfnum)) {
        return false;
      }
      c = &(dirconfs[which]);
      if (!access_conf(session()->user(), session()->GetEffectiveSl(), c)) {
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
    for (size_t i = 0; i < c->num; i++) {
      switch (type) {
      case ConferenceType::CONF_SUBS:
        if (access_sub(*session()->user(), session()->GetEffectiveSl(),
                       session()->subs().sub(c->subs[i]))) {
          addusub(ss1, ns, c->subs[i], session()->subs().sub(c->subs[i]).key);
        }
        break;
      case ConferenceType::CONF_DIRS:
        if (access_dir(*session()->user(), session()->GetEffectiveSl(),
                       session()->directories[c->subs[i]])) {
          addusub(ss1, ns, c->subs[i], 0);
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
      for (size_t i = 0; i < subconfnum; i++) {
        if (access_conf(session()->user(), session()->GetEffectiveSl(), &(subconfs[i]))) {
          for (size_t i1 = 0; i1 < subconfs[i].num; i1++) {
            if (access_sub(*session()->user(), session()->GetEffectiveSl(),
                           session()->subs().sub(subconfs[i].subs[i1]))) {
              addusub(ss1, ns, subconfs[i].subs[i1],
                  session()->subs().sub(subconfs[i].subs[i1]).key);
            }
          }
        }
      }
      break;
    case ConferenceType::CONF_DIRS:
      for (size_t i = 0; i < dirconfnum; i++) {
        if (access_conf(session()->user(), session()->GetEffectiveSl(), &(dirconfs[i]))) {
          for (size_t i1 = 0; i1 < static_cast<int>(dirconfs[i].num); i1++) {
            if (access_dir(*session()->user(), session()->GetEffectiveSl(),
                           session()->directories[dirconfs[i].subs[i1]])) {
              addusub(ss1, ns, dirconfs[i].subs[i1], 0);
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

  size_t i1 = (type == ConferenceType::CONF_DIRS && ss1[0].subnum == 0) ? 0 : 1;

  for (size_t i = 0; (i < ns) && (ss1[i].keys[0] == 0) && (ss1[i].subnum != -1); i++) {
    snprintf(ss1[i].keys, sizeof(ss1[i].keys), "%d", i1++);
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
    session()->set_current_user_sub_num(i1);
    break;
  case ConferenceType::CONF_DIRS:
    session()->set_current_user_dir_num(i1);
    break;
  }
  return true;
}

void setuconf(ConferenceType nConferenceType, int num, int nOldSubNumber) {
  switch (nConferenceType) {
  case ConferenceType::CONF_SUBS:
    if (num >= 0 && num < MAX_CONFERENCES && uconfsub[num].confnum != -1) {
      session()->SetCurrentConferenceMessageArea(num);
      setconf(nConferenceType, session()->usub, uconfsub[session()->GetCurrentConferenceMessageArea()].confnum, nOldSubNumber);
    } else {
      setconf(nConferenceType, session()->usub, -1, nOldSubNumber);
    }
    break;
  case ConferenceType::CONF_DIRS:
    if (num >= 0 && num < MAX_CONFERENCES && uconfdir[num].confnum != -1) {
      session()->SetCurrentConferenceFileArea(num);
      setconf(nConferenceType, session()->udir, uconfdir[session()->GetCurrentConferenceFileArea()].confnum, nOldSubNumber);
    } else {
      setconf(nConferenceType, session()->udir, -1, nOldSubNumber);
    }
    break;
  default:
    LOG(FATAL) << "[utility.cpp] setuconf called with nConferenceType != (ConferenceType::CONF_SUBS || ConferenceType::CONF_DIRS)\r\n";
    break;
  }
}

void changedsl() {
  int ocurconfsub = uconfsub[session()->GetCurrentConferenceMessageArea()].confnum;
  int ocurconfdir = uconfdir[session()->GetCurrentConferenceFileArea()].confnum;
  session()->UpdateTopScreen();

  userconfrec c1;
  c1.confnum = -1;

  for (size_t i = 0; i < MAX_CONFERENCES; i++) {
    uconfsub[i] = c1;
    uconfdir[i] = c1;
  }

  int nTempSubConferenceNumber = 0;
  for (size_t i = 0; i < subconfnum; i++) {
    if (access_conf(session()->user(), session()->GetEffectiveSl(), &(subconfs[i]))) {
      c1.confnum = static_cast<int16_t>(i);
      uconfsub[ nTempSubConferenceNumber++ ] = c1;
    }
  }

  int nTempDirConferenceNumber = 0;
  for (size_t i = 0; i < dirconfnum; i++) {
    if (access_conf(session()->user(), session()->GetEffectiveSl(), &(dirconfs[i ]))) {
      c1.confnum = static_cast<int16_t>(i);
      uconfdir[ nTempDirConferenceNumber++ ] = c1;
    }
  }

  for (session()->SetCurrentConferenceMessageArea(0);
       (session()->GetCurrentConferenceMessageArea() < MAX_CONFERENCES)
       && (uconfsub[session()->GetCurrentConferenceMessageArea()].confnum != -1);
       session()->SetCurrentConferenceMessageArea(session()->GetCurrentConferenceMessageArea() + 1)) {
    if (uconfsub[session()->GetCurrentConferenceMessageArea()].confnum == ocurconfsub) {
      break;
    }
  }

  if (session()->GetCurrentConferenceMessageArea() >= MAX_CONFERENCES ||
      uconfsub[session()->GetCurrentConferenceMessageArea()].confnum == -1) {
    session()->SetCurrentConferenceMessageArea(0);
  }

  for (session()->SetCurrentConferenceFileArea(0);
       (session()->GetCurrentConferenceFileArea() < MAX_CONFERENCES)
       && (uconfdir[session()->GetCurrentConferenceFileArea()].confnum != -1);
       session()->SetCurrentConferenceFileArea(session()->GetCurrentConferenceFileArea() + 1)) {
    if (uconfdir[session()->GetCurrentConferenceFileArea()].confnum == ocurconfdir) {
      break;
    }
  }

  if (session()->GetCurrentConferenceFileArea() >= MAX_CONFERENCES ||
      uconfdir[session()->GetCurrentConferenceFileArea()].confnum == -1) {
    session()->SetCurrentConferenceFileArea(0);
  }

  if (okconf(session()->user())) {
    setuconf(ConferenceType::CONF_SUBS, session()->GetCurrentConferenceMessageArea(), -1);
    setuconf(ConferenceType::CONF_DIRS, session()->GetCurrentConferenceFileArea(), -1);
  } else {
    setconf(ConferenceType::CONF_SUBS, session()->usub, -1, -1);
    setconf(ConferenceType::CONF_DIRS, session()->udir, -1, -1);
  }
}
