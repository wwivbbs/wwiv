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
#include "bbs/bputch.h"

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
#include "bbs/wsession.h"
#include "bbs/local_io.h"


void execute_ansi();

#define BPUTCH_LITERAL_PIPE_CODE -1
#define BPUTCH_NO_CODE 0
#define BPUTCH_HEART_CODE 1
#define BPUTCH_AT_MACRO_CODE 2
#define BPUTCH_PIPE_CODE 3
#define BPUTCH_CTRLO_CODE 4
#define BPUTCH_MACRO_CHAR_CODE 5

#define OUTCOMCH_BUFFER_SIZE 1024
static char s_szOutComChBuffer[ OUTCOMCH_BUFFER_SIZE + 1 ];
static int  s_nOutComChBufferPosition = 0;


/**
 * This function outputs one character to the screen, and if output to the
 * com port is enabled, the character is output there too.  ANSI graphics
 * are also trapped here, and the ansi function is called to execute the
 * ANSI codes
 */
int bputch(char c, bool bUseInternalBuffer) {
  int nc = 0, displayed = 0;
  static char pipe_color[3];

  if (change_color == BPUTCH_MACRO_CHAR_CODE) {
    change_color = BPUTCH_NO_CODE;
    return bout.bputs(static_cast<const char *>(interpret(c)));
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
    } else if (pipe_color[0] == '#' || pipe_color[0] == '#') {
      bout.Color(atoi(pipe_color + 1));
      return 0;
    } else {
      change_color = BPUTCH_LITERAL_PIPE_CODE;
    }
    if (nc >= SPACE) {
      change_color = BPUTCH_LITERAL_PIPE_CODE;
    }


    if (change_color == BPUTCH_LITERAL_PIPE_CODE) {
      bout << "|" ;
      return bout.bputs(pipe_color) + 1;
    } else {
      char szAnsiColorCode[20];
      if (nc < 16) {
        makeansi((curatr & 0xf0) | nc, szAnsiColorCode, false);
      } else {
        makeansi((curatr & 0x0f) | (nc << 4), szAnsiColorCode, false);
      }
      bout.bputs(szAnsiColorCode);
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
      bout.Color(static_cast<unsigned char>(c) - 48);
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
  } else if (c == SOFTRETURN && endofline[0]) {
    displayed = bout.bputs(endofline);
    endofline[0] = '\0';
  } else if (change_color == BPUTCH_LITERAL_PIPE_CODE) {
    change_color = BPUTCH_NO_CODE;
  }
  if (outcom && c != TAB) {
    if (!(!okansi() && (ansiptr || c == ESC))) {
      char x = okansi() ? '\xFE' : 'X';
      rputch(local_echo ? c : x, bUseInternalBuffer);
      displayed = 1;
    }
  }
  if (ansiptr) {
    if (ansiptr > 80) {
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
      int nScreenPos = session()->localIO()->WhereX();
      for (int i = nScreenPos; i < (((nScreenPos / 8) + 1) * 8); i++) {
        displayed += bputch(SPACE);
      }
    } else if (local_echo) {
      displayed = 1;
      session()->localIO()->LocalPutch(local_echo ? c : '\xFE');

      if (c == SOFTRETURN) {
        ++lines_listed;
        // Note: The call to end_tag has moved to listfiles() where it belongs.
        if (lines_listed >= (session()->screenlinest - 1)) {         // change Build3 + 5.0 to fix message read
          if (session()->user()->HasPause()) {
            pausescr();
          }
          lines_listed = 0;   // change Build3
        }
      }
    } else {
      session()->localIO()->LocalPutch('X');
      displayed = 1;
    }
  }

  return displayed;
}

/**
 * This function executes an ANSI string to change color, position the cursor, etc
 */
void execute_ansi() {
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
      session()->localIO()->LocalGotoXY(args[1] - 1, args[0] - 1);
      g_flags |= g_flag_ansi_movement;
      break;
    case 'A':
      session()->localIO()->LocalGotoXY(session()->localIO()->WhereX(), session()->localIO()->WhereY() - args[0]);
      g_flags |= g_flag_ansi_movement;
      break;
    case 'B':
      session()->localIO()->LocalGotoXY(session()->localIO()->WhereX(), session()->localIO()->WhereY() + args[0]);
      g_flags |= g_flag_ansi_movement;
      break;
    case 'C':
      session()->localIO()->LocalGotoXY(session()->localIO()->WhereX() + args[0], session()->localIO()->WhereY());
      break;
    case 'D':
      session()->localIO()->LocalGotoXY(session()->localIO()->WhereX() - args[0], session()->localIO()->WhereY());
      break;
    case 's':
      oldx = session()->localIO()->WhereX();
      oldy = session()->localIO()->WhereY();
      break;
    case 'u':
      session()->localIO()->LocalGotoXY(oldx, oldy);
      oldx = oldy = 0;
      g_flags |= g_flag_ansi_movement;
      break;
    case 'J':
      if (args[0] == 2) {
        lines_listed = 0;
        g_flags |= g_flag_ansi_movement;
        session()->localIO()->LocalCls();
      }
      break;
    case 'k':
    case 'K':
      session()->localIO()->LocalClrEol();
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

/* This function ouputs a string to the com port.  This is mainly used
 * for modem commands
 */
void rputs(const char *text) {
  // Rushfan fix for COM/IP weirdness
  if (ok_modem_stuff) {
    session()->remoteIO()->write(text, strlen(text));
  }
}

void FlushOutComChBuffer() {
  if (s_nOutComChBufferPosition > 0) {
    session()->remoteIO()->write(s_szOutComChBuffer, s_nOutComChBufferPosition);
    s_nOutComChBufferPosition = 0;
    memset(s_szOutComChBuffer, 0, OUTCOMCH_BUFFER_SIZE + 1);
  }
}

void rputch(char ch, bool bUseInternalBuffer) {
  if (ok_modem_stuff && nullptr != session()->remoteIO()) {
    if (bUseInternalBuffer) {
      if (s_nOutComChBufferPosition >= OUTCOMCH_BUFFER_SIZE) {
        FlushOutComChBuffer();
      }
      s_szOutComChBuffer[ s_nOutComChBufferPosition++ ] = ch;
    } else {
      session()->remoteIO()->put(ch);
    }
  }
}

