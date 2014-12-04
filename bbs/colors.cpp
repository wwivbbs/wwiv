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
#include "bbs/keycodes.h"


void get_colordata() {
  File file(syscfg.datadir, COLOR_DAT);
  if (!file.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
    return;
  }
  file.Read(&rescolor, sizeof(colorrec));
}

void save_colordata() {
  File file(syscfg.datadir, COLOR_DAT);
  if (!file.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
    return;
  }
  file.Write(&rescolor, sizeof(colorrec));
}

#if defined( _MSC_VER )
#pragma warning( push )
#pragma warning( disable : 4125 )
#endif

void list_ext_colors() {
  bout.GotoXY(40, 7);
  bout <<
                     "|#7\xDA\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xBF";
  bout.GotoXY(40, 8);
  bout << "|#7\xB3        |#2COLOR LIST|#7       \xB3";
  bout.GotoXY(40, 9);
  bout <<
                     "|#7\xC3\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xB4";
  bout.GotoXY(40, 10);
  bout << "|#7\xB3 . \003 \003!!\0030 \003\"\"\0030 \003##\0030 \003$$\0030 \003%%\0030 \003&&\0030" <<
                     " \003''\0030 \003((\0030 \003))\0030 \003**\0030 \003++\0037 \xB3";
  bout.GotoXY(40, 11);
  bout << "|#7\xB3 \003,,\0030 \003--\0030 \003..\0030 \003//\0030 0 \00311\0030 \00322\0030" <<
                     " \00333\0030 \00344\0030 \00355\0030 \00366\0030 \00377 \xB3";
  bout.GotoXY(40, 12);
  bout << "|#7\xB3 \00388\0030 \00399\0030 \003::\0030 \003;;\0030 \003<<\0030 \003==\0030 \003>>\0030" <<
                     " \003??\0030 \003@@\0030 \003AA\0030 \003BB\0030 \003CC\0037 \xB3";
  bout.GotoXY(40, 13);
  bout << "|#7\xB3 \003DD\0030 \003EE\0030 \003FF\0030 \003GG\0030 \003HH\0030 \003II\0030 \003JJ\0030" <<
                     " \003KK\0030 \003LL\0030 \003MM\0030 \003NN\0030 \003OO\0037 \xB3";
  bout.GotoXY(40, 14);
  bout << "|#7\xB3 \003PP\0030 \003QQ\0030 \003RR\0030 \003SS\0030 \003TT\0030 \003UU\0030 \003VV\0030" <<
                     " \003WW\0030 \003XX\0030 \003YY\0030 \003ZZ\0030 \003[[\0037 \xB3";
  bout.GotoXY(40, 15);
  bout << "|#7\xB3 \003\\\\\0030 \003]]\0030 \003^^\0030 \003__\0030 \003``\0030 \003aa\0030 \003bb\0030" <<
                     " \003cc\0030 \003dd\0030 \003ee\0030 \003ff\0030 \003gg\0037 \xB3";
  bout.GotoXY(40, 16);
  bout << "|#7\xB3 \003hh\0030 \003ii\0030 \003jj\0030 \003kk\0030 \003ll\0030 \003mm\0030 \003nn\0030" <<
                     " \003oo\0030 \003pp\0030 \003qq\0030 \003rr\0030 \003ss\0037 \xB3";
  bout.GotoXY(40, 17);
  bout << "|#7\xB3 \003tt\0030 \003uu\0030 \003vv\0030 \003ww\0030 \003xx\0030 \003yy\0030 \003zz\0030" <<
                     " \003{{\0030 \003||\0030 \003}}\0030 \003~~\0030 \003[[\0037 \xB3";
  bout.GotoXY(40, 18);
  bout <<
                     "|#7\xC3\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xB4";
  bout.GotoXY(40, 19);
  bout << "|#7\xB3                         \xB3";
  bout.GotoXY(40, 20);
  bout <<
                     "|#7\xC0\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xD9";
}

#if defined( _MSC_VER )
#pragma warning( pop )
#endif // _MSC_VER

void color_config() {
  char s[81], ch, ch1;
  unsigned char c;
  int done = 0, n;

  strcpy(s, "      SAMPLE COLOR      ");
  get_colordata();
  do {
    bputch(CL);
    bout << "Extended Color Configuration - Enter Choice, ^Z to Quit, ^R to Relist\r\n:";
    list_ext_colors();
    bout.GotoXY(2, 2);
    ch = getkey();
    if (ch == CZ) {
      done = 1;
    }
    if (ch == CR) {
      list_ext_colors();
      bout.GotoXY(2, 2);
    }
    if (ch >= 32) {
      bputch(ch);
      n = ch - 48;
      bout.nl(2);
      color_list();
      bout.Color(0);
      bout.nl();
      bout << "|#2Foreground? ";
      ch1 = onek("01234567");
      c = static_cast< unsigned char >(ch1 - '0');
      bout.GotoXY(41, 19);
      bout.SystemColor(c);
      bout << s;
      bout.Color(0);
      bout.GotoXY(1, 16);
      bout << "|#2Background? ";
      ch1 = onek("01234567");
      c = static_cast< unsigned char >(c | ((ch1 - '0') << 4));
      bout.GotoXY(41, 19);
      bout.SystemColor(c);
      bout << s;
      bout.Color(0);
      bout.GotoXY(1, 17);
      bout.nl();
      bout << "|#5Intensified? ";
      if (yesno()) {
        c |= 0x08;
      }
      bout.GotoXY(41, 19);
      bout.SystemColor(c);
      bout << s;
      bout.Color(0);
      bout.GotoXY(1, 18);
      bout.nl();
      bout << "|#5Blinking? ";
      if (yesno()) {
        c |= 0x80;
      }
      bout.nl();
      bout.GotoXY(41, 19);
      bout.SystemColor(c);
      bout << s;
      bout.Color(0);
      bout.GotoXY(1, 21);
      bout.SystemColor(c);
      bout << DescribeColorCode(c);
      bout.Color(0);
      bout.nl(2);
      bout << "|#5Is this OK? ";
      if (yesno()) {
        bout << "\r\nColor saved.\r\n\n";
        if ((n <= -1) && (n >= -16)) {
          rescolor.resx[207 + abs(n)] = static_cast< unsigned char >(c);
        } else if ((n >= 0) && (n <= 9)) {
          session()->user()->SetColor(n, c);
        } else {
          rescolor.resx[n - 10] = static_cast< unsigned char >(c);
        }
      } else {
        bout << "\r\nNot saved, then.\r\n\n";
      }
    }
  } while (!done && !hangup);
  bout.nl(2);
  bout << "|#5Save changes? ";
  if (yesno()) {
    save_colordata();
  } else {
    get_colordata();
  }
  bputch(CL);
  bout.nl(3);
}

void buildcolorfile() {
  for (int i = 0; i < 240; i++) {
    rescolor.resx[i] = static_cast< unsigned char >(i + 1);
  }

  bout.nl();
  save_colordata();
}
