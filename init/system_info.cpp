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

#include <curses.h>
#include <cstdint>
#include <string>

#include "ifcns.h"
#include "init.h"
#include "input.h"
#include "wwivinit.h"


static void print_time(unsigned short t, char *s) {
  sprintf(s, "%02.2d:%02.2d", t / 60, t % 60);
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
  textattr(COLOR_CYAN);
  Printf("System PW        : %s\n", syscfg.systempw);
  Printf("System name      : %s\n", syscfg.systemname);
  Printf("System phone     : %s\n", syscfg.systemphone);
  Printf("Closed system    : %s\n", j12);

  Printf("Newuser PW       : %s\n", syscfg.newuserpw);
  Printf("Newuser restrict : %s\n", j2);
  Printf("Newuser SL       : %-3s\n", j3);
  Printf("Newuser DSL      : %-3s\n", j4);
  Printf("Newuser gold     : %-5s\n", j5);

  Printf("Sysop name       : %s\n", syscfg.sysopname);
  Printf("Sysop low time   : %-5s\n", j6);
  Printf("Sysop high time  : %-5s\n", j7);

  Printf("Net low time     : %-5s\n", j1);
  Printf("Net high time    : %-5s\n", j10);

  Printf("Up/Download ratio: %-5s\n", j8);
  Printf("Post/Call ratio  : %-5s\n", j18);

  Printf("Max waiting      : %-3s\n", j9);
  Printf("Max users        : %-5s\n", j11);
  Printf("Caller number    : %-7s\n", j17);
  Printf("Days active      : %-7s\n", j19);

  textattr(COLOR_CYAN);
  cp = 0;
  bool done = false;
  do {
    out->GotoXY(19, cp);
    switch (cp) {
    case 0:
      editline(syscfg.systempw, 20, UPPER_ONLY, &i1, "");
      trimstr(syscfg.systempw);
      break;
    case 1:
      editline(syscfg.systemname, 50, ALL, &i1, "");
      trimstr(syscfg.systemname);
      break;
    case 2:
      editline(syscfg.systemphone, 12, UPPER_ONLY, &i1, "");
      break;
    case 3:
      editline(j12, 1, UPPER_ONLY, &i1, "");
      if (j12[0] == 'Y') {
        syscfg.closedsystem = 1;
        strcpy(j12, "Y");
      } else {
        syscfg.closedsystem = 0;
        strcpy(j12, "N");
      }
      Printf("%-1s", j12);
      break;
    case 4:
      editline(syscfg.newuserpw, 20, UPPER_ONLY, &i1, "");
      trimstr(syscfg.newuserpw);
      break;
    case 5:
      editline(j2, 16, SET, &i1, rs);
      syscfg.newuser_restrict = 0;
      for (i = 0; i < 16; i++) {
        if (j2[i] != 32) {
          syscfg.newuser_restrict |= (1 << i);
        }
      }
      break;
    case 6:
      editline(j3, 3, NUM_ONLY, &i1, "");
      syscfg.newusersl = atoi(j3);
      sprintf(j3, "%u", syscfg.newusersl);
      Printf("%-3s", j3);
      break;
    case 7:
      editline(j4, 3, NUM_ONLY, &i1, "");
      syscfg.newuserdsl = atoi(j4);
      sprintf(j4, "%u", syscfg.newuserdsl);
      Printf("%-3s", j4);
      break;
    case 8:
      editline(j5, 5, NUM_ONLY, &i1, "");
      syscfg.newusergold = (float) atoi(j5);
      sprintf(j5, "%g", syscfg.newusergold);
      Printf("%-5s", j5);
      break;
    case 9:
      editline(syscfg.sysopname, 50, ALL, &i1, "");
      trimstr(syscfg.sysopname);
      break;
    case 10:
      editline(j6, 5, UPPER_ONLY, &i1, "");
      if (get_time(j6) != 0xffff) {
        syscfg.sysoplowtime = get_time(j6);
      }
      print_time(syscfg.sysoplowtime, j6);
      Printf("%-5s", j6);
      break;
    case 11:
      editline(j7, 5, UPPER_ONLY, &i1, "");
      if (get_time(j7) != 0xffff) {
        syscfg.sysophightime = get_time(j7);
      }
      print_time(syscfg.sysophightime, j7);
      Printf("%-5s", j7);
      break;
    case 12:
      editline(j1, 5, UPPER_ONLY, &i1, "");
      if (get_time(j1) != 0xffff) {
        syscfg.netlowtime = get_time(j1);
      }
      print_time(syscfg.netlowtime, j1);
      Printf("%-5s", j1);
      break;
    case 13:
      editline(j10, 5, UPPER_ONLY, &i1, "");
      if (get_time(j10) != 0xffff) {
        syscfg.nethightime = get_time(j10);
      }
      print_time(syscfg.nethightime, j10);
      Printf("%-5s", j10);
      break;
    case 14: {
      editline(j8, 5, UPPER_ONLY, &i1, "");
      float fRatio = syscfg.req_ratio;
      sscanf(j8, "%f", &fRatio);
      if ((fRatio > 9.999) || (fRatio < 0.001)) {
        fRatio = 0.0;
      }
      syscfg.req_ratio = fRatio;
      sprintf(j8, "%5.3f", syscfg.req_ratio);
      Puts(j8);
    }
    break;
    case 15: {
      editline(j18, 5, UPPER_ONLY, &i1, "");
      float fRatio = syscfg.post_call_ratio;
      sscanf(j18, "%f", &fRatio);
      if ((fRatio > 9.999) || (fRatio < 0.001)) {
        fRatio = 0.0;
      }
      syscfg.post_call_ratio = fRatio;
      sprintf(j18, "%5.3f", syscfg.post_call_ratio);
      Puts(j18);
    }
    break;
    case 16:
      editline(j9, 3, NUM_ONLY, &i1, "");
      syscfg.maxwaiting = atoi(j9);
      sprintf(j9, "%u", syscfg.maxwaiting);
      Printf("%-3s", j9);
      break;
    case 17:
      editline(j11, 5, NUM_ONLY, &i1, "");
      syscfg.maxusers = atoi(j11);
      sprintf(j11, "%u", syscfg.maxusers);
      Printf("%-5s", j11);
      break;
    case 18:
      editline(j17, 7, NUM_ONLY, &i1, "");
      if ((unsigned long) atol(j17) != status.callernum1) {
        status.callernum1 = atol(j17);
        sprintf(j17, "%u", status.callernum1);
        Printf("%-7s", j17);
        save_status();
      }
      break;
    case 19:
      editline(j19, 7, NUM_ONLY, &i1, "");
      if (atoi(j19) != status.days) {
        status.days = atoi(j19);
        sprintf(j19, "%u", status.days);
        Printf("%-7s", j19);
        save_status();
      }
      break;
    }
    cp = GetNextSelectionPosition(0, 19, cp, i1);
    if (i1 == DONE) {
      done = true;
    }
  } while (!done && !hangup);
  save_config();
}