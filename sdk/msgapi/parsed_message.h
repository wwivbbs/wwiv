/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5                              */
/*             Copyright (C)2018-2022, WWIV Software Services             */
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
#ifndef __INCLUDED_SDK_PARSED_MESSAGE_H__
#define __INCLUDED_SDK_PARSED_MESSAGE_H__

#include <functional>
#include <string>
#include <vector>

namespace wwiv {
namespace sdk {
namespace msgapi {

/**
 * Specifies how to return the text with respect to control lines,
 * * control_lines: Return raw control lines
 * * control_lines_masked: Replace control line character with '@'
 * * no_control_lines: Skip lines containing control lines.
 */
enum class control_lines_t {
  control_lines,
  control_lines_masked,
  no_control_lines
};

/**
 * Controls the style of the text to be returned in ParsedMessageText::to_lines
 */
struct parsed_message_lines_style_t {
  // Controls the including of control lines and how to include.
  control_lines_t ctrl_lines;
  // Line length to wrap soft-wrapped lines
  int line_length{76};
  // Should the WWIV wrapping marker be added (^A) at the end of
  // a soft-wrapped line.
  bool add_wrapping_marker{true};
  // Attempt to add quote attribution lines to wrapped text of
  // existing quotes.
  bool reattribute_quotes{false};
};

/**
 * Represents a parsed (split into lines) message.  The format can either be in
 * WWIV native (control lines start with ^D0) or FTN (control lines start with ^A),
 * depending on the control_char specified.
 */
class ParsedMessageText {
public:
  typedef std::function<std::vector<std::string>(const std::string&)> splitfn;

  ParsedMessageText(std::string control_char, const std::string& text, const splitfn& s, std::string eol);
  virtual ~ParsedMessageText();
  bool add_control_line_after(const std::string& near_line, const std::string& line);
  bool add_control_line(const std::string& line);
  /** 
   * Removes a single control line starting with start_of_line 
   * returns true if a line was removed, false otherwise
   */
  bool remove_control_line(const std::string& start_of_line);
  [[nodiscard]] std::string to_string() const;
  [[nodiscard]] std::vector<std::string> to_lines(const parsed_message_lines_style_t& style) const;

private:
  const std::string control_char_;
  std::vector<std::string> lines_;
  splitfn split_fn_;
  const std::string eol_;
};

/**
 * Represents a parsed (split into lines) message  in WWIV format (control
 * lines start with ^D0) and end of line is "\r\n".
 */
class WWIVParsedMessageText final : public ParsedMessageText {
public:
  WWIVParsedMessageText(const std::string& text);
  ~WWIVParsedMessageText();
};

} // namespace msgapi
} // namespace sdk
} // namespace wwiv

#endif // __INCLUDED_SDK_PARSED_MESSAGE_H__
