/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2005, WWIV Software Services             */
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

#if !defined ( __INCLUDED_WStreamBuffer_H__ )
#define __INCLUDED_WStreamBuffer_H__


#if defined( _MSC_VER ) && !defined( _CRT_SECURE_NO_DEPRECATE )
#define _CRT_SECURE_NO_DEPRECATE
#endif	// _MSC_VER 

#include <iostream>
#include <streambuf>
#include <ios>


using std::ostream;


int  bputs( const char *pszText );
int  bputch( char c, bool bUseInternalBuffer );


class WOutStreamBuffer : public std::streambuf
{
public:
    //WOutStreamBuffer() {}
    //virtual ~WOutStreamBuffer() {}
    virtual int_type overflow( int_type c )
    {
        if ( c != EOF )
        {
            bputch( static_cast<char>( c ), false );
        }
        return c;
    }


    virtual std::streamsize xsputn( const char *pszText, std::streamsize numChars )
    {
		if ( numChars == 0 )
		{
			return 0;
		}
        char szBuffer[ 4096 ];
        strncpy( szBuffer, pszText, 4096 );
        szBuffer[ numChars ] = '\0';
        return bputs( pszText );
    }
};


#if defined(_MSC_VER)
#pragma warning( push )
#pragma warning( disable: 4511 4512 )
#endif

class WOutStream : public ostream
{
protected:
    WOutStreamBuffer buf;
public:
    WOutStream() :
#if defined(_WIN32)
		buf(),
#endif
		ostream( &buf )
    {
        init( &buf );
    }
    virtual ~WOutStream() {}
};

#if defined(_MSC_VER)
#pragma warning( pop )
#endif

#endif  // __INCLUDED_WStreamBuffer_H__
