/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services             */
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
#include "common/output.h"

#include "common/pause.h"
#include "common/remote_io.h"
#include "core/strings.h"
#include "local_io/keycodes.h"
#include <string>

using namespace wwiv::common;
using namespace wwiv::strings;

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

  if (sess().outcom() && c != TAB) {
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
  if (c == TAB) {
    const auto screen_pos = wherex();
    for (auto i = screen_pos; i < (((screen_pos / 8) + 1) * 8); i++) {
      displayed += bputch(SPACE);
    }
  } else {
    displayed = 1;
    // Pass through to SDK ansi interpreter.
    const auto last_state = ansi_->state();
    ansi_->write(c);

    if (ansi_->state() == wwiv::sdk::ansi::AnsiMode::not_in_sequence &&
        last_state == wwiv::sdk::ansi::AnsiMode::not_in_sequence) {
      // Only add to the current line if we're not in an ansi sequence.
      // Otherwise we get gibberish ansi strings that will be displayed
      // raw to the user.
      current_line_.emplace_back(c, static_cast<uint8_t>(curatr()));
    }

    const auto screen_width = user().GetScreenChars();
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
    if (c == '\r') {
      x_ = 0;
    }
    if (c == SOFTRETURN) {
      current_line_.clear();
      x_ = 0;
      bout.lines_listed_++;
      // change Build3 + 5.0 to fix message read.
      const auto ll = lines_listed();
      const auto num_sclines = sess().num_screen_lines() - 1;
      if (ll >= num_sclines) {
        if (user().HasPause()) {
          bout.pausescr();
        }
        bout.clear_lines_listed();   // change Build3
      }
    }
  }

  return displayed;
}


/* This function outputs a string to the com port.  This is mainly used
 * for modem commands
 */
void Output::rputs(const std::string& text) {
  // Rushfan fix for COM/IP weirdness
  if (sess().ok_modem_stuff()) {
    remoteIO()->write(text.c_str(), wwiv::stl::size_int(text));
  }
}

void Output::flush() {
  if (!bputch_buffer_.empty()) {
    remoteIO()->write(bputch_buffer_.c_str(), stl::size_int(bputch_buffer_));
    bputch_buffer_.clear();
  }
}

void Output::rputch(char ch, bool use_buffer_) {
  if (!sess().ok_modem_stuff() || remoteIO() == nullptr) {
    return;
  }
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
    remoteIO()->put(ch);
  }
}

