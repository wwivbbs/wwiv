/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
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
#include "bbs/output.h"

#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/com.h"
#include "bbs/instmsg.h"
#include "bbs/interpret.h"
#include "bbs/utility.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "local_io/keycodes.h"
#include "sdk/ansi/ansi.h"
#include "sdk/ansi/localio_screen.h"
#include "sdk/ansi/makeansi.h"
#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <string>

using std::ostream;
using std::string;
using namespace wwiv::strings;
using namespace wwiv::sdk::ansi;

Output::Output() { memset(charbuffer, 0, sizeof(charbuffer)); }
Output::~Output() = default;

void Output::SetLocalIO(LocalIO* local_io) {
  // We would use a()->user()->GetScreenChars() but I don't thik we
  // have a live user when we create this.
  screen_ = std::make_unique<LocalIOScreen>(local_io, 80);
  AnsiCallbacks cb;
  cb.move_ = [&](int /* x */, int /* y */) { ansi_movement_occurred_ = true; };
  ansi_ = std::make_unique<Ansi>(screen_.get(), cb, static_cast<uint8_t>(0x07));

  local_io_ = local_io;
  // Reset the curatr_provider on local_io since screen resets it.
  local_io_->set_curatr_provider(this);
}


void Output::Color(int wwivcolor) {
  bputs(MakeColor(wwivcolor));
}

void Output::ResetColors() {
  // ANSI Clear Attributes String
  bputs("\x1b[0m");
}

void Output::GotoXY(int x, int y) {
  if (okansi()) {
    // Don't get Y get too big or mTelnet will not be happy
    y = std::min<int>(y, a()->screenlinest);
    bputs(StrCat("\x1b[", y, ";", x, "H"));
  }
}

void Output::Left(int num) {
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
}

void Output::Right(int num) {
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
}

void Output::SavePosition() {
  bputs("\x1b[s");
}

void Output::RestorePosition() {
  bputs("\x1b[u");
}

void Output::nl(int nNumLines) {
  for (int i = 0; i < nNumLines; i++) {
    bputs("\r\n");
    // TODO Change this to fire a notification to a Subject
    // that we should process instant messages now.
    if (inst_msg_waiting() && !a()->chatline_) {
      process_inst_msgs();
    }
  }
}

void Output::bs() {
  bputs("\b \b");
}


void Output::SystemColor(wwiv::sdk::Color c) {
  SystemColor(static_cast<uint8_t>(c));
}

void Output::SystemColor(int c) {
  bputs(MakeSystemColor(c));
}

std::string Output::MakeColor(int wwivcolor) {
  auto c = a()->user()->color(wwivcolor);
  if (c == curatr()) {
    return "";
  }

  needs_color_reset_at_newline_ = true;
  return MakeSystemColor(c);
}

std::string Output::MakeSystemColor(int c) {
  if (!okansi()) {
    return "";
  }
  return wwiv::sdk::ansi::makeansi(c, curatr());
}

std::string Output::MakeSystemColor(wwiv::sdk::Color c) {
  return MakeSystemColor(static_cast<uint8_t>(c));
}

void Output::litebar(const std::string& msg) {
  if (okansi()) {
    bputs(fmt::sprintf("|17|15 %-78s|#0\r\n\n", msg));
  }
  else {
    bout << "|#5" << msg << "|#0\r\n\n";
  }
}

void Output::backline() {
  Color(0);
  bputch(SPACE);
  for (int i = wherex() + 1; i >= 0; i--) {
    this->bs();
  }
}

/**
 * Clears the local and remote screen using ANSI (if enabled), otherwise DEC 12
 */
void Output::cls() {
  if (okansi()) {
    bputs("\x1b[2J");
    GotoXY(1, 1);
  } else {
    bputch(CL);
  }
}

/**
 * Clears the current line to the end.
 */
void Output::clreol() {
  if (okansi()) {
    bputs("\x1b[K");
  }
}

void Output::clear_whole_line() {
  bputch('\r');
  clreol();
}

void Output::mpl(int length) {
  if (!okansi()) {
    return;
  }
  Color(4);
  bputs(string(length, ' '));
  bout.Left(length);
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
  CheckForHangup();
  if (text.empty() || a()->hangup_) { return 0; }

  auto it = std::begin(text);
  auto fin = std::end(text);
  while (it != fin) {
    // pipe codes.
    if (*it == '|') {
      it++;
      if (it == fin) { bputch('|', true);  break; }
      if (std::isdigit(*it)) {
        int color = pipecode_int(it, fin, 2);
        if (color < 16) {
          bputs(MakeSystemColor(color | (curatr() & 0xf0)));
        }
        else {
          uint8_t bg = static_cast<uint8_t>(color) << 4;
          uint8_t fg = curatr() & 0x0f;
          bputs(MakeSystemColor(bg | fg));
        }
      }
      else if (*it == '@') {
        it++;
        BbsMacroContext ctx(a()->user(), a()->mci_enabled_);
        auto s = ctx.interpret(*it++);
        bout.bputs(s);
      }
      else if (*it == '#') {
        it++;
        int color = pipecode_int(it, fin, 1);
        bputs(MakeColor(color));
      }
      else {
        bputch('|', true);
      }
    }
    else if (*it == CC) {
      it++;
      if (it == fin) { bputch(CC, true);  break; }
      unsigned char c = *it++;
      if (c >= SPACE && c <= 126) {
        bputs(MakeColor(c - '0'));
      }
    }
    else if (*it == CO) {
      it++;
      if (it == fin) { bputch(CO, true);  break; }
      it++;
      if (it == fin) { bputch(CO, true);  break; }
      BbsMacroContext ctx(a()->user(), a()->mci_enabled_);
      auto s = ctx.interpret(*it++);
      bout.bputs(s);
    } else if (it == fin) { 
      break; 
    }
    else { 
      bputch(*it++, true);
    }
  }

  flush();
  return stripcolors(text).size();
}

// This one does a newline.  Since it used to be pla. Should make
// it consistent.
int Output::bpla(const std::string& text, bool *abort) {
  bool dummy;
  int ret = bputs(text, abort, &dummy);
  if (!checka(abort, &dummy)) {
    nl();
  }
  return ret;
}

// This one doesn't do a newline. (used to be osan)
int Output::bputs(const std::string& text, bool *abort, bool *next) {
  CheckForHangup();
  checka(abort, next);
  int ret = 0;
  if (!checka(abort, next)) {
    ret = bputs(text);
  }
  return ret;
}

void Output::move_up_if_newline(int num_lines) {
  if (okansi() && !newline) {
    const auto s = StrCat("\r\x1b[", num_lines, "A");
    bputs(s);
  }
}
