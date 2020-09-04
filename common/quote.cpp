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
#include "common/quote.h"

#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/bbsutl2.h"
#include "common/com.h"
#include "common/input.h"
#include "common/message_file.h"
#include "common/printfile.h"
#include "core/datetime.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "fmt/printf.h"
#include "local_io/keycodes.h"
#include "sdk/filenames.h"
#include "sdk/msgapi/parsed_message.h"
#include <deque>
#include <memory>
#include <string>
#include <vector>

using std::string;
using std::unique_ptr;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

static std::unique_ptr<std::vector<std::string>> quotes_ind;

static string FirstLettersOfVectorAsString(const vector<string>& parts) {
  string result;
  for (const auto& part : parts) {
    result.push_back(part.front());
  }
  return result;
}

string GetQuoteInitials(const string& orig_name) {
  if (orig_name.empty()) {
    return {};
  }
  auto name = orig_name;
  if (starts_with(name, "``")) {
    name = name.substr(2);
  }

  const auto paren_start = name.find('(');
  if (paren_start != name.npos && !isdigit(name.at(paren_start + 1))) {
    const auto inner = name.substr(paren_start + 1);
    return GetQuoteInitials(inner);
  }

  const auto last = name.find_first_of("#<>()[]`");
  const auto parts =
      last != name.npos ? SplitString(name.substr(0, last), " ") : SplitString(name, " ");
  return FirstLettersOfVectorAsString(parts);
}

void clear_quotes() {
  File::Remove(FilePath(a()->temp_directory(), QUOTES_TXT), true);

  quotes_ind.reset();
}

static string to_quote_date_format(time_t t) {
  const auto dt = DateTime::from_time_t(t);
  std::ostringstream ss;
  ss << dt.to_string("%A,%B %d, %Y") << " at ";
  if (a()->user()->IsUse24HourClock()) {
    ss << dt.to_string("%H:%M");
  } else {
    ss << dt.to_string("%I:%M %p");
  }
  return ss.str();
}

static std::string to_quote_date_line(quote_date_format_t type, time_t tt, const string& tb) {
  const auto datetime = to_quote_date_format(tt);
  std::string date_line;
  switch (type) {
  case quote_date_format_t::generic:
    date_line = fmt::sprintf("%c3On %c1%s, %c2%s%c3 wrote:%c0", 0x03, 0x03, datetime, 0x03, tb,
                             0x03, 0x03);
    break;
  case quote_date_format_t::email:
    date_line = fmt::sprintf("%c3In your e-mail of %c2%s%c3, you wrote:%c0", 0x03, 0x03, datetime,
                             0x03, 0x03);
    break;
  case quote_date_format_t::post:
    date_line = fmt::sprintf("%c3In a message posted %c2%s%c3, you wrote:%c0", 0x03, 0x03,
                             datetime, 0x03, 0x03);
    break;
  case quote_date_format_t::forward:
    date_line = fmt::sprintf("%c3Message forwarded from %c2%s%c3, sent on %s.%c0", 0x03, 0x03, tb,
                             0x03, datetime, 0x03);
    break;
  case quote_date_format_t::no_quote:
  default:
    return {};
  }
  date_line.append("\r\n");
  return date_line;
}

static std::vector<std::string> create_quoted_text_from_message(std::string& raw_text,
                                                                const std::string& to_name,
                                                                quote_date_format_t type,
                                                                time_t tt) {
  const msgapi::WWIVParsedMessageText pmt(raw_text);
  msgapi::parsed_message_lines_style_t style{};
  style.line_length = 72;
  style.ctrl_lines = msgapi::control_lines_t::no_control_lines;
  style.add_wrapping_marker = false;

  auto lines = pmt.to_lines(style);
  auto it = std::begin(lines);
  const auto end = std::end(lines);

  if (lines.size() < 2) {
    return {};
  }
  const auto to_node = *it++;
  ++it;

  const auto quote_initials = GetQuoteInitials(to_name);
  std::vector<std::string> out;
  if (type != quote_date_format_t::no_quote) {
    out.emplace_back(to_quote_date_line(type, tt, properize(strip_to_node(to_node))));
  }

  for(; it != end; ++it) {
    auto line = *it;
    StringReplace(&line, "\x03""0", "\x03""5");
    out.emplace_back(fmt::sprintf("%c%c%s%c%c> %c%c%s%c0",
                     0x03, '1', quote_initials, 0x03, '7', 0x03, '5', line, 0x03));
  }
  return out;
}

void auto_quote(std::string& raw_text, const std::string& to_name, quote_date_format_t type,
                time_t tt) {
  const auto fn = FilePath(a()->temp_directory(), INPUT_MSG);
  File::Remove(fn);
  if (a()->context().hangup()) {
    return ;
  }

  TextFile f(fn, "w");
  if (!f) {
    return;
  }
  auto lines = create_quoted_text_from_message(raw_text, to_name, type, tt);
  for (const auto& l : lines) {
    f.WriteLine(l);
  }
  if (a()->user()->GetNumMessagesPosted() < 10) {
    printfile(QUOTE_NOEXT);
  }
}

void grab_quotes(std::string& raw_text, const std::string& to_name) {
  if (raw_text.back() == CZ) {
    // Since CZ isn't special on Win32/Linux. Don't write it out
    // to the quotes file.
    raw_text.pop_back();
  }

  clear_quotes();
  File f(FilePath(a()->temp_directory(), QUOTES_TXT));
  if (f.Open(File::modeDefault | File::modeCreateFile | File::modeTruncate, File::shareDenyNone)) {
    f.Write(raw_text);
  }

  quotes_ind = std::make_unique<std::vector<std::string>>(
      create_quoted_text_from_message(raw_text, to_name, quote_date_format_t::no_quote, 0));
}

void grab_quotes(messagerec* m, const std::string& message_filename, const std::string& to_name) {
  if (auto o = readfile(m, message_filename)) {
    grab_quotes(o.value(), to_name);
  }
}


std::vector<std::string> query_quote_lines() {
  std::vector<std::string> lines;

  if (!quotes_ind || quotes_ind->empty()) {
    return {};
  }
  int start_line;
  int end_line;
  auto num_lines = 0;
  do {
    start_line = 0;
    end_line = 0;
    auto iter = std::begin(*quotes_ind);
    auto end = std::end(*quotes_ind);
    num_lines = 1;
    auto abort = false;
    for (; iter != end; ++iter) {
      // Skip control line (^D)
      auto& line = *iter;
      if (!line.empty() && line.front() == 0x04) {
        continue;
      }
      StringTrimCRLF(&line);
      bout.bpla(fmt::format("{:>3} {}", num_lines++, line), &abort);
      if (abort) {
        break;
      }
      // Add line s to the list of lines.
      lines.emplace_back(line);
    };
    --num_lines;
    bout.nl();

    if (lines.empty() || a()->context().hangup()) {
      return {};
    }
    bout.format("|#2Quote from line 1-{}? (?=relist, Q=quit) ", num_lines);
    auto k = input_number_hotkey(1, {'Q','?'}, 1, num_lines);
    
    if (k.key == 'Q') {
      return {};
    }
    if (k.key == '?') {
      bout.nl();
      continue;
    }
    start_line = k.num;
    if (start_line == num_lines) {
      end_line = start_line;
    } else {
      bout.format("|#2through line {} - {} ? (Q=quit) ", start_line, num_lines);
      k = input_number_hotkey(start_line, {'Q','?'}, start_line, num_lines);
      if (k.key == 'Q') {
        return {};
      }
      if (k.key == '?') {
        bout.nl();
        continue;
      }
      end_line = k.num;
    }

    if (start_line == end_line) {
      bout << "|#5Quote line " << start_line << "? ";
    } else {
      bout << "|#5Quote lines " << start_line << "-" << end_line << "? ";
    }
    if (!noyes()) {
      return {};
    }
    break;
  } while (!a()->context().hangup());
  return std::vector<std::string>(std::begin(lines) + start_line - 1,
                                  std::begin(lines) + end_line);
}
