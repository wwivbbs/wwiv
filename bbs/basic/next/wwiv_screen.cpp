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
#include "common/context.h"
#include "common/full_screen.h"
#include "common/input.h"
#include "common/output.h"
#include "common/pause.h"
#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "sdk/user.h"
#include "wwivbasic/interpreter.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace wwiv::bbs::basic {

using wwivbasic::BasicError;
using wwivbasic::Value;

namespace {

bool has_ansi(common::Context* ctx) { return ctx && ctx->u().ansi(); }
int screen_columns(common::Context* ctx) { return ctx ? ctx->u().screen_width() : 80; }
int screen_rows(common::Context* ctx) {
  return ctx ? ctx->session_context().num_screen_lines() : 25;
}

// The strings in a script list.
std::vector<std::string> to_strings(const Value& list, const char* fn) {
  std::vector<std::string> items;
  for (const auto& v : list.list(fn)->items) {
    items.push_back(v.toString());
  }
  return items;
}

class ScriptScreen;

// Shared by FULLSCREEN and the lightbars: the screen that's open, if any.
struct ScreenState {
  ScriptScreen* open{nullptr};
};

// FULLSCREEN's screen: a common::FullScreenView, with pausing off while it's
// open. CLOSE clears the screen and puts pausing back. When the script no
// longer holds it (a DEF that opened it returns, or the script ends), pausing
// comes back too, but the screen is left as it is: the cursor goes to the
// start of the command line, which is cleared, so the BBS's next output
// starts on the bottom row instead of over the layout.
class ScriptScreen {
public:
  ScriptScreen(common::Output& out, common::Input& in, int header_lines, int width, int height,
               std::shared_ptr<ScreenState> state)
      : in_(in), out_(out), state_(std::move(state)), width_(width), height_(height) {
    out_.cls();
    pause_ = std::make_unique<common::TempDisablePause>(out_);
    view_ = std::make_unique<common::FullScreenView>(out_, in_, header_lines, width, height);
    content_top_ = view_->lines_start();
    content_height_ = view_->message_height();
    state_->open = this;
  }
  ~ScriptScreen() { close(false); }
  ScriptScreen(const ScriptScreen&) = delete;
  ScriptScreen& operator=(const ScriptScreen&) = delete;

  // Closes the screen. With clear, clears it; otherwise leaves it, with the
  // cursor at the start of the (cleared) command line.
  void close(bool clear) {
    if (!view_) {
      return;
    }
    view_.reset();
    pause_.reset();
    state_->open = nullptr;
    if (clear) {
      out_.cls();
      return;
    }
    out_.ansic(0);
    out_.goxy(1, height_);
    out_.clreol();
  }

  // The view, or an error naming the method fn if the screen is closed.
  common::FullScreenView& view(const char* fn) const {
    if (!view_) {
      throw BasicError(fmt::format("screen.{}: the screen is closed", fn));
    }
    return *view_;
  }

  [[nodiscard]] int width() const { return width_; }
  [[nodiscard]] int height() const { return height_; }
  [[nodiscard]] int content_top() const { return content_top_; }
  [[nodiscard]] int content_height() const { return content_height_; }
  [[nodiscard]] common::Input& in() const { return in_; }

  // Reads a key as READKEY does, showing the idle warning on the command
  // line. fn names the method for the error if the screen is closed.
  std::string read_key(const char* fn) const {
    auto& v = view(fn);
    return ReadScriptKey(in_, [&v] { return v.bgetch(); });
  }

private:
  common::Input& in_;
  common::Output& out_;
  // Shared with FULLSCREEN and the lightbars: this is the open screen.
  std::shared_ptr<ScreenState> state_;
  std::unique_ptr<common::TempDisablePause> pause_;
  std::unique_ptr<common::FullScreenView> view_;
  int width_;
  int height_;
  int content_top_{0};
  int content_height_{0};
};

// LIGHTBAR's list: height rows from screen row `row`, with one item
// highlighted. It redraws only the rows that change when the selection moves
// within the window.
class Lightbar {
public:
  Lightbar(common::Output& out, common::Input& in, std::vector<std::string> items, int row,
           int height, int width)
      : in_(in), out_(out), items_(std::move(items)), row_(row), height_(height), width_(width) {}

  [[nodiscard]] int count() const { return stl::size_int(items_); }
  // The selected item, or -1 if there are none.
  [[nodiscard]] int selected() const { return items_.empty() ? -1 : selected_; }
  [[nodiscard]] int top_item() const { return top_item_; }
  [[nodiscard]] const std::string& color() const { return color_; }
  [[nodiscard]] const std::string& selected_color() const { return selected_color_; }

  void set_selected(int i) {
    if (i < 0 || i >= count()) {
      throw BasicError(
          fmt::format("lightbar.selected: {} isn't an item (there are {})", i, count()));
    }
    selected_ = i;
    scroll_to_selected();
  }

  void set_items(std::vector<std::string> items) {
    items_ = std::move(items);
    selected_ = std::clamp(selected_, 0, std::max(0, count() - 1));
    top_item_ = std::clamp(top_item_, 0, max_top());
    scroll_to_selected();
    drawn_ = false;
  }

  // Makes the next update draw everything, as when the screen was cleared.
  void redraw_all() { drawn_ = false; }

  void set_color(std::string c) {
    color_ = std::move(c);
    drawn_ = false;
  }
  void set_selected_color(std::string c) {
    selected_color_ = std::move(c);
    drawn_ = false;
  }

  // Moves the selection for a movement key (UP, DOWN, PGUP, PGDN, HOME, END);
  // false for any other key.
  bool move(const std::string& key) {
    const auto last = count() - 1;
    if (key == "UP") {
      selected_ = std::max(0, selected_ - 1);
    } else if (key == "DOWN") {
      selected_ = std::max(0, std::min(last, selected_ + 1));
    } else if (key == "PGUP") {
      selected_ = std::max(0, selected_ - height_);
      top_item_ = std::max(0, top_item_ - height_);
    } else if (key == "PGDN") {
      selected_ = std::max(0, std::min(last, selected_ + height_));
      top_item_ = std::min(max_top(), top_item_ + height_);
    } else if (key == "HOME") {
      selected_ = 0;
      top_item_ = 0;
    } else if (key == "END") {
      selected_ = std::max(0, last);
      top_item_ = max_top();
    } else {
      return false;
    }
    scroll_to_selected();
    return true;
  }

  // Draws every row.
  void draw() {
    for (int i = 0; i < height_; i++) {
      draw_row(i);
    }
    drawn_ = true;
    drawn_top_ = top_item_;
    drawn_selected_ = selected_;
    park_cursor();
  }

  // Draws what changed since the last draw: everything if the list scrolled
  // or changed, otherwise the rows that were and are selected.
  void update() {
    if (!drawn_ || drawn_top_ != top_item_) {
      draw();
      return;
    }
    if (drawn_selected_ != selected_) {
      draw_row(drawn_selected_ - top_item_);
      draw_row(selected_ - top_item_);
      drawn_selected_ = selected_;
    }
    park_cursor();
  }

  // Whether RUN is running, so RUN can't run again from its own callback.
  [[nodiscard]] bool running() const { return running_; }
  void running(bool r) { running_ = r; }
  [[nodiscard]] common::Input& in() const { return in_; }

private:
  [[nodiscard]] int max_top() const { return std::max(0, count() - height_); }

  void scroll_to_selected() {
    if (selected_ < top_item_) {
      top_item_ = selected_;
    } else if (selected_ >= top_item_ + height_) {
      top_item_ = selected_ - height_ + 1;
    }
  }

  // Row i of the window: an item, or blank below the last one. Items are
  // written with MCI codes off, since they often hold what other users wrote;
  // the selected one is written without its own colors so the highlight
  // shows.
  void draw_row(int i) {
    const auto mci = out_.mci_enabled();
    const auto restore = core::finally([this, mci] { out_.set_mci_enabled(mci); });
    out_.disable_mci();
    out_.goxy(1, row_ + i);
    const auto index = top_item_ + i;
    if (index >= count()) {
      out_.outstr(color_ + std::string(static_cast<size_t>(width_), ' '));
    } else {
      const auto selected = index == selected_;
      const auto& item = items_.at(static_cast<size_t>(index));
      const auto text = selected ? std::string(stripcolors(item)) : item;
      out_.outstr(
          (selected ? selected_color_ : color_) +
          strings::pad_to_ignore_colors(strings::trim_to_size_ignore_colors(text, width_), width_));
    }
    out_.outstr("|#0");
  }

  // On the selected row, where a terminal's cursor is expected.
  void park_cursor() { out_.goxy(1, row_ + (items_.empty() ? 0 : selected_ - top_item_)); }

  common::Input& in_;
  common::Output& out_;
  bool running_{false};
  std::vector<std::string> items_;
  int row_;
  int height_;
  int width_;
  int selected_{0};
  int top_item_{0};
  std::string color_{"|#0"};
  std::string selected_color_{"|17|15"};
  // What's on the screen, for update.
  bool drawn_{false};
  int drawn_top_{0};
  int drawn_selected_{0};
};

} // namespace

// The wwiv.screen module (new in the new engine): full-screen layouts and
// lightbar lists. Both need ANSI.
void RegisterWwivScreen(wwivbasic::Interpreter& interp, const WwivBindings& b) {
  auto& m = interp.add_module("wwiv.screen");
  m.native_function("MODULE_NAME", []() { return std::string("wwiv.screen"); });

  auto& screen_class = m.class_<ScriptScreen>("screen");
  screen_class.property("width", &ScriptScreen::width)
      .property("height", &ScriptScreen::height)
      .property("content_top", &ScriptScreen::content_top)
      .property("content_height", &ScriptScreen::content_height)
      // TOP_BAR() draws the bar under the header lines.
      .method("top_bar", [](ScriptScreen& s) { s.view("top_bar").DrawTopBar(); })
      // BOTTOM_BAR(text) draws the bar over the command line, with text (if
      // any) near its right end.
      .method("bottom_bar",
              [](ScriptScreen& s, std::string text) { s.view("bottom_bar").DrawBottomBar(text); })
      // COMMAND_LINE(text) writes text on the bottom row, then puts the cursor
      // at its start.
      .method(
          "command_line",
          [](ScriptScreen& s, std::string text) { s.view("command_line").PutsCommandLine(text); })
      .method("clear_command_line",
              [](ScriptScreen& s) { s.view("clear_command_line").ClearCommandLine(); })
      // CLEAR_CONTENT() clears the rows between the bars.
      .method("clear_content", [](ScriptScreen& s) { s.view("clear_content").ClearMessageArea(); })
      // READKEY() is wwiv.io.READKEY that shows the "are you there?" warning
      // on the command line when the caller is idle.
      .method("readkey", [](ScriptScreen& s) { return s.read_key("readkey"); })
      // CLOSE() clears the screen and turns pausing back on.
      .method("close", [](ScriptScreen& s) { s.close(true); });

  // FULLSCREEN(header_lines) opens a full-screen layout: header_lines rows at
  // the top for the script, a bar, the content rows, a bar, and the command
  // line on the bottom row. One can be open at a time.
  auto state = std::make_shared<ScreenState>();
  m.native_function("FULLSCREEN", [b, state, &screen_class](int header_lines) {
    if (!has_ansi(b.ctx)) {
      throw BasicError("FULLSCREEN: needs ANSI; check wwiv.io.okansi() first");
    }
    if (state->open) {
      throw BasicError("FULLSCREEN: a screen is already open; close it first");
    }
    const auto width = screen_columns(b.ctx);
    const auto height = screen_rows(b.ctx);
    if (header_lines < 0 || height - header_lines - 3 < 1) {
      throw BasicError(
          fmt::format("FULLSCREEN: {} header lines leave no room for content on a {}-row screen",
                      header_lines, height));
    }
    return screen_class.wrap(
        std::make_shared<ScriptScreen>(b.out, b.in, header_lines, width, height, state));
  });

  auto& lightbar_class = m.class_<Lightbar>("lightbar");
  lightbar_class.property("count", &Lightbar::count)
      .property("top_item", &Lightbar::top_item)
      .property("selected", &Lightbar::selected, [](Lightbar& l, int i) { l.set_selected(i); })
      .property(
          "color", [](const Lightbar& l) { return l.color(); },
          [](Lightbar& l, std::string c) { l.set_color(std::move(c)); })
      .property(
          "selected_color", [](const Lightbar& l) { return l.selected_color(); },
          [](Lightbar& l, std::string c) { l.set_selected_color(std::move(c)); })
      // DRAW() draws the whole list.
      .method("draw", [](Lightbar& l) { l.draw(); })
      // SET_ITEMS(list) replaces the items, keeping the selection if it's
      // still in the list. RUN or DRAW shows them.
      .method("set_items",
              [](Lightbar& l, Value items) { l.set_items(to_strings(items, "set_items")); })
      // RUN() draws the list, lets the caller move through it, and returns
      // the first other key. RUN("fn") instead calls fn(key, selected) for each other
      // key, and keeps going while it returns TRUE; it redraws the list after
      // each call, and returns the key fn returned FALSE for. Both return ""
      // once the caller hangs up. While a FULLSCREEN is open, keys are read
      // through it, so an idle caller gets its warning on the command line.
      .methodl("run", [&interp, state](Lightbar& l, const std::vector<Value>& args) {
        if (args.size() > 1) {
          throw wwivbasic::ArgumentError(
              fmt::format("expected 0 or 1 arguments, got {}", args.size()));
        }
        const auto fn = args.empty() ? std::string() : args.front().toString();
        if (!fn.empty() && !interp.has_function(fn)) {
          throw BasicError(fmt::format("lightbar.run: there is no function '{}'", fn));
        }
        if (l.running()) {
          throw BasicError("lightbar.run: this lightbar is already running (RUN was called "
                           "from its own callback)");
        }
        l.running(true);
        const auto done = core::finally([&l] { l.running(false); });
        // The script may have cleared or drawn over the list since it was
        // last drawn.
        l.redraw_all();
        while (true) {
          l.update();
          // The callback may open or close a screen, so look each time.
          auto key = state->open ? state->open->read_key("readkey") : ReadScriptKey(l.in(), [&l] {
            return l.in().bgetch_event(common::Input::numlock_status_t::NUMBERS);
          });
          if (key.empty()) {
            return Value(key);
          }
          if (l.move(key)) {
            continue;
          }
          if (fn.empty() || !interp.call(fn, {Value(key), Value(l.selected())}).toBool()) {
            return Value(key);
          }
          // The callback may have drawn over the list.
          l.redraw_all();
        }
      });

  // LIGHTBAR(items, top, height) makes a lightbar list of the strings in
  // items, height rows from screen row top (1 is the top row), as wide as the
  // screen less one column. Nothing is drawn until DRAW or RUN.
  m.native_function("LIGHTBAR", [b, &lightbar_class](Value items, int top, int height) {
    if (!has_ansi(b.ctx)) {
      throw BasicError("LIGHTBAR: needs ANSI; check wwiv.io.okansi() first");
    }
    const auto rows = screen_rows(b.ctx);
    if (top < 1 || height < 1 || top + height - 1 > rows) {
      throw BasicError(fmt::format("LIGHTBAR: rows {} to {} aren't all on the screen (1 to {})",
                                   top, top + height - 1, rows));
    }
    return lightbar_class.wrap(std::make_shared<Lightbar>(
        b.out, b.in, to_strings(items, "LIGHTBAR"), top, height, screen_columns(b.ctx) - 1));
  });
}

} // namespace wwiv::bbs::basic
