/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*              Copyright (C)2016-2022, WWIV Software Services            */
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

#include "common/common_events.h"
#include "common/input.h"
#include "common/output.h"
#include "core/eventbus.h"
#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/strings.h"
#include <string>

using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::local::io;
using namespace wwiv::stl;
using namespace wwiv::strings;

namespace wwiv::common {

FullScreenView::FullScreenView(Output& output, Input& input, int numlines, int swidth, int slength)
    : bin_(input), bout_(output), num_header_lines_(numlines), screen_width_(swidth),
      screen_length_(slength) {
  message_height_ = screen_length_ - num_header_lines_ - 2 - 1;
  lines_start_ = num_header_lines_ + 2;
  lines_end_ = lines_start_ + message_height_;
  command_line_ = screen_length_;
  
  // Save the topdata.
  saved_topdata = output.localIO()->topdata();
  if (output.localIO()->topdata() != LocalIO::topdata_t::none) {
    output.localIO()->topdata(LocalIO::topdata_t::none);
    bus().invoke<UpdateTopScreenEvent>();
  }
}

FullScreenView::~FullScreenView() {
  bout_.localIO()->topdata(saved_topdata);
  bus().invoke<UpdateTopScreenEvent>();
}

void FullScreenView::PrintTimeoutWarning(int) {
  bout_.goxy(1, command_line_);
  bout_.outstr("|12Press space if you are still there.");
  bout_.clreol();
}

void FullScreenView::ClearCommandLine() {
  bout_.goxy(1, command_line_);
  bout_.clreol();
  bout_.goxy(1, command_line_);
}

void FullScreenView::PutsCommandLine(const std::string& text) {
  bout_.goxy(1, command_line_);
  bout_.outstr(text);
  bout_.clreol();
}

void FullScreenView::ClearMessageArea() {
  for (int y = lines_start_; y < lines_end_; y++) {
    bout_.goxy(1, y);
    bout_.clreol();
  }
  bout_.goxy(1, lines_start_);
}

void FullScreenView::DrawTopBar() {
  bout_.goxy(1, num_header_lines_ + 1);
  bout_.print("|#7{:c}{}{:c}", static_cast<unsigned char>(198),
              std::string(std::max<int>(40, screen_width_) - 2, static_cast<unsigned char>(205)),
              static_cast<unsigned char>(181));
}

void FullScreenView::DrawBottomBar(const std::string& text) {
  const auto y = screen_length_ - 1;
  bout_.goxy(1, y);
  const auto saved_color = bout.curatr();
  auto at_exit = finally([=] { bout.setc(saved_color); });

  bout_.print("|#7{:c}{}{:c}", static_cast<unsigned char>(198),
              std::string(screen_width_ - 2, static_cast<unsigned char>(205)),
              static_cast<unsigned char>(181));

  if (text.empty()) {
    return;
  }

  const auto x = screen_width_ - 10 - ssize(text);
  bout_.goxy(x, y);
  bout_.print("|#7{:c}|17|14 {} |16|09{:c}", static_cast<unsigned char>(181), text,
              static_cast<unsigned char>(198));
}

void FullScreenView::GotoContentAreaTop() { bout_.goxy(1, num_header_lines_ + 2); }

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

} // namespace wwiv::common
