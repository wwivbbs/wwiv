/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2014, WWIV Software Services             */
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

#include "wwiv.h"
#include "core/wwivassert.h"

//
// Local functions
//
bool setconf(unsigned int nConferenceType, int which, int nOldSubNumber);
bool access_conf(WUser * u, int sl, confrec * c);
bool access_sub(WUser * u, int sl, subboardrec * s);
bool access_dir(WUser * u, int sl, directoryrec * d);
void addusub(usersubrec * ss1, int ns, int sub, char key);


/**
 * Does user u have access to the conference
 * @return bool
 */
bool access_conf(WUser * u, int sl, confrec * c) {
  WWIV_ASSERT(u);
  WWIV_ASSERT(c);

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


bool access_sub(WUser * u, int sl, subboardrec * s) {
  WWIV_ASSERT(u);
  WWIV_ASSERT(s);

  if (sl < s->readsl) {
    return false;
  }
  if (u->GetAge() < (s->age & 0x7f)) {
    return false;
  }
  if (s->ar != 0 && !u->HasArFlag(s->ar)) {
    return false;
  }
  if ((s->anony & anony_ansi_only) && !u->HasAnsi()) {
    return false;
  }

  return true;
}


bool access_dir(WUser * u, int sl, directoryrec * d) {
  WWIV_ASSERT(u);
  WWIV_ASSERT(d);

  if (u->GetDsl() < d->dsl) {
    return false;
  }
  if (u->GetAge() < d->age) {
    return false;
  }
  if (d->dar && !u->HasDarFlag(d->dar)) {
    return false;
  }
  if ((d->mask & mask_offline) && u->GetDsl() < 100) {
    return false;
  }
  if ((d->mask & mask_wwivreg) && !u->GetWWIVRegNumber()) {
    return false;
  }
  if (sl == 0) {
    return false;
  }

  return true;
}


void addusub(usersubrec * ss1, int ns, int sub, char key) {
  WWIV_ASSERT(ss1);

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
    ss1[last].subnum = static_cast< short >(sub);
    ss1[last].keys[0] = key;
  } else {
    for (int i = last; i > last_num; i--) {
      ss1[ i ] = ss1[ i - 1 ];
    }
    ss1[last_num].subnum = static_cast< short >(sub);
    ss1[last_num].keys[0] = 0;
  }
}

// returns true on success, false on failure
// used to return 0 on success, 1 on failure
bool setconf(unsigned int nConferenceType, int which, int nOldSubNumber) {
  int i, i1, ns, osub;
  confrec *c;
  usersubrec *ss1, s1;
  char *xdc, *xtc;

  int dp = 1;
  int tp = 0;

  switch (nConferenceType) {
  case CONF_SUBS:

    ss1 = usub;
    ns = GetSession()->num_subs;
    if (nOldSubNumber == -1) {
      osub = usub[GetSession()->GetCurrentMessageArea()].subnum;
    } else {
      osub = nOldSubNumber;
    }
    xdc = dc;
    xtc = tc;
    xdc[0] = '/';
    if (which == -1) {
      c = NULL;
    } else {
      if (which < 0 || which >= subconfnum) {
        return false;
      }
      c = &(subconfs[which]);
      if (!access_conf(GetSession()->GetCurrentUser(), GetSession()->GetEffectiveSl(), c)) {
        return false;
      }
    }
    break;
  case CONF_DIRS:
    ss1 = udir;
    ns = GetSession()->num_dirs;
    if (nOldSubNumber == -1) {
      osub = udir[GetSession()->GetCurrentFileArea()].subnum;
    } else {
      osub = nOldSubNumber;
    }
    xdc = dcd;
    xtc = dtc;
    xdc[0] = '/';
    if (which == -1) {
      c = NULL;
    } else {
      if (which < 0 || which >= dirconfnum) {
        return false;
      }
      c = &(dirconfs[which]);
      if (!access_conf(GetSession()->GetCurrentUser(), GetSession()->GetEffectiveSl(), c)) {
        return false;
      }
    }
    break;
  default:
    return false;
  }

  memset(&s1, 0, sizeof(s1));
  s1.subnum = -1;

  for (i = 0; i < ns; i++) {
    ss1[i] = s1;
  }

  if (c) {
    for (i = 0; i < c->num; i++) {
      switch (nConferenceType) {
      case CONF_SUBS:
        if (access_sub(GetSession()->GetCurrentUser(), GetSession()->GetEffectiveSl(),
                       (subboardrec *) & subboards[c->subs[i]])) {
          addusub(ss1, ns, c->subs[i], subboards[c->subs[i]].key);
        }
        break;
      case CONF_DIRS:
        if (access_dir(GetSession()->GetCurrentUser(), GetSession()->GetEffectiveSl(),
                       (directoryrec *) & directories[c->subs[i]])) {
          addusub(ss1, ns, c->subs[i], 0);
        }
        break;
      default:
        std::cout << "[utility.cpp] setconf called with nConferenceType != (CONF_SUBS || CONF_DIRS)\r\n";
        WWIV_ASSERT(true);
        break;
      }
    }
  } else {
    switch (nConferenceType) {
    case CONF_SUBS:
      for (i = 0; i < subconfnum; i++) {
        if (access_conf(GetSession()->GetCurrentUser(), GetSession()->GetEffectiveSl(), &(subconfs[i]))) {
          for (i1 = 0; i1 < subconfs[i].num; i1++) {
            if (access_sub(GetSession()->GetCurrentUser(), GetSession()->GetEffectiveSl(),
                           (subboardrec *) & subboards[subconfs[i].subs[i1]])) {
              addusub(ss1, ns, subconfs[i].subs[i1], subboards[subconfs[i].subs[i1]].key);
            }
          }
        }
      }
      break;
    case CONF_DIRS:
      for (i = 0; i < dirconfnum; i++) {
        if (access_conf(GetSession()->GetCurrentUser(), GetSession()->GetEffectiveSl(), &(dirconfs[i]))) {
          for (i1 = 0; i1 < static_cast<int>(dirconfs[i].num); i1++) {
            if (access_dir(GetSession()->GetCurrentUser(), GetSession()->GetEffectiveSl(),
                           (directoryrec *) & directories[dirconfs[i].subs[i1]])) {
              addusub(ss1, ns, dirconfs[i].subs[i1], 0);
            }
          }
        }
      }
      break;
    default:
      std::cout << "[utility.cpp] setconf called with nConferenceType != (CONF_SUBS || CONF_DIRS)\r\n";
      WWIV_ASSERT(true);
      break;

    }
  }

  i1 = (nConferenceType == CONF_DIRS && ss1[0].subnum == 0) ? 0 : 1;

  for (i = 0; (i < ns) && (ss1[i].keys[0] == 0) && (ss1[i].subnum != -1); i++) {
    if (i1 < 100) {
      if (((i1 % 10) == 0) && i1) {
        xdc[dp++] = static_cast< char >('0' + (i1 / 10));
      }
    } else {
      if ((i1 % 100) == 0) {
        xtc[tp++] = static_cast< char >('0' + (i1 / 100));
      }
    }
    snprintf(ss1[i].keys, sizeof(ss1[i].keys), "%d", i1++);
  }


  xdc[dp] = '\0';
  xtc[tp] = '\0';

  for (i1 = 0; (i1 < ns) && (ss1[i1].subnum != -1); i1++) {
    if (ss1[i1].subnum == osub) {
      break;
    }
  }
  if (i1 >= ns || ss1[i1].subnum == -1) {
    i1 = 0;
  }

  switch (nConferenceType) {
  case CONF_SUBS:
    GetSession()->SetCurrentMessageArea(i1);
    break;
  case CONF_DIRS:
    GetSession()->SetCurrentFileArea(i1);
    break;
  }

  return true;
}


void setuconf(int nConferenceType, int num, int nOldSubNumber) {
  switch (nConferenceType) {
  case CONF_SUBS:
    if (num >= 0 && num < MAX_CONFERENCES && uconfsub[num].confnum != -1) {
      GetSession()->SetCurrentConferenceMessageArea(num);
      setconf(nConferenceType, uconfsub[GetSession()->GetCurrentConferenceMessageArea()].confnum, nOldSubNumber);
      return;
    }
    break;
  case CONF_DIRS:
    if (num >= 0 && num < MAX_CONFERENCES && uconfdir[num].confnum != -1) {
      GetSession()->SetCurrentConferenceFileArea(num);
      setconf(nConferenceType, uconfdir[GetSession()->GetCurrentConferenceFileArea()].confnum, nOldSubNumber);
      return;
    }
    break;
  default:
    std::cout << "[utility.cpp] setuconf called with nConferenceType != (CONF_SUBS || CONF_DIRS)\r\n";
    WWIV_ASSERT(true);
    break;
  }
  setconf(nConferenceType, -1, nOldSubNumber);
}


void changedsl() {
  int ocurconfsub = uconfsub[GetSession()->GetCurrentConferenceMessageArea()].confnum;
  int ocurconfdir = uconfdir[GetSession()->GetCurrentConferenceFileArea()].confnum;
  GetApplication()->UpdateTopScreen();

  userconfrec c1;
  c1.confnum = -1;

  int i;
  for (i = 0; i < MAX_CONFERENCES; i++) {
    uconfsub[i] = c1;
    uconfdir[i] = c1;
  }

  int nTempSubConferenceNumber = 0;
  for (i = 0; i < subconfnum; i++) {
    if (access_conf(GetSession()->GetCurrentUser(), GetSession()->GetEffectiveSl(), &(subconfs[i]))) {
      c1.confnum = static_cast< short >(i);
      uconfsub[ nTempSubConferenceNumber++ ] = c1;
    }
  }

  int nTempDirConferenceNumber = 0;
  for (i = 0; i < dirconfnum; i++) {
    if (access_conf(GetSession()->GetCurrentUser(), GetSession()->GetEffectiveSl(), &(dirconfs[i ]))) {
      c1.confnum = static_cast< short >(i);
      uconfdir[ nTempDirConferenceNumber++ ] = c1;
    }
  }

  for (GetSession()->SetCurrentConferenceMessageArea(0);
       (GetSession()->GetCurrentConferenceMessageArea() < MAX_CONFERENCES)
       && (uconfsub[GetSession()->GetCurrentConferenceMessageArea()].confnum != -1);
       GetSession()->SetCurrentConferenceMessageArea(GetSession()->GetCurrentConferenceMessageArea() + 1)) {
    if (uconfsub[GetSession()->GetCurrentConferenceMessageArea()].confnum == ocurconfsub) {
      break;
    }
  }

  if (GetSession()->GetCurrentConferenceMessageArea() >= MAX_CONFERENCES ||
      uconfsub[GetSession()->GetCurrentConferenceMessageArea()].confnum == -1) {
    GetSession()->SetCurrentConferenceMessageArea(0);
  }

  for (GetSession()->SetCurrentConferenceFileArea(0); (GetSession()->GetCurrentConferenceFileArea() < MAX_CONFERENCES)
       && (uconfdir[GetSession()->GetCurrentConferenceFileArea()].confnum != -1);
       GetSession()->SetCurrentConferenceMessageArea(GetSession()->GetCurrentConferenceFileArea() + 1)) {
    if (uconfdir[GetSession()->GetCurrentConferenceFileArea()].confnum == ocurconfdir) {
      break;
    }
  }

  if (GetSession()->GetCurrentConferenceFileArea() >= MAX_CONFERENCES ||
      uconfdir[GetSession()->GetCurrentConferenceFileArea()].confnum == -1) {
    GetSession()->SetCurrentConferenceFileArea(0);
  }

  if (okconf(GetSession()->GetCurrentUser())) {
    setuconf(CONF_SUBS, GetSession()->GetCurrentConferenceMessageArea(), -1);
    setuconf(CONF_DIRS, GetSession()->GetCurrentConferenceFileArea(), -1);
  } else {
    setconf(CONF_SUBS, -1, -1);
    setconf(CONF_DIRS, -1, -1);
  }
}
