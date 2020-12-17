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
#ifndef INCLUDED_CORE_STRINGS_H
#define INCLUDED_CORE_STRINGS_H

// ReSharper disable once CppUnusedIncludeDirective
#include <cstring> // strncpy
// ReSharper disable once CppUnusedIncludeDirective
#include <ctime>   // struct tm
#include <functional>
#include <limits>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#ifdef StrCat
#undef StrCat
#endif // StrCat

namespace wwiv::strings {

enum class JustificationType { LEFT, RIGHT };

template <typename A> std::string StrCat(const A& a) noexcept {
  try {
    std::ostringstream ss;
    ss << a;
    return ss.str();
  } catch (...) {
    return {};
  }
}

template <typename A, typename... Args> std::string StrCat(const A& a, const Args&... args) noexcept {
  try {
    std::ostringstream ss;
    ss << a << StrCat(args...);
    return ss.str();
  } catch (...) {
    return {};
  }
}

  /**
   * Safe string -> character array. Ensures the character
   * array is null-terminated.
   * NB: Will assert on Windows if out is too small. Will attempt
   * to use safe versions of the CRT as possible in the future.
   */
  template <size_t SIZE> bool to_char_array(char (&out)[SIZE], const std::string& s) noexcept {
#ifdef _MSC_VER
    strncpy_s(out, s.c_str(), SIZE);
#else
    strncpy(out, s.c_str(), SIZE);
#endif // _WIN32
    out[SIZE - 1] = '\0';
    return s.size() <= SIZE;
  }

  /**
   * Safe string -> character array.
   * ** NB: ** Does not ensure the character array is null-terminated.
   * Also won't assert on Windows if out is too small.
   */
  template <size_t SIZE> bool to_char_array_no_null(char (&out)[SIZE], const std::string& s) {
    strncpy(out, s.c_str(), SIZE);
    return s.size() <= SIZE;
  }

  /**
   * Safe string -> character array. Ensures the character
   * array is null-terminated.
   * NB: will trim the output if it's too long.
   */
  template <size_t SIZE> bool to_char_array_trim(char (&out)[SIZE], const std::string& s) noexcept {
    strncpy(out, s.c_str(), SIZE);
    out[SIZE - 1] = '\0';
    return s.size() <= SIZE;
  }

  // Comparisons
  [[nodiscard]] bool IsEquals(const char* str1, const char* str2);
  [[nodiscard]] bool iequals(const char* str1, const char* str2);
  [[nodiscard]] bool iequals(const std::string& s1, const std::string& s2);
  [[nodiscard]] int StringCompareIgnoreCase(const char* str1, const char* str2);
  [[nodiscard]] int StringCompare(const char* str1, const char* str2);

  const std::string& StringReplace(std::string* orig, const std::string& old_string,
                                   const std::string& new_string);
  std::vector<std::string> SplitString(const std::string& original_string,
                                       const std::string& delims);
  [[nodiscard]] std::vector<std::string> SplitString(const std::string& original_string,
                                       const std::string& delims, bool skip_empty);
  void SplitString(const std::string& original_string, const std::string& delims,
                   std::vector<std::string>* out);
  void SplitString(const std::string& original_string, const std::string& delims, bool skip_empty,
                   std::vector<std::string>* out);

  [[nodiscard]] bool starts_with(const std::string& input, const std::string& match);
  [[nodiscard]] bool ends_with(const std::string& input, const std::string& match);

  void StringJustify(std::string* s, int length, char bg,
                     JustificationType just_type);
  void StringTrim(char* str);
  void StringTrim(std::string* s);
  [[nodiscard]] std::string StringTrim(const std::string& orig);

  void StringTrimCRLF(std::string* s);
  void StringTrimEnd(std::string* s);
  void StringTrimEnd(char* str);
  void StringTrimBegin(std::string* s);
  void StringUpperCase(std::string* s);
  [[nodiscard]] std::string ToStringUpperCase(const std::string& s);
  void StringLowerCase(std::string* s);
  [[nodiscard]] std::string ToStringLowerCase(const std::string& s);

// Strips the string from the first occurrence of ch
  // Doesn't seem to be used anywhere. Maybe it should be removed.
  char* StringRemoveChar(const char* str, char ch);

  /**
   * Joints the strings in lines, using end_of_line in between each line.
   */
  [[nodiscard]] std::string JoinStrings(const std::vector<std::string>& lines, const std::string& end_of_line);

  // String length without colors
  [[nodiscard]] int size_without_colors(const std::string& s);

  /** returns a copy of orig trimmed to size, excluding colors. */
  [[nodiscard]] std::string trim_to_size_ignore_colors(const std::string& orig, int size);

  /**
   * Returns orig padded to size, excluding color codes.
   */
  [[nodiscard]] std::string pad_to_ignore_colors(const std::string& orig, int size);

  // String length
  [[nodiscard]] std::string::size_type size(const std::string& s);

  // String length
  [[nodiscard]] std::string::size_type size(const char* s);

  // String length as an int
  [[nodiscard]] int ssize(const char* s);

  // String length as an int
  [[nodiscard]] int ssize(const unsigned char* s);

  // String length as an int
  [[nodiscard]] int ssize(const std::string& s);

  /** returns a copy of orig trimmed to size, excluding colors. */
  std::string trim_to_size(const std::string& orig, int size);

  /** Type-safe version of toupper */
  template<class T, typename = std::enable_if_t<std::is_convertible_v<T, char>, char>>
  T to_upper_case(const T a) { return static_cast<T>(::toupper(a)); }

  template<class T>
  std::enable_if_t<std::is_convertible_v<T, char>, char>
  to_upper_case_char(const T a) { return static_cast<T>(::toupper(a)); }

  /** Type safe version of toupper */
  template<class T, typename = std::enable_if_t<std::is_convertible_v<T, char>, char>>
  T to_lower_case(const T a) { return static_cast<T>(::tolower(a)); }

  template<class T>
  std::enable_if_t<std::is_convertible_v<T, char>, char>
  to_lower_case_char(const T a) { return static_cast<T>(::tolower(a)); }

  template <typename T, typename std::enable_if<std::is_unsigned<T>::value, T>::type* = nullptr>
  T to_number(const std::string& s, int b = 10) {
    char* end;
    auto result = strtoul(s.c_str(), &end, b);
    if (errno == ERANGE) {
      return 0;
    }
    if (result > std::numeric_limits<T>::max()) {
      return std::numeric_limits<T>::max();
    }
    if (result < std::numeric_limits<T>::min()) {
      return std::numeric_limits<T>::min();
    }
    return static_cast<T>(result);
  }

  template <typename T, typename std::enable_if<std::is_signed<T>::value, T>::type* = nullptr>
  T to_number(const std::string& s, int b = 10) {
    char* end;
    auto result = strtol(s.c_str(), &end, b);
    if (errno == ERANGE) {
      return 0;
    }
    if (result > std::numeric_limits<T>::max()) {
      return std::numeric_limits<T>::max();
    }
    if (result < std::numeric_limits<T>::min()) {
      return std::numeric_limits<T>::min();
    }
    return static_cast<T>(result);
  }

  /**
   * Return true if haystack contains needed as a substring.
   *
   * Like boost::contains.
   * See http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2020/p1679r3.html
   * for proposal to add as std::string::contains.
   */
  bool contains(const std::string& haystack, const std::string_view& needle) noexcept;

  } // namespace wwiv

  // Function Prototypes
  [[nodiscard]] char* stripcolors(const char* str);
  [[nodiscard]] std::string stripcolors(const std::string& orig);
  [[nodiscard]] unsigned char upcase(unsigned char ch);
  [[nodiscard]] unsigned char locase(unsigned char ch);

  void properize(char* text);
  [[nodiscard]] std::string properize(const std::string& text);

  extern const char* DELIMS_WHITE;

  /** returns true if needle is found in haystack ,ignoring case */
  [[nodiscard]] bool ifind_first(const std::string& haystack, const std::string& needle);

#ifdef _WIN32

  #define strcasecmp(a, b) _stricmp(a, b)
  #define strncasecmp(a, b, c) _strnicmp(a, b, c)

  char* strcasestr(const char* haystack, const char* needle);

#else // _WIN32
  char* strupr(char* s);
  char* strrev(char* s);

#endif // _WIN32

#endif // __INCLUDED_STRINGS_H__
