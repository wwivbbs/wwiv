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

#include "wwiv.h"
#include "instmsg.h"
#include "core/strings.h"
#include "core/wutil.h"
#include "bbs/keycodes.h"
#include "bbs/wconstants.h"

// module private functions

void chatsound(int sf, int ef, int uf, int dly1, int dly2, int rp);


//////////////////////////////////////////////////////////////////////////////
//
// static variables for use in two_way_chat
//

#define MAXLEN 160
#define MAXLINES_SIDE 13

static int wwiv_x1, wwiv_y1, wwiv_x2, wwiv_y2, cp0, cp1;
static char(*side0)[MAXLEN], (*side1)[MAXLEN];


//////////////////////////////////////////////////////////////////////////////
//
// Makes various (local-only) sounds based upon input params. The params are:
//     sf = starting frequency, in hertz
//     ef = ending frequency, in hertz
//     uf = frequency change, in hertz, for each step
//   dly1 = delay, in milliseconds, between each step, when going from sf
//             to ef
//   dly2 = delay, in milliseconds, between each repetition of the sound
//             sequence
//     rp = number of times to play the whole sound sequence
//

void chatsound(int sf, int ef, int uf, int dly1, int dly2, int rp) {
  for (int i1 = 0; i1 < rp; i1++) {
    if (sf < ef) {
      for (int i = sf; i < ef; i += uf) {
        WWIV_Sound(i, dly1);
      }
    } else {
      for (int i = ef; i > sf; i -= uf) {
        WWIV_Sound(i, dly1);
      }
    }
    WWIV_Delay(dly2);
  }
}

//////////////////////////////////////////////////////////////////////////////
//
//
// Function called when user requests chat w/sysop.
//

void RequestChat() {
  bout.nl(2);
  if (sysop2() && !GetSession()->GetCurrentUser()->IsRestrictionChat()) {
    if (chatcall) {
      chatcall = false;
      bout << "Chat call turned off.\r\n";
      GetApplication()->UpdateTopScreen();
    } else {
      bout << "|#9Enter Reason for chat: \r\n|#0:";
      std::string chatReason;
      inputl(&chatReason, 70, true);
      if (chatReason.empty()) {
        if (!play_sdf(CHAT_NOEXT, false)) {
          chatsound(100, 800, 10, 10, 25, 5);
        }
        chatcall = true;

        char szChatReason[81];
        sprintf(szChatReason, "Chat: %s", chatReason.c_str());
        bout.nl();
        sysoplog(szChatReason);
        for (int nTemp = strlen(szChatReason); nTemp < 80; nTemp++) {
          szChatReason[nTemp] = SPACE;
        }
        szChatReason[80] = '\0';
        GetSession()->localIO()->SetChatReason(szChatReason);
        GetApplication()->UpdateTopScreen();
        bout << "Chat call turned ON.\r\n";
        bout.nl();
      }
    }
  } else {
    bout << "|#6" << syscfg.sysopname <<
                       " is not available.\r\n\n|#5Try sending feedback instead.\r\n";
    strcpy(irt, "|#1Tried Chatting");
    irt_name[0] = '\0';
    imail(1, 0);
  }
}


//////////////////////////////////////////////////////////////////////////////
//
//
// Allows selection of a name to "chat as". Returns selected string in *s.
//

void select_chat_name(char *pszSysopName) {
  GetSession()->DisplaySysopWorkingIndicator(true);
  GetSession()->localIO()->savescreen();
  strcpy(pszSysopName, syscfg.sysopname);
  curatr = GetSession()->GetChatNameSelectionColor();
  GetSession()->localIO()->MakeLocalWindow(20, 5, 43, 3);
  GetSession()->localIO()->LocalXYPuts(22, 6, "Chat As: ");
  curatr = GetSession()->GetEditLineColor();
  GetSession()->localIO()->LocalXYPuts(31, 6, charstr(30, SPACE));

  int rc;
  GetSession()->localIO()->LocalGotoXY(31, 6);
  GetSession()->localIO()->LocalEditLine(pszSysopName, 30, ALL, &rc, pszSysopName);
  if (rc != ABORTED) {
    StringTrimEnd(pszSysopName);
    int nUserNumber = atoi(pszSysopName);
    if (nUserNumber > 0 && nUserNumber <= syscfg.maxusers) {
      WUser tu;
      GetApplication()->GetUserManager()->ReadUser(&tu, nUserNumber);
      strcpy(pszSysopName, tu.GetUserNameAndNumber(nUserNumber));
    } else {
      if (!pszSysopName[0]) {
        strcpy(pszSysopName, syscfg.sysopname);
      }
    }
  } else {
    strcpy(pszSysopName, "");
  }
  GetSession()->localIO()->restorescreen();
  GetSession()->DisplaySysopWorkingIndicator(false);
}


// Allows two-way chatting until sysop aborts/exits chat. or the end of line is hit,
// then chat1 is back in control.
void two_way_chat(char *pszRollover, int nMaxLength, bool crend, char *pszSysopName) {
  char s2[100], temp1[100];
  int i, i1;

  int cm = chatting;
  int begx = GetSession()->localIO()->WhereX();
  if (pszRollover[0] != 0) {
    if (charbufferpointer) {
      char szTempBuffer[255];
      strcpy(szTempBuffer, pszRollover);
      strcat(szTempBuffer, &charbuffer[charbufferpointer]);
      strcpy(&charbuffer[1], szTempBuffer);
      charbufferpointer = 1;
    } else {
      strcpy(&charbuffer[1], pszRollover);
      charbufferpointer = 1;
    }
    pszRollover[0] = 0;
  }
  bool done = false;
  int side = 0;
  unsigned char ch = 0;
  do {
    ch = getkey();
    if (GetSession()->IsLastKeyLocal()) {
      if (GetSession()->localIO()->WhereY() == 11) {
        bout << "\x1b[12;1H";
        for (int screencount = 0; screencount < GetSession()->GetCurrentUser()->GetScreenChars(); screencount++) {
          s2[screencount] = '\xCD';
        }
        sprintf(temp1, "|B1|#2 %s chatting with %s |B0|#1", pszSysopName,
                GetSession()->GetCurrentUser()->GetUserNameAndNumber(GetSession()->usernum));
        int nNumCharsToMove = (((GetSession()->GetCurrentUser()->GetScreenChars() - strlen(stripcolors(temp1))) / 2));
        if (nNumCharsToMove) {
          strncpy(&s2[nNumCharsToMove - 1], temp1, (strlen(temp1)));
        } else {
          strcpy(s2, charstr(205, GetSession()->GetCurrentUser()->GetScreenChars() - 1));
        }
        s2[GetSession()->GetCurrentUser()->GetScreenChars()] = '\0';
        bout << s2;
        s2[0] = '\0';
        temp1[0] = '\0';
        for (int cntr = 1; cntr < 12; cntr++) {
          sprintf(s2, "\x1b[%d;%dH", cntr, 1);
          bout << s2;
          if ((cntr >= 0) && (cntr < 5)) {
            bout.Color(1);
            bout << side0[cntr + 6];
          }
          bout << "\x1b[K";
          s2[0] = 0;
        }
        sprintf(s2, "\x1b[%d;%dH", 5, 1);
        bout << s2;
        s2[0] = 0;
      } else if (GetSession()->localIO()->WhereY() > 11) {
        wwiv_x2 = (GetSession()->localIO()->WhereX() + 1);
        wwiv_y2 = (GetSession()->localIO()->WhereY() + 1);
        sprintf(s2, "\x1b[%d;%dH", wwiv_y1, wwiv_x1);
        bout << s2;
        s2[0] = 0;
      }
      side = 0;
      bout.Color(1);
    } else {
      if (GetSession()->localIO()->WhereY() >= 23) {
        for (int cntr = 13; cntr < 25; cntr++) {
          sprintf(s2, "\x1b[%d;%dH", cntr, 1);
          bout << s2;
          if ((cntr >= 13) && (cntr < 17)) {
            bout.Color(5);
            bout << side1[cntr - 7];
          }
          bout << "\x1b[K";
          s2[0] = '\0';
        }
        sprintf(s2, "\x1b[%d;%dH", 17, 1);
        bout << s2;
        s2[0] = '\0';
      } else if (GetSession()->localIO()->WhereY() < 12 && side == 0) {
        wwiv_x1 = (GetSession()->localIO()->WhereX() + 1);
        wwiv_y1 = (GetSession()->localIO()->WhereY() + 1);
        sprintf(s2, "\x1b[%d;%dH", wwiv_y2, wwiv_x2);
        bout << s2;
        s2[0] = 0;
      }
      side = 1;
      bout.Color(5);
    }
    if (cm) {
      if (chatting == 0) {
        ch = RETURN;
      }
    }
    if (ch >= SPACE) {
      if (side == 0) {
        if (GetSession()->localIO()->WhereX() < (GetSession()->GetCurrentUser()->GetScreenChars() - 1) && cp0 < nMaxLength) {
          if (GetSession()->localIO()->WhereY() < 11) {
            side0[GetSession()->localIO()->WhereY()][cp0++] = ch;
            bputch(ch);
          } else {
            side0[GetSession()->localIO()->WhereY()][cp0++] = ch;
            side0[GetSession()->localIO()->WhereY()][cp0] = 0;
            for (int cntr = 0; cntr < 12; cntr++) {
              sprintf(s2, "\x1b[%d;%dH", cntr, 1);
              bout << s2;
              if ((cntr >= 0) && (cntr < 6)) {
                bout.Color(1);
                bout << side0[cntr + 6];
                wwiv_y1 = GetSession()->localIO()->WhereY() + 1;
                wwiv_x1 = GetSession()->localIO()->WhereX() + 1;
              }
              bout << "\x1b[K";
              s2[0] = 0;
            }
            sprintf(s2, "\x1b[%d;%dH", wwiv_y1, wwiv_x1);
            bout << s2;
            s2[0] = 0;
          }
          if (GetSession()->localIO()->WhereX() == (GetSession()->GetCurrentUser()->GetScreenChars() - 1)) {
            done = true;
          }
        } else {
          if (GetSession()->localIO()->WhereX() >= (GetSession()->GetCurrentUser()->GetScreenChars() - 1)) {
            done = true;
          }
        }
      } else {
        if ((GetSession()->localIO()->WhereX() < (GetSession()->GetCurrentUser()->GetScreenChars() - 1))
            && (cp1 < nMaxLength)) {
          if (GetSession()->localIO()->WhereY() < 23) {
            side1[GetSession()->localIO()->WhereY() - 13][cp1++] = ch;
            bputch(ch);
          } else {
            side1[GetSession()->localIO()->WhereY() - 13][cp1++] = ch;
            side1[GetSession()->localIO()->WhereY() - 13][cp1] = 0;
            for (int cntr = 13; cntr < 25; cntr++) {
              sprintf(s2, "\x1b[%d;%dH", cntr, 1);
              bout << s2;
              if (cntr >= 13 && cntr < 18) {
                bout.Color(5);
                bout << side1[cntr - 7];
                wwiv_y2 = GetSession()->localIO()->WhereY() + 1;
                wwiv_x2 = GetSession()->localIO()->WhereX() + 1;
              }
              bout << "\x1b[K";
              s2[0] = '\0';
            }
            sprintf(s2, "\x1b[%d;%dH", wwiv_y2, wwiv_x2);
            bout << s2;
            s2[0] = '\0';
          }
          if (GetSession()->localIO()->WhereX() == (GetSession()->GetCurrentUser()->GetScreenChars() - 1)) {
            done = true;
          }
        } else {
          if (GetSession()->localIO()->WhereX() >= (GetSession()->GetCurrentUser()->GetScreenChars() - 1)) {
            done = true;
          }
        }
      }
    } else switch (ch) {
      case 7: {
        if (chatting && outcom) {
          rputch(7);
        }
      }
      break;
      case RETURN:                            /* C/R */
        if (side == 0) {
          side0[GetSession()->localIO()->WhereY()][cp0] = 0;
        } else {
          side1[GetSession()->localIO()->WhereY() - 13][cp1] = 0;
        }
        done = true;
        break;
      case BACKSPACE: {                           /* Backspace */
        if (side == 0) {
          if (cp0) {
            if (side0[GetSession()->localIO()->WhereY()][cp0 - 2] == 3) {
              cp0 -= 2;
              bout.Color(0);
            } else if (side0[GetSession()->localIO()->WhereY()][cp0 - 1] == 8) {
              cp0--;
              bputch(SPACE);
            } else {
              cp0--;
              bout.bs();
            }
          }
        } else if (cp1) {
          if (side1[GetSession()->localIO()->WhereY() - 13][cp1 - 2] == CC) {
            cp1 -= 2;
            bout.Color(0);
          } else  if (side1[GetSession()->localIO()->WhereY() - 13][cp1 - 1] == BACKSPACE) {
            cp1--;
            bputch(SPACE);
          } else {
            cp1--;
            bout.bs();
          }
        }
      }
      break;
      case CX:                            /* Ctrl-X */
        while (GetSession()->localIO()->WhereX() > begx) {
          bout.bs();
          if (side == 0) {
            cp0 = 0;
          } else {
            cp1 = 0;
          }
        }
        bout.Color(0);
        break;
      case CW:                            /* Ctrl-W */
        if (side == 0) {
          if (cp0) {
            do {
              if (side0[GetSession()->localIO()->WhereY()][cp0 - 2] == CC) {
                cp0 -= 2;
                bout.Color(0);
              } else if (side0[GetSession()->localIO()->WhereY()][cp0 - 1] == BACKSPACE) {
                cp0--;
                bputch(SPACE);
              } else {
                cp0--;
                bout.bs();
              }
            } while ((cp0) && (side0[GetSession()->localIO()->WhereY()][cp0 - 1] != SPACE) &&
                     (side0[GetSession()->localIO()->WhereY()][cp0 - 1] != BACKSPACE) &&
                     (side0[GetSession()->localIO()->WhereY()][cp0 - 2] != CC));
          }
        } else {
          if (cp1) {
            do {
              if (side1[GetSession()->localIO()->WhereY() - 13][cp1 - 2] == CC) {
                cp1 -= 2;
                bout.Color(0);
              } else if (side1[GetSession()->localIO()->WhereY() - 13][cp1 - 1] == BACKSPACE) {
                cp1--;
                bputch(SPACE);
              } else {
                cp1--;
                bout.bs();
              }
            } while ((cp1) && (side1[GetSession()->localIO()->WhereY() - 13][cp1 - 1] != SPACE) &&
                     (side1[GetSession()->localIO()->WhereY() - 13][cp1 - 1] != BACKSPACE) &&
                     (side1[GetSession()->localIO()->WhereY() - 13][cp1 - 2]));
          }
        }
        break;
      case CN:                            /* Ctrl-N */
        if (side == 0) {
          if ((GetSession()->localIO()->WhereX()) && (cp0 < nMaxLength)) {
            bputch(BACKSPACE);
            side0[GetSession()->localIO()->WhereY()][cp0++] = BACKSPACE;
          }
        } else  if ((GetSession()->localIO()->WhereX()) && (cp1 < nMaxLength)) {
          bputch(BACKSPACE);
          side1[GetSession()->localIO()->WhereY() - 13][cp1++] = BACKSPACE;
        }
        break;
      case CP:                            /* Ctrl-P */
        if (side == 0) {
          if (cp0 < nMaxLength - 1) {
            ch = getkey();
            if ((ch >= SPACE) && (ch <= 126)) {
              side0[GetSession()->localIO()->WhereY()][cp0++] = CC;
              side0[GetSession()->localIO()->WhereY()][cp0++] = ch;
              bout.Color(ch - 48);
            }
          }
        } else {
          if (cp1 < nMaxLength - 1) {
            ch = getkey();
            if ((ch >= SPACE) && (ch <= 126)) {
              side1[GetSession()->localIO()->WhereY() - 13][cp1++] = CC;
              side1[GetSession()->localIO()->WhereY() - 13][cp1++] = ch;
              bout.Color(ch - 48);
            }
          }
        }
        break;
      case TAB:                             /* Tab */
        if (side == 0) {
          i = 5 - (cp0 % 5);
          if (((cp0 + i) < nMaxLength)
              && ((GetSession()->localIO()->WhereX() + i) < GetSession()->GetCurrentUser()->GetScreenChars())) {
            i = 5 - ((GetSession()->localIO()->WhereX() + 1) % 5);
            for (i1 = 0; i1 < i; i1++) {
              side0[GetSession()->localIO()->WhereY()][cp0++] = SPACE;
              bputch(SPACE);
            }
          }
        } else {
          i = 5 - (cp1 % 5);
          if (((cp1 + i) < nMaxLength)
              && ((GetSession()->localIO()->WhereX() + i) < GetSession()->GetCurrentUser()->GetScreenChars())) {
            i = 5 - ((GetSession()->localIO()->WhereX() + 1) % 5);
            for (i1 = 0; i1 < i; i1++) {
              side1[GetSession()->localIO()->WhereY() - 13][cp1++] = SPACE;
              bputch(SPACE);
            }
          }
        }
        break;
      }
  } while (!done && !hangup);

  if (ch != RETURN) {
    if (side == 0) {
      i = cp0 - 1;
      while ((i > 0) && (side0[GetSession()->localIO()->WhereY()][i] != SPACE) &&
             ((side0[GetSession()->localIO()->WhereY()][i] != BACKSPACE) ||
             (side0[GetSession()->localIO()->WhereY()][i - 1] == CC))) {
        i--;
      }
      if ((i > (GetSession()->localIO()->WhereX() / 2)) && (i != (cp0 - 1))) {
        i1 = cp0 - i - 1;
        for (i = 0; i < i1; i++) {
          bputch(BACKSPACE);
        }
        for (i = 0; i < i1; i++) {
          bputch(SPACE);
        }
        for (i = 0; i < i1; i++) {
          pszRollover[i] = side0[GetSession()->localIO()->WhereY()][cp0 - i1 + i];
        }
        pszRollover[i1] = '\0';
        cp0 -= i1;
      }
      side0[GetSession()->localIO()->WhereY()][cp0] = '\0';
    } else {
      i = cp1 - 1;
      while ((i > 0) && (side1[GetSession()->localIO()->WhereY() - 13][i] != SPACE) &&
             ((side1[GetSession()->localIO()->WhereY() - 13][i] != BACKSPACE) ||
             (side1[GetSession()->localIO()->WhereY() - 13][i - 1] == CC))) {
        i--;
      }
      if ((i > (GetSession()->localIO()->WhereX() / 2)) && (i != (cp1 - 1))) {
        i1 = cp1 - i - 1;
        for (i = 0; i < i1; i++) {
          bputch(BACKSPACE);
        }
        for (i = 0; i < i1; i++) {
          bputch(SPACE);
        }
        for (i = 0; i < i1; i++) {
          pszRollover[i] = side1[GetSession()->localIO()->WhereY() - 13][cp1 - i1 + i];
        }
        pszRollover[i1] = '\0';
        cp1 -= i1;
      }
      side1[GetSession()->localIO()->WhereY() - 13][cp1] = '\0';
    }
  }
  if (crend && GetSession()->localIO()->WhereY() != 11 && GetSession()->localIO()->WhereY() < 23) {
    bout.nl();
  }
  if (side == 0) {
    cp0 = 0;
  } else {
    cp1 = 0;
  }
}

/****************************************************************************/

/*
 * High-level chat function, calls two_way_chat() if appropriate, else
 * uses normal TTY chat.
 */

void chat1(char *pszChatLine, bool two_way) {
  char cl[81], xl[81], s[255], s1[255], atr[81], s2[81], cc, szSysopName[81];

  select_chat_name(szSysopName);
  if (szSysopName[0] == '\0') {
    return;
  }

  int otag = GetSession()->tagging;
  GetSession()->tagging = 0;

  chatcall = false;
  if (two_way) {
    write_inst(INST_LOC_CHAT2, 0, INST_FLAGS_NONE);
    chatting = 2;
  } else {
    write_inst(INST_LOC_CHAT, 0, INST_FLAGS_NONE);
    chatting = 1;
  }
  double tc = timer();
  WFile chatFile(syscfg.gfilesdir, "chat.txt");

  GetSession()->localIO()->SaveCurrentLine(cl, atr, xl, &cc);
  s1[0] = '\0';

  bool oe = local_echo;
  local_echo = true;
  bout.nl(2);
  int nSaveTopData = GetSession()->topdata;
  if (!okansi()) {
    two_way = false;
  }
  if (modem_speed == 300) {
    two_way = false;
  }

  if (two_way) {
    GetSession()->localIO()->LocalCls();
    cp0 = cp1 = 0;
    if (defscreenbottom == 24) {
      GetSession()->topdata = WLocalIO::topdataNone;
      GetApplication()->UpdateTopScreen();
    }
    bout << "\x1b[2J";
    wwiv_x2 = 1;
    wwiv_y2 = 13;
    bout << "\x1b[1;1H";
    wwiv_x1 = GetSession()->localIO()->WhereX();
    wwiv_y1 = GetSession()->localIO()->WhereY();
    bout << "\x1b[12;1H";
    bout.Color(7);
    for (int screencount = 0; screencount < GetSession()->GetCurrentUser()->GetScreenChars(); screencount++) {
      bputch(static_cast< unsigned char >(205), true);
    }
    FlushOutComChBuffer();
    sprintf(s, " %s chatting with %s ", szSysopName,
            GetSession()->GetCurrentUser()->GetUserNameAndNumber(GetSession()->usernum));
    int nNumCharsToMove = ((GetSession()->GetCurrentUser()->GetScreenChars() - strlen(stripcolors(s))) / 2);
    sprintf(s1, "\x1b[12;%dH", std::max<int>(nNumCharsToMove, 0));
    bout << s1;
    bout.Color(4);
    bout << s << "\x1b[1;1H";
    s[0] = s1[0] = s2[0] = '\0';
  }
  bout << "|#7" << szSysopName << "'s here...";
  bout.nl(2);
  strcpy(s1, pszChatLine);

  if (two_way) {
    side0 = new char[MAXLINES_SIDE][MAXLEN];
    side1 = new char[MAXLINES_SIDE][MAXLEN];
    if (!side0 || !side1) {
      two_way = false;
    }
  }
  do {
    if (two_way) {
      two_way_chat(s1, MAXLEN, true, szSysopName);
    } else {
      inli(s, s1, MAXLEN, true, false);
    }
    if (chat_file && !two_way) {
      if (!chatFile.IsOpen()) {
        GetSession()->localIO()->LocalFastPuts("-] Chat file opened.\r\n");
        if (chatFile.Open(WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile)) {
          chatFile.Seek(0L, WFile::seekEnd);
          sprintf(s2, "\r\n\r\nChat file opened %s %s\r\n", fulldate(), times());
          chatFile.Write(s2, strlen(s2));
          strcpy(s2, "----------------------------------\r\n\r\n");
          chatFile.Write(s2, strlen(s2));
        }
      }
      strcat(s, "\r\n");
      chatFile.Write(s2, strlen(s2));
    } else if (chatFile.IsOpen()) {
      chatFile.Close();
      GetSession()->localIO()->LocalFastPuts("-] Chat file closed.\r\n");
    }
    if (hangup) {
      chatting = 0;
    }
  } while (chatting);

  if (chat_file) {
    chat_file = false;
  }
  if (side0) {
    delete[] side0;
    side0 = nullptr;
  }
  if (side1) {
    delete[] side1;
    side1 = nullptr;
  }
  bout.Color(0);

  if (two_way) {
    bout << "\x1b[2J";
  }

  bout.nl();
  bout << "|#7Chat mode over...\r\n\n";
  chatting = 0;
  tc = timer() - tc;
  if (tc < 0) {
    tc += SECONDS_PER_DAY_FLOAT;
  }
  extratimecall += tc;
  GetSession()->topdata = nSaveTopData;
  if (GetSession()->IsUserOnline()) {
    GetApplication()->UpdateTopScreen();
  }
  local_echo = oe;
  RestoreCurrentLine(cl, atr, xl, &cc);

  GetSession()->tagging = otag;
  if (okansi()) {
    bout << "\x1b[K";
  }
}

