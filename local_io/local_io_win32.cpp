/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services            */
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
// Always declare wwiv_windows.h first to avoid collisions on defines.
#include "local_io/local_io_win32.h"
#include "core/wwiv_windows.h"

#include "core/cp437.h"
#include "core/os.h"
#include "core/stl.h"
#include "core/strings.h"
#include "local_io/keycodes.h"
#include <algorithm>
#include <chrono>
#include <conio.h>
#include <memory>
#include <string>

namespace wwiv::local::io {

// local functions
bool HasKeyBeenPressed(HANDLE in);
unsigned char GetKeyboardChar();

using std::chrono::milliseconds;
using namespace wwiv::core;
using namespace wwiv::strings;
using wwiv::os::sound;

struct screentype {
  short x1, y1, topline1, curatr1;
  CHAR_INFO* scrn1;
};

static screentype saved_screen;
/*
 * Sets screen attribute at screen pos x,y to attribute contained in a.
 */
void Win32ConsoleIO::set_attr_xy(int x, int y, int a) {
  COORD loc;
  DWORD cb = {0};

  loc.X = static_cast<SHORT>(x);
  loc.Y = static_cast<SHORT>(y);

  WriteConsoleOutputAttribute(out_, reinterpret_cast<LPWORD>(&a), 1, loc, &cb);
}

Win32ConsoleIO::Win32ConsoleIO() {
  out_ = GetStdHandle(STD_OUTPUT_HANDLE);
  in_ = GetStdHandle(STD_INPUT_HANDLE);
  if (out_ == INVALID_HANDLE_VALUE) {
    std::cout << "\n\nCan't get console handle!.\n\n";
    abort();
  }
  CONSOLE_SCREEN_BUFFER_INFO csbi{};
  GetConsoleScreenBufferInfo(out_, &csbi);
  original_size_.X = csbi.dwSize.X;
  original_size_.Y = csbi.dwSize.Y;
  auto rect = csbi.srWindow;
  COORD bufSize;
  bufSize.X = static_cast<SHORT>(rect.Right - rect.Left + 1);
  bufSize.Y = static_cast<SHORT>(rect.Bottom - rect.Top + 1);
  // Let's not constrain it to 80x25.
  SetConsoleWindowInfo(out_, TRUE, &rect);
  SetConsoleScreenBufferSize(out_, bufSize);

  cursor_pos_.X = csbi.dwCursorPosition.X;
  cursor_pos_.Y = csbi.dwCursorPosition.Y;

  // Have to reset this info, otherwise bad things happen.
  GetConsoleScreenBufferInfo(out_, &csbi);
  GetConsoleMode(in_, &saved_input_mode_);
  SetConsoleMode(in_, 0 | ENABLE_EXTENDED_FLAGS);
}

Win32ConsoleIO::~Win32ConsoleIO() {
  COORD x{original_size_.X, original_size_.Y};
  SetConsoleScreenBufferSize(out_, x);
  SetConsoleTextAttribute(out_, 0x07);
  SetConsoleMode(in_, saved_input_mode_);
}

// This, obviously, moves the cursor to the location specified, offset from
// the protected display at the top of the screen.  Note: this function
// is 0 based, so (0,0) is the upper left hand corner.
void Win32ConsoleIO::GotoXY(int x, int y) {
  CONSOLE_SCREEN_BUFFER_INFO csbi{};
  GetConsoleScreenBufferInfo(out_, &csbi);
  // maybe just use original_size_.X ?

  x = std::max<int>(x, 0);
  x = std::min<int>(x, csbi.dwSize.X - 1);
  y = std::max<int>(y, 0);
  y += GetTopLine();
  y = std::min<int>(y, GetScreenBottom());

  cursor_pos_.X = static_cast<int16_t>(x);
  cursor_pos_.Y = static_cast<int16_t>(y);
  COORD c{cursor_pos_.X, cursor_pos_.Y};
  SetConsoleCursorPosition(out_, c);
}

/* This function returns the current X cursor position, as the number of
 * characters from the left hand side of the screen.  An X position of zero
 * means the cursor is at the left-most position
 */
int Win32ConsoleIO::WhereX() const noexcept {
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  GetConsoleScreenBufferInfo(out_, &csbi);

  cursor_pos_.X = csbi.dwCursorPosition.X;
  cursor_pos_.Y = csbi.dwCursorPosition.Y;

  return cursor_pos_.X;
}

/* This function returns the Y cursor position, as the line number from
 * the top of the logical window.  The offset due to the protected top
 * of the screen display is taken into account.  A WhereY() of zero means
 * the cursor is at the top-most position it can be at.
 */
int Win32ConsoleIO::WhereY() const noexcept {
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  GetConsoleScreenBufferInfo(out_, &csbi);
  cursor_pos_.X = csbi.dwCursorPosition.X;
  cursor_pos_.Y = csbi.dwCursorPosition.Y;
  return cursor_pos_.Y - GetTopLine();
}

void Win32ConsoleIO::Lf() {
  /* This function performs a linefeed to the screen (but not remotely) by
   * either moving the cursor down one line, or scrolling the logical screen
   * up one line.
   */
  CONSOLE_SCREEN_BUFFER_INFO csbi{};
  GetConsoleScreenBufferInfo(out_, &csbi);

  SMALL_RECT scrollRect;
  COORD dest;
  CHAR_INFO fill;

  if (cursor_pos_.Y >= GetScreenBottom()) {
    dest.X = 0;
    dest.Y = static_cast<int16_t>(GetTopLine());
    scrollRect.Top = static_cast<int16_t>(GetTopLine() + 1);
    scrollRect.Bottom = static_cast<int16_t>(GetScreenBottom());
    scrollRect.Left = 0;
    scrollRect.Right = csbi.dwSize.X - 1;
    fill.Attributes = static_cast<int16_t>(curatr());
    fill.Char.UnicodeChar = cp437_to_utf8(' ');

    ScrollConsoleScreenBuffer(out_, &scrollRect, nullptr, dest, &fill);
  } else {
    cursor_pos_.Y++;
    COORD c{cursor_pos_.X, cursor_pos_.Y};
    SetConsoleCursorPosition(out_, c);
  }
}

/**
 * Returns the local cursor to the left-most position on the screen.
 */
void Win32ConsoleIO::Cr() {
  cursor_pos_.X = 0;
  const COORD c{cursor_pos_.X, cursor_pos_.Y};
  SetConsoleCursorPosition(out_, c);
}

/**
 * Clears the local logical screen
 */
void Win32ConsoleIO::Cls() {
  const COORD coordScreen = {0, 0};
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  GetConsoleScreenBufferInfo(out_, &csbi);
  const DWORD dwConSize = csbi.dwSize.X * csbi.dwSize.Y;
  DWORD charsWriten = 0;

  FillConsoleOutputCharacter(out_, ' ', dwConSize, coordScreen, &charsWriten);
  FillConsoleOutputAttribute(out_, 7, dwConSize, coordScreen, &charsWriten);
  GotoXY(0, 0);
}

void Win32ConsoleIO::Backspace() {
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  GetConsoleScreenBufferInfo(out_, &csbi);
  /* This function moves the cursor one position to the left, or if the cursor
   * is currently at its left-most position, the cursor is moved to the end of
   * the previous line, except if it is on the top line, in which case nothing
   * happens.
   */
  if (cursor_pos_.X >= 0) {
    cursor_pos_.X--;
  } else if (cursor_pos_.Y != GetTopLine()) {
    cursor_pos_.Y--;
    cursor_pos_.X = csbi.dwSize.X - 1;
  }
  COORD c{cursor_pos_.X, cursor_pos_.Y};
  SetConsoleCursorPosition(out_, c);
}

void Win32ConsoleIO::PutchRaw(unsigned char ch) {
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  GetConsoleScreenBufferInfo(out_, &csbi);
  /* This function outputs one character to the screen, then updates the
   * cursor position accordingly, scrolling the screen if necessary.  Not that
   * this function performs no commands such as a C/R or L/F.  If a value of
   * 8, 7, 13, 10, 12 (BACKSPACE, BEEP, C/R, L/F, TOF), or any other command-
   * type characters are passed, the appropriate corresponding "graphics"
   * symbol will be output to the screen as a normal character.
   */
  DWORD cb;

  SetConsoleTextAttribute(out_, static_cast<int16_t>(curatr()));
  auto wch = cp437_to_utf8(ch);
  WriteConsoleW(out_, &wch, 1, &cb, nullptr);

  if (cursor_pos_.X < csbi.dwSize.X) {
    cursor_pos_.X++;
    return;
  }

  // Need to scroll the screen up one.
  cursor_pos_.X = 0;
  if (cursor_pos_.Y == GetScreenBottom()) {
    COORD dest;
    SMALL_RECT MoveRect;
    CHAR_INFO fill;

    // rushfan scrolling fix (was no +1)
    MoveRect.Top = static_cast<int16_t>(GetTopLine() + 1);
    MoveRect.Bottom = static_cast<int16_t>(GetScreenBottom());
    MoveRect.Left = 0;
    MoveRect.Right = csbi.dwSize.X - 1;

    fill.Attributes = static_cast<int16_t>(curatr());
    fill.Char.UnicodeChar = cp437_to_utf8(' ');

    dest.X = 0;
    // rushfan scrolling fix (was -1)
    dest.Y = static_cast<int16_t>(GetTopLine());

    ScrollConsoleScreenBufferW(out_, &MoveRect, &MoveRect, dest, &fill);
  } else {
    cursor_pos_.Y++;
  }
}

/**
 * This function outputs one character to the local screen.  C/R, L/F, TOF,
 * BS, and BELL are interpreted as commands instead of characters.
 */
void Win32ConsoleIO::Putch(unsigned char ch) {
  if (ch > 31) {
    PutchRaw(ch);
  } else if (ch == CM) {
    Cr();
  } else if (ch == CJ) {
    // Let's try CR-LF for local
    Cr();
    Lf();
  } else if (ch == CL) {
    Cls();
  } else if (ch == BACKSPACE) {
    Backspace();
  } else if (ch == CG) {
    // No bell.
  }
}

// Outputs a string to the local screen.
void Win32ConsoleIO::Puts(const std::string& text) {
  for (auto ch : text) {
    Putch(ch);
  }
}

void Win32ConsoleIO::PutsXY(int x, int y, const std::string& text) {
  GotoXY(x, y);
  FastPuts(text);
}

void Win32ConsoleIO::PutsXYA(int x, int y, int a, const std::string& text) {
  GotoXY(x, y);

  const auto old_color = curatr();
  curatr(a);
  FastPuts(text);

  curatr(old_color);
}

void Win32ConsoleIO::FastPuts(const std::string& text) {
  // This RAPIDLY outputs ONE LINE to the screen only and is not exactly stable.
  DWORD cb = 0;

  SetConsoleTextAttribute(out_, static_cast<int16_t>(curatr()));
  const auto s = cp437_to_utf8w(text);
  WriteConsoleW(out_, s.c_str(), wwiv::stl::size_int(s), &cb, nullptr);
  cursor_pos_.X = cursor_pos_.X + static_cast<int16_t>(cb);
}

void Win32ConsoleIO::set_protect(int l) {
  // set_protect sets the number of lines protected at the top of the screen.
  if (l != GetTopLine()) {
    COORD coord;
    coord.X = 0;
    coord.Y = static_cast<int16_t>(l);

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(out_, &csbi);

    if (l > GetTopLine()) {
      if ((WhereY() + GetTopLine() - l) < 0) {
        CHAR_INFO lpFill;
        SMALL_RECT scrnl;

        scrnl.Top = static_cast<int16_t>(GetTopLine());
        scrnl.Left = 0;
        scrnl.Bottom = static_cast<int16_t>(GetScreenBottom());
        scrnl.Right = csbi.dwSize.X;

        lpFill.Char.UnicodeChar = cp437_to_utf8(' ');
        lpFill.Attributes = 0;

        coord.X = 0;
        coord.Y = static_cast<int16_t>(l);
        ScrollConsoleScreenBufferW(out_, &scrnl, nullptr, coord, &lpFill);
        GotoXY(WhereX(), WhereY() + l - GetTopLine());
      }
    } else {
      DWORD written;
      FillConsoleOutputAttribute(out_, 0, (GetTopLine() - l) * csbi.dwSize.X, coord, &written);
    }
  }
  SetTopLine(l);
}

void Win32ConsoleIO::savescreen() {
  COORD topleft;
  CONSOLE_SCREEN_BUFFER_INFO bufinfo;
  SMALL_RECT region;

  GetConsoleScreenBufferInfo(out_, &bufinfo);
  topleft.Y = topleft.X = region.Top = region.Left = 0;
  region.Bottom = static_cast<int16_t>(bufinfo.dwSize.Y - 1);
  region.Right = static_cast<int16_t>(bufinfo.dwSize.X - 1);

  if (!saved_screen.scrn1) {
    saved_screen.scrn1 =
        static_cast<CHAR_INFO*>(malloc((bufinfo.dwSize.X * bufinfo.dwSize.Y) * sizeof(CHAR_INFO)));
  }

  ReadConsoleOutput(out_, (CHAR_INFO*)saved_screen.scrn1, bufinfo.dwSize, topleft, &region);
  saved_screen.x1 = static_cast<int16_t>(WhereX());
  saved_screen.y1 = static_cast<int16_t>(WhereY());
  saved_screen.topline1 = static_cast<int16_t>(GetTopLine());
  saved_screen.curatr1 = static_cast<int16_t>(curatr());
}

/*
 * restorescreen restores a screen previously saved with savescreen
 */
void Win32ConsoleIO::restorescreen() {
  if (saved_screen.scrn1) {
    // COORD size;
    COORD topleft;
    CONSOLE_SCREEN_BUFFER_INFO bufinfo;
    SMALL_RECT region;

    GetConsoleScreenBufferInfo(out_, &bufinfo);
    topleft.Y = topleft.X = region.Top = region.Left = 0;
    region.Bottom = static_cast<int16_t>(bufinfo.dwSize.Y - 1);
    region.Right = static_cast<int16_t>(bufinfo.dwSize.X - 1);

    WriteConsoleOutputW(out_, saved_screen.scrn1, bufinfo.dwSize, topleft, &region);
    free(saved_screen.scrn1);
    saved_screen.scrn1 = nullptr;
  }
  SetTopLine(saved_screen.topline1);
  curatr(saved_screen.curatr1);
  GotoXY(saved_screen.x1, saved_screen.y1);
}

/**
 * IsLocalKeyPressed - returns whether or not a key been pressed at the local console.
 *
 * @return true if a key has been pressed at the local console, false otherwise
 */
bool Win32ConsoleIO::KeyPressed() {
  if (extended_key_waiting_) {
    return true;
  }

  return HasKeyBeenPressed(in_);
}

/*
 * returns the ASCII code of the next character waiting in the
 * keyboard buffer.  If there are no characters waiting in the
 * keyboard buffer, then it waits for one.
 *
 * A value of 0 is returned for all extended keys (such as F1,
 * Alt-X, etc.).  The function must be called again upon receiving
 * a value of 0 to obtain the value of the extended key pressed.
 */
unsigned char Win32ConsoleIO::GetChar() {
  if (extended_key_waiting_) {
    extended_key_waiting_ = false;
    return GetKeyboardChar();
  }
  const auto rc = GetKeyboardChar();
  if (rc == 0 || rc == 0xe0) {
    extended_key_waiting_ = true;
  }
  return rc;
}

void Win32ConsoleIO::MakeLocalWindow(int x, int y, int xlen, int ylen) {
  // Make sure that we are within the range of {(0,0), (80,GetScreenBottom())}
  curatr(GetUserEditorColor());
  xlen = std::min(xlen, 80);
  if (ylen > (GetScreenBottom() + 1 - GetTopLine())) {
    ylen = (GetScreenBottom() + 1 - GetTopLine());
  }
  if ((x + xlen) > 80) {
    x = 80 - xlen;
  }
  if ((y + ylen) > GetScreenBottom() + 1) {
    y = GetScreenBottom() + 1 - ylen;
  }

  const auto xx = WhereX();
  const auto yy = WhereY();

  // we expect to be offset by GetTopLine()
  y += GetTopLine();

  // large enough to hold 80x50
  CHAR_INFO ci[4000];

  // pos is the position within the buffer
  COORD pos = {0, 0};
  COORD size;
  SMALL_RECT rect;

  // rect is the area on screen the buffer is to be drawn
  rect.Top = static_cast<int16_t>(y);
  rect.Left = static_cast<int16_t>(x);
  rect.Right = static_cast<int16_t>(xlen + x - 1);
  rect.Bottom = static_cast<int16_t>(ylen + y - 1);

  // size of the buffer to use (lower right hand coordinate)
  size.X = static_cast<int16_t>(xlen);
  size.Y = static_cast<int16_t>(ylen);

  // our current position within the CHAR_INFO buffer
  auto nCiPtr = 0;

  // Loop through Y, each time looping through X adding the right character
  for (auto yloop = 0; yloop < size.Y; yloop++) {
    for (auto xloop = 0; xloop < size.X; xloop++) {
      ci[nCiPtr].Attributes = static_cast<int16_t>(GetUserEditorColor());
      if ((yloop == 0) || (yloop == size.Y - 1)) {
        ci[nCiPtr].Char.UnicodeChar = cp437_to_utf8('\xC4'); // top and bottom
      } else {
        if ((xloop == 0) || (xloop == size.X - 1)) {
          ci[nCiPtr].Char.UnicodeChar = cp437_to_utf8('\xB3'); // right and left sides
        } else {
          ci[nCiPtr].Char.UnicodeChar = cp437_to_utf8('\x20'); // nothing... Just filler (space)
        }
      }
      nCiPtr++;
    }
  }

  // sum of the lengths of the previous lines (+0) is the start of next line

  ci[0].Char.UnicodeChar = cp437_to_utf8('\xDA');        // upper left
  ci[xlen - 1].Char.UnicodeChar = cp437_to_utf8('\xBF'); // upper right

  ci[xlen * (ylen - 1)].Char.UnicodeChar = cp437_to_utf8('\xC0');            // lower left
  ci[xlen * (ylen - 1) + xlen - 1].Char.UnicodeChar = cp437_to_utf8('\xD9'); // lower right

  // Send it all to the screen with 1 WIN32 API call (Windows 95's console mode API support
  // is MUCH slower than NT/Win2k, therefore it is MUCH faster to render the buffer off
  // screen and then write it with one fell swoop.

  WriteConsoleOutputW(out_, ci, size, pos, &rect);

  // Draw shadow around boxed window
  for (auto i = 0; i < xlen; i++) {
    set_attr_xy(x + 1 + i, y + ylen, 0x08);
  }

  for (auto i1 = 0; i1 < ylen; i1++) {
    set_attr_xy(x + xlen, y + 1 + i1, 0x08);
  }

  GotoXY(xx, yy);
}

void Win32ConsoleIO::SetCursor(int cursorStyle) {
  CONSOLE_CURSOR_INFO cursInfo;

  switch (cursorStyle) {
  case cursorNone: {
    cursInfo.dwSize = 20;
    cursInfo.bVisible = false;
    SetConsoleCursorInfo(out_, &cursInfo);
  } break;
  case cursorSolid: {
    cursInfo.dwSize = 100;
    cursInfo.bVisible = true;
    SetConsoleCursorInfo(out_, &cursInfo);
  } break;
  case cursorNormal:
  default: {
    cursInfo.dwSize = 20;
    cursInfo.bVisible = true;
    SetConsoleCursorInfo(out_, &cursInfo);
  }
  }
}

void Win32ConsoleIO::ClrEol() {
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  DWORD cb;

  GetConsoleScreenBufferInfo(out_, &csbi);
  const auto len = csbi.dwSize.X - WhereX();

  FillConsoleOutputCharacter(out_, ' ', len, csbi.dwCursorPosition, &cb);
  FillConsoleOutputAttribute(out_, static_cast<WORD>(curatr()), len, csbi.dwCursorPosition, &cb);
}

// NOTE: This only works for 80x25 WFC
void Win32ConsoleIO::WriteScreenBuffer(const char* buffer) {
  CHAR_INFO ci[2000] {};
  const auto* p = buffer;

  for (auto& i : ci) {
    i.Char.UnicodeChar = cp437_to_utf8(*p++);
    i.Attributes = static_cast<WORD>(*p++);
  }

  const COORD pos = {0, 0};
  const COORD size = {80, 25};
  SMALL_RECT rect = {0, 0, 79, 24};

  WriteConsoleOutputW(out_, ci, size, pos, &rect);
}

int Win32ConsoleIO::GetDefaultScreenBottom() const noexcept {
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  GetConsoleScreenBufferInfo(out_, &csbi);
  return csbi.srWindow.Bottom - csbi.srWindow.Top;
}

bool HasKeyBeenPressed(HANDLE in) {
  DWORD num_events; // NumPending
  GetNumberOfConsoleInputEvents(in, &num_events);
  if (num_events == 0) {
    return false;
  }

  const auto input = std::make_unique<INPUT_RECORD[]>(num_events);
  ZeroMemory(input.get(), sizeof(INPUT_RECORD) * num_events);

  DWORD actually_read;
  if (PeekConsoleInput(in, input.get(), num_events, &actually_read)) {
    for (DWORD i = 0; i < actually_read; i++) {
      if (input[i].EventType != KEY_EVENT) {
        continue;
      }
      if (!input[i].Event.KeyEvent.bKeyDown) {
        continue;
      }
      if (input[i].Event.KeyEvent.wVirtualKeyCode == VK_SHIFT ||
          input[i].Event.KeyEvent.wVirtualKeyCode == VK_CONTROL ||
          input[i].Event.KeyEvent.wVirtualKeyCode == VK_MENU) {
        continue;
      }
      if (input[i].Event.KeyEvent.dwControlKeyState & LEFT_ALT_PRESSED ||
          input[i].Event.KeyEvent.dwControlKeyState & RIGHT_ALT_PRESSED) {
        // VLOG(3) << "ALT KEY uchar=" << input[i].Event.KeyEvent.uChar.AsciiChar << "} ";
      }
      // VLOG(3) << "{KeyCode=" << input[i].Event.KeyEvent.wVirtualKeyCode << "; ScanCode="
      //          << input[i].Event.KeyEvent.wVirtualScanCode << ", char="
      //          << input[i].Event.KeyEvent.uChar.AsciiChar << "} ";
      return true;
    }
  }
  return false;
}

// Use _getwch to work around https://github.com/wwivbbs/wwiv/issues/1113
unsigned char GetKeyboardChar() { return static_cast<unsigned char>(_getwch()); }

static int GetEditLineStringLength(const char* text) {
  auto i = ssize(text);
  while (i >= 0 && (/*text[i-1] == 32 ||*/ static_cast<unsigned char>(text[i - 1]) == 176)) {
    --i;
  }
  return i;
}

void Win32ConsoleIO::EditLine(char* in, int len, AllowedKeys allowed_keys,
                              EditlineResult* returncode, const char* allowed) {

  const auto oldatr = curatr();
  const auto cx = WhereX();
  const auto cy = WhereY();
  for (auto i = wwiv::strings::ssize(in); i < len; i++) {
    in[i] = '\xb0'; // 176
  }
  in[len] = '\0';
  curatr(GetEditLineColor());
  FastPuts(in);
  GotoXY(cx, cy);
  auto done = false;
  auto pos = 0;
  auto insert = false;
  do {
    if (auto ch = GetChar(); ch == 0 || ch == 224) {
      ch = GetChar();
      switch (ch) {
      case F1:
        done = true;
        *returncode = EditlineResult::DONE;
        break;
      case HOME:
        pos = 0;
        GotoXY(cx, cy);
        break;
      case END:
        pos = GetEditLineStringLength(in);
        GotoXY(cx + pos, cy);
        break;
      case RARROW:
        if (pos < GetEditLineStringLength(in)) {
          pos++;
          GotoXY(cx + pos, cy);
        }
        break;
      case LARROW:
        if (pos > 0) {
          pos--;
          GotoXY(cx + pos, cy);
        }
        break;
      case UARROW:
      case CO:
        done = true;
        *returncode = EditlineResult::PREV;
        break;
      case DARROW:
        done = true;
        *returncode = EditlineResult::NEXT;
        break;
      case KEY_INSERT:
        if (allowed_keys != AllowedKeys::SET) {
          insert = !insert;
        }
        break;
      case KEY_DELETE:
        if (allowed_keys != AllowedKeys::SET) {
          for (int i = pos; i < len; i++) {
            in[i] = in[i + 1];
          }
          in[len - 1] = '\xb0'; // 176
          PutsXY(cx, cy, in);
          GotoXY(cx + pos, cy);
        }
        break;
      }
    } else {
      if (ch > 31) {
        if (allowed_keys == AllowedKeys::UPPER_ONLY) {
          ch = to_upper_case<unsigned char>(ch);
        }
        if (allowed_keys == AllowedKeys::SET) {
          ch = to_upper_case<unsigned char>(ch);
          if (ch != SPACE) {
            auto looking_for_space = true;
            for (int i = 0; i < len; i++) {
              if (ch == allowed[i] && looking_for_space) {
                looking_for_space = false;
                pos = i;
                GotoXY(cx + pos, cy);
                if (in[pos] == SPACE) {
                  ch = allowed[pos];
                } else {
                  ch = SPACE;
                }
              }
            }
            if (looking_for_space) {
              ch = allowed[pos];
            }
          }
        }
        if (pos < len &&
            (allowed_keys == AllowedKeys::ALL || allowed_keys == AllowedKeys::UPPER_ONLY ||
             allowed_keys == AllowedKeys::SET ||
             allowed_keys == AllowedKeys::NUM_ONLY &&
             (ch >= '0' && ch <= '9' || ch == SPACE))) {
          if (insert) {
            for (auto i = len - 1; i > pos; i--) {
              in[i] = in[i - 1];
            }
            in[pos++] = ch;
            PutsXY(cx, cy, in);
            GotoXY(cx + pos, cy);
          } else {
            in[pos++] = ch;
            Putch(ch);
          }
        }
      } else {
        // ch is > 32
        switch (ch) {
        case RETURN:
        case TAB:
          done = true;
          *returncode = EditlineResult::NEXT;
          break;
        case ESC:
          done = true;
          *returncode = EditlineResult::ABORTED;
          break;
        case CA:
          pos = 0;
          GotoXY(cx, cy);
          break;
        case CE:
          pos = GetEditLineStringLength(in); // len;
          GotoXY(cx + pos, cy);
          break;
        case BACKSPACE:
          if (pos > 0) {
            if (insert) {
              for (int i = pos - 1; i < len; i++) {
                in[i] = in[i + 1];
              }
              in[len - 1] = '\xb0'; // 176
              pos--;
              PutsXY(cx, cy, in);
              GotoXY(cx + pos, cy);
            } else {
              const auto ell = GetEditLineStringLength(in);
              pos--;
              if (pos == (ell - 1)) {
                in[pos] = '\xb0'; // 176
              } else {
                in[pos] = SPACE;
              }
              PutsXY(cx, cy, in);
              GotoXY(cx + pos, cy);
            }
          }
          break;
        }
      }
    }
  } while (!done);

  auto z = ssize(in);
  while (z >= 0 && static_cast<unsigned char>(in[z - 1]) == 176) {
    --z;
  }
  in[z] = '\0';

  char szFinishedString[260];
  snprintf(szFinishedString, sizeof(szFinishedString), "%-255s", in);
  szFinishedString[len] = '\0';
  GotoXY(cx, cy);
  curatr(oldatr);
  FastPuts(szFinishedString);
  GotoXY(cx, cy);
}

void Win32ConsoleIO::UpdateNativeTitleBar(const std::string& system_name, int instance_number) {
  // Set console title
  const auto s = StrCat("WWIV Node ", instance_number, " (", system_name, ")");
  SetConsoleTitle(s.c_str());
}

}
