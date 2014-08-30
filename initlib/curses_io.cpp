/**************************************************************************/
/*                                                                        */
/*                 WWIV Initialization Utility Version 5.0                */
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
#include <cstring>
#include <iostream>
#include <memory>
#include <sstream>

#include "curses.h"
#include "curses_io.h"
#include "core/strings.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif  // _WIN32

using std::auto_ptr;
using std::string;
using wwiv::strings::StringPrintf;

extern const char *wwiv_version;
extern const char *beta_version;
extern const char *wwiv_date;
extern unsigned short wwiv_num_version;

static const char* copyrightString = "Copyright (c) 1998-2014, WWIV Software Services";

#if defined ( _WIN32 )
static HANDLE hConOut;
static COORD originalConsoleSize;
#endif

CursesIO* out;

void CursesFooter::ShowHelpItems(const std::vector<HelpItem>& help_items) const {
  window_->Move(0, 0);
  window_->ClrtoEol();
  for (const auto& h : help_items) {
    window_->SetColor(SchemeId::FOOTER_KEY);
    window_->AddStr(h.key);
    window_->SetColor(SchemeId::FOOTER_TEXT);
    window_->AddStr("-");
    window_->AddStr(h.description);
    window_->AddStr(" ");
  }
  window_->Refresh();
}

void CursesFooter::ShowContextHelp(const std::string& help_text) const {
  window_->Move(0, 0);
  window_->ClrtoEol();
  window_->AddStr(help_text);
}

void CursesFooter::SetDefaultFooter() const {
  window_->Erase();
  window_->Move(0, 0);
  window_->SetColor(SchemeId::FOOTER_KEY);
  window_->MvAddStr(0, 0, "Esc");
  window_->SetColor(SchemeId::FOOTER_TEXT);
  window_->AddStr("-Exit ");
  window_->Refresh();
}

CursesIO::CursesIO() 
    : max_x_(0), max_y_(0), window_(nullptr), footer_(nullptr), 
      header_(nullptr), indicator_mode_(IndicatorMode::NONE) {

#ifdef _WIN32
  // Save the original window size and also resize the window buffer size
  // to match the visible window size so we do not have scroll bars.  The
  // scroll bars don't work with text based UIs well so we will remove them
  // like we do in bbs.exe
  hConOut = GetStdHandle(STD_OUTPUT_HANDLE);
  if (hConOut != INVALID_HANDLE_VALUE) {
    CONSOLE_SCREEN_BUFFER_INFO consoleBufferInfo;
    GetConsoleScreenBufferInfo(hConOut, &consoleBufferInfo);
    originalConsoleSize = consoleBufferInfo.dwSize;
    SMALL_RECT rect = consoleBufferInfo.srWindow;
    COORD bufSize;
    bufSize.X = static_cast<short>(rect.Right - rect.Left + 1);
    bufSize.Y = static_cast<short>(rect.Bottom - rect.Top + 1);
    bufSize.X = static_cast<short>(std::min<SHORT>(bufSize.X, 80));
    bufSize.Y = static_cast<short>(std::min<SHORT>(bufSize.Y, 25));
    SetConsoleWindowInfo(hConOut, TRUE, &rect);
    SetConsoleScreenBufferSize(hConOut, bufSize);

    // Have to reset this info, otherwise bad things happen.
    GetConsoleScreenBufferInfo(hConOut, &consoleBufferInfo);
  }
#endif  // _WIN32

  initscr();
  raw();
  keypad(stdscr, TRUE);
  noecho();
  nonl();
  start_color();
  color_scheme_.reset(new ColorScheme());

  int stdscr_maxx = getmaxx(stdscr);
  int stdscr_maxy = getmaxy(stdscr);
  header_ = new CursesWindow(nullptr, color_scheme_.get(), 2, 0, 0, 0);
  footer_ = new CursesFooter(new CursesWindow(nullptr, color_scheme_.get(), 2, 0, stdscr_maxy-2, 0), 
    color_scheme_.get());
  header_->Bkgd(color_scheme_->GetAttributesForScheme(SchemeId::HEADER));
  const string s = StringPrintf("WWIV %s%s Initialization/Configuration Program.", wwiv_version, beta_version);
  header_->SetColor(SchemeId::HEADER);
  header_->MvAddStr(0, 0, s);
  header_->SetColor(SchemeId::HEADER_COPYRIGHT);
  header_->MvAddStr(1, 0, copyrightString);
  footer_->window()->Bkgd(color_scheme_->GetAttributesForScheme(SchemeId::HEADER));
  header_->Refresh();
  footer_->window()->Refresh();
  header_->RedrawWin();

  window_ = new CursesWindow(nullptr, color_scheme_.get(), stdscr_maxy-4, stdscr_maxx, 2, 0);
  window_->Keypad(true);

  touchwin(stdscr);
  max_x_ = window_->GetMaxX();
  max_y_ = window_->GetMaxY();
  window_->TouchWin();
  window_->Refresh();
}

CursesIO::~CursesIO() {
  delete footer_;
  delete header_;
  delete window_;
  endwin();

#ifdef _WIN32
  // restore the original window size.
  SetConsoleScreenBufferSize(hConOut, originalConsoleSize);
#endif  // _WIN32
}

/**
 * Clears the local logical screen
 */
void CursesIO::Cls(chtype background_char) {
  window_->SetColor(SchemeId::NORMAL);
  window_->Bkgd(background_char);
  window_->Clear();
  window_->Refresh();
  window_->GotoXY(0, 0);
}

void CursesIO::SetIndicatorMode(IndicatorMode mode) {
  if (mode == indicator_mode_) {
    return;
  }
  int x = max_x_ - 5;
  std::string s = "   ";
  switch (mode) {
  case IndicatorMode::INSERT:
    s = "INS";
    break;
  case IndicatorMode::OVERWRITE:
    s = "OVR";
    break;
  }
  header_->MvAddStr(1, x, s.c_str());
  header_->Refresh();
  indicator_mode_ = mode;
}

CursesWindow* CursesIO::CreateBoxedWindow(const std::string& title, int nlines, int ncols) {
  auto_ptr<CursesWindow> window(new CursesWindow(window_, color_scheme_.get(), nlines, ncols));
  window->SetColor(SchemeId::WINDOW_BOX);
  window->Box(0, 0);
  window->SetTitle(title);
  window->SetColor(SchemeId::WINDOW_TEXT);
  return window.release();
}

// static
void CursesIO::Init() {
    out = new CursesIO();
}
