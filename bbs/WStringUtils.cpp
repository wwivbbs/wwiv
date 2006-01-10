/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2004, WWIV Software Services             */
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

#include "WStringUtils.h"
#include <cstdio>
#include <string>
#include <iostream>
#include <algorithm>
#include <cstdarg>
#include "wwivassert.h"
#include "wtypes.h"
#include "WConstants.h"
#include "incl1.h"

#ifdef _WIN32
#define WWIV_STRICMP( a, b ) stricmp( a, b )
#define WWIV_STRNICMP( a, b, c) strnicmp( a, b, c )
#else
#define WWIV_STRICMP( a, b ) strcasecmp( a, b )
#define WWIV_STRNICMP( a, b, c) strncasecmp( a, b, c )
#endif

extern unsigned char *translate_letters[];


bool IsColorCode( char c );


namespace wwiv
{
	namespace stringUtils
	{
		/**
		 * sprintf type function for STL string classes.
		 * @param str The STL string
		 * @param pszFormattedText The format specifier
		 * @param ... Variable arguments
		 */
		std::string::size_type FormatString( std::string& str, const char *pszFormattedText, ... )
		{
			va_list ap;
			char szBuffer[ 1024 ];

			va_start( ap, pszFormattedText );
			vsnprintf( szBuffer, 1024, pszFormattedText, ap );
			va_end( ap );
			str = szBuffer;
			return str.length();
		}

		/**
		 * Gets the length of the C style string.  This function returns an int
		 * instead of a size_t, so using this function can avoid warnings of
		 * signed vs. unsigned comparasons.
		 *
		 * @param pszString the C style string
		 * @return The length as an integer
		 */
        int GetStringLength( const char * pszString )
        {
            WWIV_ASSERT( pszString );
            return static_cast<int>( strlen( pszString ) );
        }

		/**
		 * Compares the strings
		 * @param pszString1 string to compare
		 * @param pszString1 other string to compare
		 * @return true of the strings contain the same contents
		 */
		bool IsEquals( const char *pszString1, const char *pszString2 )
		{
			WWIV_ASSERT( pszString1 );
			WWIV_ASSERT( pszString2 );

			return ( strcmp( pszString1, pszString2 ) == 0 ) ? true : false;
		}

		/**
		 * Compares the strings, ignoring case
		 * @param pszString1 string to compare
		 * @param pszString1 other string to compare
		 * @return true of the strings contain the same contents ignoring case
		 */
		bool IsEqualsIgnoreCase( const char *pszString1, const char *pszString2 )
		{
			WWIV_ASSERT( pszString1 );
			WWIV_ASSERT( pszString2 );

			return ( WWIV_STRICMP( pszString1, pszString2 ) == 0 ) ? true : false;
		}


        int StringCompareIgnoreCase( const char *pszString1, const char *pszString2 )
        {
			WWIV_ASSERT( pszString1 );
			WWIV_ASSERT( pszString2 );

			return WWIV_STRICMP( pszString1, pszString2 );
        }


        int StringCompare( const char *pszString1, const char *pszString2 )
        {
			WWIV_ASSERT( pszString1 );
			WWIV_ASSERT( pszString2 );

			return strcmp( pszString1, pszString2 );
        }

        short StringToShort( const char *pszString )
        {
			WWIV_ASSERT( pszString );
            return static_cast<short>( atoi( pszString ) );
        }


        unsigned short StringToUnsignedShort( const char *pszString )
        {
			WWIV_ASSERT( pszString );
            return static_cast<unsigned short>( atoi( pszString ) );
        }


        char StringToChar( const char *pszString )
        {
			WWIV_ASSERT( pszString );
            return static_cast<char>( atoi( pszString ) );
        }


        unsigned char StringToUnsignedChar( const char *pszString )
        {
			WWIV_ASSERT( pszString );
            return static_cast<unsigned char>( atoi( pszString ) );
        }

	}
}


/**
 * Returns string comprised of char chRepeatChar, nStringLength characters in length
 * @param nStringLength The number of characters to create the string
 * @param chRepeatChar The character to repeat.
 * @return The string containing rc repeated len times.
 */
const char *charstr( int nStringLength, int chRepeatChar )
{
    static char szTempBuffer[ 255 ];

    if ( chRepeatChar == 0 || nStringLength < 1 )
    {
        return "";
    }

	nStringLength = std::min<int>( nStringLength, 160 );

    memset( szTempBuffer, chRepeatChar, nStringLength );
    szTempBuffer[ nStringLength ] = '\0';
    return szTempBuffer;
}


/**
 * Removes the whitespace from the end of the string
 * @param pszString The string from which to remove the trailing whitespace
 */
void StringTrimEnd( char *pszString )
{
	WWIV_ASSERT( pszString );
    int i = strlen( pszString );
    while ( ( i > 0 ) && ( pszString[i - 1] == SPACE ) )
    {
        WWIV_ASSERT( i > 0 );
        --i;
    }
    WWIV_ASSERT( i >= 0 );
    pszString[i] = '\0';
}


/**
 * Is the character c a possible color code. (is it #, B, or a digit)
 * @param c The Character to test.
 */
bool IsColorCode( char c )
{
    if ( !c )
    {
        return false;
    }
    if ( c == '#' || c == 'B' || isdigit( c ) )
    {
        return true;
    }
    return false;
}



// %%TODO: Can pszOrig be const char*?

/**
 * Removes the WWIV color codes and pipe codes from the string
 *
 * @param pszOrig The text from which to remove the color codes.
 * @return A new string without the color codes
 */
char *stripcolors( const char *pszOrig )
// Takes input string and returns same string stripped of color codes.
{
	WWIV_ASSERT( pszOrig );
    static char szNewString[ 255 ];
    const char * po = pszOrig;
    char * pn = szNewString;
    while( *po )
    {
        if ( ( *po == '|' ) &&
             IsColorCode( *( po + 1 ) ) &&
             IsColorCode( *( po + 2 ) ) )
        {
            po += 2;
        }
        else if ( *po == 3 && *(po+1) && isdigit( *( po+1 ) ) )
        {
            po++;
        }
        else
        {
            *pn++ = *po;
        }
        po++;
    }
    *pn++ = '\0';
    return szNewString;
}


/**
 * Translates the character ch into uppercase using WWIV's translation tables
 * @param ch The character to translate
 * @return The uppercase version of the character
 */
unsigned char upcase(unsigned char ch)
{
    unsigned char *ss = (unsigned char*) strchr((const char*) translate_letters[0], ch);
    if (ss)
    {
        ch = translate_letters[1][ss - translate_letters[0]];
    }
    return ch;
}


/**
 * Translates the character ch into lower using WWIV's translation tables
 * @param ch The character to translate
 * @return The lowercase version of the character
 */
unsigned char locase(unsigned char ch)
{
    unsigned char *ss = (unsigned char*)  strchr((const char*) translate_letters[1], ch);

    if ( ss )
    {
        ch = translate_letters[0][ss - translate_letters[1]];
    }

    return ch;
}


/**
 * Returns a string justified and padded with "bg".
 * @param pszString The text to justify
 * @param nLength the length of the text
 * @param the character to use as the background
 * @param nJustificationType one of the following:
 *			JUSTIFY_LEFT
 *			JUSTIFY_RIGHT
 *			JUSTIFY_CENTER
 * @return the justified text.
 */
char *StringJustify( char *pszString, int nLength, int bg, int nJustificationType )
{
    WWIV_ASSERT( pszString );

    pszString[nLength] = '\0';
    int x = strlen( pszString );

    if ( x < nLength )
    {
        switch ( nJustificationType )
        {
        case JUSTIFY_LEFT:
            {
                memset(pszString + x, bg, nLength - x);
            }
            break;
        case JUSTIFY_RIGHT:
            {
                memmove(pszString + nLength - x, pszString, x);
                memset(pszString, bg, nLength - x);
            }
            break;
        case JUSTIFY_CENTER:
            {
                int x1 = (nLength - x) / 2;
                memmove(pszString + x1, pszString, x);
                memset(pszString, bg, x1);
                memset(pszString + x + x1, bg, nLength - (x + x1));
            }
            break;
        default:
            WWIV_ASSERT( false );   // incorrect type.
        }
    }
    return pszString;
}


/**
 * Removes spaces from the beginning and the end of the string s.
 * @param s the string from which to remove spaces
 * @return s with spaces removed.
 */
char *StringTrim( char *pszString )
{
	WWIV_ASSERT( pszString );

    // find real end of it
    int nStringLen = strlen(pszString);
    while ( ( nStringLen > 0 ) && isspace( static_cast<unsigned char>( pszString[nStringLen - 1] ) ) )
    {
        --nStringLen;
    }

    // find real beginning
    int nStartPos = 0;
    while ( ( nStartPos < nStringLen ) && isspace( static_cast<unsigned char>( pszString[nStartPos] ) ) )
    {
        ++nStartPos;
    }

    // knock spaces off the length
    nStringLen -= nStartPos;

    // move over the desired subsection
    memmove(pszString, pszString + nStartPos, nStringLen );

    // ensure null-terminated
    pszString[ nStringLen ] = '\0';

    return pszString;
}


std::string& StringTrimBegin( std::string& s )
{
    while( s[ 0 ] == ' ' )
    {
        s.erase( s.begin() );
    }
    return s;
}


std::string& StringTrimEnd( std::string& s )
{
    while( s[ s.size() - 1 ] == ' ' )
    {
#if defined ( _WIN32 ) && ( _MSC_VER < 1300 )
        s.erase( s.end() - 1 );
#else
        s.erase( --s.end() );
#endif
    }
    return s;
}


/**
 * Removes spaces from the beginning and the end of the string s.
 * @param s the string from which to remove spaces
 * @return s with spaces removed.
 */
std::string& StringTrim( std::string& s )
{
    return StringTrimEnd( StringTrimBegin( s ) );
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
char *stristr(char *pszString, char *pszPattern)
{
	WWIV_ASSERT( pszString );
	WWIV_ASSERT( pszPattern );

    int len = strlen(pszPattern), pos = 0;

    while (pszString[pos])
    {
        if (WWIV_STRNICMP(pszString + pos, pszPattern, len) == 0)
        {
            return (pszString + pos);
        }
        ++pos;
    }
    return NULL;
}


/**
 * single_space() takes an input string and reduces repeated spaces in the
 * string to one space, e.g. "The    brown  fox" becomes "The brown fox", to
 * facilitate "tokenizing" a string.
 */
void single_space(char *pszText)
{
	WWIV_ASSERT( pszText );

	if ( pszText )
	{
    	char *pInputBuffer = pszText;
        char *pOutputBuffer = pszText;
		int i = 0;
        int cnt = 0;

		while (*pInputBuffer)
		{
			if (isspace(*pInputBuffer) && cnt)
			{
				pInputBuffer++;
			}
			else
			{
				if (!isspace(*pInputBuffer))
				{
					cnt = 0;
				}
				else
				{
					*pInputBuffer = ' ';
					cnt = 1;
				}
				pOutputBuffer[i++] = *pInputBuffer++;
			}
		}
		pOutputBuffer[i] = '\0';
	}
}


char *stptok(const char *pszText, char *pszToken, size_t nTokenLength, char *brk)
{
	bool bCountThis, bFoundFirst = false;

	pszToken[0] = '\0';

	WWIV_ASSERT(pszText);
	WWIV_ASSERT(pszToken);
	WWIV_ASSERT(brk);

	if ( !pszText || !*pszText )
	{
		return NULL;
	}

	char* lim = pszToken + nTokenLength - 1;
	while (*pszText && pszToken < lim)
	{
		bCountThis = true;
		for (char* b = brk; *b; b++)
		{
			if (*pszText == *b)
			{
				if (bFoundFirst)
				{
					*pszToken = 0;
					return const_cast<char *>( pszText );
				}
				else
				{
					bCountThis = false;
				}
			}
		}
		if ( bCountThis )
		{
			*pszToken++ = *pszText++;
			bFoundFirst = true;
		}
		else
		{
			pszText++;
		}
	}
	*pszToken = '\0';
	return const_cast<char *>( pszText );
}

char *StringRemoveWhitespace(char *str)
{
	WWIV_ASSERT(str);

    if (str)
	{
        char *obuf, *nbuf;
        for (obuf = str, nbuf = str; *obuf; ++obuf)
		{
            if (!isspace(*obuf))
			{
                *nbuf++ = *obuf;
			}
        }
        *nbuf = '\0';
    }
    return str;
}


char *StringRemoveChar( const char *pszString, char chCharacterToRemove )
{
	static char s_strip_string[ 255 ];

	WWIV_ASSERT(pszString);

	strcpy(s_strip_string, "");

	int i1  = 0;
	for (int i = 0; i < wwiv::stringUtils::GetStringLength(pszString); i++)
	{
		if ( pszString[i] != chCharacterToRemove )
		{
			s_strip_string[i1] = pszString[i];
			i1++;
		}
		else
		{
			s_strip_string[i1] = '\0';
            break;
		}
	}

	//if last char is a space, remove it too
	if (s_strip_string[i1-1] == ' ')
	{
		i1--;
	}

	s_strip_string[i1] = '\0';

	return static_cast< char * > ( s_strip_string );
}


char *StringReplace(char *pszString, size_t nMaxBufferSize, char *pszOldString, char *pszNewString)
{
    char *p, *q;

	WWIV_ASSERT(pszString);
	WWIV_ASSERT(pszOldString);
	WWIV_ASSERT(pszNewString);

    if (NULL == (p = strstr(pszString, pszOldString)))
	{
        return pszString;
	}
    int nOldLen = strlen(pszOldString);
    int nNewLen = strlen(pszNewString);
    if ((strlen(pszString) + nNewLen - nOldLen + 1) > nMaxBufferSize)
    {
        return NULL;
    }
    memmove(q = p + nNewLen, p + nOldLen, strlen(p + nOldLen) + 1);
    memcpy(p, pszNewString, nNewLen);
    return q;
}


#if defined( WWIV_STRICMP )
#undef WWIV_STRICMP
#endif // WWIV_STRICMP

