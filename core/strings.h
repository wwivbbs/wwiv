/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2017, WWIV Software Services             */
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
#ifndef __INCLUDED_STRINGS_H__
#define __INCLUDED_STRINGS_H__

#include <cstdint>
#include <cstring> // strncpy
#include <ctime> // struct tm
#include <functional>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

namespace wwiv {
namespace strings {

enum class JustificationType {
  LEFT, RIGHT
};

template<typename A>
std::string StrCat(const A& a) {
  std::ostringstream ss;
  ss << a;
  return ss.str();
}

template<typename A, typename... Args>
std::string StrCat(const A& a, const Args&... args) {
  std::ostringstream ss;
  ss << a << StrCat(args...);
  return ss.str();
}

template <size_t SIZE>
bool to_char_array(char(&out)[SIZE], const std::string& s) {
#ifdef _MSC_VER
  strncpy_s(out, s.c_str(), SIZE);
#else
  strncpy(out, s.c_str(), SIZE);
#endif  // _WIN32
  out[SIZE - 1] = '\0';
  return s.size() <= SIZE;
}

std::string StringPrintf(const char *formatted_text, ...);
int GetStringLength(const char* str);

// Comparisons
bool IsEquals(const char *str1, const char *str2);
bool IsEqualsIgnoreCase(const char *str1, const char *str2);
bool iequals(const char* s1, const char* s2);
bool iequals(const std::string& s1, const std::string& s2);
int StringCompareIgnoreCase(const char *str1, const char *str2);
int StringCompare(const char *str1, const char *str2);

const std::string& StringReplace(std::string* orig, const std::string& old_string, const std::string& new_string);
std::vector<std::string> SplitString(const std::string& original_string, const std::string& delims);
std::vector<std::string> SplitString(const std::string& original_string, const std::string& delims, bool skip_empty);
void SplitString(const std::string& original_string, const std::string& delims, std::vector<std::string>* out);
void SplitString(const std::string& original_string, const std::string& delims, bool skip_empty, std::vector<std::string>* out);

bool starts_with(const std::string& input, const std::string& match);
bool ends_with(const std::string& input, const std::string& match);

void StringJustify(std::string* s, std::string::size_type length, char bg, JustificationType just_type);
void StringTrim(char *str);
void StringTrim(std::string* s);
void StringTrimCRLF(std::string* s);
void StringTrimEnd(std::string* s);
void StringTrimEnd(char *str);
void StringTrimBegin(std::string* s);
void StringUpperCase(std::string* s);
std::string ToStringUpperCase(const std::string& s);
void StringLowerCase(std::string* s);
std::string ToStringLowerCase(const std::string& s);
void StringRemoveWhitespace(std::string* s);


const char *charstr(std::string::size_type length, char fill);
char *StringRemoveWhitespace(char *str);
// Strips the string from the first occurence of ch
// Doesn't seem to be used anywhere. Maybe it should be removed.
char *StringRemoveChar(const char *str, char ch);

/**
 * Joints the strings in lines, using end_of_line in between each line.
 */
std::string JoinStrings(const std::vector<std::string>& lines, const std::string& end_of_line);

// Time and STL things to come in C++14 or GCC5

/** 
 * Like std::put_time.  GCC 4.x doesn't support it.
 */
std::string put_time(const struct tm *tm_info, const std::string& fmt_arg);

// String length without colors
std::string::size_type size_without_colors(const std::string& s);

/** returns a copy of orig trimmed to size, excluding colors. */
std::string trim_to_size_ignore_colors(const std::string& orig, std::string::size_type size);

/**
 * Returns orig padded to size, excluding color codes.
 */
std::string pad_to_ignore_colors(const std::string& orig, std::string::size_type size);

/** Typesafe version of toupper */
template<class T>
const T to_upper_case(const T a) {
  return static_cast<T>(::toupper(a));
}

/** Typesafe version of tolower */
template<class T>
const T to_lower_case(const T a) {
  return static_cast<T>(::tolower(a));
}

template <typename T, typename R>
static T StringToT(std::function<R(const std::string&, int)> f, const std::string& s, int b) {
  try {
    R ret = f(s, b);
    if (ret > std::numeric_limits<T>::max()) {
      return std::numeric_limits<T>::max();
    }
    if (ret < std::numeric_limits<T>::min()) {
      return std::numeric_limits<T>::min();
    }
    return static_cast<T>(ret);
  }
  catch (const std::logic_error&) {
    // Handle invalid_argument and out_of_range.
    return 0;
  }
}

template<typename T, typename std::enable_if<std::is_unsigned<T>::value, T>::type* = nullptr>
T to_number(const std::string& s, int b = 10) {
  return StringToT<T, unsigned long>(
    [](const std::string& s, int b) { return std::stoul(s, nullptr, b); }, s, b);
}

template<typename T, typename std::enable_if<std::is_signed<T>::value, T>::type* = nullptr>
T to_number(const std::string& s, int b = 10) {
  return StringToT<T, long>(
    [](const std::string& s, int b) { return std::stol(s, nullptr, b); }, s, b);
}


}  // namespace strings

}  // namespace wwiv

// Function Prototypes
char *stripcolors(const char *pszOrig);
std::string stripcolors(const std::string& orig);
unsigned char upcase(unsigned char ch);
unsigned char locase(unsigned char ch);

void properize(char *text);
std::string properize(const std::string& text);

extern const char *DELIMS_WHITE;

#ifdef _WIN32
#define vsnprintf _vsnprintf
#define snprintf _snprintf

#define strcasecmp( a, b ) _stricmp( a, b )
#define strncasecmp( a, b, c) _strnicmp( a, b, c )

char *strcasestr(const char *str, const char *pszPattern);

#else  // _WIN32
char *strupr(char *s);
char *strlwr(char *s);
char * strrev(char *s);

#endif  // _WIN32

#endif  // __INCLUDED_STRINGS_H__
