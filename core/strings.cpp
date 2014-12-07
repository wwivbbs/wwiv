/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2014, WWIV Software Services             */
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

const string& StringReplace(string* orig, const string old_string, const string new_string) {
  string::size_type pos = orig->find(old_string, 0);
  while (pos != string::npos) {
    orig->replace(pos, old_string.length(),  new_string);
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
  s->erase(std::remove_if(s->begin(), s->end(), ::isspace), s->end());
}

bool starts_with(const std::string& input, const std::string& match) {
  return input.size() >= match.size()
      && std::equal(match.begin(), match.end(), input.begin());
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
void StringJustify(string* s, int length, char bg, JustificationType just_type) {
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

}  // namespace strings
}  // namespace wwiv

/**
 * Returns string comprised of char chRepeatChar, nStringLength characters in length
 * @param nStringLength The number of characters to create the string
 * @param chRepeatChar The character to repeat.
 * @return The string containing rc repeated len times.
 */
const char *charstr(int nStringLength, int chRepeatChar) {
  static char szTempBuffer[ 255 ];

  if (chRepeatChar == 0 || nStringLength < 1) {
    return "";
  }

  nStringLength = std::min<int>(nStringLength, 160);

  memset(szTempBuffer, chRepeatChar, nStringLength);
  szTempBuffer[ nStringLength ] = '\0';
  return szTempBuffer;
}


/**
 * Removes the whitespace from the end of the string
 * @param pszString The string from which to remove the trailing whitespace
 */
void StringTrimEnd(char *pszString) {
  WWIV_ASSERT(pszString);
  string::size_type i = strlen(pszString);
  while ((i > 0) && (pszString[i - 1] == 32)) {
    WWIV_ASSERT(i > 0);
    --i;
  }
  WWIV_ASSERT(i >= 0);
  pszString[i] = '\0';
}


/**
 * Is the character c a possible color code. (is it #, B, or a digit)
 * @param c The Character to test.
 */
bool IsColorCode(char c) {
  if (!c) {
    return false;
  }
  if (c == '#' || c == 'B' || isdigit(c)) {
    return true;
  }
  return false;
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
  ostringstream os;
  for (string::const_iterator i = orig.begin(); i != orig.end(); i++) {
    if (*i == '|' && (i + 1) != orig.end() && (i + 2) != orig.end() && IsColorCode(*(i + 1)) && IsColorCode(*(i + 2))) {
      ++i;
      ++i;
    } else if (*i == 3 && i + 1 < orig.end() && isdigit(*(i + 1))) {
      ++i;
    } else {
      os << *i;
    }
  }
  return string(os.str());
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


/**
 * Removes spaces from the beginning and the end of the string s.
 * @param s the string from which to remove spaces
 * @return s with spaces removed.
 */
char *StringTrim(char *pszString) {
  WWIV_ASSERT(pszString);

  // find real end of it
  int nStringLen = strlen(pszString);
  while ((nStringLen > 0) && isspace(static_cast<unsigned char>(pszString[nStringLen - 1]))) {
    --nStringLen;
  }

  // find real beginning
  int nStartPos = 0;
  while ((nStartPos < nStringLen) && isspace(static_cast<unsigned char>(pszString[nStartPos]))) {
    ++nStartPos;
  }

  // knock spaces off the length
  nStringLen -= nStartPos;

  // move over the desired subsection
  memmove(pszString, pszString + nStartPos, nStringLen);

  // ensure null-terminated
  pszString[ nStringLen ] = '\0';

  return pszString;
}


/**
 * Removes spaces from the beginning and the end of the string s.
 * @param s the string from which to remove spaces
 * @return s with spaces removed.
 */
string StringTrim(string* s) {
  string::size_type pos = s->find_first_not_of(DELIMS_WHITE);
  s->erase(0, pos);

  pos = s->find_last_not_of(DELIMS_WHITE);
  s->erase(pos + 1);

  return *s;
}

string StringTrimBegin(string* s) {
  string::size_type pos = s->find_first_not_of(DELIMS_WHITE);
  s->erase(0, pos);
  return *s;
}

string StringTrimEnd(string* s) {
  string::size_type pos = s->find_last_not_of(DELIMS_WHITE);
  s->erase(pos + 1);
  return *s;
}

string StringUpperCase(string* s) {
  std::transform(s->begin(), s->end(), s->begin(), (int(*)(int)) toupper);
  return *s;
}

string StringLowerCase(string* s) {
  std::transform(s->begin(), s->end(), s->begin(), (int(*)(int)) tolower);
  return *s;
}

/**
 * Returns a pointer to the 1st occurence of pszPattern inside of s1 in a case
 * insensitive manner
 *
 * @param pszString The whole string
 * @param pszPattern The string to search for
 *
 * @return pointer inside of pszString which contains pszPattern.
 */
char *stristr(char *pszString, char *pszPattern) {
  WWIV_ASSERT(pszString);
  WWIV_ASSERT(pszPattern);

  int len = strlen(pszPattern), pos = 0;

  while (pszString[pos]) {
    if (strncasecmp(pszString + pos, pszPattern, len) == 0) {
      return (pszString + pos);
    }
    ++pos;
  }
  return nullptr;
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

char *StringRemoveChar(const char *pszString, char chCharacterToRemove) {
  static char s_strip_string[ 255 ];

  WWIV_ASSERT(pszString);

  strcpy(s_strip_string, "");

  int i1  = 0;
  for (int i = 0; i < wwiv::strings::GetStringLength(pszString); i++) {
    if (pszString[i] != chCharacterToRemove) {
      s_strip_string[i1] = pszString[i];
      i1++;
    } else {
      s_strip_string[i1] = '\0';
      break;
    }
  }

  //if last char is a space, remove it too
  if (s_strip_string[i1 - 1] == ' ') {
    i1--;
  }

  s_strip_string[i1] = '\0';

  return static_cast< char * >(s_strip_string);
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

string properize(const string text) {
  if (text.empty()) {
    return string("");
  }

  char last = ' ';
  ostringstream os;
  for (string::const_iterator i = text.begin(); i != text.end(); i++) {
    if (last == ' ' || last == '-' || last == '.') {
      os << wwiv::UpperCase<char>(*i);
    } else {
      os << wwiv::LowerCase<char>(*i);
    }
    last = *i;
  }
  return string(os.str());
}

#ifndef _WIN32

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