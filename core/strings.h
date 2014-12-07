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
#ifndef __INCLUDED_STRINGS_H__
#define __INCLUDED_STRINGS_H__

#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

extern const char *DELIMS_WHITE;

#ifdef _WIN32

#define vsnprintf _vsnprintf
#define snprintf _snprintf

#endif   // _WIN32


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

std::string StringPrintf(const char *pszFormattedText, ...);
int GetStringLength(const char* pszString);
bool IsEquals(const char *pszString1, const char *pszString2);
bool IsEqualsIgnoreCase(const char *pszString1, const char *pszString2);
int  StringCompareIgnoreCase(const char *pszString1, const char *pszString2);
int  StringCompare(const char *pszString1, const char *pszString2);

int16_t StringToShort(const std::string& s);
uint16_t StringToUnsignedShort(const std::string& s);
char StringToChar(const std::string& s);
uint8_t StringToUnsignedChar(const std::string& s);

const std::string& StringReplace(std::string* orig, const std::string old_string, const std::string new_string);
std::vector<std::string> SplitString(const std::string& original_string, const std::string& delims);
void SplitString(const std::string& original_string, const std::string& delims, std::vector<std::string>* out);

void RemoveWhitespace(std::string* s);
bool starts_with(const std::string& input, const std::string& match);
bool ends_with(const std::string& input, const std::string& match);

void StringJustify(std::string* s, int length, char bg, JustificationType just_type);

}  // namespace strings

template<class T>
const T UpperCase(const T a) {
  int nRet = ::toupper(a);
  return static_cast<T>(nRet);
}

template<class T>
const T LowerCase(const T a) {
  int nRet = ::tolower(a);
  return static_cast<T>(nRet);
}

}  // namespace wwiv

// Function Prototypes
const char *charstr(int nStringLength, int chRepeatChar);
void StringTrimEnd(char *pszString);
char *stripcolors(const char *pszOrig);
std::string stripcolors(const std::string& orig);
unsigned char upcase(unsigned char ch);
unsigned char locase(unsigned char ch);
void StringTrim(char *pszString);
void StringTrim(std::string* s);
void StringTrimEnd(std::string* s);
void StringTrimBegin(std::string* s);
std::string StringUpperCase(std::string* s);
std::string StringLowerCase(std::string* s);
char *stristr(char *pszString, char *pszPattern);
char *StringRemoveWhitespace(char *str);
char *StringRemoveChar(const char *pszString, char chCharacterToRemove);

void properize(char *pszText);
std::string properize(const std::string text);

#ifdef _WIN32
#define strcasecmp( a, b ) _stricmp( a, b )
#define strncasecmp( a, b, c) _strnicmp( a, b, c )

#else  // _WIN32
char *strupr(char *s);
char *strlwr(char *s);
char * strrev(char *s);

#endif  // _WIN32

#endif  // __INCLUDED_STRINGS_H__
