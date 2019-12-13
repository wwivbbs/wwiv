/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2019, WWIV Software Services             */
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
#include "bbs/syschat.h"

#include "bbs/batch.h"
#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/bbsutl1.h"
#include "bbs/datetime.h"
#include "bbs/email.h"
#include "bbs/input.h"
#include "bbs/instmsg.h"
#include "bbs/sysoplog.h"
#include "bbs/utility.h"
#include "core/datetime.h"
#include "core/os.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "local_io/keycodes.h"
#include "local_io/wconstants.h"
#include "sdk/config.h"
#include "sdk/names.h"
#include "sdk/filenames.h"
#include <algorithm>
#include <chrono>
#include <string>

using std::string;
using std::chrono::duration_cast;
using namespace std::chrono;
using namespace wwiv::core;
using namespace wwiv::os;
using namespace wwiv::sdk;
using namespace wwiv::strings;

// module private functions
void chatsound(int sf, int ef, int uf, int dly1, int dly2, int rp);

#define ABORTED 8

//////////////////////////////////////////////////////////////////////////////
//
// static variables for use in two_way_chat
//

#define MAXLEN 160
#define MAXLINES_SIDE 13

static int wwiv_x1, wwiv_y1, wwiv_x2, wwiv_y2, cp0, cp1;
static char (*side0)[MAXLEN], (*side1)[MAXLEN];

static bool s_chat_file = false;

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

void toggle_chat_file() {
  s_chat_file = !s_chat_file;
}

void chatsound(int sf, int ef, int uf, int dly1, int dly2, int rp) {
  for (int i1 = 0; i1 < rp; i1++) {
    if (sf < ef) {
      for (int i = sf; i < ef; i += uf) {
        sound(i, milliseconds(dly1));
      }
    } else {
      for (int i = ef; i > sf; i -= uf) {
        sound(i, milliseconds(dly1));
      }
    }
    sleep_for(milliseconds(dly2));
  }
}

//////////////////////////////////////////////////////////////////////////////
//
//
// Function called when user requests chat w/sysop.
//

void RequestChat() {
  bout.nl(2);
  if (sysop2() && !a()->user()->IsRestrictionChat()) {
    if (a()->chatcall()) {
      a()->clear_chatcall();
      bout << "Chat call turned off.\r\n";
      a()->UpdateTopScreen();
    } else {
      bout << "|#9Enter Reason for chat: \r\n|#0:";
      auto chatReason = input_text(70);
      if (!chatReason.empty()) {
        if (!play_sdf(CHAT_NOEXT, false)) {
          chatsound(100, 800, 10, 10, 25, 5);
        }
        const auto cr = fmt::format("Chat: {}", chatReason);
        bout.nl();
        sysoplog() << cr;
        a()->SetChatReason(cr);
        a()->UpdateTopScreen();
        bout << "Chat call turned ON.\r\n";
        bout.nl();
      }
    }
  } else {
    bout << "|#6" << a()->config()->sysop_name()
         << " is not available.\r\n\n|#5Try sending feedback instead.\r\n";
    imail("|#1Tried Chatting", 1, 0);
  }
}

//////////////////////////////////////////////////////////////////////////////
//
//
// Allows selection of a name to "chat as". Returns selected string in *s.
//

static std::string select_chat_name() {
  a()->DisplaySysopWorkingIndicator(true);
  a()->localIO()->savescreen();
  std::string sysop_name = a()->config()->sysop_name();
  bout.curatr(a()->GetChatNameSelectionColor());
  a()->localIO()->MakeLocalWindow(20, 5, 43, 3);
  a()->localIO()->PutsXY(22, 6, "Chat As: ");
  bout.curatr(a()->localIO()->GetEditLineColor());
  a()->localIO()->PutsXY(31, 6, std::string(30, SPACE));

  a()->localIO()->GotoXY(31, 6);
  int rc = a()->localIO()->EditLine(sysop_name, 30, AllowedKeys::ALL);
  if (rc != ABORTED) {
    StringTrimEnd(&sysop_name);
    auto user_number = to_number<int>(sysop_name);
    if (user_number > 0 && user_number <= a()->config()->max_users()) {
      sysop_name =  a()->names()->UserName(user_number);
    } else {
      if (!sysop_name[0]) {
        sysop_name = a()->config()->sysop_name();
      }
    }
  } else {
    sysop_name.clear();
  }
  a()->localIO()->restorescreen();
  a()->DisplaySysopWorkingIndicator(false);

  return sysop_name;
}

// Allows two-way chatting until sysop aborts/exits chat. or the end of line is hit,
// then chat1 is back in control.
static void two_way_chat(std::string* rollover, int max_length, bool crend, const std::string& sysop_name) {
  char s2[100], temp1[100];
  int i, i1;

  auto cm = a()->chatting_;
  auto begx = a()->localIO()->WhereX();
  if (!rollover->empty()) {
    if (bout.charbufferpointer_) {
      char szTempBuffer[255];
      to_char_array(szTempBuffer, *rollover);
      strcat(szTempBuffer, &bout.charbuffer[bout.charbufferpointer_]);
      strcpy(&bout.charbuffer[1], szTempBuffer);
      bout.charbufferpointer_ = 1;
    } else {
      strcpy(&bout.charbuffer[1], rollover->c_str());
      bout.charbufferpointer_ = 1;
    }
    rollover->clear();
  }
  bool done = false;
  int side = 0;
  unsigned char ch = 0;
  do {
    ch = bout.getkey();
    if (bout.IsLastKeyLocal()) {
      if (a()->localIO()->WhereY() == 11) {
        bout.GotoXY(1, 12);
        for (auto screencount = 0; screencount < a()->user()->GetScreenChars(); screencount++) {
          s2[screencount] = '\xCD';
        }
        const string unn = a()->names()->UserName(a()->usernum);
        sprintf(temp1, "|17|#2 %s chatting with %s |16|#1", sysop_name.c_str(), unn.c_str());
        int nNumCharsToMove = (((a()->user()->GetScreenChars() - strlen(stripcolors(temp1))) / 2));
        if (nNumCharsToMove) {
          strncpy(&s2[nNumCharsToMove - 1], temp1, (strlen(temp1)));
        } else {
          to_char_array(s2, std::string(a()->user()->GetScreenChars() - 1, '\xCD'));
        }
        s2[a()->user()->GetScreenChars()] = '\0';
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
          bout.clreol();
          s2[0] = 0;
        }
        bout.GotoXY(1, 5);
        s2[0] = 0;
      } else if (a()->localIO()->WhereY() > 11) {
        wwiv_x2 = (a()->localIO()->WhereX() + 1);
        wwiv_y2 = (a()->localIO()->WhereY() + 1);
        bout.GotoXY(wwiv_x1, wwiv_y1);
        s2[0] = 0;
      }
      side = 0;
      bout.Color(1);
    } else {
      if (a()->localIO()->WhereY() >= 23) {
        for (int cntr = 13; cntr < 25; cntr++) {
          sprintf(s2, "\x1b[%d;%dH", cntr, 1);
          bout << s2;
          if ((cntr >= 13) && (cntr < 17)) {
            bout.Color(5);
            bout << side1[cntr - 7];
          }
          bout.clreol();
          s2[0] = '\0';
        }
        sprintf(s2, "\x1b[%d;%dH", 17, 1);
        bout << s2;
        s2[0] = '\0';
      } else if (a()->localIO()->WhereY() < 12 && side == 0) {
        wwiv_x1 = (a()->localIO()->WhereX() + 1);
        wwiv_y1 = (a()->localIO()->WhereY() + 1);
        sprintf(s2, "\x1b[%d;%dH", wwiv_y2, wwiv_x2);
        bout << s2;
        s2[0] = 0;
      }
      side = 1;
      bout.Color(5);
    }
    if (cm) {
      if (a()->chatting_ == 0) {
        ch = RETURN;
      }
    }
    if (ch >= SPACE) {
      if (side == 0) {
        if (a()->localIO()->WhereX() < (a()->user()->GetScreenChars() - 1) && cp0 < max_length) {
          if (a()->localIO()->WhereY() < 11) {
            side0[a()->localIO()->WhereY()][cp0++] = ch;
            bout.bputch(ch);
          } else {
            side0[a()->localIO()->WhereY()][cp0++] = ch;
            side0[a()->localIO()->WhereY()][cp0] = 0;
            for (int cntr = 0; cntr < 12; cntr++) {
              sprintf(s2, "\x1b[%d;%dH", cntr, 1);
              bout << s2;
              if ((cntr >= 0) && (cntr < 6)) {
                bout.Color(1);
                bout << side0[cntr + 6];
                wwiv_y1 = a()->localIO()->WhereY() + 1;
                wwiv_x1 = a()->localIO()->WhereX() + 1;
              }
              bout << "\x1b[K";
              s2[0] = 0;
            }
            sprintf(s2, "\x1b[%d;%dH", wwiv_y1, wwiv_x1);
            bout << s2;
            s2[0] = 0;
          }
          if (a()->localIO()->WhereX() == (a()->user()->GetScreenChars() - 1)) {
            done = true;
          }
        } else {
          if (a()->localIO()->WhereX() >= (a()->user()->GetScreenChars() - 1)) {
            done = true;
          }
        }
      } else {
        if ((a()->localIO()->WhereX() < (a()->user()->GetScreenChars() - 1)) &&
            (cp1 < max_length)) {
          if (a()->localIO()->WhereY() < 23) {
            side1[a()->localIO()->WhereY() - 13][cp1++] = ch;
            bout.bputch(ch);
          } else {
            side1[a()->localIO()->WhereY() - 13][cp1++] = ch;
            side1[a()->localIO()->WhereY() - 13][cp1] = 0;
            for (int cntr = 13; cntr < 25; cntr++) {
              sprintf(s2, "\x1b[%d;%dH", cntr, 1);
              bout << s2;
              if (cntr >= 13 && cntr < 18) {
                bout.Color(5);
                bout << side1[cntr - 7];
                wwiv_y2 = a()->localIO()->WhereY() + 1;
                wwiv_x2 = a()->localIO()->WhereX() + 1;
              }
              bout << "\x1b[K";
              s2[0] = '\0';
            }
            sprintf(s2, "\x1b[%d;%dH", wwiv_y2, wwiv_x2);
            bout << s2;
            s2[0] = '\0';
          }
          if (a()->localIO()->WhereX() == (a()->user()->GetScreenChars() - 1)) {
            done = true;
          }
        } else {
          if (a()->localIO()->WhereX() >= (a()->user()->GetScreenChars() - 1)) {
            done = true;
          }
        }
      }
    } else
      switch (ch) {
      case 7: {
        if (a()->chatting_ && a()->context().outcom()) {
          bout.rputch(7);
        }
      } break;
      case RETURN: /* C/R */
        if (side == 0) {
          side0[a()->localIO()->WhereY()][cp0] = 0;
        } else {
          side1[a()->localIO()->WhereY() - 13][cp1] = 0;
        }
        done = true;
        break;
      case BACKSPACE: { /* Backspace */
        if (side == 0) {
          if (cp0) {
            if (side0[a()->localIO()->WhereY()][cp0 - 2] == 3) {
              cp0 -= 2;
              bout.Color(0);
            } else if (side0[a()->localIO()->WhereY()][cp0 - 1] == 8) {
              cp0--;
              bout.bputch(SPACE);
            } else {
              cp0--;
              bout.bs();
            }
          }
        } else if (cp1) {
          if (side1[a()->localIO()->WhereY() - 13][cp1 - 2] == CC) {
            cp1 -= 2;
            bout.Color(0);
          } else if (side1[a()->localIO()->WhereY() - 13][cp1 - 1] == BACKSPACE) {
            cp1--;
            bout.bputch(SPACE);
          } else {
            cp1--;
            bout.bs();
          }
        }
      } break;
      case CX: /* Ctrl-X */
        while (a()->localIO()->WhereX() > begx) {
          bout.bs();
          if (side == 0) {
            cp0 = 0;
          } else {
            cp1 = 0;
          }
        }
        bout.Color(0);
        break;
      case CW: /* Ctrl-W */
        if (side == 0) {
          if (cp0) {
            do {
              if (side0[a()->localIO()->WhereY()][cp0 - 2] == CC) {
                cp0 -= 2;
                bout.Color(0);
              } else if (side0[a()->localIO()->WhereY()][cp0 - 1] == BACKSPACE) {
                cp0--;
                bout.bputch(SPACE);
              } else {
                cp0--;
                bout.bs();
              }
            } while ((cp0) && (side0[a()->localIO()->WhereY()][cp0 - 1] != SPACE) &&
                     (side0[a()->localIO()->WhereY()][cp0 - 1] != BACKSPACE) &&
                     (side0[a()->localIO()->WhereY()][cp0 - 2] != CC));
          }
        } else {
          if (cp1) {
            do {
              if (side1[a()->localIO()->WhereY() - 13][cp1 - 2] == CC) {
                cp1 -= 2;
                bout.Color(0);
              } else if (side1[a()->localIO()->WhereY() - 13][cp1 - 1] == BACKSPACE) {
                cp1--;
                bout.bputch(SPACE);
              } else {
                cp1--;
                bout.bs();
              }
            } while ((cp1) && (side1[a()->localIO()->WhereY() - 13][cp1 - 1] != SPACE) &&
                     (side1[a()->localIO()->WhereY() - 13][cp1 - 1] != BACKSPACE) &&
                     (side1[a()->localIO()->WhereY() - 13][cp1 - 2]));
          }
        }
        break;
      case CN: /* Ctrl-N */
        if (side == 0) {
          if ((a()->localIO()->WhereX()) && (cp0 < max_length)) {
            bout.bputch(BACKSPACE);
            side0[a()->localIO()->WhereY()][cp0++] = BACKSPACE;
          }
        } else if ((a()->localIO()->WhereX()) && (cp1 < max_length)) {
          bout.bputch(BACKSPACE);
          side1[a()->localIO()->WhereY() - 13][cp1++] = BACKSPACE;
        }
        break;
      case CP: /* Ctrl-P */
        if (side == 0) {
          if (cp0 < max_length - 1) {
            ch = bout.getkey();
            if ((ch >= SPACE) && (ch <= 126)) {
              side0[a()->localIO()->WhereY()][cp0++] = CC;
              side0[a()->localIO()->WhereY()][cp0++] = ch;
              bout.Color(ch - 48);
            }
          }
        } else {
          if (cp1 < max_length - 1) {
            ch = bout.getkey();
            if ((ch >= SPACE) && (ch <= 126)) {
              side1[a()->localIO()->WhereY() - 13][cp1++] = CC;
              side1[a()->localIO()->WhereY() - 13][cp1++] = ch;
              bout.Color(ch - 48);
            }
          }
        }
        break;
      case TAB: /* Tab */
        if (side == 0) {
          i = 5 - (cp0 % 5);
          if (((cp0 + i) < max_length) &&
              ((a()->localIO()->WhereX() + i) < a()->user()->GetScreenChars())) {
            i = 5 - ((a()->localIO()->WhereX() + 1) % 5);
            for (i1 = 0; i1 < i; i1++) {
              side0[a()->localIO()->WhereY()][cp0++] = SPACE;
              bout.bputch(SPACE);
            }
          }
        } else {
          i = 5 - (cp1 % 5);
          if (((cp1 + i) < max_length) &&
              ((a()->localIO()->WhereX() + i) < a()->user()->GetScreenChars())) {
            i = 5 - ((a()->localIO()->WhereX() + 1) % 5);
            for (i1 = 0; i1 < i; i1++) {
              side1[a()->localIO()->WhereY() - 13][cp1++] = SPACE;
              bout.bputch(SPACE);
            }
          }
        }
        break;
      }
  } while (!done && !a()->hangup_);

  if (ch != RETURN) {
    if (side == 0) {
      i = cp0 - 1;
      while ((i > 0) && (side0[a()->localIO()->WhereY()][i] != SPACE) &&
             ((side0[a()->localIO()->WhereY()][i] != BACKSPACE) ||
              (side0[a()->localIO()->WhereY()][i - 1] == CC))) {
        i--;
      }
      if ((i > static_cast<int>(a()->localIO()->WhereX() / 2)) && (i != (cp0 - 1))) {
        i1 = cp0 - i - 1;
        for (i = 0; i < i1; i++) {
          bout.bputch(BACKSPACE);
        }
        for (i = 0; i < i1; i++) {
          bout.bputch(SPACE);
        }
        rollover->clear();
        for (i = 0; i < i1; i++) {
          rollover->push_back(side0[a()->localIO()->WhereY()][cp0 - i1 + i]);
        }
        cp0 -= i1;
      }
      side0[a()->localIO()->WhereY()][cp0] = '\0';
    } else {
      i = cp1 - 1;
      while ((i > 0) && (side1[a()->localIO()->WhereY() - 13][i] != SPACE) &&
             ((side1[a()->localIO()->WhereY() - 13][i] != BACKSPACE) ||
              (side1[a()->localIO()->WhereY() - 13][i - 1] == CC))) {
        i--;
      }
      if ((i > static_cast<int>(a()->localIO()->WhereX() / 2)) && (i != (cp1 - 1))) {
        i1 = cp1 - i - 1;
        for (i = 0; i < i1; i++) {
          bout.bputch(BACKSPACE);
        }
        for (i = 0; i < i1; i++) {
          bout.bputch(SPACE);
        }
        rollover->clear();
        for (i = 0; i < i1; i++) {
          rollover->push_back(side1[a()->localIO()->WhereY() - 13][cp1 - i1 + i]);
        }
        cp1 -= i1;
      }
      side1[a()->localIO()->WhereY() - 13][cp1] = '\0';
    }
  }
  if (crend && a()->localIO()->WhereY() != 11 && a()->localIO()->WhereY() < 23) {
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

void chat1(const char* chat_line, bool two_way) {
  if (!okansi()) {
    two_way = false;
  }

  auto sysop_name = select_chat_name();
  if (!sysop_name.empty()) {
    return;
  }

  a()->clear_chatcall();
  if (two_way) {
    write_inst(INST_LOC_CHAT2, 0, INST_FLAGS_NONE);
    a()->chatting_ = 2;
  } else {
    write_inst(INST_LOC_CHAT, 0, INST_FLAGS_NONE);
    a()->chatting_ = 1;
  }
  auto tc_start = steady_clock::now();
  File chatFile(PathFilePath(a()->config()->gfilesdir(), DROPFILE_CHAIN_TXT));

  SavedLine line = bout.SaveCurrentLine();

  bout.nl(2);
  int nSaveTopData = a()->topdata;
  if (two_way) {
    a()->Cls();
    cp0 = cp1 = 0;
    if (a()->defscreenbottom == 24) {
      a()->topdata = LocalIO::topdataNone;
      a()->UpdateTopScreen();
    }
    bout.cls();
    wwiv_x2 = 1;
    wwiv_y2 = 13;
    bout.GotoXY(1, 1);
    wwiv_x1 = a()->localIO()->WhereX();
    wwiv_y1 = a()->localIO()->WhereY();
    bout.GotoXY(1, 12);
    bout.Color(7);
    for (auto screencount = 0; screencount < a()->user()->GetScreenChars(); screencount++) {
      bout.bputch(static_cast<unsigned char>(205), true);
    }
    bout.flush();
    const string unn = a()->names()->UserName(a()->usernum);
    auto s = fmt::format(" {} chatting with {} ", sysop_name, unn);
    auto x = (a()->user()->GetScreenChars() - stripcolors(s).size()) / 2;
    bout.GotoXY(std::max<int>(x, 0), 12);
    bout << "|#4" << s;
    bout.GotoXY(1, 1);
  }
  bout << "|#7" << sysop_name << "'s here...";
  bout.nl(2);
  std::string rollover = chat_line;

  if (two_way) {
    side0 = new char[MAXLINES_SIDE][MAXLEN];
    side1 = new char[MAXLINES_SIDE][MAXLEN];
    if (!side0 || !side1) {
      two_way = false;
    }
  }
  do {
    std::string s;
    if (two_way) {
      two_way_chat(&rollover, MAXLEN, true, sysop_name);
    } else {
      inli(&s, &rollover, MAXLEN, true, false);
    }
    if (s_chat_file && !two_way) {
      if (!chatFile.IsOpen()) {
        a()->localIO()->Puts("-] Chat file opened.\r\n");
        if (chatFile.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
          chatFile.Seek(0L, File::Whence::end);
          const auto t = times();
          const auto f = fulldate();
          chatFile.Write(fmt::format("\r\n\r\nChat file opened {} {}\r\n", f, t));
          chatFile.Write("----------------------------------\r\n\r\n");
        }
      }
      s.append("\r\n");
    } else if (chatFile.IsOpen()) {
      chatFile.Close();
      a()->localIO()->Puts("-] Chat file closed.\r\n");
    }
    if (a()->hangup_) {
      a()->chatting_ = 0;
    }
  } while (a()->chatting_);

  s_chat_file = false;
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
    bout.cls();
  }

  bout.nl();
  bout << "|#7Chat mode over...\r\n\n";
  a()->chatting_ = 0;
  auto tc_used = duration_cast<seconds>(steady_clock::now() - tc_start);
  a()->add_extratimecall(tc_used);
  a()->topdata = nSaveTopData;
  if (a()->IsUserOnline()) {
    a()->UpdateTopScreen();
  }
  bout.RestoreCurrentLine(line);
  bout.clreol();
}
