/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2016, WWIV Software Services             */
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

#include <algorithm>
#include <cstdarg>
#include <string>

#include "bbs/keycodes.h"
#include "bbs/com.h"
#include "bbs/bbs.h"
#include "bbs/fcns.h"
#include "bbs/instmsg.h"
#include "bbs/output.h"
#include "bbs/vars.h"
#include "core/strings.h"

using std::ostream;
using std::string;
using wwiv::strings::StringPrintf;

outputstreambuf::outputstreambuf() {}
outputstreambuf::~outputstreambuf() {}

std::ostream::int_type outputstreambuf::overflow(std::ostream::int_type c) {
  if (c != EOF) {
    bout.bputch(static_cast<char>(c), false);
  }
  return c;
}

std::streamsize outputstreambuf::xsputn(const char *text, std::streamsize num_chars) {
  if (num_chars == 0) {
    return 0;
  }
  CheckForHangup();
  if (hangup) {
    // Returning 0 here would set the fail bit on the stream, which
    // is not what we want, so pretend that we emitted all of the characters.
    return num_chars;
  }
  for (int i = 0; i < num_chars; i++) {
    if (text[i] == 0) {
      // Hit an embedded \0, stop early.
      break;
    }
    bout.bputch(text[i], true);
  }
  bout.flush();
  return num_chars;
}

void Output::Color(int wwivColor) {
  int c = '\0';

  if (wwivColor <= -1 && wwivColor >= -16) {
    c = (a()->user()->HasColor() ?
         rescolor.resx[207 + std::abs(wwivColor)] : a()->user()->GetBWColor(0));
  }
  if (wwivColor >= 0 && wwivColor <= 9) {
    c = (a()->user()->HasColor() ?
         a()->user()->GetColor(wwivColor) : a()->user()->GetBWColor(wwivColor));
  }
  if (wwivColor >= 10 && wwivColor <= 207) {
    c = (a()->user()->HasColor() ?
         rescolor.resx[wwivColor - 10] : a()->user()->GetBWColor(0));
  }
  if (c == curatr) {
    return;
  }

  SystemColor(c);

  char buffer[81];
  memset(buffer, 0, sizeof(buffer));
  makeansi(a()->user()->HasColor() ?
           a()->user()->GetColor(0) : a()->user()->GetBWColor(0), buffer, false);
  endofline_ = buffer;
}

void Output::ResetColors() {
  // ANSI Clear Attributes String
  bputs("\x1b[0m");
}

void Output::GotoXY(int x, int y) {
  if (okansi()) {
    y = std::min<int>(y, a()->screenlinest);    // Don't get Y get too big or mTelnet will not be happy
    *this << "\x1b[" << y << ";" << x << "H";
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
  string s = ss.str();
  bputs(s);
}

void Output::SavePosition() {
  bputs("\x1b[s");
}

void Output::RestorePosition() {
  bputs("\x1b[u");
}

void Output::nl(int nNumLines) {
  for (int i = 0; i < nNumLines; i++) {
    if (!endofline_.empty()) {
      bputs(endofline_);
      endofline_.clear();
    }
    bputs("\r\n");
    // TODO Change this to fire a notification to a Subject
    // that we should process instant messages now.
    if (inst_msg_waiting() && !bChatLine) {
      process_inst_msgs();
    }
  }
}

void Output::bs() {
  bool bSavedEcho = local_echo;
  local_echo = true;
  bputs("\b \b");
  local_echo = bSavedEcho;
}

void Output::SystemColor(wwiv::sdk::Color color) {
  SystemColor(static_cast<uint8_t>(color));
}

void Output::SystemColor(int nColor) {
  char szBuffer[255];
  makeansi(nColor, szBuffer, false);
  bputs(szBuffer);
}

void Output::litebar(const char *formatText, ...) {
  va_list ap;
  char s[1024];

  va_start(ap, formatText);
  vsnprintf(s, sizeof(s), formatText, ap);
  va_end(ap);

#ifdef OLD_LITEBAR
  if (strlen(s) % 2 != 0) {
    strcat(s, " ");
  }
  int i = (74 - strlen(s)) / 2;
  if (okansi()) {
    char s1[1024];
    snprintf(s1, sizeof(s1), "%s%s%s", charstr(i, ' '), stripcolors(s), charstr(i, ' '));
    *this << "\x1B[0;1;37m" << string(strlen(s1) + 4, '\xDC') << wwiv::endl;
    *this << "\x1B[0;34;47m  " << s1 << "  \x1B[40m\r\n";
    *this << "\x1B[0;1;30m" << string(strlen(s1) + 4, '\xDF') << wwiv::endl;
  } else {
    *this << std::string(i, ' ') << s << wwiv::endl;
  }
#else
  const string header = StringPrintf("|B1|15 %-78s", s);
  bout << header << "|#0\r\n\n";
#endif
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
 * Moves the cursor to the end of the line using ANSI sequences.  If the user
 * does not have ansi, this this function does nothing.
 */
void Output::clreol() {
  if (okansi()) {
    bputs("\x1b[K");
  }
}

void Output::mpl(int length) {
  if (!okansi()) {
    return;
  }
  Color(4);
  bputs(string(length, ' '));
  bputs(StringPrintf("\x1b[%dD", length));
}

int Output::bputs(const string& text) {
  CheckForHangup();
  if (text.empty() || hangup) { return 0; }

  for (char c : text) {
    bputch(c, true);
  }

  flush();
  return text.size();
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

int Output::bprintf(const char *formatText, ...) {
  va_list ap;
  char szBuffer[4096];

  va_start(ap, formatText);
  vsnprintf(szBuffer, sizeof(szBuffer), formatText, ap);
  va_end(ap);
  return bputs(szBuffer);
}
