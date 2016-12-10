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
#include "bbs/output.h"

#include "bbs/bgetch.h"
#include "bbs/bbs.h"
#include "bbs/bbsutl1.h"
#include "bbs/com.h"
#include "bbs/interpret.h"
#include "bbs/keycodes.h"
#include "bbs/pause.h"
#include "bbs/utility.h"
#include "bbs/vars.h"
#include "bbs/remote_io.h"
#include "bbs/wconstants.h"
#include "bbs/application.h"
#include "bbs/local_io.h"

#define BPUTCH_LITERAL_PIPE_CODE -1
#define BPUTCH_NO_CODE 0
#define BPUTCH_HEART_CODE 1
#define BPUTCH_AT_MACRO_CODE 2
#define BPUTCH_PIPE_CODE 3
#define BPUTCH_CTRLO_CODE 4
#define BPUTCH_MACRO_CHAR_CODE 5

/**
 * This function executes an ANSI string to change color, position the cursor, etc
 */
void Output::execute_ansi() {
  static int oldx = 0;
  static int oldy = 0;

  if (ansistr[1] != '[') {
    // try to fix things after really nasty ANSI comes in - RF 20021105
    ansiptr = 0;
    // do nothing if invalid ANSI string.
  } else {
    int args[11];
    char temp[11];
    static const char clrlst[] = "04261537";

    int argptr = 0;
    int tempptr = 0;
    int ptr = 2;
    int count;
    for (count = 0; count < 10; count++) {
      args[count] = temp[count] = 0;
    }
    char cmd = ansistr[ansiptr - 1];
    ansistr[ansiptr - 1] = 0;
    while ((ansistr[ptr]) && (argptr < 10) && (tempptr < 10)) {
      if (ansistr[ptr] == ';') {
        temp[tempptr] = 0;
        tempptr = 0;
        args[argptr++] = atoi(temp);
      } else {
        temp[tempptr++] = ansistr[ptr];
      }
      ++ptr;
    }
    if (tempptr && (argptr < 10)) {
      temp[tempptr] = 0;
      args[argptr++] = atoi(temp);
    }
    if ((cmd >= 'A') && (cmd <= 'D') && !args[0]) {
      args[0] = 1;
    }
    switch (cmd) {
    case 'f':
    case 'H':
    {
      int y = args[0] - 1;
      int x = args[1] - 1;
      x_ = x;
      local_io_->GotoXY(x, y);
      g_flags |= g_flag_ansi_movement;
    } break;
    case 'A':
      local_io_->GotoXY(local_io_->WhereX(), local_io_->WhereY() - args[0]);
      g_flags |= g_flag_ansi_movement;
      break;
    case 'B':
      local_io_->GotoXY(local_io_->WhereX(), local_io_->WhereY() + args[0]);
      g_flags |= g_flag_ansi_movement;
      break;
    case 'C':
      x_ += args[0];
      local_io_->GotoXY(local_io_->WhereX() + args[0], local_io_->WhereY());
      break;
    case 'D':
      x_ -= args[0];
      local_io_->GotoXY(local_io_->WhereX() - args[0], local_io_->WhereY());
      break;
    case 's':
      oldx = local_io_->WhereX();
      oldy = local_io_->WhereY();
      break;
    case 'u':
      local_io_->GotoXY(oldx, oldy);
      x_ = oldx;
      g_flags |= g_flag_ansi_movement;
      break;
    case 'J':
      if (args[0] == 2) {
        bout.clear_lines_listed();
        g_flags |= g_flag_ansi_movement;
        local_io_->Cls();
        x_ = 0;
      }
      break;
    case 'k':
    case 'K':
      local_io_->ClrEol();
      break;
    case 'm':
      if (!argptr) {
        argptr = 1;
        args[0] = 0;
      }
      for (count = 0; count < argptr; count++) {
        switch (args[count]) {
        case 0:
          curatr = 0x07;
          break;
        case 1:
          curatr = curatr | 0x08;
          break;
        case 4:
          break;
        case 5:
          curatr = curatr | 0x80;
          break;
        case 7:
          ptr = curatr & 0x77;
          curatr = (curatr & 0x88) | (ptr << 4) | (ptr >> 4);
          break;
        case 8:
          curatr = 0;
          break;
        default:
          if ((args[count] >= 30) && (args[count] <= 37)) {
            curatr = (curatr & 0xf8) | (clrlst[args[count] - 30] - '0');
          } else if ((args[count] >= 40) && (args[count] <= 47)) {
            curatr = (curatr & 0x8f) | ((clrlst[args[count] - 40] - '0') << 4);
          }
          break;  // moved up a line...
        }
      }
    }
    ansiptr = 0;
  }
}

/**
 * This function outputs one character to the screen, and if output to the
 * com port is enabled, the character is output there too.  ANSI graphics
 * are also trapped here, and the ansi function is called to execute the
 * ANSI codes
 */
int Output::bputch(char c, bool use_buffer) {
  int nc = 0, displayed = 0;
  static char pipe_color[3];

  if (change_color == BPUTCH_MACRO_CHAR_CODE) {
    change_color = BPUTCH_NO_CODE;
    return bputs(interpret(c));
  } else if (change_color == BPUTCH_CTRLO_CODE) {
    if (c == CO) {
      change_color = BPUTCH_MACRO_CHAR_CODE;
    } else {
      change_color = BPUTCH_NO_CODE;
    }
    return 0;
  } else if (change_color == BPUTCH_PIPE_CODE) {
    change_color = BPUTCH_NO_CODE;
    pipe_color[1] = c;
    pipe_color[2] = '\0';

    if (isdigit(static_cast<unsigned char>(pipe_color[0]))) {
      if (isdigit(pipe_color[1]) || (pipe_color[1] == ' ')) {
        nc = atoi(pipe_color);
      } else {
        change_color = BPUTCH_LITERAL_PIPE_CODE;
      }
    } else if (pipe_color[0] == ' ' && isdigit(pipe_color[1])) {
      nc = atoi(pipe_color + 1);
    } else if (pipe_color[0] == 'b' || pipe_color[0] == 'B') {
      nc = 16 + atoi(pipe_color + 1);
    } else if (pipe_color[0] == '#') {
      Color(atoi(pipe_color + 1));
      return 0;
    } else {
      change_color = BPUTCH_LITERAL_PIPE_CODE;
    }
    if (nc >= SPACE) {
      change_color = BPUTCH_LITERAL_PIPE_CODE;
    }


    if (change_color == BPUTCH_LITERAL_PIPE_CODE) {
      bout << "|" ;
      return bputs(pipe_color) + 1;
    } else {
      char szAnsiColorCode[20];
      if (nc < 16) {
        makeansi((curatr & 0xf0) | nc, szAnsiColorCode, false);
      } else {
        makeansi((curatr & 0x0f) | (nc << 4), szAnsiColorCode, false);
      }
      bputs(szAnsiColorCode);
    }
    return 0; // color was printed, no chars displayed
  } else if (change_color == BPUTCH_AT_MACRO_CODE) {
    // RF20020908 - Mod to allow |@<macro char> macros in WWIV
    if (c == '@') {
      change_color = BPUTCH_MACRO_CHAR_CODE;
      return 0;
    }
    pipe_color[0] = c;
    change_color = BPUTCH_PIPE_CODE;
    return 0;
  } else if (change_color == BPUTCH_HEART_CODE) {
    change_color = BPUTCH_NO_CODE;
    if ((c >= SPACE) && (static_cast<unsigned char>(c) <= 126)) {
      Color(static_cast<unsigned char>(c) - 48);
    }
    return 0;
  }

  if (c == CC) {
    change_color = BPUTCH_HEART_CODE;
    return 0;
  } else if (c == CO) {
    change_color = BPUTCH_CTRLO_CODE;
    return 0;
  } else if (c == '|' && change_color != BPUTCH_LITERAL_PIPE_CODE) {
    change_color = BPUTCH_AT_MACRO_CODE;
    return 0;
  } else if (c == SOFTRETURN && !endofline_.empty()) {
    displayed = bputs(endofline_);
    endofline_.clear();
  } else if (change_color == BPUTCH_LITERAL_PIPE_CODE) {
    change_color = BPUTCH_NO_CODE;
  }
  if (outcom && c != TAB) {
    if (!(!okansi() && (ansiptr || c == ESC))) {
      char x = okansi() ? '\xFE' : 'X';
      rputch(local_echo ? c : x, use_buffer);
      displayed = 1;
    }
  }
  if (ansiptr) {
    if (ansiptr >= 80) {
      // Something really bad happened here, we need to stop the memory from
      // getting trashed.
      ansiptr = 0;
      memset(ansistr, 0, sizeof(ansistr));
    }
    ansistr[ansiptr++] = c;
    ansistr[ansiptr] = 0;
    if ((((c < '0') || (c > '9')) && (c != '[') && (c != ';')) ||
        (ansistr[1] != '[') || (ansiptr > 75)) {
      // The below two lines kill thedraw's ESC[?7h seq
      if (!((ansiptr == 2 && c == '?') || (ansiptr == 3 && ansistr[2] == '?'))) {
        execute_ansi();
      }
    }
  } else if (c == ESC) {
    ansistr[0] = ESC;
    ansiptr = 1;
    ansistr[ansiptr] = '\0';
  } else {
    if (c == TAB) {
      int nScreenPos = wherex();
      for (int i = nScreenPos; i < (((nScreenPos / 8) + 1) * 8); i++) {
        displayed += bputch(SPACE);
      }
    } else if (local_echo) {
      displayed = 1;
      auto display_char = local_echo ? c : '\xFE';
      localIO()->Putch(display_char);

      current_line_.push_back({display_char, curatr});
      x_++;

      if (c == SOFTRETURN) {
        current_line_.clear();
        x_ = 0;
        bout.lines_listed_++;
        // change Build3 + 5.0 to fix message read.
        if (bout.lines_listed() >= (a()->screenlinest - 1)) {
          if (a()->user()->HasPause()) {
            pausescr();
          }
          bout.clear_lines_listed();   // change Build3
        }
      }
    } else {
      localIO()->Putch('X');
      displayed = 1;
    }
  }

  return displayed;
}


/* This function ouputs a string to the com port.  This is mainly used
 * for modem commands
 */
void Output::rputs(const char *text) {
  // Rushfan fix for COM/IP weirdness
  if (ok_modem_stuff) {
    a()->remoteIO()->write(text, strlen(text));
  }
}

void Output::flush() {
  if (bputch_buffer_.empty()) {
    return;
  }

  a()->remoteIO()->write(bputch_buffer_.c_str(), bputch_buffer_.size());
  bputch_buffer_.clear();
}

void Output::rputch(char ch, bool bUseInternalBuffer) {
  if (ok_modem_stuff && nullptr != a()->remoteIO()) {
    if (bUseInternalBuffer) {
      if (bputch_buffer_.size() > 1024) {
        flush();
      }
      bputch_buffer_.push_back(ch);
    } else {
      a()->remoteIO()->put(ch);
    }
  }
}

