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
#include <algorithm>
#include <vector>
#include "bbs/conf.h"

#include "wwiv.h"
#include "core/wtextfile.h"

static int disable_conf_cnt = 0;

/* Max line length in conference files */
#define MAX_CONF_LINE 4096

/* To prevent heap fragmentation, allocate confrec.subs in multiples. */
#define CONF_MULTIPLE ( GetSession()->GetMaxNumberMessageAreas() / 5 )

// Locals
char* GetGenderAllowed(int nGender, char *pszGenderAllowed);
int  modify_conf(int conftype, int which);
void insert_conf(int conftype, int n);
void delete_conf(int conftype, int n);
bool create_conf_file(int conftype);
bool str_to_numrange(const char *pszNumbersText, std::vector<int>& list);


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
    if (okconf(GetSession()->GetCurrentUser())) {
      g_flags |= g_flag_disable_conf;
      ocs = GetSession()->GetCurrentConferenceMessageArea();
      oss = usub[GetSession()->GetCurrentMessageArea()].subnum;
      ocd = GetSession()->GetCurrentConferenceFileArea();
      osd = udir[GetSession()->GetCurrentFileArea()].subnum;
      setuconf(CONF_SUBS, -1, oss);
      setuconf(CONF_DIRS, -1, osd);
    }
  } else if (disable_conf_cnt) {
    disable_conf_cnt--;
    if ((disable_conf_cnt == 0) && (g_flags & g_flag_disable_conf)) {
      g_flags &= ~g_flag_disable_conf;
      setuconf(CONF_SUBS, ocs, oss);
      setuconf(CONF_DIRS, ocd, osd);
    }
  }
}

void reset_disable_conf() {
  disable_conf_cnt = 0;
}


/*
 * Returns info on conference types
 */
int get_conf_info(int conftype, int *num, confrec ** cpp,
                  char *pszFileName, int *num_s, userconfrec ** uc) {
  switch (conftype) {
  case CONF_SUBS:
    if (cpp) {
      *cpp = subconfs;
    }
    if (num) {
      *num = subconfnum;
    }
    if (pszFileName) {
      sprintf(pszFileName, "%s%s", syscfg.datadir, SUBS_CNF);
    }
    if (num_s) {
      *num_s = GetSession()->num_subs;
    }
    if (uc) {
      *uc = uconfsub;
    }
    return 0;
  case CONF_DIRS:
    if (cpp) {
      *cpp = dirconfs;
    }
    if (num) {
      *num = dirconfnum;
    }
    if (pszFileName) {
      sprintf(pszFileName, "%s%s", syscfg.datadir, DIRS_CNF);
    }
    if (num_s) {
      *num_s = GetSession()->num_dirs;
    }
    if (uc) {
      *uc = uconfdir;
    }
    return 0;
  }
  return 1;
}


/*
 * Presents user with selection of conferences, gets selection, and changes
 * conference.
 */
void jump_conf(int conftype) {
  int i;
  char s[81], s1[101];
  confrec *cp = NULL;
  userconfrec *uc = NULL;
  int nc;

  GetSession()->bout.DisplayLiteBar(" [ %s Conference Selection ] ", syscfg.systemname);
  get_conf_info(conftype, &nc, &cp, NULL, NULL, &uc);
  bool abort = false;
  strcpy(s, " ");
  GetSession()->bout.NewLine();
  for (i = 0; (i < MAX_CONFERENCES) && (uc[i].confnum != -1); i++) {
    sprintf(s1, "|#2%c|#7)|#1 %s", cp[uc[i].confnum].designator,
            stripcolors(reinterpret_cast<char*>(cp[uc[i].confnum].name)));
    pla(s1, &abort);
    s[i + 1] = cp[uc[i].confnum].designator;
    s[i + 2] = '\0';
  }

  GetSession()->bout.NewLine();
  GetSession()->bout << "|#2Select [" << &s[1] << ", <space> to quit]: ";
  char ch = onek(s);
  if (ch != ' ') {
    for (i = 0; (i < MAX_CONFERENCES) && (uc[i].confnum != -1); i++) {
      if (ch == cp[uc[i].confnum].designator) {
        setuconf(conftype, i, -1);
        break;
      }
    }
  }
}


/*
 * Removes, inserts, or swaps subs/dirs in all conferences of a specified
 * type.
 */
void update_conf(int conftype, SUBCONF_TYPE * sub1, SUBCONF_TYPE * sub2, int action) {
  confrec *cp = NULL;
  int nc, num_s;
  int pos1, pos2;
  int i, i1;
  confrec c;

  if (get_conf_info(conftype, &nc, &cp, NULL, &num_s, NULL)) {
    return;
  }

  if (!nc || !cp) {
    return;
  }

  switch (action) {
  case CONF_UPDATE_INSERT:
    if (!sub1) {
      return;
    }
    for (i = 0; i < nc; i++) {
      c = cp[i];
      if ((c.num) && (c.subs)) {
        for (i1 = 0; i1 < c.num; i1++) {
          if (c.subs[i1] >= *sub1) {
            c.subs[i1] = c.subs[i1] + 1;
          }
        }
      }
      cp[i] = c;
    }
    save_confs(conftype, -1, NULL);
    break;
  case CONF_UPDATE_DELETE:
    if (!sub1) {
      return;
    }
    for (i = 0; i < nc; i++) {
      c = cp[i];
      if ((c.num) && (c.subs)) {
        if (in_conference(*sub1, &c) != -1) {
          delsubconf(conftype, &c, sub1);
        }
      }
      for (i1 = 0; i1 < c.num; i1++) {
        if (c.subs[i1] >= *sub1) {
          c.subs[i1] = c.subs[i1] - 1;
        }
      }
      cp[i] = c;
    }
    save_confs(conftype, -1, NULL);
    break;
  case CONF_UPDATE_SWAP:
    if ((!sub1) || (!sub2)) {
      return;
    }
    for (i = 0; i < nc; i++) {
      c = cp[i];
      pos1 = in_conference(*sub1, &c);
      pos2 = in_conference(*sub2, &c);
      if (pos1 != -1) {
        c.subs[pos1] = *sub2;
      }
      if (pos2 != -1) {
        c.subs[pos2] = *sub1;
      }
      cp[i] = c;
    }
    save_confs(conftype, -1, NULL);
    break;
  }
}



/*
 * Returns bitmapped word representing an AR or DAR string.
 */
int str_to_arword(const char *arstr) {
  int rar = 0;
  char s[81];

  strcpy(s, arstr);
  WWIV_STRUPR(s);

  for (int i = 0; i < 16; i++) {
    if (strchr(s, i + 'A') != NULL) {
      rar |= (1 << i);
    }
  }
  return rar;
}

/*
 * Converts an int to a string representing those ARs (DARs).
 */
char *word_to_arstr(int ar) {
  static char arstr[17];
  int i, i1 = 0;

  if (ar) {
    for (i = 0; i < 16; i++) {
      if ((1 << i) & ar) {
        arstr[i1++] = static_cast< char >('A' + i);
      }
    }
  }
  arstr[i1] = '\0';
  if (!arstr[0]) {
    strcpy(arstr, "-");
  }
  return arstr;
}


/*
 * Returns first available conference designator, of a specified conference
 * type.
 */
char first_available_designator(int conftype) {
  confrec *cp = NULL;
  int nc;

  if (get_conf_info(conftype, &nc, &cp, NULL, NULL, NULL)  || nc == MAX_CONFERENCES) {
    return 0;
  }

  for (int i = 0; i < MAX_CONFERENCES; i++) {
    int i1;
    for (i1 = 0; i1 < nc; i1++) {
      if (cp[i1].designator == ('A' + i)) {
        break;
      }
    }
    if (i1 >= nc) {
      return static_cast< char >(i + 'A');
    }
  }

  return 0;
}


/*
 * Returns 1 if subnum is allocated to conference c, 0 otherwise.
 */
int in_conference(int subnum, confrec * c) {
  int nInit = -1;

  if (!c || !c->subs || c->num == 0) {
    return -1;
  }
  for (unsigned short i = 0; i < c->num; i++) {
    if (nInit == -1 && c->subs[i] == subnum) {
      nInit = i;
    }
  }

  return nInit;
}


/*
 * Saves conferences of a specified conference-type.
 */
void save_confs(int conftype, int whichnum, confrec * c) {
  char szFileName[MAX_PATH];
  int num, num1;
  confrec *cp = NULL;

  if (get_conf_info(conftype, &num, &cp, szFileName, NULL, NULL)) {
    return;
  }

  WTextFile f(szFileName, "wt");
  if (!f.IsOpen()) {
    GetSession()->bout.NewLine();
    GetSession()->bout << "|#6Couldn't write to conference file: " << szFileName << wwiv::endl;
    return;
  }
  num1 = num;
  if (whichnum == num) {
    num1++;
  }

  f.WriteFormatted("%s\n\n", "/* !!!-!!! Do not edit this file - use WWIV's conf editor! !!!-!!! */");
  for (int i = 0; i < num1; i++) {
    if (whichnum == i) {
      if (c) {
        f.WriteFormatted("~%c %s\n!%d %d %d %d %d %d %d %u %d %s ",
                         c->designator, c->name, c->status, c->minsl, c->maxsl, c->mindsl,
                         c->maxdsl, c->minage, c->maxage, c->minbps, c->sex,
                         word_to_arstr(c->ar));
        f.WriteFormatted("%s\n@", word_to_arstr(c->dar));
        if (((c->num) > 0) && (c->subs != NULL)) {
          for (int i2 = 0; i2 < c->num; i2++) {
            f.WriteFormatted("%d ", c->subs[i2]);
          }
        }
        f.WriteFormatted("\n\n");
      } else {
        continue;
      }
    }
    if (i < num) {
      f.WriteFormatted("~%c %s\n!%d %d %d %d %d %d %d %u %d %s ",
                       cp[i].designator, cp[i].name, cp[i].status,
                       cp[i].minsl, cp[i].maxsl, cp[i].mindsl,
                       cp[i].maxdsl, cp[i].minage, cp[i].maxage,
                       cp[i].minbps, cp[i].sex, word_to_arstr(cp[i].ar));
      f.WriteFormatted("%s\n@", word_to_arstr(cp[i].dar));
      if (cp[i].num > 0) {
        for (int i2 = 0; i2 < cp[i].num; i2++) {
          f.WriteFormatted("%d ", cp[i].subs[i2]);
        }
      }
      f.WriteFormatted("\n\n");
    }
  }
  f.Close();
}


/*
 * Lists subs/dirs/whatever allocated to a specific conference.
 */
void showsubconfs(int conftype, confrec * c) {
  char s[120], szIndex[5], confstr[MAX_CONFERENCES + 1], ts[2];

  if (!c) {
    return;
  }

  int num, nc;
  confrec *cp;
  if (get_conf_info(conftype, &nc, &cp, NULL, &num, NULL)) {
    return;
  }

  bool abort = false;
  pla("|#2NN  Name                                    Indx ConfList", &abort);
  pla("|#7--- ======================================= ---- ==========================", &abort);

  for (int i = 0; i < num && !abort; i++) {
    int test = in_conference(i, c);
    sprintf(szIndex, "%d", test);

    // build conf list string
    strcpy(confstr, "");
    for (int i1 = 0; i1 < nc; i1++) {
      if (in_conference(i, &cp[i1]) != -1) {
        sprintf(ts, "%c", cp[i1].designator);
        strcat(confstr, ts);
      }
    }
    sort_conf_str(confstr);

    switch (conftype) {
    case CONF_SUBS:
      sprintf(s, "|#1%3d |#2%-39.39s |#7%4.4s %s", i, stripcolors(subboards[i].name),
              (test > -1) ? szIndex : charstr(4, '-'), confstr);
      break;
    case CONF_DIRS:
      sprintf(s, "|#1%3d |#2%-39.39s |#9%4.4s %s", i, stripcolors(directories[i].name),
              (test > -1) ? szIndex : charstr(4, '-'), confstr);
      break;
    }
    pla(s, &abort);
  }
}


/*
 * Takes a string like "100-150,201,333" and returns pointer to list of
 * numbers. Number of numbers in the list is returned in numinlist.
 */
bool str_to_numrange(const char *pszNumbersText, std::vector<int>& list) {
  SUBCONF_TYPE intarray[1024];

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
    if (hangup) {
      return false;
    }

    std::string temp = extractword(word, pszNumbersText, ",");
    int nRangeCount = wordcount(temp, " -\t\r\n");
    switch (nRangeCount) {
    case 0:
      break;
    case 1: {
      //This means there is no number in the range, it's just ,###,###
      int nNumber = atoi(extractword(1, temp, " -\t\r\n"));
      if (nNumber < 1024 && nNumber >= 0) {
        intarray[ nNumber ] = 1;
      }
    }
    break;
    default: {
      // We're dealing with a range here, so it should be "XXX-YYY"
      // convert the left and right strings to numbers
      int nLowNumber = atoi(extractword(1, temp, " -\t\r\n,"));
      int nHighNumber = atoi(extractword(2, temp, " -\t\r\n,"));
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
  for (int loop = 0; loop < 1024; loop++) {
    if (intarray[loop]) {
      list.push_back(loop);
    }
  }
  return true;
}


/*
 * Function to add one subconf (sub/dir/whatever) to a conference.
 */
void addsubconf(int conftype, confrec * c, SUBCONF_TYPE * which) {
  std::vector<int> intlist;

  if (!c || !c->subs) {
    return;
  }

  int num;
  if (get_conf_info(conftype, NULL, NULL, NULL, &num, NULL)) {
    return;
  }

  if (num < 1) {
    return;
  }

  if (c->num >= 1023) {
    GetSession()->bout << "Maximum number of subconfs already in that conference.\r\n";
    return;
  }
  if (which == NULL) {
    GetSession()->bout.NewLine();
    GetSession()->bout << "|#2Add: ";
    std::string text;
    input(text, 60, true);
    if (text.empty()) {
      return;
    }
    str_to_numrange(text.c_str(), intlist);
  } else {
    intlist.clear();
    intlist.push_back(*which);
  }

  // add the subconfs now
  for (std::vector<int>::iterator iter = intlist.begin(); iter != intlist.end(); ++iter) {
    int nConference = *iter;
    if (nConference >= num) {
      break;
    }
    if (in_conference(nConference, c) > -1) {
      continue;
    }
    if (c->num >= c->maxnum) {
      c->maxnum = c->maxnum + static_cast< unsigned short >(CONF_MULTIPLE);
      SUBCONF_TYPE *tptr = static_cast<SUBCONF_TYPE *>(BbsAllocA(c->maxnum * sizeof(SUBCONF_TYPE)));
      WWIV_ASSERT(tptr != NULL);
      if (tptr) {
        memmove(tptr, c->subs, (c->maxnum - CONF_MULTIPLE) * sizeof(SUBCONF_TYPE));
        free(c->subs);
        c->subs = tptr;
      } else {
        GetSession()->bout << "|#6Not enough memory left to insert anything.\r\n";
        return;
      }
    }
    c->subs[c->num] = static_cast< unsigned short >(nConference);
    c->num++;
  }
}


/*
 * Function to delete one subconf (sub/dir/whatever) from a conference.
 */
void delsubconf(int conftype, confrec * c, SUBCONF_TYPE * which) {
  if (!c || !c->subs || c->num < 1) {
    return;
  }

  int num;
  if (get_conf_info(conftype, NULL, NULL, NULL, &num, NULL)) {
    return;
  }

  std::vector<int> intlist;
  if (which == NULL) {
    GetSession()->bout.NewLine();
    GetSession()->bout << "|#2Remove: ";
    std::string text;
    input(text, 60, true);
    if (text.empty()) {
      return;
    }
    str_to_numrange(text.c_str(), intlist);
  } else {
    intlist.push_back(*which);
  }

  for (std::vector<int>::iterator iter = intlist.begin(); iter != intlist.end(); ++iter) {
    int nConference = *iter;
    if (nConference >= num) {
      break;
    }
    int pos = in_conference(nConference, c);
    if (pos < 0) {
      continue;
    }
    for (int i2 = pos; i2 < c->num; i2++) {
      c->subs[i2] = c->subs[i2 + 1];
    }
    c->num--;
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
int modify_conf(int conftype,  int which) {
  int  changed = 0;
  bool ok   = false;
  bool done = false;
  int num, ns;
  SUBCONF_TYPE i;
  char ch, ch1, s[81];

  int n = which;

  confrec *cp;
  if (get_conf_info(conftype, &num, &cp, NULL, &ns, NULL) || (n >= static_cast<int>(num))) {
    return 0;
  }

  confrec c = cp[n];

  do {
    char szGenderAllowed[ 21 ];
    GetSession()->bout.ClearScreen();

    GetSession()->bout << "|#9A) Designator           : |#2" << c.designator << wwiv::endl;
    GetSession()->bout << "|#9B) Conf Name            : |#2" << c.name << wwiv::endl;
    GetSession()->bout << "|#9C) Min SL               : |#2" << static_cast<int>(c.minsl) << wwiv::endl;
    GetSession()->bout << "|#9D) Max SL               : |#2" << static_cast<int>(c.maxsl) << wwiv::endl;
    GetSession()->bout << "|#9E) Min DSL              : |#2" << static_cast<int>(c.mindsl) << wwiv::endl;
    GetSession()->bout << "|#9F) Max DSL              : |#2" << static_cast<int>(c.maxdsl) << wwiv::endl;
    GetSession()->bout << "|#9G) Min Age              : |#2" << static_cast<int>(c.minage) << wwiv::endl;
    GetSession()->bout << "|#9H) Max Age              : |#2" << static_cast<int>(c.maxage) << wwiv::endl;
    GetSession()->bout << "|#9I) ARs Required         : |#2" << word_to_arstr(c.ar) << wwiv::endl;
    GetSession()->bout << "|#9J) DARs Required        : |#2" << word_to_arstr(c.dar) << wwiv::endl;
    GetSession()->bout << "|#9K) Min BPS Required     : |#2" << c.minbps << wwiv::endl;
    GetSession()->bout << "|#9L) Gender Allowed       : |#2" << GetGenderAllowed(c.sex, szGenderAllowed) << wwiv::endl;
    GetSession()->bout << "|#9M) Ansi Required        : |#2" << YesNoString((c.status & conf_status_ansi) ? true : false) <<
                       wwiv::endl;
    GetSession()->bout << "|#9N) WWIV RegNum Required : |#2" << YesNoString((c.status & conf_status_wwivreg) ? true : false)
                       << wwiv::endl;
    GetSession()->bout << "|#9O) Available            : |#2" << YesNoString((c.status & conf_status_offline) ? true : false)
                       << wwiv::endl;
    GetSession()->bout << "|#9S) Edit SubConferences\r\n";
    GetSession()->bout << "|#9Q) Quit ConfEdit\r\n";
    GetSession()->bout.NewLine();

    GetSession()->bout << "|#7(|#2Q|#7=|#1Quit|#7) ConfEdit [|#1A|#7-|#1O|#7,|#1S|#7,|#1[|#7,|#1]|#7] : ";
    ch = onek("QSABCDEFGHIJKLMNO[]", true);

    switch (ch) {
    case '[':
      cp[n] = c;
      save_confs(conftype, n, NULL);
      if (n == 0) {
        for (n = MAX_CONFERENCES; get_conf_info(conftype, &num, &cp, NULL, &ns, NULL) || (n >= num); n--)
          ;
      } else {
        --n;
      }
      if (get_conf_info(conftype, &num, &cp, NULL, &ns, NULL) || (n >= num)) {
        n = 0;
        if (get_conf_info(conftype, &num, &cp, NULL, &ns, NULL) || (n >= static_cast<int>(num))) {
          return 0;
        }
      }
      c = cp[n];
      continue;
    case ']':
      cp[n] = c;
      save_confs(conftype, n, NULL);
      if (n == num) {
        n = 0;
      } else {
        ++n;
      }
      if (get_conf_info(conftype, &num, &cp, NULL, &ns, NULL) || (n >= num)) {
        n = 0;
        if (get_conf_info(conftype, &num, &cp, NULL, &ns, NULL) || (n >= num)) {
          return 0;
        }
      }
      c = cp[n];
      continue;
    case 'A':
      GetSession()->bout.NewLine();
      GetSession()->bout << "|#2New Designator: ";
      ch1 = onek("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
      ok = true;
      for (i = 0; i < num; i++) {
        if (i == n) {
          i++;
        }
        if (ok && cp && (cp[i].designator == ch1)) {
          ok = false;
        }
      }
      if (!ok) {
        GetSession()->bout.NewLine();
        GetSession()->bout << "|#6That designator already in use!\r\n";
        pausescr();
        break;
      }
      c.designator = ch1;
      changed = 1;
      break;
    case 'B': {
      GetSession()->bout.NewLine();
      GetSession()->bout << "|#2Conference Name: ";
      std::string conferenceName;
      inputl(conferenceName, 60);
      if (!conferenceName.empty()) {
        strcpy(reinterpret_cast<char*>(c.name), conferenceName.c_str());
        changed = 1;
      }
    }
    break;
    case 'C': {
      GetSession()->bout.NewLine();
      GetSession()->bout << "|#2Min SL: ";
      std::string minSl;
      input(minSl, 3);
      if (!minSl.empty()) {
        if (atoi(minSl.c_str()) >= 0 && atoi(minSl.c_str()) <= 255) {
          c.minsl = wwiv::strings::StringToUnsignedChar(minSl.c_str());
          changed = 1;
        }
      }
    }
    break;
    case 'D':
      GetSession()->bout.NewLine();
      GetSession()->bout << "|#2Max SL: ";
      input(s, 3);
      if (s[0]) {
        if ((atoi(s) >= 0) && (atoi(s) <= 255)) {
          c.maxsl = wwiv::strings::StringToUnsignedChar(s);
          changed = 1;
        }
      }
      break;
    case 'E':
      GetSession()->bout.NewLine();
      GetSession()->bout << "|#2Min DSL: ";
      input(s, 3);
      if (s[0]) {
        if ((atoi(s) >= 0) && (atoi(s) <= 255)) {
          c.mindsl = wwiv::strings::StringToUnsignedChar(s);
          changed = 1;
        }
      }
      break;
    case 'F':
      GetSession()->bout.NewLine();
      GetSession()->bout << "|#2Max DSL";
      input(s, 3);
      if (s[0]) {
        if ((atoi(s) >= 0) && (atoi(s) <= 255)) {
          c.maxdsl = wwiv::strings::StringToUnsignedChar(s);
          changed = 1;
        }
      }
      break;
    case 'G':
      GetSession()->bout.NewLine();
      GetSession()->bout << "|#2Min Age: ";
      input(s, 2);
      if (s[0]) {
        c.minage = wwiv::strings::StringToUnsignedChar(s);
        changed = 1;
      }
      break;
    case 'H':
      GetSession()->bout.NewLine();
      GetSession()->bout << "|#2Max Age: ";
      input(s, 3);
      if (s[0]) {
        if ((atoi(s) >= 0) && (atoi(s) <= 255)) {
          c.maxage = wwiv::strings::StringToUnsignedChar(s);
          changed = 1;
        }
      }
      break;
    case 'I':
      GetSession()->bout.NewLine();
      GetSession()->bout << "|#2Toggle which AR requirement? ";
      ch1 = onek("\rABCDEFGHIJKLMNOP ");
      switch (ch1) {
      case ' ':
      case '\r':
        break;
      default:
        c.ar ^= (1 << (ch1 - 'A'));
        changed = 1;
        break;
      }
      break;
    case 'J':
      GetSession()->bout.NewLine();
      GetSession()->bout << "|#2Toggle which DAR requirement? ";
      ch1 = onek("\rABCDEFGHIJKLMNOP ");
      switch (ch1) {
      case ' ':
      case '\r':
        break;
      default:
        c.dar ^= (1 << (ch1 - 'A'));
        changed = 1;
        break;
      }
      break;
    case 'K':
      GetSession()->bout.NewLine();
      GetSession()->bout << "|#2Min BPS Rate: ";
      input(s, 5);
      if (s[0]) {
        c.minbps = (SUBCONF_TYPE)(atol(s));
        changed = 1;
      }
      break;
    case 'L':
      GetSession()->bout.NewLine();
      changed = 1;
      GetSession()->bout << "|#5(Q=Quit) Gender Allowed: (M)ale, (F)emale, (A)ll: ";
      ch1 = onek("MFAQ");
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
      break;
    case 'M':
      GetSession()->bout.NewLine();
      changed = 1;
      c.status &= ~conf_status_ansi;
      GetSession()->bout << "|#5Require ANSI for this conference? ";
      if (yesno()) {
        c.status |= conf_status_ansi;
      }
      break;
    case 'N':
      GetSession()->bout.NewLine();
      changed = 1;
      c.status &= ~conf_status_wwivreg;
      GetSession()->bout << "|#5Require WWIV RegNum for this conference? ";
      if (yesno()) {
        c.status |= conf_status_wwivreg;
      }
      break;
    case 'O':
      GetSession()->bout.NewLine();
      changed = 1;
      c.status &= ~conf_status_offline;
      GetSession()->bout << "|#5Make this conference available to users? ";
      if (!noyes()) {
        c.status |= conf_status_offline;
      }
      break;
    case 'S':
      ok = false;
      do {
        GetSession()->bout.NewLine();
        GetSession()->bout << "|#2A)dd, R)emove, C)lear, F)ill, Q)uit, S)tatus: ";
        ch1 = onek("QARCFS");
        switch (ch1) {
        case 'A':
          addsubconf(conftype, &c, NULL);
          changed = 1;
          break;
        case 'R':
          delsubconf(conftype, &c, NULL);
          changed = 1;
          break;
        case 'C':
          for (i = 0; i < ns; i++) {
            if (in_conference(i, &c) > -1) {
              delsubconf(conftype, &c, &i);
              changed = 1;
            }
          }
          break;
        case 'F':
          for (i = 0; i < ns; i++) {
            if (in_conference(i, &c) < 0) {
              addsubconf(conftype, &c, &i);
              changed = 1;
            }
          }
          break;
        case 'Q':
          ok = true;
          break;
        case 'S':
          GetSession()->bout.NewLine();
          showsubconfs(conftype, &c);
          break;
        }
      } while (!ok);
      for (i = 0; i < ns; i++) {
        if (in_conference(i, &c) > -1) {
          delsubconf(conftype, &c, &i);
          addsubconf(conftype, &c, &i);
        }
      }
      break;
    case 'Q':
      done = true;
      break;
    }
  } while (!done && !hangup);

  cp[n] = c;
  return changed;
}


/*
 * Function for inserting one conference.
 */
void insert_conf(int conftype,  int n) {
  confrec c;
  int num;

  if (get_conf_info(conftype, &num, NULL, NULL, NULL, NULL) || (n > num)) {
    return;
  }

  c.designator = first_available_designator(conftype);

  if (c.designator == 0) {
    return;
  }

  sprintf(reinterpret_cast<char*>(c.name), "%s%c", "Conference ", c.designator);
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
  c.subs = NULL;

  save_confs(conftype, n, &c);

  read_in_conferences(conftype);

  if (modify_conf(conftype, n)) {
    save_confs(conftype, -1, NULL);
  }
}


/*
 * Function for deleting one conference.
 */
void delete_conf(int conftype,  int n) {
  int num;

  if (get_conf_info(conftype, &num, NULL, NULL, NULL, NULL) || (n >= num)) {
    return;
  }

  save_confs(conftype, n, NULL);
  read_in_conferences(conftype);
}


/*
 * Function for editing conferences.
 */
void conf_edit(int conftype) {
  bool done = false;
  char ch;
  confrec *cp;
  int num;

  if (get_conf_info(conftype, &num, &cp, NULL, NULL, NULL)) {
    return;
  }

  GetSession()->bout.ClearScreen();
  list_confs(conftype, 1);

  do {
    GetSession()->bout.NewLine();
    GetSession()->bout << "|#2I|#7)|#1nsert, |#2D|#7)|#1elete, |#2M|#7)|#1odify, |#2Q|#7)|#1uit, |#2? |#7 : ";
    ch = onek("QIDM?", true);
    get_conf_info(conftype, &num, &cp, NULL, NULL, NULL);
    switch (ch) {
    case 'D':
      if (num == 1) {
        GetSession()->bout << "\r\n|#6Cannot delete all conferences!\r\n";
      } else {
        int ec = select_conf("Delete which conference? ", conftype, 0);
        if (ec >= 0) {
          delete_conf(conftype, ec);
        }
      }
      break;
    case 'I':
      if (num == MAX_CONFERENCES) {
        GetSession()->bout << "\r\n|#6Cannot insert any more conferences!\r\n";
      } else {
        int ec = select_conf("Insert before which conference ('$'=at end)? ", conftype, 0);
        if (ec != -1) {
          if (ec == -2) {
            ec = num;
          }
          insert_conf(conftype, ec);
        }
      }
      break;
    case 'M': {
      int ec = select_conf("Modify which conference? ", conftype, 0);
      if (ec >= 0) {
        if (modify_conf(conftype, ec)) {
          save_confs(conftype, -1, NULL);
        }
      }
    }
    break;
    case 'Q':
      done = true;
      break;
    case '?':
      GetSession()->bout.ClearScreen();
      list_confs(conftype, 1);
      break;
    }
  } while (!done && !hangup);
  if (!GetApplication()->GetWfcStatus()) {
    changedsl();
  }
}


/*
 * Lists conferences of a specified type. If OP_FLAGS_SHOW_HIER is set,
 * then the subs (dirs, whatever) in each conference are also shown.
 */
void list_confs(int conftype, int ssc) {
  char s[121], s1[121];
  bool abort = false;
  int i, i2, num, num_s;
  confrec *cp;

  if (get_conf_info(conftype, &num, &cp, NULL, &num_s, NULL)) {
    return;
  }

  pla("|#2  Des Name                    LSL HSL LDSL HDSL LAge HAge LoBPS AR  DAR S A W", &abort);
  pla("|#7ษออออ อออออออออออออออออออออออ อออ อออ ออออ ออออ ออออ ออออ อออออ อออ อออ อ อ อ", &abort);

  for (i = 0; (i < num && !abort); i++) {
    sprintf(s, "%cอ|B1|15 %c |B0|#1 %-23.23s %3d %3d %4d %4d %4d %4d %5u %-3.3s ",
            (i == (num - 1)) ? 'ศ' : 'ฬ', cp[i].designator, cp[i].name, cp[i].minsl,
            cp[i].maxsl, cp[i].mindsl, cp[i].maxdsl, cp[i].minage, cp[i].maxage,
            cp[i].minbps, word_to_arstr(cp[i].ar));
    sprintf(s1, "%-3.3s %c %1.1s %1.1s",
            word_to_arstr(cp[i].dar),
            (cp[i].sex) ? ((cp[i].sex == 2) ? 'A' : 'F') : 'M',
            YesNoString((cp[i].status & conf_status_ansi) ? true : false),
            YesNoString((cp[i].status & conf_status_wwivreg)  ? true : false));
    strcat(s, s1);
    GetSession()->bout.Color(7);
    pla(s, &abort);
    if (GetApplication()->HasConfigFlag(OP_FLAGS_SHOW_HIER)) {
      if ((cp[i].num > 0) && (cp[i].subs != NULL) && (ssc)) {
        for (i2 = 0; ((i2 < cp[i].num) && !abort); i2++) {
          if (cp[i].subs[i2] < num_s) {
            GetSession()->bout.Color(7);
            sprintf(s, "%c  %cฤฤฤ |#9",
                    (i != (num - 1)) ? 'บ' : ' ',
                    (i2 == (cp[i].num - 1)) ? 'ภ' : 'ร');
            switch (conftype) {
            case CONF_SUBS:
              sprintf(s1, "%s%-3d : %s", "Sub #", subconfs[i].subs[i2],
                      stripcolors(subboards[cp[i].subs[i2]].name));
              break;
            case CONF_DIRS:
              sprintf(s1, "%s%-3d : %s", "Dir #", dirconfs[i].subs[i2],
                      stripcolors(directories[cp[i].subs[i2]].name));
              break;
            }
            strcat(s, s1);
            pla(s, &abort);
          }
        }
      }
    }
  }
  GetSession()->bout.NewLine();
}


/*
 * Allows user to select a conference. Returns the conference selected
 * (0-based), or -1 if the user aborted. Error message is printed for
 * invalid conference selections.
 */
int select_conf(const char *pszPromptText, int conftype, int listconfs) {
  int i = 0, i1, sl = 0;
  bool ok = false;
  char *mmk;

  do {
    if (listconfs || sl) {
      GetSession()->bout.NewLine();
      list_confs(conftype, 0);
      sl = 0;
    }
    if (pszPromptText && pszPromptText[0]) {
      GetSession()->bout.NewLine();
      GetSession()->bout <<  "|#1" << pszPromptText;
    }
    mmk = mmkey(0);
    if (!mmk[0]) {
      i = -1;
      ok = true;
    } else {
      switch (mmk[0]) {
      case '?':
        sl = 1;
        break;
      default:
        switch (conftype) {
        case CONF_SUBS:
          for (i1 = 0; i1 < subconfnum; i1++) {
            if (mmk[0] == subconfs[i1].designator) {
              ok = true;
              i = i1;
              break;
            }
          }
          break;
        case CONF_DIRS:
          for (i1 = 0; i1 < dirconfnum; i1++) {
            if (mmk[0] == dirconfs[i1].designator) {
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
        GetSession()->bout << "\r\n|#6Invalid conference designator!\r\n";
      }
    }
  } while (!ok && !hangup);
  return i;
}


/*
 * Creates a default conference file. Should only be called if no conference
 * file for that conference type already exists.
 */
bool create_conf_file(int conftype) {
  char szFileName[MAX_PATH];
  int num;

  if (get_conf_info(conftype, NULL, NULL, szFileName, &num, NULL)) {
    return false;
  }

  WTextFile f(szFileName, "wt");
  if (!f.IsOpen()) {
    return false;
  }

  f.WriteFormatted("%s\n\n", "/* !!!-!!! Do not edit this file - use WWIV's conf editor! !!!-!!! */");
  f.WriteFormatted("~A General\n!0 0 255 0 255 0 255 0 2 - -\n@");
  for (int i = 0; i < num; i++) {
    f.WriteFormatted("%d ", i);
  }
  f.WriteFormatted("\n\n");

  f.Close();
  return true;
}


/*
 * Reads in conferences and returns pointer to conference data. Out-of-memory
 * messages are shown if applicable.
 */
confrec *read_conferences(const char *pszFileName, int *nc, int max) {
  char ts[128], *ss;
  int i, i1, i2, i3, cc = 0;
  bool ok = true;

  *nc = get_num_conferences(pszFileName);
  if (*nc < 1) {
    return NULL;
  }

  if (!WFile::Exists(pszFileName)) {
    return NULL;
  }

  WTextFile f(pszFileName, "rt");
  if (!f.IsOpen()) {
    return NULL;
  }

  unsigned long l = static_cast<long>(*nc) * sizeof(confrec);
  confrec *conferences = static_cast<confrec *>(BbsAllocA(l));
  WWIV_ASSERT(conferences != NULL);
  if (!conferences) {
    std::cout << "Out of memory reading file [" << pszFileName << "]." << std::endl;
    f.Close();
    return NULL;
  }
  memset(conferences, 0, l);
  char * ls = static_cast<char*>(BbsAllocA(MAX_CONF_LINE));
  WWIV_ASSERT(ls != NULL);
  if (!ls) {
    f.Close();
    return NULL;
  }
  while (f.ReadLine(ls, MAX_CONF_LINE) && cc < MAX_CONFERENCES) {
    int nw = wordcount(ls, DELIMS_WHITE);
    if (nw) {
      strncpy(ts, extractword(1, ls, DELIMS_WHITE), sizeof(ts));
      if (ts[0]) {
        switch (ts[0]) {
        case '~':
          if ((nw > 1) && (strlen(ts) > 1) && (strlen(ls) > 3)) {
            char * bs = strtok(&ls[3], "\r\n");
            if (bs) {
              conferences[cc].designator = ts[1];
              strncpy(reinterpret_cast<char*>(conferences[cc].name), bs, sizeof(conferences[cc].name));
            }
          }
          break;
        case '!':
          if ((!conferences[cc].designator) || (nw != 11)) {
            break;
          }
          for (i = 1; i <= nw; i++) {
            strncpy(ts, extractword(i, ls, DELIMS_WHITE), sizeof(ts));
            switch (i) {
            case 1:
              if (strlen(ts) >= 2) {
                conferences[cc].status = wwiv::strings::StringToUnsignedShort(&ts[1]);
              }
              break;
            case 2:
              conferences[cc].minsl = wwiv::strings::StringToUnsignedChar(ts);
              break;
            case 3:
              conferences[cc].maxsl = wwiv::strings::StringToUnsignedChar(ts);
              break;
            case 4:
              conferences[cc].mindsl = wwiv::strings::StringToUnsignedChar(ts);
              break;
            case 5:
              conferences[cc].maxdsl = wwiv::strings::StringToUnsignedChar(ts);
              break;
            case 6:
              conferences[cc].minage = wwiv::strings::StringToUnsignedChar(ts);
              break;
            case 7:
              conferences[cc].maxage = wwiv::strings::StringToUnsignedChar(ts);
              break;
            case 8:
              conferences[cc].minbps = wwiv::strings::StringToUnsignedShort(ts);
              break;
            case 9:
              conferences[cc].sex = wwiv::strings::StringToUnsignedChar(ts);
              break;
            case 10:
              conferences[cc].ar = static_cast<unsigned short>(str_to_arword(ts));
              break;
            case 11:
              conferences[cc].dar = static_cast<unsigned short>(str_to_arword(ts));
              break;
            }
          }
          break;
        case '@':
          if (!conferences[cc].designator) {
            break;
          }
          if (strlen(extractword(1, ls, DELIMS_WHITE)) < 2) {
            conferences[cc].num = 0;
          } else {
            conferences[cc].num = static_cast<unsigned short>(nw);
          }
          conferences[cc].maxnum = conferences[cc].num;
          l = static_cast<long>(conferences[cc].num * sizeof(unsigned int) + 1);
          conferences[cc].subs = static_cast<unsigned short *>(BbsAllocA(l));
          WWIV_ASSERT(conferences[cc].subs != NULL);
          ok = (conferences[cc].subs != NULL) ? true : false;
          if (ok) {
            memset(conferences[cc].subs, 0, l);
          } else {
            std::cout << "Out of memory on conference file #" << cc + 1 << ", " <<
                      syscfg.datadir << pszFileName << "." << std::endl;
            for (i2 = 0; i2 < cc; i2++) {
              free(conferences[i2].subs);
            }
            free(conferences);
            free(ls);
            f.Close();
            return NULL;
          }
          i = 0;
          i1 = 0;
          for (ss = strtok(ls, DELIMS_WHITE); ((ss) && (i++ < nw)); ss = strtok(NULL, DELIMS_WHITE)) {
            if (i == 1) {
              if (strlen(ss) >= 2) {
                i3 = atoi(ss + 1);
              } else {
                i3 = -1;
              }
            } else {
              i3 = atoi(ss);
            }
            if ((i3 >= 0) && (i3 < max)) {
              conferences[cc].subs[i1++] = static_cast<unsigned short>(i3);
            }
          }
          conferences[cc].num = static_cast<unsigned short>(i1);
          cc++;
          break;
        }
      }
    }
  }
  f.Close();
  free(ls);
  return conferences;
}


/*
 * Reads in a conference type. Creates default conference file if it is
 * necessary. If conferences cannot be read, then BBS aborts.
 */
void read_in_conferences(int conftype) {
  int i, max;
  int *np = NULL;
  char s[81];
  confrec **cpp = NULL;

  if (get_conf_info(conftype, NULL, NULL, s, &max, NULL)) {
    return;
  }

  switch (conftype) {
  case CONF_SUBS:
    cpp = &subconfs;
    np = &subconfnum;
    break;
  case CONF_DIRS:
    cpp = &dirconfs;
    np = &dirconfnum;
    break;
  }

  // free up any old memory
  if (*cpp) {
    for (i = 0; i < *np; i++) {
      if ((*cpp)[i].subs) {
        free((*cpp)[i].subs);
      }
    }
    free(*cpp);
    *cpp = NULL;
  }
  if (!WFile::Exists(s)) {
    if (!create_conf_file(conftype)) {
      std::cout << "Problem creating conferences." << std::endl;
      GetApplication()->AbortBBS();
    }
  }
  *cpp = read_conferences(s, np, max);
  if (!(*cpp)) {
    std::cout << "Problem reading conferences." << std::endl;
    GetApplication()->AbortBBS();

  }
}


/*
 * Reads all conferences into memory. Creates default conference files if
 * none exist. If called when conferences already in memory, then memory
 * for "old" conference data is deallocated first.
 */
void read_all_conferences() {
  read_in_conferences(CONF_SUBS);
  read_in_conferences(CONF_DIRS);
}


/*
 * Returns number of conferences in a specified conference file.
 */
int get_num_conferences(const char *pszFileName) {
  char ls[MAX_CONF_LINE];
  int i = 0;

  if (!WFile::Exists(pszFileName)) {
    return 0;
  }
  WTextFile f(pszFileName, "rt");
  if (!f.IsOpen()) {
    return 0;
  }

  while (f.ReadLine(ls, sizeof(ls))) {
    if (ls[0] == '~') {
      i++;
    }
  }

  f.Close();
  return i;
}


/*
 * Returns number of "words" in a specified string, using a specified set
 * of characters as delimiters.
 */
int wordcount(const std::string& instr, const char *delimstr) {
  char szTempBuffer[MAX_CONF_LINE];
  int i = 0;

  strcpy(szTempBuffer, instr.c_str());
  for (char *s = strtok(szTempBuffer, delimstr); s; s = strtok(NULL, delimstr)) {
    i++;
  }
  return i;
}


/*
 * Returns pointer to string representing the nth "word" of a string, using
 * a specified set of characters as delimiters.
 */
char *extractword(int ww, const std::string& instr, const char *delimstr) {
  char szTempBuffer[MAX_CONF_LINE];
  static char rs[41];
  int i = 0;

  if (!ww) {
    return "";
  }

  strcpy(szTempBuffer, instr.c_str());
  for (char *s = strtok(szTempBuffer, delimstr); s && (i++ < ww); s = strtok(NULL, delimstr)) {
    if (i == ww) {
      strcpy(rs, s);
      return rs;
    }
  }
  return "";
}


void sort_conf_str(char *pszConferenceStr) {
  char s1[81], s2[81];

  strncpy(s1, pszConferenceStr, sizeof(s1));
  strncpy(s2, charstr(MAX_CONFERENCES, 32), sizeof(s2));

  for (char i = 'A'; i <= 'Z'; i++) {
    if (strchr(s1, i) != NULL) {
      s2[i - 'A'] = i;
    }
  }
  StringTrimEnd(s2);
  strcpy(pszConferenceStr, s2);
}

