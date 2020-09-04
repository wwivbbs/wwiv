/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*              Copyright (C)2016-2020, WWIV Software Services            */
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
#include "common/full_screen.h"

//#include "bbs/bbs.h"
#include "common/bgetch.h"
#include "common/input.h"
#include "common/output.h"
#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/strings.h"
#include <iterator>
#include <memory>
#include <string>

using std::string;
using std::unique_ptr;
using namespace wwiv::common;
using namespace wwiv::stl;
using namespace wwiv::strings;


FullScreenView::FullScreenView(Output& output, int numlines, int swidth, int slength) 
: bout_(output), num_header_lines_(numlines), screen_width_(swidth), screen_length_(slength) {
  message_height_ = screen_length_ - num_header_lines_ - 2 - 1;
  lines_start_ = num_header_lines_ + 2;
  lines_end_ = lines_start_ + message_height_;
  command_line_ = screen_length_;
}

FullScreenView::~FullScreenView() = default;

void FullScreenView::PrintTimeoutWarning(int) {
  bout_.GotoXY(1, command_line_);
  bout_.bputs("|12Press space if you are still there.");
  bout_.clreol();
}

void FullScreenView::ClearCommandLine() {
  bout_.GotoXY(1, command_line_);
  bout_.clreol();
  bout_.GotoXY(1, command_line_);
}

void FullScreenView::PutsCommandLine(const std::string& text) {
  bout_.GotoXY(1, command_line_);
  bout_ << text;
  bout_.clreol();
}

void FullScreenView::ClearMessageArea() {
  for (int y = lines_start_; y < lines_end_; y++) {
    bout_.GotoXY(1, y);
    bout_.clreol();
  }
  bout_.GotoXY(1, lines_start_);
}

void FullScreenView::DrawTopBar() {
  bout_.GotoXY(1, num_header_lines_ + 1);
  std::ostringstream ss;
  ss << "|#7" << static_cast<unsigned char>(198) 
     << string(screen_width_ - 2, static_cast<unsigned char>(205))
     << static_cast<unsigned char>(181);
  bout_.bputs(ss.str());
}

void FullScreenView::DrawBottomBar(const std::string& text) {
  auto y = screen_length_ - 1;
  bout_.GotoXY(1, y);
  const auto saved_color = bout.curatr();
  wwiv::core::ScopeExit at_ext([=] { bout.SystemColor(saved_color); });

  bout_ << "|09" << static_cast<unsigned char>(198)
        << string(screen_width_ - 2, static_cast<unsigned char>(205))
        << static_cast<unsigned char>(181);

  if (text.empty()) {
    return;
  }

  int x = screen_width_ - 10 - ssize(text);
  bout_.GotoXY(x, y);
  bout_ << "|09" << static_cast<unsigned char>(181) << "|17|14 " << text
        << " |16|09" << static_cast<unsigned char>(198);
}

void FullScreenView::GotoContentAreaTop() {
  bout_.GotoXY(1, num_header_lines_ + 2); 
}

int FullScreenView::bgetch() { 
  return bin.bgetch_event(Input::numlock_status_t::NUMBERS,
                      [&](Input::bgetch_timeout_status_t status, int s) {
                        if (status == Input::bgetch_timeout_status_t::WARNING) {
      PrintTimeoutWarning(s);
                        } else if (status == Input::bgetch_timeout_status_t::CLEAR) {
      ClearCommandLine();
    }
  });
}
