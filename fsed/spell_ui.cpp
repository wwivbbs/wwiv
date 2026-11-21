/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                Copyright (C)2026, WWIV Software Services               */
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
#include "fsed/spell_ui.h"

#include "common/context.h"
#include "common/full_screen.h"
#include "common/input.h"
#include "common/output.h"
#include "core/log.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "fsed/model.h"
#include "fsed/spell.h"
#include "fsed/spell_config.h"
#include "fsed/spell_walk.h"
#include "fsed/view.h"
#include "sdk/config.h"
#include "sdk/menus/menu_set.h"
#include "sdk/user.h"
#include <algorithm>
#include <string>
#include <vector>

namespace wwiv::fsed {

using namespace wwiv::common;
using namespace wwiv::stl;
using namespace wwiv::strings;

// Bright white on red.
static constexpr int SPELL_HIGHLIGHT_COLOR = 0x4f;

namespace {

class FsedSpellCheckIO final : public SpellCheckIO {
public:
  FsedSpellCheckIO(Context& ctx, FsedView& view) : ctx_(ctx), view_(view) {}

  void show_word(FsedModel& ed, const spell_word_t& word,
                 const std::vector<std::string>& suggestions) override {
    // Move the cursor (and the view if needed) to the word. The current
    // line is drawn without interpreting pipe codes, so cell == column.
    const auto previous_line = ed.curli;
    const auto mvl = std::max<int>(1, view_.max_view_lines());
    auto top = view_.top_line();
    const auto scroll = word.line < top || word.end_line >= top + mvl;
    if (scroll) {
      top = std::max<int>(0, word.line - mvl / 2);
      view_.set_top_line(top);
    }
    ed.curli = word.line;
    ed.cx = word.x;
    ed.cy = word.line - top;
    if (scroll) {
      ed.invalidate_to_eof(top);
    } else {
      if (previous_line != word.line && previous_line < size_int(ed)) {
        ed.invalidate_range(previous_line, previous_line);
      }
      ed.invalidate_range(word.line, word.end_line);
    }
    // Update the bottom bar cache first so that idle processing in bgetch
    // does not draw over the suggestions.
    view_.draw_bottom_bar(ed);
    draw_suggestions(suggestions);
    highlight(ed, word, top);
    prompt();
  }

  void hide_word(FsedModel& ed, const spell_word_t& word) override {
    if (word.line < size_int(ed)) {
      ed.invalidate_range(word.line, std::min<int>(word.end_line, size_int(ed) - 1));
    }
    view_.fs().DrawBottomBar("");
  }

  int getkey(FsedModel& ed) override { return view_.bgetch(ed); }

  std::string input_replacement(FsedModel&, const std::string& word) override {
    auto& fs = view_.fs();
    fs.PutsCommandLine("|#9Replace with: |#0");
    const auto max_len = std::max<int>(size_int(word), 40);
    return StringTrim(fs.in().input_text(word, max_len));
  }

  void message(FsedModel&, const std::string& text) override {
    auto& fs = view_.fs();
    fs.PutsCommandLine(StrCat(text, " |#9(press any key)"));
    fs.bgetch();
  }

  void after_edit(FsedModel& ed, int) override { ed.invalidate_to_eof(view_.top_line()); }

  [[nodiscard]] bool hangup() const override { return ctx_.session_context().hangup(); }

private:
  void draw_suggestions(const std::vector<std::string>& suggestions) {
    auto& fs = view_.fs();
    // DrawBottomBar right aligns the text 10 columns from the right edge.
    const auto max_len = std::max<int>(10, fs.screen_width() - 14);
    std::string text;
    auto n = 1;
    for (const auto& s : suggestions) {
      auto item = fmt::format("{}){}", n++, s);
      const auto sep = text.empty() ? 0 : 1;
      if (size_int(text) + sep + size_int(item) > max_len) {
        break;
      }
      if (sep) {
        text.push_back(' ');
      }
      text.append(item);
    }
    if (text.empty()) {
      text = "No suggestions";
    }
    fs.DrawBottomBar(text);
  }

  void highlight(const FsedModel& ed, const spell_word_t& word, int top) {
    auto& fs = view_.fs();
    auto& out = fs.out();
    const auto saved_color = out.curatr();
    for (auto l = word.line; l <= word.end_line && l < size_int(ed); l++) {
      const auto y = l - top + fs.lines_start();
      if (y >= fs.lines_end()) {
        break;
      }
      const auto& cells = ed.line(l).cells();
      const auto x0 = l == word.line ? word.x : 0;
      const auto x1 =
          std::min<int>(l == word.end_line ? word.end_x : size_int(cells), size_int(cells));
      out.goxy(x0 + 1, y);
      out.setc(SPELL_HIGHLIGHT_COLOR);
      for (auto x = x0; x < x1; x++) {
        out.outchr(cells.at(x).ch);
      }
      if (l != word.line) {
        // Only the current line is drawn cell by cell. The continuation line
        // of a character wrapped word was drawn with its pipe codes turned
        // into colors, so the columns after the word may not line up with
        // the cells. Draw the rest of it cell by cell too.
        auto last_color = -1;
        for (auto x = x1; x < size_int(cells); x++) {
          if (cells.at(x).wwiv_color != last_color) {
            last_color = cells.at(x).wwiv_color;
            out.ansic(last_color);
          }
          out.outchr(cells.at(x).ch);
        }
      }
    }
    out.setc(saved_color);
  }

  void prompt() {
    auto& fs = view_.fs();
    fs.PutsCommandLine(StrCat("|#9", spell_prompt_for_width(fs.screen_width())));
  }

  Context& ctx_;
  FsedView& view_;
};

} // namespace

static void spell_message(FsedView& view, const std::string& text) {
  auto& fs = view.fs();
  fs.PutsCommandLine(StrCat(text, " |#9(press any key)"));
  fs.bgetch();
  view.ClearCommandLine();
}

bool fsed_spell_check(Context& ctx, FsedModel& ed, FsedView& view, spell_check_trigger_t trigger) {
  const auto from_menu = trigger == spell_check_trigger_t::menu;
  auto& config = ctx.config();
  const auto& board = config.spell();
  if (!board.enabled) {
    if (from_menu) {
      spell_message(view, "|#6Spell check is not enabled on this BBS.");
    }
    return true;
  }
  const auto datadir = config.datadir();
  const auto usernum = ctx.session_context().user_num();
  const auto user = load_spell_user_config(datadir, usernum);
  const auto& menu_set_dictionary = ctx.session_context().current_menu_set().menu_set.dictionary;
  const auto settings = effective_spell_settings(board, menu_set_dictionary, user);
  if (!from_menu && !settings.check_on_save) {
    return true;
  }
  if (!spell_check_compiled_in()) {
    if (from_menu) {
      spell_message(view, "|#6Spell check is not available on this BBS.");
    }
    return true;
  }
  auto dict = create_spell_dictionary(
      settings.dictionary,
      spell_dictionary_search_path(settings.dict_dir, config.root_directory()));
  if (!dict || !dict->available()) {
    LOG(WARNING) << "Spell check dictionary not found: " << settings.dictionary;
    if (from_menu) {
      spell_message(view, fmt::format("|#6Spell check is not available (no dictionary {}).",
                                      settings.dictionary));
    }
    return true;
  }

  SpellWordList board_words(spell_board_words_path(datadir));
  if (board_words.Load()) {
    for (const auto& w : board_words.words()) {
      dict->add(w);
    }
  }
  SpellWordList user_words(spell_user_words_path(datadir, usernum));
  if (usernum > 0 && user_words.Load()) {
    for (const auto& w : user_words.words()) {
      dict->add(w);
    }
  }

  const auto saved_curli = ed.curli;
  const auto saved_cx = ed.cx;
  const auto saved_top = view.top_line();

  FsedSpellCheckIO io(ctx, view);
  const auto stats = spell_check_walk(ed, *dict, io, [&](const std::string& word) {
    if (usernum > 0 && user_words.add(word) && !user_words.Save()) {
      const auto u8 = user_words.path().u8string();
      LOG(WARNING) << "Unable to save spell check words: " << std::string(u8.begin(), u8.end());
    }
  });

  // Put the cursor back where it was (the text may have moved).
  ed.curli = std::min<int>(saved_curli, std::max<int>(0, size_int(ed) - 1));
  ed.cx = std::min<int>(saved_cx, size_int(ed.curline()));
  const auto mvl = std::max<int>(1, view.max_view_lines());
  auto top = saved_top;
  if (ed.curli < top || ed.curli >= top + mvl) {
    top = std::max<int>(0, ed.curli - mvl / 2);
  }
  view.set_top_line(top);
  ed.cy = ed.curli - top;
  view.redraw(ed);
  ed.invalidate_to_eof(0);

  switch (stats.result) {
  case spell_check_result_t::cancel:
    return false;
  case spell_check_result_t::clean:
    if (from_menu) {
      spell_message(view, "|#5No spelling errors found.");
    }
    break;
  case spell_check_result_t::finished:
    if (from_menu) {
      spell_message(view, "|#5Spell check complete.");
    }
    break;
  case spell_check_result_t::unavailable:
  case spell_check_result_t::quit:
  case spell_check_result_t::hangup:
    break;
  }
  return true;
}

} // namespace wwiv::fsed
