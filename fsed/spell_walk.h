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
#ifndef INCLUDED_FSED_SPELL_WALK_H
#define INCLUDED_FSED_SPELL_WALK_H

#include "fsed/spell.h"
#include "fsed/spell_text.h"
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace wwiv::fsed {

class FsedModel;

/** The prompt shown for each misspelled word. */
inline constexpr const char* SPELL_PROMPT =
    "(1-9)=use suggestion  (R)eplace  (I)gnore  i(G)nore all  (A)dd to dictionary  (Q)uit";

/**
 * The same prompt with single spaces (79 columns) for screens that are too
 * narrow for SPELL_PROMPT (84 columns), i.e. 80 columns.
 */
inline constexpr const char* SPELL_PROMPT_NARROW =
    "(1-9)=use suggestion (R)eplace (I)gnore i(G)nore all (A)dd to dictionary (Q)uit";

/** A shorter prompt for screens narrower than SPELL_PROMPT_NARROW. */
inline constexpr const char* SPELL_PROMPT_SHORT = "1-9 (R)eplace (I)gnore i(G)nore all (A)dd (Q)uit";

/** The shortest prompt, for very narrow screens. */
inline constexpr const char* SPELL_PROMPT_TINY = "1-9 R I G A Q";

/**
 * The longest spell check prompt that fits on a screen screen_width columns
 * wide, leaving the last column empty so the terminal does not wrap. If none
 * fit, SPELL_PROMPT_TINY is cut to fit.
 */
[[nodiscard]] std::string spell_prompt_for_width(int screen_width);

/** Most suggestions offered for one word (keys 1-9). */
inline constexpr int SPELL_MAX_SUGGESTIONS = 9;

/** Position in the editor (line index, cell index). */
struct spell_pos_t {
  int line{0};
  int x{0};
};

/**
 * Replaces the cells of word with replacement (CP437) and re-wraps the
 * paragraph that contains it. The replacement uses the color of the first
 * character of the word.
 *
 * The paragraph is the run of lines joined by soft wraps (wrapped() or a
 * trailing ^A). It is rewrapped to ed.max_line_len() - 1 columns, breaking
 * after the last space that fits (or mid word if there is none). Lines
 * outside of the paragraph are not changed, but may move up or down.
 *
 * The line color (the color used for typing) of the last line of the
 * paragraph is kept.
 *
 * Returns the position just past the replacement, which is where checking
 * continues. Returns std::nullopt (and leaves ed unchanged) if the rewrapped
 * paragraph would need more lines than ed.maxli() allows.
 */
std::optional<spell_pos_t> spell_replace_word(FsedModel& ed, const spell_word_t& word,
                               const std::string& replacement);

/** How the walk through the misspelled words ended. */
enum class spell_check_result_t {
  // No dictionary (Hunspell not compiled in or the dictionary is missing).
  unavailable,
  // No misspelled words were found.
  clean,
  // Every misspelled word was handled.
  finished,
  // The user pressed Q.
  quit,
  // The user pressed ESC.
  cancel,
  // The caller hung up.
  hangup,
};

/** What happened during the walk. */
struct spell_check_stats_t {
  spell_check_result_t result{spell_check_result_t::clean};
  // Number of misspelled words shown to the user.
  int shown{0};
  // Number of words replaced.
  int replaced{0};
  // Number of words added to the dictionary.
  int added{0};
};

/**
 * What the walk needs from the screen. The FSED implementation draws on the
 * editor, the tests use a fake.
 */
class SpellCheckIO {
public:
  virtual ~SpellCheckIO() = default;
  /**
   * Shows the misspelled word (highlighted in the editor) and the
   * suggestions (CP437, at most SPELL_MAX_SUGGESTIONS) and the prompt.
   */
  virtual void show_word(FsedModel& ed, const spell_word_t& word,
                         const std::vector<std::string>& suggestions) = 0;
  /** Removes the highlight from the word. */
  virtual void hide_word(FsedModel& ed, const spell_word_t& word) = 0;
  /** Gets the next key. */
  virtual int getkey(FsedModel& ed) = 0;
  /** Asks for the replacement (CP437) for word. Empty means none. */
  virtual std::string input_replacement(FsedModel& ed, const std::string& word) = 0;
  /** Shows a message (WWIV pipe codes allowed) and waits for a key. */
  virtual void message(FsedModel& ed, const std::string& text) = 0;
  /** Called after the text changed (starting at first_line). */
  virtual void after_edit(FsedModel& ed, int first_line) = 0;
  /** True if the caller has hung up. */
  [[nodiscard]] virtual bool hangup() const = 0;
};

/**
 * Called when the user adds a word (UTF-8) to their dictionary so that it
 * can be saved to their personal word list.
 */
using spell_add_word_fn = std::function<void(const std::string& utf8_word)>;

/**
 * Walks through the misspelled words in ed from the top, asking the user
 * what to do with each one:
 *   1-9  replace with that suggestion
 *   R    replace with a word the user types
 *        (if the message has no room for the replacement, the user is told
 *        and asked again)
 *   I    ignore this one time
 *   G    ignore every time for the rest of this check
 *   A    add to the user's dictionary (and ignore)
 *   Q    stop checking
 *   ESC  stop checking and cancel (i.e. do not save)
 * Any other key asks again.
 */
spell_check_stats_t spell_check_walk(FsedModel& ed, SpellDictionary& dict, SpellCheckIO& io,
                                     const spell_add_word_fn& on_add = nullptr);

/**
 * The suggestions for word (CP437) converted to CP437, without duplicates
 * or ones that can not be shown in CP437, at most SPELL_MAX_SUGGESTIONS.
 * Suggestions that differ from word by two swapped neighboring letters
 * (ignoring case) are listed first, then other single words, then
 * suggestions that contain a space. Within each group the Hunspell
 * order is kept.
 */
[[nodiscard]] std::vector<std::string> spell_suggestions(const SpellDictionary& dict,
                                                         const std::string& word);

} // namespace wwiv::fsed

#endif
