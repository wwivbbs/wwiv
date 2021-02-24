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
#include "core/strings.h"
#include "fmt/printf.h"
#include "local_io/keycodes.h"
#include "sdk/ansi/ansi.h"
#include "sdk/ansi/localio_screen.h"
#include "sdk/ansi/makeansi.h"
#include <algorithm>
#include <cctype>
#include <string>

using std::ostream;
using std::string;
using namespace wwiv::common;
using namespace wwiv::strings;
using namespace wwiv::sdk::ansi;

// static global bout.
Output bout;

Output::Output() = default;
Output::~Output() = default;

static bool okansi(const wwiv::sdk::User& user) {
  return user.HasAnsi();
}

void Output::SetLocalIO(LocalIO* local_io) {
  // We would use user().GetScreenChars() but I don't think we
  // have a live user when we create this.
  screen_ = std::make_unique<LocalIOScreen>(local_io, 80);
  AnsiCallbacks cb;
  cb.move_ = [&](int /* x */, int /* y */) { ansi_movement_occurred_ = true; };
  ansi_ = std::make_unique<Ansi>(screen_.get(), cb, static_cast<uint8_t>(0x07));

  IOBase::SetLocalIO(local_io);
  // Reset the curatr_provider on local_io since screen resets it.
  localIO()->set_curatr_provider(this);
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
  if (okansi(user())) {
    bputs(fmt::sprintf("|17|15 %-78s|#0\r\n\n", msg));
  }
  else {
    format("|#5{}|#0\r\n\n", msg);
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
  bputs(string(length, ' '));
  bout.Left(length);
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

int Output::bputs(const string& text) {
  core::bus().invoke<CheckForHangupEvent>();
  if (text.empty() || sess().hangup()) { return 0; }
  auto& ctx = macro_context_provider_();

  auto it = std::cbegin(text);
  const auto fin = std::cend(text);
  const auto start_time = std::chrono::system_clock::now();
  auto num_written = 0;
  const auto cps = sess().bps() / 10;
  while (it != fin) {
    ++num_written;
    if (cps > 0) {
      while (std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::system_clock::now() - start_time)
                 .count() < (num_written * 1000 / cps)) {
        bout.flush();
        os::sleep_for(std::chrono::milliseconds(10));
      }
    }

    // pipe codes.
    if (*it == '|') {
      ++it;
      if (it == fin) {
        bputch('|', true);
        break;
      }
      if (std::isdigit(*it)) {
        const auto color = pipecode_int(it, fin, 2);
        if (color < 16) {
          bputs(MakeSystemColor(color | (curatr() & 0xf0)));
        } else {
          const auto bg = static_cast<uint8_t>(color << 4);
          const uint8_t fg = curatr() & 0x0f;
          bputs(MakeSystemColor(bg | fg));
        }
      } else if (*it == '@' || *it == '{' || *it == '[') {
        auto r = ctx.interpret(it, fin);
        if (r.cmd == interpreted_cmd_t::text) {
          // Don't use bout here since we can loop.
          for (const auto rich : r.text) {
            bputch(rich, true);
          }
        } else if (r.cmd == interpreted_cmd_t::movement) {
          do_movement(r);
        }
      } else if (*it == '#') {
        ++it;
        const auto color = pipecode_int(it, fin, 1);
        bputs(MakeColor(color));
      } else {
        bputch('|', true);
      }
    }
    else if (*it == CC) {
      ++it;
      if (it == fin) { bputch(CC, true);  break; }
      const unsigned char c = *it++;
      if (c >= SPACE && c <= 126) {
        bputs(MakeColor(c - '0'));
      }
    }
    else if (*it == CO) {
      ++it;
      if (it == fin) { bputch(CO, true);  break; }
      ++it;
      if (it == fin) { bputch(CO, true);  break; }
      auto s = ctx.interpret_macro_char(*it++);
      bout.bputs(s);
    } else if (it == fin) { 
      break; 
    }
    else { 
      bputch(*it++, true);
    }
  }

  flush();
  return ssize(stripcolors(text));
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
  bin.checka(abort, next);
  auto ret = 0;
  if (!bin.checka(abort, next)) {
    ret = bputs(text);
  }
  return ret;
}

void Output::move_up_if_newline(int num_lines) {
  if (okansi(user()) && !newline) {
    const auto s = fmt::format("\r\x1b[{}A", num_lines);
    bputs(s);
  }
}
