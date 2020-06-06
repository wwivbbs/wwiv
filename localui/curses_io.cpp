/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*             Copyright (C)1998-2020, WWIV Software Services             */
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
#include "core/wwiv_windows.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>

#include "core/strings.h"
#include "localui/wwiv_curses.h"
#include "localui/curses_io.h"

using std::unique_ptr;
using std::string;
using namespace wwiv::strings;

static const char* copyrightString = "Copyright (c) 1998-2020, WWIV Software Services";

#if defined ( _WIN32 )
static HANDLE hConOut;
static COORD originalConsoleSize;
#endif

CursesIO* curses_out = nullptr;

void CursesFooter::ShowHelpItems(int line, const std::vector<HelpItem>& help_items) const {
  if (line < 0 || line > 1) {
    line = 0;
  }
  window_->Move(line, 0);
  window_->ClrtoEol();
  for (const auto& h : help_items) {
    window_->SetColor(SchemeId::FOOTER_KEY);
    window_->Puts(h.key);
    window_->SetColor(SchemeId::FOOTER_TEXT);
    window_->Puts("-");
    window_->Puts(h.description);
    window_->Puts(" ");
  }
  window_->Refresh();
}

void CursesFooter::ShowContextHelp(const std::string& help_text) const {
  window_->Move(1, 0);
  window_->ClrtoEol();
  window_->Puts(help_text);
}

void CursesFooter::SetDefaultFooter() const {
  window_->Erase();
  window_->Move(0, 0);
  window_->SetColor(SchemeId::FOOTER_KEY);
  window_->PutsXY(0, 0, "Esc");
  window_->SetColor(SchemeId::FOOTER_TEXT);
  window_->Puts("-Exit ");
  window_->Refresh();
}

CursesIO::CursesIO(const string& title) 
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
    auto rect = consoleBufferInfo.srWindow;
    COORD bufSize;
    bufSize.X = static_cast<SHORT>(rect.Right - rect.Left + 1);
    bufSize.Y = static_cast<SHORT>(rect.Bottom - rect.Top + 1);
    bufSize.X = static_cast<SHORT>(std::min<SHORT>(bufSize.X, 80));
    bufSize.Y = static_cast<SHORT>(std::min<SHORT>(bufSize.Y, 25));
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

  if (stdscr_maxx < 80) {
    endwin();
    throw std::runtime_error(StrCat("Screen width must be at least 80, was: ", stdscr_maxx));
  }
  if (stdscr_maxy < 20) {
    endwin();
    throw std::runtime_error(StrCat("Screen height must be at least 20, was: ", stdscr_maxy));
  }

  header_.reset(new CursesWindow(nullptr, color_scheme_.get(), 2, 0));
  footer_.reset(new CursesFooter(new CursesWindow(nullptr, color_scheme_.get(), 2, 0, stdscr_maxy-2, 0), 
    color_scheme_.get()));
  header_->Bkgd(color_scheme_->GetAttributesForScheme(SchemeId::HEADER));
  header_->SetColor(SchemeId::HEADER);
  header_->PutsXY(0, 0, title);
  header_->SetColor(SchemeId::HEADER_COPYRIGHT);
  header_->PutsXY(0, 1, copyrightString);
  footer_->window()->Bkgd(color_scheme_->GetAttributesForScheme(SchemeId::HEADER));
  header_->Refresh();
  footer_->window()->Refresh();
  header_->RedrawWin();

  window_.reset(new CursesWindow(nullptr, color_scheme_.get(), stdscr_maxy-4, stdscr_maxx, 2, 0));
  window_->Keypad(true);

  touchwin(stdscr);
  max_x_ = window_->GetMaxX();
  max_y_ = window_->GetMaxY();
  window_->TouchWin();
  window_->Refresh();
}

CursesIO::~CursesIO() {
  endwin();

#ifdef _WIN32
  // restore the original window size.
  SetConsoleScreenBufferSize(hConOut, originalConsoleSize);
#endif  // _WIN32
}

/**
 * Clears the local logical screen
 */
void CursesIO::Cls(uint32_t background_char) {
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
  std::string s = "   ";
  if (mode == IndicatorMode::INSERT) {
    s = "INS";
  } else if (mode == IndicatorMode::OVERWRITE) {
    s = "OVR";
  }
  header_->PutsXY(max_x_ - 5, 1, s);
  header_->Refresh();
  indicator_mode_ = mode;
}

CursesWindow* CursesIO::CreateBoxedWindow(const std::string& title, int nlines, int ncols) {
  unique_ptr<CursesWindow> window(new CursesWindow(window_.get(), color_scheme_.get(), nlines, ncols));
  window->SetColor(SchemeId::WINDOW_BOX);
  window->Box(0, 0);
  window->SetTitle(title);
  window->SetColor(SchemeId::WINDOW_TEXT);
  return window.release();
}

// static
void CursesIO::Init(const std::string& title) {
  const auto once_init = [=]() { curses_out = new CursesIO(title); return true; };
  [[maybe_unused]] static auto initialized_once = once_init();
}

// static 
CursesIO* CursesIO::Get() {
  return curses_out;
}


// static

int CursesIO::GetMaxX() const {
  return getmaxx(stdscr);
}

int CursesIO::GetMaxY() const {
  return getmaxy(stdscr);
}

