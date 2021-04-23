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

#include "common/common_events.h"
#include "common/input.h"
#include "common/macro_context.h"
#include "core/eventbus.h"
#include "core/file.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "local_io/keycodes.h"
#include "sdk/ansi/ansi.h"
#include "sdk/ansi/localio_screen.h"
#include "sdk/ansi/makeansi.h"
#include <algorithm>
#include <cctype>
#include <string>

using namespace std::chrono;
using namespace wwiv::common;
using namespace wwiv::local::io;
using namespace wwiv::os;
using namespace wwiv::strings;
using namespace wwiv::sdk::ansi;

// static global "bout"
Output bout;

static constexpr char BBS_STR_INI[] = "bbs.str.ini";

Output::Output() = default;

Language& Output::lang() {
  if (!lang_) {
    auto mi = wwiv::core::FilePath(context().session_context().dirs().current_menu_directory(),
                                   BBS_STR_INI);
    auto gi =
        wwiv::core::FilePath(context().session_context().dirs().gfiles_directory(), BBS_STR_INI);
    lang_ = std::make_unique<Language>(mi, gi);
  }
  DCHECK(lang_);
  return *lang_;
}

Output::~Output() = default;

static bool okansi(const wwiv::sdk::User& user) {
  return user.ansi();
}

void Output::SetLocalIO(LocalIO* local_io) {
  // We would use user().screen_width() but I don't think we
  // have a live user when we create this.
  screen_ = std::make_unique<LocalIOScreen>(local_io, 80);
  AnsiCallbacks cb;
  cb.move_ = [&](int /* x */, int /* y */) { ansi_movement_occurred_ = true; };
  ansi_ = std::make_unique<Ansi>(screen_.get(), cb, static_cast<uint8_t>(0x07));

  IOBase::SetLocalIO(local_io);
  // Reset the curatr_provider on local_io since screen resets it.
  localIO()->set_curatr_provider(this);

  // This is kinda a hack to do it here, but the constructor is too early.  By
  // this point the system is started up, bout created, and also all other global
  // static objects have been constructed.
  // If this is a problem, we can make a new method to do this and have the bbs,
  // fsed, etc all call it after setting the LocalIO.
  core::bus().add_handler<UpdateMenuSet>([this](const UpdateMenuSet& u) {
    if (current_menu_set_ != u.name) {
      current_menu_set_ = u.name;
      // Reload language support.
      auto mi = core::FilePath(context().session_context().dirs().current_menu_directory(),
                               BBS_STR_INI);
      auto gi = core::FilePath(context().session_context().dirs().gfiles_directory(),
                               BBS_STR_INI);
      lang_ = std::make_unique<Language>(mi, gi);
    }
  });
}

void Output::Color(int wwiv_color) {
  const auto saved_x{x_};
  bputs(MakeColor(wwiv_color));
  x_ = saved_x;
}

void Output::ResetColors() {
  const auto saved_x{x_};
  // ANSI Clear Attributes String
  bputs("\x1b[0m");
  x_ = saved_x;
}

void Output::GotoXY(int x, int y) {
  if (okansi(user())) {
    // Don't get Y get too big or mTelnet will not be happy
    y = std::min<int>(y, sess().num_screen_lines());
    bputs(StrCat("\x1b[", y, ";", x, "H"));
  }
  x_ = x;
}

void Output::Up(int num) {
  if (num == 0) {
    return;
  }
  std::ostringstream ss;
  ss << "\x1b[";
  if (num > 1) {
    ss << num;
  }
  ss << "A";
  bputs(ss.str());
}

void Output::Down(int num) {
  if (num == 0) {
    return;
  }
  std::ostringstream ss;
  ss << "\x1b[";
  if (num > 1) {
    ss << num;
  }
  ss << "B";
  bputs(ss.str());
}

void Output::Left(int num) {
  const auto saved_x = x_;
  if (num == 0) {
    return;
  }
  std::ostringstream ss;
  ss << "\x1b[";
  if (num > 1) {
    ss << num;
  }
  ss << "D";
  bputs(ss.str());
  x_ = std::max<int>(0, saved_x - num);
}

void Output::Right(int num) {
  const auto saved_x{x_};
  if (num == 0) {
    return;
  }
  std::ostringstream ss;
  ss << "\x1b[";
  if (num > 1) {
    ss << num;
  }
  ss << "C";
  bputs(ss.str());
  x_ = std::max<int>(0, saved_x + num);

}

void Output::SavePosition() {
  bputs("\x1b[s");
}

void Output::RestorePosition() {
  bputs("\x1b[u");
}

void Output::nl(int num_lines) {
  if (num_lines == 0) {
    return;
  }
  for (auto i = 0; i < num_lines; i++) {
    bputs("\r\n");
    core::bus().invoke(ProcessInstanceMessages{});
  }
  x_ = 1;
}

void Output::bs() {
  if (okansi(user())) {
    Left(1);
    bputch(' ');
    Left(1);
  } else {
    bputs("\b \b");
  }
}


void Output::SystemColor(wwiv::sdk::Color c) {
  SystemColor(static_cast<uint8_t>(c));
}

void Output::SystemColor(int c) {
  bputs(MakeSystemColor(c));
}

std::string Output::MakeColor(int wwiv_color) {
  const auto c = user().color(wwiv_color);
  if (c == curatr()) {
    return "";
  }

  needs_color_reset_at_newline_ = true;
  return MakeSystemColor(c);
}

std::string Output::MakeSystemColor(int c) const {
  if (!okansi(user())) {
    return "";
  }
  return makeansi(c, curatr());
}

std::string Output::MakeSystemColor(sdk::Color color) const {
  return MakeSystemColor(static_cast<uint8_t>(color));
}

void Output::litebar(const std::string& msg) {
  const auto len = user().screen_width() - 2;
  if (okansi(user())) {
    bputs(fmt::format("|17|15 {:<{}}|#0\r\n\n", msg, len));
  }
  else {
    format("|#5 {}|#0\r\n\n", msg);
  }
}

void Output::backline() {
  Color(0);
  bputch(SPACE);
  for (auto i = wherex() + 1; i >= 0; i--) {
    this->bs();
  }
}

/**
 * Clears the local and remote screen using ANSI (if enabled), otherwise DEC 12
 */
void Output::cls() {
  // Adding color 0 so previous color would not be picked up. #1245
  Color(0);  
  if (okansi(user())) {
    bputs("\x1b[2J");
    GotoXY(1, 1);
  } else {
    bputch(CL);
  }
  clear_lines_listed();
}

/**
 * Clears the current line to the end.
 */
void Output::clreol(int ct) {
  const auto saved_x{x_};
  if (okansi(user())) {
    if (ct == 0) {
      bputs("\x1b[K");
    } else {
      format("\x1b[{}K", ct);
    }
  }
  x_ = saved_x;
}

void Output::clear_whole_line() {
  bputch('\r');
  clreol();
}

void Output::mpl(int length) {
  if (!okansi(user())) {
    return;
  }
  Color(4);
  bputs(std::string(length, ' '));
  Left(length);
}

int Output::PutsXY(int x, int y, const std::string& text) {
  GotoXY(x, y);
  return bputs(text);
}

int Output::PutsXYSC(int x, int y, int a, const std::string& text) {
  GotoXY(x, y);
  SystemColor(a);
  return bputs(text);
}

int Output::PutsXYA(int x, int y, int color, const std::string& text) {
  GotoXY(x, y);
  Color(color);
  return bputs(text);
}

void Output::do_movement(const Interpreted& r) {
  if (r.left) {
    Left(r.left);
  }
  if (r.right) {
    Right(r.right);
  }
  if (r.up) {
    Up(r.up);
  }
  if (r.down) {
    Down(r.down);
  }
  if (r.x != -1 && r.y != -1) {
    GotoXY(r.x, r.y);
  }
  if (r.clreol) {
    clreol();
  }
  if (r.clrbol) {
    clreol();
  }
  if (r.cls) {
    cls();
  }
  if (r.nl) {
    nl(r.nl);
  }
}

template <typename T>
static int pipecode_int(T& it, const T end, int num_chars) {
  std::string s;
  while (it != end && num_chars-- > 0 && std::isdigit(static_cast<uint8_t>(*it))) {
    s.push_back(*it);
    ++it; 
  }
  return to_number<int>(s);
}

int Output::bputs(const std::string& text) {
  core::bus().invoke<CheckForHangupEvent>();
  if (text.empty() || sess().hangup()) {
    return 0;
  }
  auto& ctx = macro_context_provider_();

  auto it = std::cbegin(text);
  const auto fin = std::cend(text);
  const auto start_time = std::chrono::system_clock::now();
  auto num_written = 0;
  const auto cps = sess().bps() / 10;
  while (it != fin) {
    if (cps > 0) {
      while (std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::system_clock::now() - start_time)
                 .count() < (num_written * 1000 / cps)) {
        flush();
        os::sleep_for(std::chrono::milliseconds(10));
      }
    }

    // pipe codes.
    if (*it == '|') {
      ++it;
      if (it == fin) {
        num_written += bputch('|', true);
        break;
      }
      if (std::isdigit(*it)) {
        if (const auto color = pipecode_int(it, fin, 2); color < 16) {
          SystemColor(color | (curatr() & 0xf0));
        } else {
          const auto bg = static_cast<uint8_t>(color << 4);
          const uint8_t fg = curatr() & 0x0f;
          SystemColor(bg | fg);
        }
      } else if (*it == '@' || *it == '{' || *it == '[') {
        if (auto r = ctx.interpret(it, fin); r.cmd == interpreted_cmd_t::text) {
          // Don't use bout here since we can loop.
          if (r.needs_reinterpreting) {
            num_written += bputs(r.text);
          } else {
            for (const auto rich : r.text) {
              num_written += bputch(rich, true);
            }
          }
        } else if (r.cmd == interpreted_cmd_t::movement) {
          do_movement(r);
        }
      } else if (*it == '#') {
        ++it;
        const auto color = pipecode_int(it, fin, 1);
        Color(color);
      } else {
        num_written += bputch('|', true);
      }
    }
    else if (*it == CC) {
      ++it;
      if (it == fin) {
        num_written += bputch(CC, true);
        break;
      }
      if (const unsigned char c = *it++; c >= SPACE && c <= 126) {
        Color(c-'0');
      }
    }
    else if (*it == CO) {
      ++it;
      if (it == fin) {
        num_written += bputch(CO, true);
        break;
      }
      ++it;
      if (it == fin) {
        num_written += bputch(CO, true);
        break;
      }
      auto s = ctx.interpret_macro_char(*it++);
      num_written += bputs(s);
    } else if (it == fin) { 
      break; 
    }
    else { 
      num_written += bputch(*it++, true);
    }
  }

  flush();
  return num_written;
}

// This one does a newline.  Since it used to be pla. Should make
// it consistent.
int Output::bpla(const std::string& text, bool *abort) {
  bool dummy;
  const auto ret = bputs(text, abort, &dummy);
  if (!bin.checka(abort, &dummy)) {
    nl();
  }
  return ret;
}

// This one doesn't do a newline. (used to be osan)
int Output::bputs(const std::string& text, bool *abort, bool *next) {
  core::bus().invoke<CheckForHangupEvent>();
  if (bin.checka(abort, next)) {
    return 0;
  }
  return bputs(text);
}

void Output::move_up_if_newline(int num_lines) {
  if (okansi(user()) && !newline) {
    Up(num_lines);
  }
}

/**
 * Displays the text in the strings file referred to by key.
 */
int Output::str(const std::string& key) {
  // Process arguments
  const auto format_str = lang().value(key);
  return bputs(format_str);
}

void Output::back_puts(const std::string& text, int color, std::chrono::duration<double> char_dly, std::chrono::duration<double> string_dly) {
  Color(color);
  sleep_for(char_dly);
  for (const auto ch : text) {
    bputch(ch);
    sleep_for(char_dly);
  }

  sleep_for(string_dly);
  for (int i = 0; i < stl::size_int(text); i++) {
    bs();
    sleep_for(5ms);
  }  
}

void Output::rainbow(const std::string& text) {
  int c = 0;
  for (char ch : text) {
    Color(c++);
    bputch(ch, true);
    c %= 8;
  }
  flush();
}

void Output::spin_puts(const std::string& text, int color) {
  if (!okansi(user())) {
    bputs(text);
    return;
  }
  Color(color);
  const auto dly = milliseconds(30);
  for (const auto ch : text) {
    sleep_for(dly);
    bputch('/', false);
    Left(1);
    sleep_for(dly);
    bputch('-', false);
    Left(1);
    sleep_for(dly);
    bputch('\\', false);
    Left(1);
    sleep_for(dly);
    bputch('|', false);
    Left(1);
    sleep_for(dly);
    bputch(ch);
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

    const auto screen_width = user().screen_width();
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
      lines_listed_++;
      // change Build3 + 5.0 to fix message read.
      const auto ll = lines_listed();
      if (const auto num_sclines = sess().num_screen_lines() - 1; ll >= num_sclines) {
        if (user().pause()) {
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

