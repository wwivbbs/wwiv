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
#include "fsed/spell_walk.h"

#include "core/stl.h"
#include "core/strings.h"
#include "fsed/line.h"
#include "fsed/model.h"
#include <algorithm>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace wwiv::fsed {

using namespace wwiv::stl;
using namespace wwiv::strings;

static constexpr int SPELL_ESC = 27;

// Soft wrapped: the next line continues this paragraph.
static bool is_soft_wrapped(const line_t& l) {
  return l.wrapped() || (!l.cells().empty() && l.cells().back().ch == '\x01');
}

std::string spell_prompt_for_width(int screen_width) {
  // Leave the last column empty so the terminal does not wrap.
  const auto max_len = std::max<int>(0, screen_width - 1);
  for (const auto* p : {SPELL_PROMPT, SPELL_PROMPT_NARROW, SPELL_PROMPT_SHORT}) {
    if (size_int(std::string(p)) <= max_len) {
      return p;
    }
  }
  std::string tiny = SPELL_PROMPT_TINY;
  if (size_int(tiny) > max_len) {
    tiny.resize(max_len);
  }
  return tiny;
}

std::optional<spell_pos_t> spell_replace_word(FsedModel& ed, const spell_word_t& word,
                                              const std::string& replacement) {
  const auto num_lines = size_int(ed);
  if (word.line < 0 || word.line >= num_lines) {
    return spell_pos_t{word.line, word.x};
  }
  // Find the paragraph.
  auto first = word.line;
  while (first > 0) {
    const auto& prev = ed.line(first - 1);
    if (prev.cells().empty() || !is_soft_wrapped(prev)) {
      break;
    }
    --first;
  }
  auto last = std::min<int>(std::max<int>(word.line, word.end_line), num_lines - 1);
  while (last + 1 < num_lines && is_soft_wrapped(ed.line(last))) {
    ++last;
  }
  const auto last_soft = is_soft_wrapped(ed.line(last));
  const auto last_color = ed.line(last).wwiv_color();

  // Join it into one run of cells (without the ^A soft wrap markers). Like
  // FsedModel's paragraph reflow and to_lines(false), put a space between
  // two lines when neither side of the break is whitespace, so a character
  // wrapped "hello" / "world" stays two words. spell_tokenize joins lines
  // the same way.
  std::vector<cell_t> cells;
  auto start = -1;
  auto end = -1;
  for (auto l = first; l <= last; l++) {
    auto lc = ed.line(l).cells();
    if (!lc.empty() && lc.back().ch == '\x01') {
      lc.pop_back();
    }
    if (!cells.empty() && !lc.empty()) {
      const auto a = cells.back().ch;
      const auto b = lc.front().ch;
      if (a != ' ' && a != '\t' && b != ' ' && b != '\t') {
        cells.emplace_back(cells.back().wwiv_color, ' ');
      }
    }
    if (l == word.line) {
      start = size_int(cells) + std::min<int>(word.x, size_int(lc));
    }
    if (l == word.end_line) {
      end = size_int(cells) + std::min<int>(word.end_x, size_int(lc));
    }
    cells.insert(cells.end(), lc.begin(), lc.end());
  }
  if (start < 0) {
    start = 0;
  }
  if (end < start) {
    end = start;
  }

  // Splice in the replacement using the color of the word.
  auto color = 0;
  if (start < size_int(cells)) {
    color = cells.at(start).wwiv_color;
  } else if (!cells.empty()) {
    color = cells.back().wwiv_color;
  }
  std::vector<cell_t> rep;
  for (const auto ch : replacement) {
    rep.emplace_back(color, ch);
  }
  cells.erase(cells.begin() + start, cells.begin() + end);
  cells.insert(cells.begin() + start, rep.begin(), rep.end());
  const auto resume = start + size_int(rep);

  // Rewrap the same way FsedModel::reflow_paragraph does: a line holds at
  // most max_line_len cells, breaking after the last space or tab that fits,
  // and a word with nowhere to break is split at max_line_len.
  const auto width = std::max<int>(1, ed.max_line_len());
  std::vector<std::pair<int, int>> chunks; // [begin, end)
  auto pos = 0;
  const auto total = size_int(cells);
  while (pos < total) {
    auto take = total - pos;
    if (take > width) {
      auto sp = -1;
      for (auto k = width - 1; k > 0; k--) {
        const auto ch = cells.at(pos + k).ch;
        if (ch == ' ' || ch == '\t') {
          sp = k;
          break;
        }
      }
      take = sp > 0 ? sp + 1 : width;
    }
    chunks.emplace_back(pos, pos + take);
    pos += take;
  }
  if (chunks.empty()) {
    chunks.emplace_back(0, 0);
  }

  // Like FsedModel::insert_line, never go past the line limit.
  const auto old_para_lines = last - first + 1;
  if (size_int(chunks) > old_para_lines &&
      num_lines - old_para_lines + size_int(chunks) > ed.maxli()) {
    return std::nullopt;
  }

  // Where to continue.
  spell_pos_t result{first, 0};
  for (auto i = 0; i < size_int(chunks); i++) {
    const auto [b, e] = chunks.at(i);
    if (resume < e || i + 1 == size_int(chunks)) {
      result = {first + i, resume - b};
      break;
    }
  }

  std::vector<line_t> lines;
  lines.reserve(num_lines + chunks.size());
  for (auto l = 0; l < first; l++) {
    lines.push_back(ed.line(l));
  }
  for (auto i = 0; i < size_int(chunks); i++) {
    const auto [b, e] = chunks.at(i);
    line_t nl;
    nl.assign(std::vector<cell_t>(cells.begin() + b, cells.begin() + e));
    const auto is_last = i + 1 == size_int(chunks);
    nl.wrapped(!is_last || last_soft);
    if (is_last) {
      // Keep the color picked for typing (Ctrl-P) at the end of the paragraph.
      nl.set_wwiv_color(last_color);
    }
    lines.push_back(nl);
  }
  for (auto l = last + 1; l < num_lines; l++) {
    lines.push_back(ed.line(l));
  }
  ed.set_lines(std::move(lines));

  // Keep the cursor on the text.
  ed.curli = std::min<int>(ed.curli, std::max<int>(0, size_int(ed) - 1));
  ed.cx = std::min<int>(ed.cx, size_int(ed.line(ed.curli)));
  return result;
}

// True if suggestion is word with exactly one pair of neighboring letters
// swapped, ignoring case. "The" is a transposition of "Teh".
static bool is_adjacent_transposition(const std::string& word, const std::string& suggestion) {
  if (word.size() != suggestion.size() || word.size() < 2) {
    return false;
  }
  const auto a = ToStringLowerCase(word);
  const auto b = ToStringLowerCase(suggestion);
  auto i = 0;
  const auto n = size_int(a);
  while (i < n && a[i] == b[i]) {
    ++i;
  }
  if (i + 1 >= n || a[i] != b[i + 1] || a[i + 1] != b[i]) {
    return false;
  }
  return a.compare(i + 2, std::string::npos, b, i + 2) == 0;
}

// 0: swapped-letter typo, 1: a single word, 2: a split like "T eh".
static int suggestion_rank(const std::string& word, const std::string& suggestion) {
  if (is_adjacent_transposition(word, suggestion)) {
    return 0;
  }
  if (suggestion.find(' ') != std::string::npos) {
    return 2;
  }
  return 1;
}

std::vector<std::string> spell_suggestions(const SpellDictionary& dict, const std::string& word) {
  std::vector<std::string> out;
  for (const auto& s : dict.suggest(spell_to_utf8(word))) {
    auto c = spell_from_utf8(s);
    if (c.empty() || c == word) {
      continue;
    }
    if (c.find('?') != std::string::npos && s.find('?') == std::string::npos) {
      // Can not be shown in CP437.
      continue;
    }
    if (std::find(out.begin(), out.end(), c) != out.end()) {
      continue;
    }
    out.push_back(c);
  }
  std::stable_sort(out.begin(), out.end(), [&](const std::string& a, const std::string& b) {
    return suggestion_rank(word, a) < suggestion_rank(word, b);
  });
  if (size_int(out) > SPELL_MAX_SUGGESTIONS) {
    out.resize(SPELL_MAX_SUGGESTIONS);
  }
  return out;
}

static bool before(const spell_word_t& w, const spell_pos_t& p) {
  return w.line < p.line || (w.line == p.line && w.x < p.x);
}

spell_check_stats_t spell_check_walk(FsedModel& ed, SpellDictionary& dict, SpellCheckIO& io,
                                     const spell_add_word_fn& on_add) {
  spell_check_stats_t stats{};
  if (!dict.available()) {
    stats.result = spell_check_result_t::unavailable;
    return stats;
  }
  std::set<std::string> ignore_all;
  spell_pos_t resume{0, 0};

  // Every word moves resume forward, but be safe.
  for (auto guard = 0; guard < 100000; guard++) {
    if (io.hangup()) {
      stats.result = spell_check_result_t::hangup;
      return stats;
    }
    std::optional<spell_word_t> next;
    for (const auto& w : spell_tokenize(ed)) {
      if (before(w, resume) || ignore_all.count(w.word) > 0) {
        continue;
      }
      if (!dict.check(spell_to_utf8(w.word))) {
        next = w;
        break;
      }
    }
    if (!next) {
      stats.result = stats.shown > 0 ? spell_check_result_t::finished : spell_check_result_t::clean;
      return stats;
    }
    const auto w = next.value();
    ++stats.shown;
    const auto suggestions = spell_suggestions(dict, w.word);
    io.show_word(ed, w, suggestions);
    const spell_pos_t after_word{w.end_line, w.end_x};

    // Returns false if there was no room for the replacement, in which case
    // the word is shown again.
    auto replace = [&](const std::string& r) {
      io.hide_word(ed, w);
      const auto pos = spell_replace_word(ed, w, r);
      if (!pos) {
        io.message(ed, "|#6No room in the message for that replacement.");
        io.show_word(ed, w, suggestions);
        return false;
      }
      resume = pos.value();
      ++stats.replaced;
      io.after_edit(ed, w.line);
      return true;
    };

    for (auto handled = false; !handled;) {
      if (io.hangup()) {
        io.hide_word(ed, w);
        stats.result = spell_check_result_t::hangup;
        return stats;
      }
      const auto key = io.getkey(ed);
      if (key == SPELL_ESC) {
        io.hide_word(ed, w);
        stats.result = spell_check_result_t::cancel;
        return stats;
      }
      if (key <= 0 || key > 255) {
        continue;
      }
      // ASCII only; std::toupper depends on the C locale for 0x80-0xFF.
      const auto ch = (key >= 'a' && key <= 'z') ? key - 'a' + 'A' : key;
      if (ch >= '1' && ch <= '9') {
        if (const auto n = ch - '1'; n < size_int(suggestions)) {
          handled = replace(suggestions.at(n));
        }
        continue;
      }
      switch (ch) {
      case 'R': {
        const auto r = io.input_replacement(ed, w.word);
        if (r.empty()) {
          // Nothing typed, ask again.
          io.show_word(ed, w, suggestions);
          continue;
        }
        handled = replace(r);
      } break;
      case 'I':
        io.hide_word(ed, w);
        resume = after_word;
        handled = true;
        break;
      case 'G':
        io.hide_word(ed, w);
        ignore_all.insert(w.word);
        resume = after_word;
        handled = true;
        break;
      case 'A': {
        io.hide_word(ed, w);
        const auto utf8 = spell_to_utf8(w.word);
        dict.add(utf8);
        if (on_add) {
          on_add(utf8);
        }
        // In case the dictionary could not add it.
        ignore_all.insert(w.word);
        ++stats.added;
        resume = after_word;
        handled = true;
      } break;
      case 'Q':
        io.hide_word(ed, w);
        stats.result = spell_check_result_t::quit;
        return stats;
      default:
        break;
      }
    }
  }
  stats.result = spell_check_result_t::finished;
  return stats;
}

} // namespace wwiv::fsed
