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

#include <string>
#include <vector>

#define JUSTIFY_LEFT   0
#define JUSTIFY_RIGHT  1
#define JUSTIFY_CENTER 2


namespace wwiv {
namespace strings {

std::string StringPrintf(const char *pszFormattedText, ...);
std::string StrCat(std::string s1, std::string s2);
std::string StrCat(std::string s1, std::string s2, std::string s3);
std::string StrCat(std::string s1, std::string s2, std::string s3, std::string s4);
std::string StrCat(std::string s1, std::string s2, std::string s3, std::string s4, std::string s5);
std::string StrCat(std::string s1, std::string s2, std::string s3, std::string s4, std::string s5, std::string s6);
int GetStringLength(const char* pszString);
bool IsEquals(const char *pszString1, const char *pszString2);
bool IsEqualsIgnoreCase(const char *pszString1, const char *pszString2);
int  StringCompareIgnoreCase(const char *pszString1, const char *pszString2);
int  StringCompare(const char *pszString1, const char *pszString2);
short StringToShort(const char *pszString);
unsigned short StringToUnsignedShort(const char *pszString);
char StringToChar(const char *pszString);
unsigned char StringToUnsignedChar(const char *pszString);
const std::string& StringReplace(std::string* orig, const std::string old_string, const std::string new_string);
std::vector<std::string> SplitString(const std::string& original_string, const std::string& delims);
void SplitString(const std::string& original_string, const std::string& delims, std::vector<std::string>* out);


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
char *StringJustify(char *pszString, int nLength, int bg, int nJustificationType);
char *StringTrim(char *pszString);
std::string StringTrim(std::string* s);
std::string StringTrimEnd(std::string* s);
std::string StringTrimBegin(std::string* s);
std::string StringUpperCase(std::string* s);
std::string StringLowerCase(std::string* s);
char *stristr(char *pszString, char *pszPattern);
void single_space(char *pszText);
char *StringRemoveWhitespace(char *str);
char *StringRemoveChar(const char *pszString, char chCharacterToRemove);

void properize(char *pszText);
std::string properize(const std::string text);

#ifdef _WIN32
#define strcasecmp( a, b ) stricmp( a, b )
#define strncasecmp( a, b, c) strnicmp( a, b, c )
#endif  // _WIN32

#endif  // __INCLUDED_STRINGS_H__
