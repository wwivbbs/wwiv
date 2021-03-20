/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services             */
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
#include "core/strings.h"

#include "stl.h"
#include "core/log.h"
#include "fmt/format.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>


const unsigned char* translate_letters[] = {
    reinterpret_cast<const unsigned char *>("abcdefghijklmnopqrstuvwxyz�������"),
    reinterpret_cast<const unsigned char *>("ABCDEFGHIJKLMNOPQRSTUVWXYZ�������"),
    nullptr,
};
const char* DELIMS_WHITE = " \t\r\n";
const char* DELIMS_CRLF = "\r\n";

namespace wwiv::strings {

/**
 * Compares the strings
 * @param str1 string to compare
 * @param str2 other string to compare
 * @return true of the strings contain the same contents
 */
bool IsEquals(const char* str1, const char* str2) {
  return StringCompare(str1, str2) == 0;
}

/**
 * Compares the strings, ignoring case
 * @param str1 string to compare
 * @param str2 other string to compare
 * @return true of the strings contain the same contents ignoring case
 */
bool iequals(const char* str1, const char* str2) {
  CHECK(str1 != nullptr);
  CHECK(str2 != nullptr);

  return StringCompareIgnoreCase(str1, str2) == 0;
}

bool iequals(const std::string& s1, const std::string& s2) {
  return s1.size() == s2.size() &&
         std::equal(s1.begin(), s1.end(), s2.begin(),
                    [](const char& c1, const char& c2)
                    {
                      return (std::tolower(c1) == std::tolower(c2));
                    });
}

int StringCompareIgnoreCase(const char* str1, const char* str2) {
  CHECK(str1 != nullptr);
  CHECK(str2 != nullptr);

  return strcasecmp(str1, str2);
}

int StringCompare(const char* str1, const char* str2) {
  CHECK(str1 != nullptr);
  CHECK(str2 != nullptr);

  return strcmp(str1, str2);
}

const std::string& StringReplace(std::string* orig, const std::string& old_string,
                                 const std::string& new_string) {
  auto pos = orig->find(old_string, 0);
  while (pos != std::string::npos) {
    orig->replace(pos, old_string.length(), new_string);
    pos = orig->find(old_string, pos + new_string.length());
  }
  return *orig;
}

std::vector<std::string> SplitString(const std::string& original_string, const std::string& delims) {
  return SplitString(original_string, delims, true);
}

std::vector<std::string> SplitString(const std::string& original_string, const std::string& delims, bool skip_empty) {
  std::vector<std::string> v;
  SplitString(original_string, delims, skip_empty, &v);
  return v;
}

void SplitString(const std::string& original_string, const std::string& delims, std::vector<std::string>* out) {
  SplitString(original_string, delims, true, out);
}

void SplitString(const std::string& original_string, const std::string& delims, bool skip_empty,
                 std::vector<std::string>* out) {
  auto s(original_string);
  for (auto found = s.find_first_of(delims); found != std::string::npos;
       s = s.substr(found + 1), found = s.find_first_of(delims)) {
    if (found > 0) {
      out->push_back(s.substr(0, found));
    } else if (!skip_empty && found == 0) {
      // Add empty lines.
      out->push_back({});
    }
  }
  if (!s.empty()) {
    out->push_back(s);
  }
}

std::tuple<std::string, std::string> SplitOnce(const std::string& original_string, const std::string& delims) {
  if (const auto idx = original_string.find_first_of(delims); idx != std::string::npos) {
    return std::make_tuple(original_string.substr(0, idx), original_string.substr(idx + 1));
  }
  return std::make_tuple(original_string, "");
}

std::tuple<std::string, std::string> SplitOnceLast(const std::string& original_string,
    const std::string& delims) {
  if (const auto idx = original_string.find_last_of(delims); idx != std::string::npos) {
    return std::make_tuple(original_string.substr(0, idx), original_string.substr(idx + 1));
  }
  return std::make_tuple(original_string, "");
}


bool starts_with(const std::string& input, const std::string& match) {
  return input.size() >= match.size()
         && std::equal(std::begin(match), std::end(match), std::begin(input));
}

bool ends_with(const std::string& input, const std::string& match) {
  return input.size() >= match.size()
         && std::equal(match.rbegin(), match.rend(), input.rbegin());
}

/**
 * Returns a string justified and padded with "bg".
 *
 * @param s The text to justify
 * @param length the length of the text
 * @param bg the character to use as the background
 * @param just_type one of the following: LEFT, RIGHT
 */
void StringJustify(std::string* s, int length, char bg, JustificationType just_type) {
  if (ssize(*s) > length) {
    *s = s->substr(0, length);
    return;
  }
  if (ssize(*s) == length) {
    return;
  }

  const auto delta = length - s->size();
  switch (just_type) {
  case JustificationType::LEFT: {
    s->resize(length, bg);
  }
  break;
  case JustificationType::RIGHT: {
    const auto tmp(*s);
    *s = StrCat(std::string(delta, bg), tmp);
  }
  break;
  }
}


/**
 * Removes spaces from the beginning and the end of the string s.
 *
 * @param str the string from which to remove spaces
 */
void StringTrim(char* str) {
  std::string s(str);
  StringTrim(&s);
  strcpy(str, s.c_str());
}

/**
* Removes spaces from the beginning and the end of the string s.
*
* @param s the string from which to remove spaces
*/
void StringTrim(std::string* s) {
  auto pos = s->find_first_not_of(DELIMS_WHITE);
  s->erase(0, pos);

  pos = s->find_last_not_of(DELIMS_WHITE);
  s->erase(pos + 1);
}

/**
* Removes CF and LF from the beginning and the end of the string s.
*
* @param s the string from which to remove spaces
*/
void StringTrimCRLF(std::string* s) {
  auto pos = s->find_first_not_of(DELIMS_CRLF);
  s->erase(0, pos);

  pos = s->find_last_not_of(DELIMS_CRLF);
  s->erase(pos + 1);
}

/**
* Removes spaces from the beginning and the end of the string s and
* returns it as a new string
*
* @param orig the string from which to remove spaces
* @return orig with spaces removed.
*/
std::string StringTrim(const std::string& orig) {
  if (orig.empty()) {
    return {};
  }
  auto s(orig);
  auto pos = s.find_first_not_of(DELIMS_WHITE);
  s.erase(0, pos);

  pos = s.find_last_not_of(DELIMS_WHITE);
  s.erase(pos + 1);
  return s;
}

void StringTrimBegin(std::string* s) {
  const auto pos = s->find_first_not_of(DELIMS_WHITE);
  s->erase(0, pos);
}

void StringTrimEnd(std::string* s) {
  const auto pos = s->find_last_not_of(DELIMS_WHITE);
  s->erase(pos + 1);
}

/**
 * Removes the whitespace from the end of the string
 * @param str The string from which to remove the trailing whitespace
 */
void StringTrimEnd(char* str) {
  std::string s(str);
  StringTrimEnd(&s);
  strcpy(str, s.c_str());
}

void StringUpperCase(std::string* s) {
  std::transform(std::begin(*s), std::end(*s), std::begin(*s), to_upper_case<char>);
}

std::string ToStringUpperCase(const std::string& orig) {
  auto s(orig);
  StringUpperCase(&s);
  return s;
}

static char tolower_char(int c) {
  return static_cast<char>(tolower(c));
}

void StringLowerCase(std::string* s) {
  std::transform(std::begin(*s), std::end(*s), std::begin(*s), tolower_char);
}

std::string ToStringLowerCase(const std::string& orig) {
  std::string s(orig);
  StringLowerCase(&s);
  return s;
}

char* StringRemoveChar(const char* str, char ch) {
  static char s_strip_string[255];

  CHECK(str != nullptr);
  strcpy(s_strip_string, "");

  int i1 = 0;
  for (size_t i = 0; i < strlen(str); i++) {
    if (ch != str[i]) {
      s_strip_string[i1] = str[i];
      i1++;
    } else {
      s_strip_string[i1] = '\0';
      break;
    }
  }

  //if last char is a space, remove it too.
  if (s_strip_string[i1 - 1] == ' ') {
    i1--;
  }
  s_strip_string[i1] = '\0';
  return s_strip_string;
}

std::string JoinStrings(const std::vector<std::string>& lines, const std::string& end_of_line) {
  std::string out;
  for (const auto& line : lines) {
    out += line;
    out += end_of_line;
  }
  return out;
}

int size_without_colors(const std::string& s) {
  const auto stripped = stripcolors(s);
  return stl::size_int(stripped);
}

std::string trim_to_size_ignore_colors(const std::string& orig, int size) {
  auto s{orig};
  while (size_without_colors(s) > size) {
    s.pop_back();
  }
  return s;
}

std::string pad_to_ignore_colors(const std::string& orig, int size) {
  const auto len = size_without_colors(orig);
  if (size <= len) {
    return orig;
  }
  return orig + std::string(size - len, ' ');
}

std::string::size_type size(const std::string& s) {
  return s.size();
}

std::string::size_type size(const char* s) {
  if (s == nullptr) {
    return 0;
  }
  return static_cast<std::string::size_type>(strlen(s));
}

// String length without colors as an int. size_int for compatibility
// with stl::size_int
int ssize(const char* s) { return static_cast<int>(size(s)); }
int size_int(const char* s) { return static_cast<int>(size(s)); }

// String length without colors as an int
int ssize(const unsigned char* s) {
  return static_cast<int>(size(reinterpret_cast<const char*>(s)));
}

int ssize(const std::string& s) {
  return static_cast<int>(s.size());
}

std::string trim_to_size(const std::string& orig, int max_size) {
  auto s(orig);
  while (ssize(s) > max_size) {
    s.pop_back();
  }
  return s;
}

} // namespace wwiv

/**
 * Is the character c a possible color code. (is it #, B, or a digit)
 * @param c The Character to test.
 */
static bool IsColorCode(char c) {
  if (!c) {
    return false;
  }
  return c == '#' || isdigit(c);
}

bool wwiv::strings::contains(const std::string& haystack, const std::string_view& needle) noexcept {
  try {
    return haystack.find(needle) != std::string::npos;
  } catch (...) {
    DLOG(FATAL) << "Caught exeption in wwiv::strings::contains: '"
                <<  haystack << "' : '" << needle << "'";
    return false;
  }
}

char* stripcolors(const char* str) {
  CHECK(str);
  static char s[255];
  const auto result = stripcolors(std::string(str));
  strcpy(s, result.c_str());
  return s;
}

template <typename I>
static bool is_ansi_seq_start(I& i, const std::string& orig) {
  auto left = std::string(i, end(orig));
  if (left.size() < 3) {
    return false;
  }
  if (left.at(1) != '[') {
    return false;
  }
  return left.find('m') != std::string::npos;
}

/**
 * Removes the WWIV color codes and pipe codes from the string
 *
 * @param orig The text from which to remove the color codes.
 * @return A new string without the color codes
 */
std::string stripcolors(const std::string& orig) {
  std::string out;
  for (auto i = begin(orig); i != end(orig); ++i) {
    if (*i == 27 && (i + 1) != end(orig)) {
      auto left = std::string(i, end(orig));
      if (!is_ansi_seq_start(i, orig)) {
        out.push_back(*i);
        continue;
      }
      // skip everything until we have the end of the ansi sequence.
      while (i != end(orig) && !std::isalpha(*i)) {
        ++i;
      }
      if (i != end(orig)) {
        ++i;
      }
    }
    if (i == end(orig)) {
      break;
    }
    if ((i + 1) != end(orig)
        && (i + 2) != end(orig)
        && *i == '|'
        && IsColorCode(*(i + 1))
        && IsColorCode(*(i + 2))) {
      ++i;
      ++i;
    } else if ((i + 1) != end(orig)
               && *i == 3
               && isdigit(*(i + 1))) {
      ++i;
    } else {
      out.push_back(*i);
    }
  }
  return out;
}

/**
 * Translates the character ch into uppercase using WWIV's translation tables
 * @param ch The character to translate
 * @return The uppercase version of the character
 */
unsigned char upcase(unsigned char ch) {
  if (const auto * ss = reinterpret_cast<const unsigned char*>(strchr(reinterpret_cast<const char*>(translate_letters[0]), ch)); ss) {
    ch = translate_letters[1][ss - translate_letters[0]];
  }
  return ch;
}

/**
 * Translates the character ch into lower using WWIV's translation tables
 * @param ch The character to translate
 * @return The lowercase version of the character
 */
unsigned char locase(unsigned char ch) {
  if (auto * ss = (unsigned char*)strchr((const char*)translate_letters[1], ch); ss) {
    ch = translate_letters[0][ss - translate_letters[1]];
  }
  return ch;
}

void properize(char* text) {
  if (text == nullptr) {
    return;
  }

  for (auto i = 0; i < wwiv::strings::ssize(text); i++) {
    if ((i == 0) || ((i > 0) && ((text[i - 1] == ' ') || (text[i - 1] == '-') ||
                                 (text[i - 1] == '.')))) {
      text[i] = wwiv::strings::to_upper_case<char>(text[i]);
    } else {
      text[i] = wwiv::strings::to_lower_case(text[i]);
    }
  }
}

std::string properize(const std::string& text) {
  if (text.empty()) {
    return std::string("");
  }

  char last = ' ';
  std::ostringstream os;
  for (auto ch : text) {
    if (last == ' ' || last == '-' || last == '.') {
      os << wwiv::strings::to_upper_case<char>(ch);
    } else {
      os << wwiv::strings::to_lower_case<char>(ch);
    }
    last = ch;
  }
  return os.str();
}

bool ifind_first(const std::string& haystack, const std::string& needle) {
  const auto it =
      std::search(std::begin(haystack), std::end(haystack), std::begin(needle), std::end(needle),
                  [](char a, char b) { return std::toupper(a) == std::toupper(b); });
  return it != std::end(haystack);
}

#ifdef _WIN32
// from strcasestr
char* strcasestr_i(const char* haystack, const char* needle);

char* strcasestr(const char* haystack, const char* needle) {
  if (strlen(needle) == 0) {
    // unlike strstr() and wcsstr() passing an empty string results in NULL being returned.
    // See http://msdn.microsoft.com/en-us/library/windows/desktop/bb773439%28v=vs.85%29.aspx
    return const_cast<char*>(haystack);
  }
  return strcasestr_i(haystack, needle);
}
#else

/** Converts string to uppercase */
char *strupr(char *s) {
  for (int i = 0; s[i] != 0; i++) {
    s[i] = wwiv::strings::to_upper_case<char>(s[i]);
  }

  return s;
}

// Reverses a string
char *strrev(char *s) {
  CHECK(s != nullptr);
  char szTempBuffer[255];
  auto str = static_cast<int>(strlen(s));
  CHECK_LE(str, 255);

  for (int i = str; i > - 1; i--) {
    s[i] = szTempBuffer[str - i];
  }
  strcpy(s, szTempBuffer);
  return s;
}

#endif  // _WIN32
