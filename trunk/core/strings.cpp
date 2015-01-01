/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2015, WWIV Software Services             */
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

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/wwivassert.h"
#include "core/wwivport.h"

#ifdef _WIN32
#define NOMINMAX
#include "Shlwapi.h"
#pragma comment(lib, "Shlwapi.lib")
#undef StrCat
#endif  // _WIN32


using std::numeric_limits;
using std::stoi;
using std::string;
using std::out_of_range;

unsigned char *translate_letters[] = {
  (unsigned char *)"abcdefghijklmnopqrstuvwxyzáÑÜÇÅî§",
  (unsigned char *)"ABCDEFGHIJKLMNOPQRSTUVWXYZÄéèêöô•",
  0L,
};
const char *DELIMS_WHITE = " \t\r\n";

bool IsColorCode(char c);

using std::ostringstream;
using std::string;
using std::stringstream;
using std::vector;

namespace wwiv {
namespace strings {

/**
 * sprintf type function for STL string classes.
 * @param pszFormattedText The format specifier
 * @param ... Variable arguments
 */
string StringPrintf(const char *pszFormattedText, ...) {
  char szBuffer[ 1024 ];

  va_list ap;
  va_start(ap, pszFormattedText);
  vsnprintf(szBuffer, sizeof(szBuffer), pszFormattedText, ap);
  va_end(ap);
  return string(szBuffer);
}

/**
 * Gets the length of the C style string.  This function returns an int
 * instead of a size_t, so using this function can avoid warnings of
 * signed vs. unsigned comparasons.
 *
 * @param pszString the C style string
 * @return The length as an integer
 */
int GetStringLength(const char* pszString) {
  WWIV_ASSERT(pszString);
  return static_cast<int>(strlen(pszString));
}

/**
 * Compares the strings
 * @param pszString1 string to compare
 * @param pszString1 other string to compare
 * @return true of the strings contain the same contents
 */
bool IsEquals(const char* pszString1, const char* pszString2) {
  WWIV_ASSERT(pszString1);
  WWIV_ASSERT(pszString2);

  return (strcmp(pszString1, pszString2) == 0) ? true : false;
}

/**
 * Compares the strings, ignoring case
 * @param pszString1 string to compare
 * @param pszString1 other string to compare
 * @return true of the strings contain the same contents ignoring case
 */
bool IsEqualsIgnoreCase(const char *pszString1, const char *pszString2) {
  WWIV_ASSERT(pszString1);
  WWIV_ASSERT(pszString2);

  return (StringCompareIgnoreCase(pszString1, pszString2) == 0) ? true : false;
}

int StringCompareIgnoreCase(const char *pszString1, const char *pszString2) {
  WWIV_ASSERT(pszString1);
  WWIV_ASSERT(pszString2);

  return strcasecmp(pszString1, pszString2);
}

int StringCompare(const char *pszString1, const char *pszString2) {
  WWIV_ASSERT(pszString1);
  WWIV_ASSERT(pszString2);

  return strcmp(pszString1, pszString2);
}

template <typename T, typename R>
static T StringToT(std::function<R(const string&)> f, const string& s) {
  try {
    R ret = f(s);
    if (ret > numeric_limits<T>::max()) {
      return numeric_limits<T>::max();
    }
    if (ret < numeric_limits<T>::min()) {
      return numeric_limits<T>::min();
    }
    return static_cast<T>(ret);
  } catch (std::logic_error) {
    // Handle invalid_argument and out_of_range.
    return 0;
  }
}

int16_t StringToShort(const string& s) {
  return StringToT<int16_t, int>(
      [](const string& s) { return std::stoi(s); }, s);
}

uint16_t StringToUnsignedShort(const string& s) {
  return StringToT<uint16_t, int>(
      [](const string& s) { return std::stoul(s); }, s);
}

char StringToChar(const string& s) {
  return StringToT<char, int>(
      [](const string& s) { return std::stoi(s); }, s);
}

uint8_t StringToUnsignedChar(const string& s) {
  return StringToT<uint8_t, int>(
      [](const string& s) { return std::stoul(s); }, s);
}

const string& StringReplace(string* orig, const string& old_string, const string& new_string) {
  string::size_type pos = orig->find(old_string, 0);
  while (pos != string::npos) {
    orig->replace(pos, old_string.length(), new_string);
    pos = orig->find(old_string, pos + new_string.length());
  }
  return *orig;
}

vector<string> SplitString(const string& original_string, const string& delims) {
  vector<string> v;
  SplitString(original_string, delims, &v);
  return v;
}

void SplitString(const string& original_string, const string& delims, vector<string>* out) {
  string s(original_string);
  for (string::size_type found = s.find_first_of(delims); found != string::npos; s = s.substr(found + 1), found = s.find_first_of(delims)) {
    if (found) {
      out->push_back(s.substr(0, found));
    }
  }
  if (!s.empty()){
    out->push_back(s);
  }
}

void RemoveWhitespace(string* s) {
  s->erase(std::remove_if(std::begin(*s), std::end(*s), ::isspace), std::end(*s));
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
 * @param pszString The text to justify
 * @param nLength the length of the text
 * @param bg the character to use as the background
 * @param nJustificationType one of the following:
 *      LEFT
 *      RIGHT
 * @return the justified text.
 */
void StringJustify(string* s, string::size_type length, char bg, JustificationType just_type) {
  if (s->size() > length) {
    *s = s->substr(0, length);
    return;
  } else if (s->size() == length) {
    return;
  }

  string::size_type delta = length - s->size();
  switch (just_type) {
  case JustificationType::LEFT: {
    s->resize(length, bg);
  } break;
  case JustificationType::RIGHT: {
    string tmp(*s);
    *s = StrCat(string(delta, bg), tmp);
  } break;
  }
}


/**
 * Removes spaces from the beginning and the end of the string s.
 * @param s the string from which to remove spaces
 * @return s with spaces removed.
 */
void StringTrim(char *pszString) {
  string s(pszString);
  StringTrim(&s);
  strcpy(pszString, s.c_str());
}

/**
 * Removes spaces from the beginning and the end of the string s.
 * @param s the string from which to remove spaces
 * @return s with spaces removed.
 */
void StringTrim(string* s) {
  string::size_type pos = s->find_first_not_of(DELIMS_WHITE);
  s->erase(0, pos);

  pos = s->find_last_not_of(DELIMS_WHITE);
  s->erase(pos + 1);
}

void StringTrimBegin(string* s) {
  string::size_type pos = s->find_first_not_of(DELIMS_WHITE);
  s->erase(0, pos);
}

void StringTrimEnd(string* s) {
  string::size_type pos = s->find_last_not_of(DELIMS_WHITE);
  s->erase(pos + 1);
}

/**
 * Removes the whitespace from the end of the string
 * @param pszString The string from which to remove the trailing whitespace
 */
void StringTrimEnd(char *pszString) {
  string s(pszString);
  StringTrimEnd(&s);
  strcpy(pszString, s.c_str());
}

void StringUpperCase(string* s) {
  std::transform(std::begin(*s), std::end(*s), std::begin(*s), (int(*)(int)) toupper);
}

void StringLowerCase(string* s) {
  std::transform(std::begin(*s), std::end(*s), std::begin(*s), (int(*)(int)) tolower);
}

/**
 * Returns string comprised of char chRepeatChar, nStringLength characters in length
 * @param nStringLength The number of characters to create the string
 * @param chRepeatChar The character to repeat.
 * @return The string containing rc repeated len times.
 */
const char *charstr(string::size_type length, int fill) {
  static string result;
  result = string(std::min<int>(length, 160), fill);
  return result.c_str();
}


char *StringRemoveWhitespace(char *str) {
  WWIV_ASSERT(str);

  if (str) {
    char *obuf, *nbuf;
    for (obuf = str, nbuf = str; *obuf; ++obuf) {
      if (!isspace(*obuf)) {
        *nbuf++ = *obuf;
      }
    }
    *nbuf = '\0';
  }
  return str;
}

char *StringRemoveChar(const char *pszString, char ch) {
  static char s_strip_string[ 255 ];

  WWIV_ASSERT(pszString);
  strcpy(s_strip_string, "");

  int i1 = 0;
  for (int i = 0; i < wwiv::strings::GetStringLength(pszString); i++) {
    if (pszString[i] != ch) {
      s_strip_string[i1] = pszString[i];
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


}  // namespace strings
}  // namespace wwiv


/**
 * Is the character c a possible color code. (is it #, B, or a digit)
 * @param c The Character to test.
 */
bool IsColorCode(char c) {
  if (!c) {
    return false;
  }
  return (c == '#' || c == 'B' || isdigit(c));
}

char *stripcolors(const char *pszOrig) {
  WWIV_ASSERT(pszOrig);
  static char szNewString[255];
  const string result = stripcolors(string(pszOrig));
  strcpy(szNewString, result.c_str());
  return szNewString;
}

/**
 * Removes the WWIV color codes and pipe codes from the string
 *
 * @param pszOrig The text from which to remove the color codes.
 * @return A new string without the color codes
 */
string stripcolors(const string& orig) {
  string out;
  for (auto i = begin(orig); i != end(orig); i++) {
    if (*i == '|' && (i + 1) != end(orig) && (i + 2) != end(orig) && IsColorCode(*(i + 1)) && IsColorCode(*(i + 2))) {
      ++i;
      ++i;
    } else if (*i == 3 && i + 1 < end(orig) && isdigit(*(i + 1))) {
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
  unsigned char *ss = (unsigned char*) strchr((const char*) translate_letters[0], ch);
  if (ss) {
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
  unsigned char *ss = (unsigned char*)  strchr((const char*) translate_letters[1], ch);
  if (ss) {
    ch = translate_letters[0][ss - translate_letters[1]];
  }
  return ch;
}

void properize(char *pszText) {
  if (pszText == nullptr) {
    return;
  }

  for (int i = 0; i < wwiv::strings::GetStringLength(pszText); i++) {
    if ((i == 0) || ((i > 0) && ((pszText[i - 1] == ' ') || (pszText[i - 1] == '-') ||
                                 (pszText[i - 1] == '.')))) {
      pszText[i] = wwiv::UpperCase<char>(pszText[i]);
    } else {
      pszText[i] = wwiv::LowerCase(pszText[i]);
    }
  }
}

string properize(const string& text) {
  if (text.empty()) {
    return string("");
  }

  char last = ' ';
  ostringstream os;
  for (auto ch : text) {
    if (last == ' ' || last == '-' || last == '.') {
      os << wwiv::UpperCase<char>(ch);
    } else {
      os << wwiv::LowerCase<char>(ch);
    }
    last = ch;
  }
  return os.str();
}

#ifdef _WIN32
char *strcasestr(const char *haystack, const char *needle) {
  if (strlen(needle) == 0) {
    // unlike strstr() and wcsstr() passing an emtpy string results in NULL being returned.
    // See http://msdn.microsoft.com/en-us/library/windows/desktop/bb773439%28v=vs.85%29.aspx
    return const_cast<char*>(haystack);
  }
  return StrStrI(haystack, needle);
}
#else

/** Converts string to uppercase */
char *strupr(char *s) {
  for (int i = 0; s[i] != 0; i++) {
    s[i] = wwiv::UpperCase<char>(s[i]);
  }

  return s;
}

/** Converts string to lowercase */
char *strlwr(char *s) {
  for (int i = 0; s[i] != 0; i++) {
    s[i] = wwiv::LowerCase(s[i]);
  }

  return s;
}

// Reverses a string
char *strrev(char *pszBufer) {
  WWIV_ASSERT(pszBufer);
  char szTempBuffer[255];
  int str = strlen(pszBufer);
  WWIV_ASSERT(str <= 255);

  for (int i = str; i > - 1; i--) {
    pszBufer[i] = szTempBuffer[str - i];
  }
  strcpy(pszBufer, szTempBuffer);
  return pszBufer;
}

#endif  // _WIN32
