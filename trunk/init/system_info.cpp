/**************************************************************************/
/*                                                                        */
/*                 WWIV Initialization Utility Version 5.0                */
/*                 Copyright (C)2014, WWIV Software Services              */
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
#include "user_editor.h"

#include <cstdlib>
#include <cstring>
#include <string>

#include "ifcns.h"
#include "init.h"
#include "input.h"
#include "utility.h"
#include "wwivinit.h"


static void print_time(unsigned short t, char *s) {
  sprintf(s, "%02d:%02d", t / 60, t % 60);
}

static unsigned short get_time(char *s) {
  if (s[2] != ':') {
    return 0xffff;
  }

  unsigned short h = atoi(s);
  unsigned short m = atoi(s + 3);

  if (h > 23 || m > 59) {
    return 0xffff;
  }

  unsigned short t = h * 60 + m;

  return t;
}

/* change newuserpw, systempw, systemname, systemphone, sysopname,
   newusersl, newuserdsl, maxwaiting, closedsystem, systemnumber,
   maxusers, newuser_restrict, sysconfig, sysoplowtime, sysophightime,
   req_ratio, newusergold, sl, autoval
 */
void sysinfo1() {
  char j0[15], j1[10], j2[20], j3[5], j4[5], j5[20], j6[10], j7[10], j8[10],
       j9[5], j10[10], j11[10], j12[5], j17[10], j18[10], j19[10], rs[20];
  int i, i1, cp;

  char ch1 = '0';

  read_status();

  if (status.callernum != 65535) {
    status.callernum1 = (long)status.callernum;
    status.callernum = 65535;
    save_status();
  }

  sprintf(j3, "%u", syscfg.newusersl);
  sprintf(j4, "%u", syscfg.newuserdsl);
  print_time(syscfg.sysoplowtime, j6);
  print_time(syscfg.sysophightime, j7);
  print_time(syscfg.netlowtime, j1);
  print_time(syscfg.nethightime, j10);
  sprintf(j9, "%u", syscfg.maxwaiting);
  sprintf(j11, "%u", syscfg.maxusers);
  if (syscfg.closedsystem) {
    strcpy(j12, "Y");
  } else {
    strcpy(j12, "N");
  }
  sprintf(j17, "%u", status.callernum1);
  sprintf(j19, "%u", status.days);
  sprintf(j5, "%g", syscfg.newusergold);
  sprintf(j8, "%5.3f", syscfg.req_ratio);
  sprintf(j18, "%5.3f", syscfg.post_call_ratio);
  sprintf(j0, "%d", syscfg.wwiv_reg_number);

  strcpy(rs, restrict_string);
  for (i = 0; i <= 15; i++) {
    if (rs[i] == ' ') {
      rs[i] = ch1++;
    }
    if (syscfg.newuser_restrict & (1 << i)) {
      j2[i] = rs[i];
    } else {
      j2[i] = 32;
    }
  }
  j2[16] = 0;
  out->Cls();
  out->SetColor(SchemeId::NORMAL);
  out->window()->Printf("System PW        : %s\n", syscfg.systempw);
  out->window()->Printf("System name      : %s\n", syscfg.systemname);
  out->window()->Printf("System phone     : %s\n", syscfg.systemphone);
  out->window()->Printf("Closed system    : %s\n", j12);

  out->window()->Printf("Newuser PW       : %s\n", syscfg.newuserpw);
  out->window()->Printf("Newuser restrict : %s\n", j2);
  out->window()->Printf("Newuser SL       : %-3s\n", j3);
  out->window()->Printf("Newuser DSL      : %-3s\n", j4);
  out->window()->Printf("Newuser gold     : %-5s\n", j5);

  out->window()->Printf("Sysop name       : %s\n", syscfg.sysopname);
  out->window()->Printf("Sysop low time   : %-5s\n", j6);
  out->window()->Printf("Sysop high time  : %-5s\n", j7);

  out->window()->Printf("Net low time     : %-5s\n", j1);
  out->window()->Printf("Net high time    : %-5s\n", j10);

  out->window()->Printf("Up/Download ratio: %-5s\n", j8);
  out->window()->Printf("Post/Call ratio  : %-5s\n", j18);

  out->window()->Printf("Max waiting      : %-3s\n", j9);
  out->window()->Printf("Max users        : %-5s\n", j11);
  out->window()->Printf("Caller number    : %-7s\n", j17);
  out->window()->Printf("Days active      : %-7s\n", j19);

  out->SetColor(SchemeId::NORMAL);
  cp = 0;
  bool done = false;
  do {
    out->window()->GotoXY(19, cp);
    switch (cp) {
    case 0:
      editline(out->window(), syscfg.systempw, 20, UPPER_ONLY, &i1, "");
      trimstr(syscfg.systempw);
      break;
    case 1:
      editline(out->window(), syscfg.systemname, 50, ALL, &i1, "");
      trimstr(syscfg.systemname);
      break;
    case 2:
      editline(out->window(), syscfg.systemphone, 12, UPPER_ONLY, &i1, "");
      break;
    case 3:
      editline(out->window(), j12, 1, UPPER_ONLY, &i1, "");
      if (j12[0] == 'Y') {
        syscfg.closedsystem = 1;
        strcpy(j12, "Y");
      } else {
        syscfg.closedsystem = 0;
        strcpy(j12, "N");
      }
      out->window()->Printf("%-1s", j12);
      break;
    case 4:
      editline(out->window(), syscfg.newuserpw, 20, UPPER_ONLY, &i1, "");
      trimstr(syscfg.newuserpw);
      break;
    case 5:
      editline(out->window(), j2, 16, SET, &i1, rs);
      syscfg.newuser_restrict = 0;
      for (i = 0; i < 16; i++) {
        if (j2[i] != 32) {
          syscfg.newuser_restrict |= (1 << i);
        }
      }
      break;
    case 6:
      editline(out->window(), j3, 3, NUM_ONLY, &i1, "");
      syscfg.newusersl = atoi(j3);
      sprintf(j3, "%u", syscfg.newusersl);
      out->window()->Printf("%-3s", j3);
      break;
    case 7:
      editline(out->window(), j4, 3, NUM_ONLY, &i1, "");
      syscfg.newuserdsl = atoi(j4);
      sprintf(j4, "%u", syscfg.newuserdsl);
      out->window()->Printf("%-3s", j4);
      break;
    case 8:
      editline(out->window(), j5, 5, NUM_ONLY, &i1, "");
      syscfg.newusergold = (float) atoi(j5);
      sprintf(j5, "%g", syscfg.newusergold);
      out->window()->Printf("%-5s", j5);
      break;
    case 9:
      editline(out->window(), syscfg.sysopname, 50, ALL, &i1, "");
      trimstr(syscfg.sysopname);
      break;
    case 10:
      editline(out->window(), j6, 5, UPPER_ONLY, &i1, "");
      if (get_time(j6) != 0xffff) {
        syscfg.sysoplowtime = get_time(j6);
      }
      print_time(syscfg.sysoplowtime, j6);
      out->window()->Printf("%-5s", j6);
      break;
    case 11:
      editline(out->window(), j7, 5, UPPER_ONLY, &i1, "");
      if (get_time(j7) != 0xffff) {
        syscfg.sysophightime = get_time(j7);
      }
      print_time(syscfg.sysophightime, j7);
      out->window()->Printf("%-5s", j7);
      break;
    case 12:
      editline(out->window(), j1, 5, UPPER_ONLY, &i1, "");
      if (get_time(j1) != 0xffff) {
        syscfg.netlowtime = get_time(j1);
      }
      print_time(syscfg.netlowtime, j1);
      out->window()->Printf("%-5s", j1);
      break;
    case 13:
      editline(out->window(), j10, 5, UPPER_ONLY, &i1, "");
      if (get_time(j10) != 0xffff) {
        syscfg.nethightime = get_time(j10);
      }
      print_time(syscfg.nethightime, j10);
      out->window()->Printf("%-5s", j10);
      break;
    case 14: {
      editline(out->window(), j8, 5, UPPER_ONLY, &i1, "");
      float fRatio = syscfg.req_ratio;
      sscanf(j8, "%f", &fRatio);
      if ((fRatio > 9.999) || (fRatio < 0.001)) {
        fRatio = 0.0;
      }
      syscfg.req_ratio = fRatio;
      sprintf(j8, "%5.3f", syscfg.req_ratio);
      out->window()->Puts(j8);
    }
    break;
    case 15: {
      editline(out->window(), j18, 5, UPPER_ONLY, &i1, "");
      float fRatio = syscfg.post_call_ratio;
      sscanf(j18, "%f", &fRatio);
      if ((fRatio > 9.999) || (fRatio < 0.001)) {
        fRatio = 0.0;
      }
      syscfg.post_call_ratio = fRatio;
      sprintf(j18, "%5.3f", syscfg.post_call_ratio);
      out->window()->Puts(j18);
    }
    break;
    case 16:
      editline(out->window(), j9, 3, NUM_ONLY, &i1, "");
      syscfg.maxwaiting = atoi(j9);
      sprintf(j9, "%u", syscfg.maxwaiting);
      out->window()->Printf("%-3s", j9);
      break;
    case 17:
      editline(out->window(), j11, 5, NUM_ONLY, &i1, "");
      syscfg.maxusers = atoi(j11);
      sprintf(j11, "%u", syscfg.maxusers);
      out->window()->Printf("%-5s", j11);
      break;
    case 18:
      editline(out->window(), j17, 7, NUM_ONLY, &i1, "");
      if ((unsigned long) atol(j17) != status.callernum1) {
        status.callernum1 = atol(j17);
        sprintf(j17, "%u", status.callernum1);
        out->window()->Printf("%-7s", j17);
        save_status();
      }
      break;
    case 19:
      editline(out->window(), j19, 7, NUM_ONLY, &i1, "");
      if (atoi(j19) != status.days) {
        status.days = atoi(j19);
        sprintf(j19, "%u", status.days);
        out->window()->Printf("%-7s", j19);
        save_status();
      }
      break;
    }
    cp = GetNextSelectionPosition(0, 19, cp, i1);
    if (i1 == DONE) {
      done = true;
    }
  } while (!done);
  save_config();
}
