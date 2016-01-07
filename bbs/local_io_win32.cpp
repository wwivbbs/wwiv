/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2016,WWIV Software Services             */
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
#include "bbs/local_io_win32.h"

#include <algorithm>
#include <chrono>
#include <memory>
#include <conio.h>
#include <string>
#include <vector>

#include "bbs/datetime.h"
#include "bbs/wstatus.h"
#include "bbs/wconstants.h"
#include "bbs/fcns.h"
#include "bbs/vars.h"
#include "bbs/wsession.h"
#include "core/os.h"
#include "core/strings.h"

// local functions
bool HasKeyBeenPressed(HANDLE in);
unsigned char GetKeyboardChar();

using std::chrono::milliseconds;
using std::string;
using std::unique_ptr;
using std::vector;
using namespace wwiv::strings;
using wwiv::os::sound;

#define PREV                1
#define NEXT                2
#define DONE                4
#define ABORTED             8

struct screentype {
  short x1, y1, topline1, curatr1;
  CHAR_INFO* scrn1;
};

static screentype m_ScreenSave;
/*
 * Sets screen attribute at screen pos x,y to attribute contained in a.
 */
void Win32ConsoleIO::set_attr_xy(int x, int y, int a) {
  COORD loc = {0};
  DWORD cb = {0};

  loc.X = static_cast<SHORT>(x);
  loc.Y = static_cast<SHORT>(y);

  WriteConsoleOutputAttribute(out_, reinterpret_cast< LPWORD >(&a), 1, loc, &cb);
}

Win32ConsoleIO::Win32ConsoleIO() : LocalIO() {
  SetTopLine(0);
  SetScreenBottom(0);
  ExtendedKeyWaiting = 0;

  out_ = GetStdHandle(STD_OUTPUT_HANDLE);
  in_  = GetStdHandle(STD_INPUT_HANDLE);
  if (out_ == INVALID_HANDLE_VALUE) {
    std::cout << "\n\nCan't get console handle!.\n\n";
    abort();
  }
  GetConsoleScreenBufferInfo(out_, &buffer_info_);
  original_size_ = buffer_info_.dwSize;
  SMALL_RECT rect = buffer_info_.srWindow;
  COORD bufSize;
  bufSize.X = static_cast<SHORT>(rect.Right - rect.Left + 1);
  bufSize.Y = static_cast<SHORT>(rect.Bottom - rect.Top + 1);
  bufSize.X = static_cast<SHORT>(std::min<SHORT>(bufSize.X, 80));
  bufSize.Y = static_cast<SHORT>(std::min<SHORT>(bufSize.Y, 25));
  SetConsoleWindowInfo(out_, TRUE, &rect);
  SetConsoleScreenBufferSize(out_, bufSize);

  cursor_pos_.X = buffer_info_.dwCursorPosition.X;
  cursor_pos_.Y = buffer_info_.dwCursorPosition.Y;

  // Have to reset this info, otherwise bad things happen.
  GetConsoleScreenBufferInfo(out_, &buffer_info_);
  GetConsoleMode(in_, &saved_input_mode_);
  SetConsoleMode(in_, 0);
}

Win32ConsoleIO::~Win32ConsoleIO() {
  SetConsoleScreenBufferSize(out_, original_size_);
  SetConsoleTextAttribute(out_, 0x07);
  SetConsoleMode(in_, saved_input_mode_);
}


// This, obviously, moves the cursor to the location specified, offset from
// the protected dispaly at the top of the screen.  Note: this function
// is 0 based, so (0,0) is the upper left hand corner.
void Win32ConsoleIO::LocalGotoXY(int x, int y) {
  x = std::max<int>(x, 0);
  x = std::min<int>(x, 79);
  y = std::max<int>(y, 0);
  y += GetTopLine();
  y = std::min<int>(y, GetScreenBottom());

  cursor_pos_.X = static_cast<int16_t>(x);
  cursor_pos_.Y = static_cast<int16_t>(y);
  SetConsoleCursorPosition(out_, cursor_pos_);
}

/* This function returns the current X cursor position, as the number of
* characters from the left hand side of the screen.  An X position of zero
* means the cursor is at the left-most position
*/
size_t Win32ConsoleIO::WhereX() {
  GetConsoleScreenBufferInfo(out_, &buffer_info_);

  cursor_pos_.X = buffer_info_.dwCursorPosition.X;
  cursor_pos_.Y = buffer_info_.dwCursorPosition.Y;

  return cursor_pos_.X;
}

/* This function returns the Y cursor position, as the line number from
* the top of the logical window.  The offset due to the protected top
* of the screen display is taken into account.  A WhereY() of zero means
* the cursor is at the top-most position it can be at.
*/
size_t Win32ConsoleIO::WhereY() {
  GetConsoleScreenBufferInfo(out_, &buffer_info_);
  cursor_pos_.X = buffer_info_.dwCursorPosition.X;
  cursor_pos_.Y = buffer_info_.dwCursorPosition.Y;
  return cursor_pos_.Y - GetTopLine();
}

void Win32ConsoleIO::LocalLf() {
/* This function performs a linefeed to the screen (but not remotely) by
* either moving the cursor down one line, or scrolling the logical screen
* up one line.
*/
  SMALL_RECT scrollRect;
  COORD dest;
  CHAR_INFO fill;

  if (static_cast<size_t>(cursor_pos_.Y) >= GetScreenBottom()) {
    dest.X = 0;
    dest.Y = static_cast<int16_t>(GetTopLine());
    scrollRect.Top = static_cast<int16_t>(GetTopLine() + 1);
    scrollRect.Bottom = static_cast<int16_t>(GetScreenBottom());
    scrollRect.Left = 0;
    scrollRect.Right = 79;
    fill.Attributes = static_cast<int16_t>(curatr);
    fill.Char.AsciiChar = ' ';

    ScrollConsoleScreenBuffer(out_, &scrollRect, nullptr, dest, &fill);
  } else {
    cursor_pos_.Y++;
    SetConsoleCursorPosition(out_, cursor_pos_);
  }
}

/**
 * Returns the local cursor to the left-most position on the screen.
 */
void Win32ConsoleIO::LocalCr() {
  cursor_pos_.X = 0;
  SetConsoleCursorPosition(out_, cursor_pos_);
}

/**
 * Clears the local logical screen
 */
void Win32ConsoleIO::LocalCls() {
  int nOldCurrentAttribute = curatr;
  curatr = 0x07;
  SMALL_RECT scrollRect;
  COORD dest;
  CHAR_INFO fill;

  dest.X = 32767;
  dest.Y = 0;
  scrollRect.Top = static_cast<int16_t>(GetTopLine());
  scrollRect.Bottom = static_cast<int16_t>(GetScreenBottom());
  scrollRect.Left = 0;
  scrollRect.Right = 79;
  fill.Attributes = static_cast<int16_t>(curatr);
  fill.Char.AsciiChar = ' ';

  ScrollConsoleScreenBuffer(out_, &scrollRect, nullptr, dest, &fill);

  LocalGotoXY(0, 0);
  lines_listed = 0;
  curatr = nOldCurrentAttribute;
}

void Win32ConsoleIO::LocalBackspace() {
/* This function moves the cursor one position to the left, or if the cursor
* is currently at its left-most position, the cursor is moved to the end of
* the previous line, except if it is on the top line, in which case nothing
* happens.
*/
  if (cursor_pos_.X >= 0) {
    cursor_pos_.X--;
  } else if (static_cast<size_t>(cursor_pos_.Y) != GetTopLine()) {
    cursor_pos_.Y--;
    cursor_pos_.X = 79;
  }
  SetConsoleCursorPosition(out_, cursor_pos_);
}

void Win32ConsoleIO::LocalPutchRaw(unsigned char ch) {
/* This function outputs one character to the screen, then updates the
* cursor position accordingly, scolling the screen if necessary.  Not that
* this function performs no commands such as a C/R or L/F.  If a value of
* 8, 7, 13, 10, 12 (BACKSPACE, BEEP, C/R, L/F, TOF), or any other command-
* type characters are passed, the appropriate corresponding "graphics"
* symbol will be output to the screen as a normal character.
*/
  DWORD cb;

  SetConsoleTextAttribute(out_, static_cast<int16_t>(curatr));
  WriteConsole(out_, &ch, 1, &cb, nullptr);

  if (cursor_pos_.X <= 79) {
    cursor_pos_.X++;
    return;
  }

  // Need to scroll the screen up one.
  cursor_pos_.X = 0;
  if (static_cast<size_t>(cursor_pos_.Y) == GetScreenBottom()) {
    COORD dest;
    SMALL_RECT MoveRect;
    CHAR_INFO fill;

    // rushfan scrolling fix (was no +1)
    MoveRect.Top    = static_cast<int16_t>(GetTopLine() + 1);
    MoveRect.Bottom = static_cast<int16_t>(GetScreenBottom());
    MoveRect.Left   = 0;
    MoveRect.Right  = 79;

    fill.Attributes = static_cast<int16_t>(curatr);
    fill.Char.AsciiChar = ' ';

    dest.X = 0;
    // rushfan scrolling fix (was -1)
    dest.Y = static_cast<int16_t>(GetTopLine());

    ScrollConsoleScreenBuffer(out_, &MoveRect, &MoveRect, dest, &fill);
  } else {
    cursor_pos_.Y++;
  }
}

/**
 * This function outputs one character to the local screen.  C/R, L/F, TOF,
 * BS, and BELL are interpreted as commands instead of characters.
 */
void Win32ConsoleIO::LocalPutch(unsigned char ch) {
  if (ch > 31) {
    LocalPutchRaw(ch);
  } else if (ch == CM) {
    LocalCr();
  } else if (ch == CJ) {
    LocalLf();
  } else if (ch == CL) {
    LocalCls();
  } else if (ch == BACKSPACE) {
    LocalBackspace();
  } else if (ch == CG) {
    if (!outcom) {
      // TODO Make the bell sound configurable.
      sound(500, milliseconds(4));
    }
  }
}

// Outputs a string to the local screen.
void Win32ConsoleIO::LocalPuts(const string& text) {
  for (char ch : text) {
    LocalPutch(ch);
  }
}

void Win32ConsoleIO::LocalXYPuts(int x, int y, const string& text) {
  LocalGotoXY(x, y);
  LocalFastPuts(text);
}

void Win32ConsoleIO::LocalFastPuts(const string& text) {
// This RAPIDLY outputs ONE LINE to the screen only and is not exactly stable.
  DWORD cb = 0;
  int len = text.length();

  SetConsoleTextAttribute(out_, static_cast<int16_t>(curatr));
  WriteConsole(out_, text.c_str(), len, &cb, nullptr);
  cursor_pos_.X = cursor_pos_.X + static_cast<int16_t>(cb);
}

int  Win32ConsoleIO::LocalPrintf(const char *formatted_text, ...) {
  va_list ap;
  char szBuffer[1024];

  va_start(ap, formatted_text);
  int nNumWritten = vsnprintf(szBuffer, sizeof(szBuffer), formatted_text, ap);
  va_end(ap);
  LocalFastPuts(szBuffer);
  return nNumWritten;
}

int  Win32ConsoleIO::LocalXYPrintf(int x, int y, const char *formatted_text, ...) {
  va_list ap;
  char szBuffer[1024];

  va_start(ap, formatted_text);
  int nNumWritten = vsnprintf(szBuffer, sizeof(szBuffer), formatted_text, ap);
  va_end(ap);
  LocalXYPuts(x, y, szBuffer);
  return nNumWritten;
}

int  Win32ConsoleIO::LocalXYAPrintf(int x, int y, int nAttribute, const char *formatted_text, ...) {
  va_list ap;
  char szBuffer[1024];

  va_start(ap, formatted_text);
  int nNumWritten = vsnprintf(szBuffer, sizeof(szBuffer), formatted_text, ap);
  va_end(ap);

  // bout.SystemColor( nAttribute );
  int nOldColor = curatr;
  curatr = nAttribute;
  LocalXYPuts(x, y, szBuffer);
  curatr = nOldColor;
  return nNumWritten;
}

void Win32ConsoleIO::set_protect(WSession* session, int l) {
// set_protect sets the number of lines protected at the top of the screen.
  if (static_cast<size_t>(l) != GetTopLine()) {
    COORD coord;
    coord.X = 0;
    coord.Y = static_cast<int16_t>(l);

    if (static_cast<size_t>(l) > GetTopLine()) {
      if ((WhereY() + GetTopLine() - l) < 0) {
        CHAR_INFO lpFill;
        SMALL_RECT scrnl;

        scrnl.Top = static_cast<int16_t>(GetTopLine());
        scrnl.Left = 0;
        scrnl.Bottom = static_cast<int16_t>(GetScreenBottom());
        scrnl.Right = 79; //%%TODO - JZ Make the console size user defined

        lpFill.Char.AsciiChar = ' ';
        lpFill.Attributes = 0;

        coord.X = 0;
        coord.Y = static_cast<int16_t>(l);
        ScrollConsoleScreenBuffer(out_, &scrnl, nullptr, coord, &lpFill);
        LocalGotoXY(WhereX(), WhereY() + l - GetTopLine());
      }
    } else {
      DWORD written;
      FillConsoleOutputAttribute(out_, 0, (GetTopLine() - l) * 80, coord, &written);
    }
  }
  SetTopLine(l);
  if (!session->using_modem) {
    session->screenlinest = defscreenbottom + 1 - GetTopLine();
  }
}

void Win32ConsoleIO::savescreen() {
  COORD topleft;
  CONSOLE_SCREEN_BUFFER_INFO bufinfo;
  SMALL_RECT region;

  GetConsoleScreenBufferInfo(out_, &bufinfo);
  topleft.Y = topleft.X = region.Top = region.Left = 0;
  region.Bottom = static_cast<int16_t>(bufinfo.dwSize.Y - 1);
  region.Right  = static_cast<int16_t>(bufinfo.dwSize.X - 1);

  if (!m_ScreenSave.scrn1) {
    m_ScreenSave.scrn1 = static_cast< CHAR_INFO *>(malloc((bufinfo.dwSize.X * bufinfo.dwSize.Y) * sizeof(CHAR_INFO)));
  }

  ReadConsoleOutput(out_, (CHAR_INFO *)m_ScreenSave.scrn1, bufinfo.dwSize, topleft, &region);
  m_ScreenSave.x1 = static_cast<int16_t>(WhereX());
  m_ScreenSave.y1 = static_cast<int16_t>(WhereY());
  m_ScreenSave.topline1 = static_cast<int16_t>(GetTopLine());
  m_ScreenSave.curatr1 = static_cast<int16_t>(curatr);
}

/*
 * restorescreen restores a screen previously saved with savescreen
 */
void Win32ConsoleIO::restorescreen() {
  if (m_ScreenSave.scrn1) {
    // COORD size;
    COORD topleft;
    CONSOLE_SCREEN_BUFFER_INFO bufinfo;
    SMALL_RECT region;

    GetConsoleScreenBufferInfo(out_, &bufinfo);
    topleft.Y = topleft.X = region.Top = region.Left = 0;
    region.Bottom = static_cast<int16_t>(bufinfo.dwSize.Y - 1);
    region.Right  = static_cast<int16_t>(bufinfo.dwSize.X - 1);

    WriteConsoleOutput(out_, m_ScreenSave.scrn1, bufinfo.dwSize, topleft, &region);
    free(m_ScreenSave.scrn1);
    m_ScreenSave.scrn1 = nullptr;
  }
  SetTopLine(m_ScreenSave.topline1);
  curatr = m_ScreenSave.curatr1;
  LocalGotoXY(m_ScreenSave.x1, m_ScreenSave.y1);
}

// int topdata, tempsysop, sysop, is_user_online, 
void Win32ConsoleIO::tleft(WSession* session, bool temp_sysop, bool sysop, bool user_online) {
  static const vector<string> top_screen_items = {
    "",
    "Temp Sysop",
    "",
    "", // was Alert
    "อออออออ",
    "Available",
    "อออออออออออ",
    "%s chatting with %s"
  };

  int cx = WhereX();
  int cy = WhereY();
  int ctl = GetTopLine();
  int cc = curatr;
  curatr = GetTopScreenColor();
  SetTopLine(0);
  double nsln = nsl();
  int nLineNumber = (chatcall && (session->topdata == LocalIO::topdataUser)) ? 5 : 4;

  if (session->topdata) {
    LocalXYPuts(1, nLineNumber, session->GetCurrentSpeed());
    for (int i = WhereX(); i < 23; i++) {
      LocalPutch(static_cast<unsigned char>('\xCD'));
    }

    if (temp_sysop) {
      LocalXYPuts(23, nLineNumber, top_screen_items[1]);
    }
    LocalXYPuts(54, nLineNumber, top_screen_items[4]);

    if (sysop) {
      LocalXYPuts(64, nLineNumber, top_screen_items[5]);
    } else {
      LocalXYPuts(64, nLineNumber, top_screen_items[6]);
    }
  }
  switch (session->topdata) {
  case LocalIO::topdataSystem:
    if (user_online) {
      LocalXYPrintf(18, 3, "T-%6.2f", nsln / SECONDS_PER_MINUTE_FLOAT);
    }
    break;
  case LocalIO::topdataUser: {
    if (user_online) {
      LocalXYPrintf(18, 3, "T-%6.2f", nsln / SECONDS_PER_MINUTE_FLOAT);
    } else {
      LocalXYPrintf(18, 3, session->user()->GetPassword());
    }
  }
  break;
  }
  SetTopLine(ctl);
  curatr = cc;
  LocalGotoXY(cx, cy);
}

void Win32ConsoleIO::UpdateTopScreen(WStatus* pStatus, WSession *session, int nInstanceNumber) {
  char i;
  char sl[82], ar[17], dar[17], restrict[17], rst[17], lo[90];

  int lll = lines_listed;

  if (so() && !incom) {
    session->topdata = LocalIO::topdataNone;
  }

  if (syscfg.sysconfig & sysconfig_titlebar) {
    // Only set the titlebar if the user wanted it that way.
    const string username_num = session->names()->UserName(session->usernum);
    string title = StringPrintf("WWIV Node %d (User: %s)", nInstanceNumber,
              username_num.c_str());
    ::SetConsoleTitle(title.c_str());
  }

  switch (session->topdata) {
  case LocalIO::topdataNone:
    set_protect(session, 0);
    break;
  case LocalIO::topdataSystem:
    set_protect(session, 5);
    break;
  case LocalIO::topdataUser:
    if (chatcall) {
      set_protect(session, 6);
    } else {
      if (GetTopLine() == 6) {
        set_protect(session, 0);
      }
      set_protect(session, 5);
    }
    break;
  }
  int cx = WhereX();
  int cy = WhereY();
  int nOldTopLine = GetTopLine();
  int cc = curatr;
  curatr = GetTopScreenColor();
  SetTopLine(0);
  for (i = 0; i < 80; i++) {
    sl[i] = '\xCD';
  }
  sl[80] = '\0';

  switch (session->topdata) {
  case LocalIO::topdataNone:
    break;
  case LocalIO::topdataSystem: {
    LocalXYPrintf(0, 0, "%-50s  Activity for %8s:      ", syscfg.systemname, pStatus->GetLastDate());

    LocalXYPrintf(0, 1, "Users: %4u       Total Calls: %5lu      Calls Today: %4u    Posted      :%3u ",
                  pStatus->GetNumUsers(), pStatus->GetCallerNumber(),
                  pStatus->GetNumCallsToday(), pStatus->GetNumLocalPosts());

    const string username_num = session->names()->UserName(session->usernum);
    LocalXYPrintf(0, 2, "%-36s      %-4u min   /  %2u%%    E-mail sent :%3u ",
                  username_num.c_str(),
                  pStatus->GetMinutesActiveToday(),
                  static_cast<int>(10 * pStatus->GetMinutesActiveToday() / 144),
                  pStatus->GetNumEmailSentToday());

    LocalXYPrintf(0, 3, "SL=%3u   DL=%3u               FW=%3u      Uploaded:%2u files    Feedback    :%3u ",
                  session->user()->GetSl(),
                  session->user()->GetDsl(),
                  fwaiting,
                  pStatus->GetNumUploadsToday(),
                  pStatus->GetNumFeedbackSentToday());
  }
  break;
  case LocalIO::topdataUser: {
    strcpy(rst, restrict_string);
    for (i = 0; i <= 15; i++) {
      if (session->user()->HasArFlag(1 << i)) {
        ar[i] = static_cast<char>('A' + i);
      } else {
        ar[i] = SPACE;
      }
      if (session->user()->HasDarFlag(1 << i)) {
        dar[i] = static_cast<char>('A' + i);
      } else {
        dar[i] = SPACE;
      }
      if (session->user()->HasRestrictionFlag(1 << i)) {
        restrict[i] = rst[i];
      } else {
        restrict[i] = SPACE;
      }
    }
    dar[16] = '\0';
    ar[16] = '\0';
    restrict[16] = '\0';
    if (!wwiv::strings::IsEquals(session->user()->GetLastOn(), date())) {
      strcpy(lo, session->user()->GetLastOn());
    } else {
      snprintf(lo, sizeof(lo), "Today:%2d", session->user()->GetTimesOnToday());
    }

    const string username_num = session->names()->UserName(session->usernum);
    LocalXYAPrintf(0, 0, curatr, "%-35s W=%3u UL=%4u/%6lu SL=%3u LO=%5u PO=%4u",
                   username_num.c_str(),
                   session->user()->GetNumMailWaiting(),
                   session->user()->GetFilesUploaded(),
                   session->user()->GetUploadK(),
                   session->user()->GetSl(),
                   session->user()->GetNumLogons(),
                   session->user()->GetNumMessagesPosted());

    char szCallSignOrRegNum[ 41 ];
    if (session->user()->GetWWIVRegNumber()) {
      snprintf(szCallSignOrRegNum, sizeof(szCallSignOrRegNum), "%lu", session->user()->GetWWIVRegNumber());
    } else {
      strcpy(szCallSignOrRegNum, session->user()->GetCallsign());
    }
    LocalXYPrintf(0, 1, "%-20s %12s  %-6s DL=%4u/%6lu DL=%3u TO=%5.0lu ES=%4u",
                  session->user()->GetRealName(),
                  session->user()->GetVoicePhoneNumber(),
                  szCallSignOrRegNum,
                  session->user()->GetFilesDownloaded(),
                  session->user()->GetDownloadK(),
                  session->user()->GetDsl(),
                  static_cast<long>((session->user()->GetTimeOn() + timer() - timeon) / SECONDS_PER_MINUTE_FLOAT),
                  session->user()->GetNumEmailSent() + session->user()->GetNumNetEmailSent());

    LocalXYPrintf(0, 2, "ARs=%-16s/%-16s R=%-16s EX=%3u %-8s FS=%4u",
                  ar, dar, restrict, session->user()->GetExempt(),
                  lo, session->user()->GetNumFeedbackSent());

    LocalXYPrintf(0, 3, "%-40.40s %c %2u %-16.16s           FW= %3u",
                  session->user()->GetNote(),
                  session->user()->GetGender(),
                  session->user()->GetAge(),
                  ctypes(session->user()->GetComputerType()).c_str(), fwaiting);

    if (chatcall) {
      LocalXYPuts(0, 4, m_chatReason);
    }
  }
  break;
  }
  if (nOldTopLine != 0) {
    LocalXYPuts(0, nOldTopLine - 1, sl);
  }
  SetTopLine(nOldTopLine);
  LocalGotoXY(cx, cy);
  curatr = cc;
  session->tleft(false);

  lines_listed = lll;
}

/**
 * IsLocalKeyPressed - returns whether or not a key been pressed at the local console.
 *
 * @return true if a key has been pressed at the local console, false otherwise
 */
bool Win32ConsoleIO::LocalKeyPressed() {
  if (ExtendedKeyWaiting) {
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
unsigned char Win32ConsoleIO::LocalGetChar() {
  if (ExtendedKeyWaiting) {
    ExtendedKeyWaiting = 0;
    return GetKeyboardChar();
  }
  unsigned char rc = GetKeyboardChar();
  if (rc == 0 || rc == 0xe0) {
    ExtendedKeyWaiting = 1;
  }
  return rc;
}

void Win32ConsoleIO::SaveCurrentLine(char *cl, char *atr, char *xl, char *cc) {
  *cc = static_cast<char>(curatr);
  strcpy(xl, endofline);
  {
    WORD Attr[80];
    CONSOLE_SCREEN_BUFFER_INFO ConInfo;
    GetConsoleScreenBufferInfo(out_, &ConInfo);

    int len = ConInfo.dwCursorPosition.X;
    ConInfo.dwCursorPosition.X = 0;
    DWORD cb;
    ReadConsoleOutputCharacter(out_, cl, len, ConInfo.dwCursorPosition, &cb);
    ReadConsoleOutputAttribute(out_, Attr, len, ConInfo.dwCursorPosition, &cb);

    for (int i = 0; i < len; i++) {
      atr[i] = static_cast<char>(Attr[i]);   // atr is 8bit char, Attr is 16bit
    }
  }
  cl[ WhereX() ]  = 0;
  atr[ WhereX() ] = 0;
}

void Win32ConsoleIO::MakeLocalWindow(int x, int y, int xlen, int ylen) {
  // Make sure that we are within the range of {(0,0), (80,GetScreenBottom())}
  curatr = GetUserEditorColor();
  xlen = std::min(xlen, 80);
  if (static_cast<size_t>(ylen) > (GetScreenBottom() + 1 - GetTopLine())) {
    ylen = (GetScreenBottom() + 1 - GetTopLine());
  }
  if ((x + xlen) > 80) {
    x = 80 - xlen;
  }
  if (static_cast<size_t>(y + ylen) > GetScreenBottom() + 1) {
    y = GetScreenBottom() + 1 - ylen;
  }

  int xx = WhereX();
  int yy = WhereY();

  // we expect to be offset by GetTopLine()
  y += GetTopLine();

  // large enough to hold 80x50
  CHAR_INFO ci[4000];

  // pos is the position within the buffer
  COORD pos = {0, 0};
  COORD size;
  SMALL_RECT rect;

  // rect is the area on screen the buffer is to be drawn
  rect.Top    = static_cast<int16_t>(y);
  rect.Left   = static_cast<int16_t>(x);
  rect.Right  = static_cast<int16_t>(xlen + x - 1);
  rect.Bottom = static_cast<int16_t>(ylen + y - 1);

  // size of the buffer to use (lower right hand coordinate)
  size.X      = static_cast<int16_t>(xlen);
  size.Y      = static_cast<int16_t>(ylen);

  // our current position within the CHAR_INFO buffer
  int nCiPtr  = 0;

  // Loop through Y, each time looping through X adding the right character
  for (int yloop = 0; yloop < size.Y; yloop++) {
    for (int xloop = 0; xloop < size.X; xloop++) {
      ci[nCiPtr].Attributes = static_cast<int16_t>(GetUserEditorColor());
      if ((yloop == 0) || (yloop == size.Y - 1)) {
        ci[nCiPtr].Char.AsciiChar   = '\xC4';      // top and bottom
      } else {
        if ((xloop == 0) || (xloop == size.X - 1)) {
          ci[nCiPtr].Char.AsciiChar   = '\xB3';  // right and left sides
        } else {
          ci[nCiPtr].Char.AsciiChar   = '\x20';   // nothing... Just filler (space)
        }
      }
      nCiPtr++;
    }
  }

  // sum of the lengths of the previous lines (+0) is the start of next line

  ci[0].Char.AsciiChar                    = '\xDA';      // upper left
  ci[xlen - 1].Char.AsciiChar               = '\xBF';    // upper right

  ci[xlen * (ylen - 1)].Char.AsciiChar        = '\xC0';  // lower left
  ci[xlen * (ylen - 1) + xlen - 1].Char.AsciiChar = '\xD9'; // lower right

  // Send it all to the screen with 1 WIN32 API call (Windows 95's console mode API support
  // is MUCH slower than NT/Win2k, therefore it is MUCH faster to render the buffer off
  // screen and then write it with one fell swoop.

  WriteConsoleOutput(out_, ci, size, pos, &rect);

  // Draw shadow around boxed window
  for (int i = 0; i < xlen; i++) {
    set_attr_xy(x + 1 + i, y + ylen, 0x08);
  }

  for (int i1 = 0; i1 < ylen; i1++) {
    set_attr_xy(x + xlen, y + 1 + i1, 0x08);
  }

  LocalGotoXY(xx, yy);
}

void Win32ConsoleIO::SetCursor(int cursorStyle) {
  CONSOLE_CURSOR_INFO cursInfo;

  switch (cursorStyle) {
  case LocalIO::cursorNone: {
    cursInfo.dwSize = 20;
    cursInfo.bVisible = false;
    SetConsoleCursorInfo(out_, &cursInfo);
  }
  break;
  case LocalIO::cursorSolid: {
    cursInfo.dwSize = 100;
    cursInfo.bVisible = true;
    SetConsoleCursorInfo(out_, &cursInfo);
  }
  break;
  case LocalIO::cursorNormal:
  default: {
    cursInfo.dwSize = 20;
    cursInfo.bVisible = true;
    SetConsoleCursorInfo(out_, &cursInfo);
  }
  }
}

void Win32ConsoleIO::LocalClrEol() {
  CONSOLE_SCREEN_BUFFER_INFO ConInfo;
  DWORD cb;
  int len = 80 - WhereX();

  GetConsoleScreenBufferInfo(out_, &ConInfo);
  FillConsoleOutputCharacter(out_, ' ', len, ConInfo.dwCursorPosition, &cb);
  FillConsoleOutputAttribute(out_, (WORD) curatr, len, ConInfo.dwCursorPosition, &cb);
}

void Win32ConsoleIO::LocalWriteScreenBuffer(const char *buffer) {
  CHAR_INFO ci[2000];
  const char *p = buffer;

  for (int i = 0; i < 2000; i++) {
    ci[i].Char.AsciiChar = *p++;
    ci[i].Attributes     = *p++;
  }

  COORD pos = { 0, 0};
  COORD size = { 80, 25 };
  SMALL_RECT rect = { 0, 0, 79, 24 };

  WriteConsoleOutput(out_, ci, size, pos, &rect);
}

size_t Win32ConsoleIO::GetDefaultScreenBottom() {
  return buffer_info_.dwSize.Y - 1;
}

bool HasKeyBeenPressed(HANDLE in) {
  DWORD num_events;  // NumPending
  GetNumberOfConsoleInputEvents(in, &num_events);
  if (num_events == 0) {
    return false;
  }

  unique_ptr<INPUT_RECORD[]> input(new INPUT_RECORD[num_events]);
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
      if (input[i].Event.KeyEvent.wVirtualKeyCode == VK_SHIFT   ||
          input[i].Event.KeyEvent.wVirtualKeyCode == VK_CONTROL ||
          input[i].Event.KeyEvent.wVirtualKeyCode == VK_MENU) {
        continue;
      }
      if (input[i].Event.KeyEvent.dwControlKeyState & LEFT_ALT_PRESSED ||
          input[i].Event.KeyEvent.dwControlKeyState & RIGHT_ALT_PRESSED) {
        //std::cerr << "ALT KEY uchar=" << input[i].Event.KeyEvent.uChar.AsciiChar << "} ";
      } 
      //std::cerr << "{KeyCode=" << input[i].Event.KeyEvent.wVirtualKeyCode << "; ScanCode=" 
      //          << input[i].Event.KeyEvent.wVirtualScanCode << ", char=" 
      //          << input[i].Event.KeyEvent.uChar.AsciiChar << "} ";
      return true;
    }
  }
  return false;
}

unsigned char GetKeyboardChar() {
  return static_cast<unsigned char>(_getch());
}

static int GetEditLineStringLength(const char *text) {
  size_t i = strlen(text);
  while (i >= 0 && (/*text[i-1] == 32 ||*/ static_cast<unsigned char>(text[i - 1]) == 176)) {
    --i;
  }
  return i;
}

void Win32ConsoleIO::LocalEditLine(char *pszInOutText, int len, int editor_status,
    int *returncode, char *pszAllowedSet) {
  
  int oldatr = curatr;
  int cx = WhereX();
  int cy = WhereY();
  for (size_t i = strlen(pszInOutText); i < static_cast<size_t>(len); i++) {
    pszInOutText[i] = static_cast<unsigned char>(176);
  }
  pszInOutText[len] = '\0';
  curatr = GetEditLineColor();
  LocalFastPuts(pszInOutText);
  LocalGotoXY(cx, cy);
  bool done = false;
  int pos = 0;
  bool insert = false;
  do {
    unsigned char ch = LocalGetChar();
    if (ch == 0 || ch == 224) {
      ch = LocalGetChar();
      switch (ch) {
      case F1:
        done = true;
        *returncode = DONE;
        break;
      case HOME:
        pos = 0;
        LocalGotoXY(cx, cy);
        break;
      case END:
        pos = GetEditLineStringLength(pszInOutText);
        LocalGotoXY(cx + pos, cy);
        break;
      case RARROW:
        if (pos < GetEditLineStringLength(pszInOutText)) {
          pos++;
          LocalGotoXY(cx + pos, cy);
        }
        break;
      case LARROW:
        if (pos > 0) {
          pos--;
          LocalGotoXY(cx + pos, cy);
        }
        break;
      case UARROW:
      case CO:
        done = true;
        *returncode = PREV;
        break;
      case DARROW:
        done = true;
        *returncode = NEXT;
        break;
      case INSERT:
        if (editor_status != SET) {
          insert = !insert;
        }
        break;
      case KEY_DELETE:
        if (editor_status != SET) {
          for (int i = pos; i < len; i++) {
            pszInOutText[i] = pszInOutText[ i + 1 ];
          }
          pszInOutText[ len - 1 ] = static_cast<unsigned char>(176);
          LocalXYPuts(cx, cy, pszInOutText);
          LocalGotoXY(cx + pos, cy);
        }
        break;
      }
    } else {
      if (ch > 31) {
        if (editor_status == UPPER_ONLY) {
          ch = wwiv::UpperCase<unsigned char>(ch);
        }
        if (editor_status == SET) {
          ch = wwiv::UpperCase<unsigned char>(ch);
          if (ch != SPACE) {
            bool bLookingForSpace = true;
            for (int i = 0; i < len; i++) {
              if (ch == pszAllowedSet[i] && bLookingForSpace) {
                bLookingForSpace = false;
                pos = i;
                LocalGotoXY(cx + pos, cy);
                if (pszInOutText[pos] == SPACE) {
                  ch = pszAllowedSet[pos];
                } else {
                  ch = SPACE;
                }
              }
            }
            if (bLookingForSpace) {
              ch = pszAllowedSet[pos];
            }
          }
        }
        if ((pos < len) && ((editor_status == ALL) || (editor_status == UPPER_ONLY) || (editor_status == SET) ||
                            ((editor_status == NUM_ONLY) && (((ch >= '0') && (ch <= '9')) || (ch == SPACE))))) {
          if (insert) {
            for (int i = len - 1; i > pos; i--) {
              pszInOutText[i] = pszInOutText[i - 1];
            }
            pszInOutText[ pos++ ] = ch;
            LocalXYPuts(cx, cy, pszInOutText);
            LocalGotoXY(cx + pos, cy);
          } else {
            pszInOutText[pos++] = ch;
            LocalPutch(ch);
          }
        }
      } else {
        // ch is > 32
        switch (ch) {
        case RETURN:
        case TAB:
          done = true;
          *returncode = NEXT;
          break;
        case ESC:
          done = true;
          *returncode = ABORTED;
          break;
        case CA:
          pos = 0;
          LocalGotoXY(cx, cy);
          break;
        case CE:
          pos = GetEditLineStringLength(pszInOutText);   // len;
          LocalGotoXY(cx + pos, cy);
          break;
        case BACKSPACE:
          if (pos > 0) {
            if (insert) {
              for (int i = pos - 1; i < len; i++) {
                pszInOutText[i] = pszInOutText[i + 1];
              }
              pszInOutText[len - 1] = static_cast<unsigned char>(176);
              pos--;
              LocalXYPuts(cx, cy, pszInOutText);
              LocalGotoXY(cx + pos, cy);
            } else {
              int nStringLen = GetEditLineStringLength(pszInOutText);
              pos--;
              if (pos == (nStringLen - 1)) {
                pszInOutText[ pos ] = static_cast<unsigned char>(176);
              } else {
                pszInOutText[ pos ] = SPACE;
              }
              LocalXYPuts(cx, cy, pszInOutText);
              LocalGotoXY(cx + pos, cy);
            }
          }
          break;
        }
      }
    }
  } while (!done);

  int z = strlen(pszInOutText);
  while (z >= 0 && static_cast<unsigned char>(pszInOutText[z - 1]) == 176) {
    --z;
  }
  pszInOutText[z] = '\0';

  char szFinishedString[260];
  snprintf(szFinishedString, sizeof(szFinishedString), "%-255s", pszInOutText);
  szFinishedString[ len ] = '\0';
  LocalGotoXY(cx, cy);
  curatr = oldatr;
  LocalFastPuts(szFinishedString);
  LocalGotoXY(cx, cy);
}

void Win32ConsoleIO::UpdateNativeTitleBar(WSession* session) {
  // Set console title
  std::stringstream consoleTitleStream;
  consoleTitleStream << "WWIV Node " << session->instance_number() << " (" << syscfg.systemname << ")";
  SetConsoleTitle(consoleTitleStream.str().c_str());
}
