/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2007, WWIV Software Services             */
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

#if !defined ( __INCLUDED_WStringUtils_H__ )
#define __INCLUDED_WStringUtils_H__

#if defined( _MSC_VER ) && !defined( _CRT_SECURE_NO_DEPRECATE )
#define _CRT_SECURE_NO_DEPRECATE
#endif	// _MSC_VER 


#include <string>

namespace wwiv
{
	namespace stringUtils
	{
		std::string::size_type FormatString( std::string& str, const char *pszFormattedText, ... );
        int GetStringLength( const char * pszString );
		bool IsEquals( const char *pszString1, const char *pszString2 );
		bool IsEqualsIgnoreCase( const char *pszString1, const char *pszString2 );
        int  StringCompareIgnoreCase( const char *pszString1, const char *pszString2 );
        int  StringCompare( const char *pszString1, const char *pszString2 );
        short StringToShort( const char *pszString );
        unsigned short StringToUnsignedShort( const char *pszString );
        char StringToChar( const char *pszString );
        unsigned char StringToUnsignedChar( const char *pszString );
	}

	template<class _Ty>
		const _Ty UpperCase(const _Ty a)
	{
		int nRet = ::toupper( a );
		return static_cast< _Ty >( nRet );
	}


	template<class _Ty>
		const _Ty LowerCase(const _Ty a)
	{
		int nRet = ::tolower( a );
		return static_cast< _Ty >( nRet );
	}


}


// Function Prototypes
const char *charstr( int nStringLength, int chRepeatChar );
void StringTrimEnd( char *pszString );
std::string& StringTrim( std::string& s );
char *stripcolors( const char *pszOrig );
unsigned char upcase( unsigned char ch );
unsigned char locase( unsigned char ch );
char *StringJustify(char *pszString, int nLength, int bg, int nJustificationType);
char *StringTrim(char *pszString);
std::string& StringTrim( std::string& s );
std::string& StringTrimEnd( std::string& s );
std::string& StringTrimBegin( std::string& s );
char *stristr(char *pszString, char *pszPattern);
void single_space(char *pszText);
char *stptok(const char *pszText, char *pszToken, size_t nTokenLength, const char *brk);
char *StringRemoveWhitespace(char *str);
char *StringRemoveChar( const char *pszString, char chCharacterToRemove );
char *StringReplace(char *pszString, size_t nMaxBufferSize, char *pszOldString, char *pszNewString);
std::string& StringUpperCase( std::string& s );
std::string& StringLowerCase( std::string& s );

#if defined ( _WIN32 ) && ( _MSC_VER > 1310 )
#define WWIV_STRDUP( s ) _strdup( s )
#define WWIV_STRUPR( s ) _strupr( s )
#define WWIV_STRREV( s ) _strrev( s )
#define WWIV_STRLWR( s ) _strlwr( s )
#define WWIV_STRICMP( a, b ) _stricmp( a, b )
#define WWIV_STRNICMP( a, b, c) _strnicmp( a, b, c )

#elif defined( _WIN32 ) && ( _MSC_VER <= 1310 )
#define WWIV_STRDUP( s ) strdup( s )
#define WWIV_STRUPR( s ) strupr( s )
#define WWIV_STRREV( s ) strrev( s )
#define WWIV_STRLWR( s ) strlwr( s )
#define WWIV_STRICMP( a, b ) stricmp( a, b )
#define WWIV_STRNICMP( a, b, c) strnicmp( a, b, c )

#else
#define WWIV_STRDUP( s ) strdup( s )
#define WWIV_STRUPR( s ) strupr( s )
#define WWIV_STRREV( s ) strrev( s )
#define WWIV_STRLWR( s ) strlwr( s )
#define WWIV_STRICMP( a, b ) strcasecmp( a, b )
#define WWIV_STRNICMP( a, b, c) strncasecmp( a, b, c )
#endif 


#endif // __INCLUDED_WStringUtils_H__

