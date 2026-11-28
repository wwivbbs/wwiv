/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2026, WWIV Software Services                  */
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
/**************************************************************************/
#include "bbs/basic/next/wwiv_bindings.h"

#include "bbs/basic/next/script_keys.h"

#include "bbs/utility.h"
#include "common/context.h"
#include "common/input.h"
#include "common/output.h"
#include "core/scope_exit.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "local_io/keycodes.h"
#include "sdk/user.h"
#include "wwivbasic/console.h"
#include "wwivbasic/interpreter.h"

#include <memory>
#include <string>
#include <vector>

namespace wwiv::bbs::basic {

using wwivbasic::BasicError;
using wwivbasic::Value;
using Args = std::vector<Value>;

namespace {

// Writes each argument with outstr, as the legacy PUTS, OUTSTR, YN and NY do.
void outstr_each(common::Output& out, const Args& args) {
  for (const auto& a : args) {
    out.outstr(wwivbasic::from_value<std::string>(a));
  }
}

// Turns the "More?" prompt on and off for a script, the way a printfile's
// "pause on/off" option or |{set pause ...} does: by setting the user's pause
// flag and the session's disable_pause. It puts both back when the script
// ends (when the interpreter, which holds this, is destroyed), so a script
// can't leave the user's setting changed.
class ScriptPause {
public:
  explicit ScriptPause(common::Context* ctx) : ctx_(ctx) {}
  ~ScriptPause() {
    if (ctx_ && saved_) {
      ctx_->u().set_flag(sdk::User::pauseOnPage, saved_user_pause_);
      ctx_->session_context().disable_pause(saved_disable_pause_);
    }
  }
  ScriptPause(const ScriptPause&) = delete;
  ScriptPause& operator=(const ScriptPause&) = delete;

  // Returns whether pausing was on.
  bool set(bool on) {
    if (!ctx_) {
      return false;
    }
    auto& sess = ctx_->session_context();
    auto& u = ctx_->u();
    if (!saved_) {
      saved_ = true;
      saved_user_pause_ = u.pause();
      saved_disable_pause_ = sess.disable_pause();
    }
    const auto was_on = u.pause();
    u.set_flag(sdk::User::pauseOnPage, on);
    sess.disable_pause(!on);
    return was_on;
  }

private:
  common::Context* ctx_;
  // Whether the settings below were saved (on the first PAUSING call).
  bool saved_{false};
  bool saved_user_pause_{false};
  bool saved_disable_pause_{false};
};

} // namespace

// The wwiv.io module.
void RegisterWwivIo(wwivbasic::Interpreter& interp, const WwivBindings& b) {
  auto& m = interp.add_module("wwiv.io");

  m.native_function("MODULE_NAME", []() { return std::string("wwiv.io"); });

  // PUTS is the old name for OUTSTR. Both write each argument.
  m.native_functionl("PUTS", [b](const Args& args) {
    outstr_each(b.out, args);
    return Value();
  });
  m.native_functionl("OUTSTR", [b](const Args& args) {
    outstr_each(b.out, args);
    return Value();
  });

  // PL writes each argument followed by a newline.
  m.native_functionl("PL", [b](const Args& args) {
    for (const auto& a : args) {
      b.out.outstr(wwivbasic::from_value<std::string>(a));
      b.out.nl();
    }
    return Value();
  });

  // New in the new engine: OUTSTR and PL with MCI off, for text other users
  // wrote (such as posts), as the BBS's message reader shows it. Colors still
  // show, but |{...} expressions and |@ macros aren't run against the caller's
  // session.
  m.native_functionl("OUTSTR_NOMCI", [b](const Args& args) {
    const auto mci = b.out.mci_enabled();
    const auto restore = core::finally([&b, mci] { b.out.set_mci_enabled(mci); });
    b.out.disable_mci();
    outstr_each(b.out, args);
    return Value();
  });
  m.native_functionl("PL_NOMCI", [b](const Args& args) {
    const auto mci = b.out.mci_enabled();
    const auto restore = core::finally([&b, mci] { b.out.set_mci_enabled(mci); });
    b.out.disable_mci();
    for (const auto& a : args) {
      b.out.outstr(wwivbasic::from_value<std::string>(a));
      b.out.nl();
    }
    return Value();
  });

  m.native_functionl("PRINTFILE", [b](const Args& args) {
    if (args.size() > 1) {
      throw BasicError(
          fmt::format("PRINTFILE: expected 0 or 1 argument(s), got {}", args.size()));
    }
    if (!args.empty()) {
      b.out.printfile(wwivbasic::from_value<std::string>(args[0]));
    }
    return Value();
  });

  // GETS([max_length]) reads a line of at most max_length characters. With no
  // length (or one that isn't positive) it reads nothing and returns "".
  m.native_functionl("GETS", [b](const Args& args) {
    if (args.size() > 1) {
      throw BasicError(fmt::format("GETS: expected 0 or 1 argument(s), got {}", args.size()));
    }
    const auto max_length = args.empty() ? 0 : wwivbasic::from_value<int>(args[0]);
    if (max_length <= 0) {
      return Value();
    }
    return Value(b.console.read_line(max_length));
  });

  m.native_function("GETKEY", [b]() { return b.console.read_key(); });

  m.native_function("CLS", [b]() { b.out.cls(); });

  m.native_functionl("NL", [b](const Args& args) {
    if (args.size() > 1) {
      throw BasicError(fmt::format("NL: expected 0 or 1 argument(s), got {}", args.size()));
    }
    b.out.nl(args.empty() ? 1 : wwivbasic::from_value<int>(args[0]));
    return Value();
  });

  // YN(prompt...) defaults to No, NY(prompt...) to Yes; both return 1 or 0.
  m.native_functionl("YN", [b](const Args& args) {
    outstr_each(b.out, args);
    return Value(b.in.yesno() ? 1 : 0);
  });
  m.native_functionl("NY", [b](const Args& args) {
    outstr_each(b.out, args);
    return Value(b.in.noyes() ? 1 : 0);
  });

  m.native_function("PAUSE", [b]() { b.out.pausescr(); });

  // New in the new engine: for full-screen UIs. The names are WWIV's own
  // where WWIV has one (4.24's okansi, ansic, prt, pla, onek, nsl; goxy as
  // GOTOXY), otherwise Synchronet's. Screen functions do nothing without ANSI.

  // OKANSI() is TRUE if the caller has ANSI on.
  m.native_function("OKANSI", [ctx = b.ctx]() { return ctx && ctx->u().ansi(); });

  // GOTOXY(x, y) moves the cursor to column x, row y, counting from 1.
  m.native_function("GOTOXY", [b](int x, int y) {
    if (x < 1 || y < 1) {
      throw BasicError(fmt::format("GOTOXY: x and y start at 1, got {}, {}", x, y));
    }
    b.out.goxy(x, y);
  });

  // CLREOL() clears from the cursor to the end of the line.
  m.native_function("CLREOL", [b]() { b.out.clreol(); });

  // ANSIC(n) selects color n (0 to 9) of the caller's colors, as |#n does.
  auto check_color = [](const char* fn, int n) {
    if (n < 0 || n > 9) {
      throw BasicError(fmt::format("{}: color must be from 0 to 9, got {}", fn, n));
    }
  };
  m.native_function("ANSIC", [b, check_color](int n) {
    check_color("ANSIC", n);
    b.out.ansic(n);
  });

  // PRT(n, text...) writes text in color n, then goes back to color 0.
  m.native_functionl("PRT", [b, check_color](const Args& args) {
    if (args.empty()) {
      throw BasicError("PRT: expected a color and text");
    }
    const auto n = wwivbasic::from_value<int>(args[0]);
    check_color("PRT", n);
    b.out.ansic(n);
    outstr_each(b.out, Args(args.begin() + 1, args.end()));
    b.out.ansic(0);
    return Value();
  });

  // PLA(text) writes a line the caller can abort (with space or ^C);
  // returns TRUE if they did.
  m.native_function("PLA", [b](std::string text) {
    bool abort{false};
    b.out.bpla(text, &abort);
    return abort;
  });

  // ONEK(keys) waits for the caller to press one of keys (case doesn't
  // matter) and returns it in upper case. Once the caller hangs up it returns
  // the first of keys, so put the one that quits first. It doesn't echo the
  // key or start a new line.
  m.native_function("ONEK", [b](std::string keys) {
    keys = strings::ToStringUpperCase(keys);
    if (keys.empty()) {
      throw BasicError("ONEK: expected at least one key");
    }
    while (true) {
      const auto k = strings::ToStringUpperCase(b.console.read_key());
      if (k.empty()) {
        // Hung up.
        return std::string(1, keys.front());
      }
      if (keys.find(k.front()) != std::string::npos) {
        return k;
      }
    }
  });

  // READKEY() waits for a key. It returns the character, or for a special key
  // its name: UP, DOWN, LEFT, RIGHT, HOME, END, PGUP, PGDN, INS, DEL, F1 to
  // F10 (so a result longer than one character is a special key). Enter is
  // CHR(13). Keys it has no name for, or that the BBS handles itself (Ctrl+T
  // for time left, say), are skipped. Once the caller hangs up it returns "".
  m.native_function("READKEY", [b]() {
    return ReadScriptKey(
        b.in, [&b] { return b.in.bgetch_event(common::Input::numlock_status_t::NUMBERS); });
  });

  // SCREEN_COLUMNS() and SCREEN_ROWS() are the size of the caller's screen.
  m.native_function("SCREEN_COLUMNS",
                    [ctx = b.ctx]() { return ctx ? ctx->u().screen_width() : 80; });
  m.native_function("SCREEN_ROWS", [ctx = b.ctx]() {
    return ctx ? ctx->session_context().num_screen_lines() : 25;
  });

  // The pipe and heart color codes in text: STRIP_COLORS(s) removes them,
  // LEN_COLORS(s) is how wide s shows, and PAD_COLORS(s, n) pads s with spaces
  // to show n wide.
  m.native_function("STRIP_COLORS", [](std::string s) { return std::string(::stripcolors(s)); });
  m.native_function("LEN_COLORS", [](std::string s) { return strings::size_without_colors(s); });
  // n is at most 65535, as for SPACE and STRING.
  m.native_function("PAD_COLORS", [](std::string s, int n) {
    static constexpr int kMaxPad = 65535;
    if (n > kMaxPad) {
      throw BasicError(fmt::format("PAD_COLORS: {} is more than {} characters", n, kMaxPad));
    }
    return strings::pad_to_ignore_colors(s, n);
  });

  // PAUSING(on) turns the "More?" prompt on or off, and returns whether it
  // was on. The user's own setting is put back when the script ends. (The
  // caller can still turn it back on or off with Ctrl+Y.)
  auto pause = std::make_shared<ScriptPause>(b.ctx);
  m.native_function("PAUSING", [pause](bool on) { return pause->set(on); });

  // NSL() is the number of seconds left in this call (4.24's nsl).
  // TODO: nsl() reads a(): the time added to this call (extratimecall) is
  // only on Application. Use the Context once that moves to SessionContext.
  m.native_function("NSL", []() { return static_cast<int>(nsl()); });
}

} // namespace wwiv::bbs::basic
