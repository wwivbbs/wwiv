/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5                              */
/*             Copyright (C)2018-2021, WWIV Software Services             */
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
#include "sdk/msgapi/parsed_message.h"

#include "core/stl.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace wwiv::sdk::msgapi {

using namespace wwiv::stl;
using namespace wwiv::strings;

constexpr char CZ = 26;

static std::vector<std::string> split_wwiv_style_message_text(const std::string& s) {
  auto temp(s);
  const auto cz_pos = temp.find(CZ);
  if (cz_pos != std::string::npos) {
    // We stop the message at control-Z if it exists.
    temp = temp.substr(0, cz_pos);
  }
  // Instead of splitting on \r\n, we remove the \n and then
  // split on just \r.  This also is great that it handles
  // the cases where we end in only \r properly.
  temp.erase(std::remove(temp.begin(), temp.end(), 10), temp.end());
  // Use SplitString(..., false) so we don't skip blank lines.
  auto orig =  SplitString(temp, "\r", false);

  std::vector<std::string> out;
  std::string current_line;
  for (auto& l : orig) {
    if (l.empty()) {
      out.emplace_back("");
      continue;
    }
    if (l.back() == 0x01) { // ^A
      l.pop_back();
      current_line.append(l);
      continue;
    }
    if (current_line.empty()) {
      out.emplace_back(l);
    } else {
      out.emplace_back(current_line.append(l));
      current_line.clear();
    }
  }
  return out;
}

ParsedMessageText::ParsedMessageText(std::string control_char, const std::string& text,
                                     const splitfn& split_fn, std::string eol)
    : control_char_(std::move(control_char)),
      split_fn_(split_fn), eol_(std::move(eol)) {
  if (text.empty()) {
    return;
  }
  if (text.back() == CZ) {
    auto t = text;
    t.pop_back();
    lines_ = split_fn(t);
  } else {
    lines_ = split_fn(text);
  }
  // TODO, lines needs to be a structure that includes metadata
  // like  centered line, or soft-wrapped line.  WWIV and
  // FTN have different ways of soft-wrapping and need
  // to handle both.
}

ParsedMessageText::~ParsedMessageText() = default;

bool ParsedMessageText::add_control_line_after(const std::string& near_line,
                                               const std::string& line) {
  for (auto it = std::begin(lines_); it != std::end(lines_); ) {
    auto l = *it;
    if (!l.empty() && starts_with(l, control_char_)) {
      l = l.substr(control_char_.size());
      if (l.find(near_line) != std::string::npos) {
        // current item has it.
        if (it == lines_.end()) {
          // at the end of the list, add to the end.
          lines_.push_back(StrCat(control_char_, line));
        } else {
          // not at the end of the list, add it *after* the current item.
          ++it;
          lines_.insert(it, StrCat(control_char_, line));
        }
        return true;
      }
    }
    ++it;
  }
  return false;
}

bool ParsedMessageText::add_control_line(const std::string& line) {
  auto it = lines_.begin();
  auto found_control_line = false;
  while (it != lines_.end()) {
    auto l = *it;
    if (l.empty()) {
      ++it;
      continue;
    }
    if (starts_with(l, control_char_)) {
      found_control_line = true;
    } else if (found_control_line) {
      // We've seen control lines before, and now we don't.
      // Insert here so we're at the end of the control lines.
      lines_.insert(it, StrCat(control_char_, line));
      return true;
    }
    ++it;
  }
  lines_.push_back(StrCat(control_char_, line));
  return true;
}

bool ParsedMessageText::remove_control_line(const std::string& start_of_line) {
  for (auto it = std::begin(lines_); it != std::end(lines_);) {
    auto l = *it;
    if (!l.empty() && starts_with(l, control_char_)) {
      l = l.substr(control_char_.size());
      if (l.find(start_of_line) != std::string::npos) {
        it = lines_.erase(it);
        return true;
      }
    }
    ++it;
  }
  return false;
}


std::string ParsedMessageText::to_string() const {
  return JoinStrings(lines_, eol_) + static_cast<char>(CZ);
}

static bool has_quote(const std::string& text) {
  if (const auto quote_end = text.find_first_of(">)]");
      quote_end != std::string::npos && quote_end < 10) {
    return true;
  }
  return false;
}

std::vector<std::string>
ParsedMessageText::to_lines(const parsed_message_lines_style_t& style) const {
  std::vector<std::string> out;
  for (auto l : lines_) {
    if (l.empty()) {
      out.emplace_back("");
      continue;
    }
    const auto is_control_line = starts_with(l, control_char_);
    if (!is_control_line) {
      std::string last_line;
      do {
        if (l == last_line) {
          // We have a loop!
          VLOG(1) << "LOOP on line: '" << l << "'";
          out.push_back(l.substr(0, style.line_length));
          break;
        }
        last_line = l;
        const auto size_wc = size_without_colors(l);
        if (size_wc <= style.line_length) {
          if (style.reattribute_quotes && has_quote(l)) {
            // If we already have quote line here and we are reattributing them,
            // then clean up any space in front of it.
            StringTrimBegin(&l);
          }
          out.push_back(l);
          break;
        }
        // We have a long line
        auto pos = style.line_length;
        while (pos > 0 && l[pos] > 32) {
          pos--;
        }
        if (pos == 0) {
          pos = style.line_length;
        }
        if (pos < 16) {
          // We have nothing useful here.  Just dump what we can of L
          VLOG(1) << "LOOP (<16) on line: '" << l << "'";
          out.push_back(l.substr(0, style.line_length));
          break;
        }
        auto subset_of_l = l.substr(0, pos);
        l = l.substr(pos);
        if (!l.empty() && l.front() == ' ') {
          // Trim any leading space char that may have been wrapped from last line.
          l = l.substr(1);
        }
        if (style.reattribute_quotes) {
          // Here we try to re-attribute quote by looking for quote markers
          // and adding them to the wrapped lines.
          if (auto quote_end = subset_of_l.find_first_of(">)]");
              quote_end != std::string::npos && quote_end < 10) {
            ++quote_end;
            auto possible_quote = subset_of_l.substr(0, quote_end);
            if (const auto possible_quote_start = possible_quote.find_last_of(' ');
                possible_quote_start != std::string::npos) {
              possible_quote = possible_quote.substr(possible_quote_start + 1);
              // Trim trailing space on quote.
              if (possible_quote.back() == ' ') {
                possible_quote.pop_back();
              }
            }
            l = StrCat(possible_quote, " ", l);
            // Remove any space from before the non-wrapped line so it matches.
            StringTrimBegin(&subset_of_l);
          }
        }
        if (style.add_wrapping_marker) {
          // A ^A at the end of the line means it was soft wrapped.
          out.push_back(fmt::sprintf("%s%c", subset_of_l, 0x01));
        } else {
          out.push_back(subset_of_l);
        }
      } while (true);
      continue;
    }
    switch (style.ctrl_lines) {
    case control_lines_t::control_lines:
      out.push_back(l);
      break;
    case control_lines_t::control_lines_masked: {
      auto s{l};
      out.push_back(StringReplace(&s, control_char_, "@"));
    } break;
    case control_lines_t::no_control_lines:
      if (!is_control_line) {
        out.push_back(l);
      }
      break;
    default:
      out.push_back(l);
    }
  }
  return out;
}

WWIVParsedMessageText::WWIVParsedMessageText(const std::string& text)
    : ParsedMessageText("\004"
                        "0",
                        text, split_wwiv_style_message_text,
                        "\r\n") {}
  
WWIVParsedMessageText::~WWIVParsedMessageText() = default;

} // namespace wwiv
