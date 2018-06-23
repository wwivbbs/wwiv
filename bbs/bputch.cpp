/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2017, WWIV Software Services             */
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
#include "core/log.h"
#include "core/strings.h"

using namespace wwiv::strings;

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
        args[argptr++] = to_number<int>(temp);
      } else {
        temp[tempptr++] = ansistr[ptr];
      }
      ++ptr;
    }
    if (tempptr && (argptr < 10)) {
      temp[tempptr] = 0;
      args[argptr++] = to_number<int>(temp);
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
      ansi_movement_occurred_ = true;
    } break;
    case 'A':
      local_io_->GotoXY(local_io_->WhereX(), local_io_->WhereY() - args[0]);
      ansi_movement_occurred_ = true;
      break;
    case 'B':
      local_io_->GotoXY(local_io_->WhereX(), local_io_->WhereY() + args[0]);
      ansi_movement_occurred_ = true;
      break;
    case 'C': {
      x_ += args[0];
      const int screen_width = a()->user()->GetScreenChars();
      x_ = std::min(x_, screen_width - 1); 
      auto wx = local_io_->WhereX();
      local_io_->GotoXY(wx + args[0], local_io_->WhereY());
    } break;
    case 'D': {
      x_ -= args[0];
      x_ = std::max<int>(0, x_);
      auto x = static_cast<int>(local_io_->WhereX()) - args[0];
      x = std::max<int>(0, x);
      local_io_->GotoXY(x, local_io_->WhereY());
    } break;
    case 's':
      oldx = local_io_->WhereX();
      oldy = local_io_->WhereY();
      break;
    case 'u':
      local_io_->GotoXY(oldx, oldy);
      x_ = oldx;
      ansi_movement_occurred_ = true;
      break;
    case 'J':
      if (args[0] == 2) {
        bout.clear_lines_listed();
        ansi_movement_occurred_ = true;
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
  int displayed = 0;

  if (c == SOFTRETURN && needs_color_reset_at_newline_) {
    Color(0);
    needs_color_reset_at_newline_ = false;
  }

  if (outcom && c != TAB) {
    if (!(!okansi() && (ansiptr || c == ESC))) {
      if (c == SOFTRETURN) {
#ifdef __unix__
        rputch('\r', use_buffer);
#endif  // __unix__
        rputch('\n', use_buffer);
      } else {
        rputch(c, use_buffer);
      }
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
    } else {
      displayed = 1;
      localIO()->Putch(c);
      current_line_.push_back({c, static_cast<uint8_t>(curatr)});
      const auto screen_width = a()->user()->GetScreenChars();
      if (c == BACKSPACE) {
        --x_;  
        if (x_ < 0) {
          x_ = screen_width - 1;
        }
      } else {
        ++x_;
      }
      // Wrap at screen_width
      if (x_ >= static_cast<int>(screen_width)) {
        x_ %= screen_width;
      }

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
    }
  }

  return displayed;
}


/* This function outputs a string to the com port.  This is mainly used
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

void Output::rputch(char ch, bool use_buffer_) {
  if (ok_modem_stuff && nullptr != a()->remoteIO()) {
    if (use_buffer_) {
      if (bputch_buffer_.size() > 1024) {
        flush();
      }
      bputch_buffer_.push_back(ch);
    } else if (!bputch_buffer_.empty()) {
      // If we have stuff in the buffer, and now are asked
      // to send an unbuffered character, we must send the
      // contents of the buffer 1st.
      bputch_buffer_.push_back(ch);
      flush();
    }
    else {
      a()->remoteIO()->put(ch);
    }
  }
}

