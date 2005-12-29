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

#include "WTextFile.h"
#include "WFile.h"
#include <iostream>
#include <fcntl.h>
#include <cerrno>
#include <sys/stat.h>
#include <io.h>
#include <cstdio>
#include <stdlib.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>


const int WTextFile::WAIT_TIME = 10;
const int WTextFile::TRIES = 100;


/**
 * debug levels:
 *
 *	0 turns all debug operations off
 *	1 or greater shows file information if the file must be waited upon.
 *	2 or greater shows file information when an attempt is made to open a file.
 *	3 or greater shows file information BEFORE any attempt is made to open a file.
 */

FILE* WTextFile::OpenImpl( const char* pszFileName, const char* pszFileMode )
{
	FILE *hFile;

	//if ( GetSession()->GetGlobalDebugLevel() > 2 )
	//{
	//	std::cout << "\rfsh_open " << pszFileName << ", access=" << pszFileMode << ".\r\n";
	//}

	int share = SH_DENYWR;
	int md = 0;
	if (strchr(pszFileMode, 'w') != NULL)
	{
		share = SH_DENYRD;
		md = O_RDWR | O_CREAT | O_TRUNC;
	}
	else if (strchr(pszFileMode, 'a') != NULL)
	{
		share = SH_DENYRD;
		md = O_RDWR | O_CREAT;
	}
	else
	{
		md = O_RDONLY;
	}

	if (strchr(pszFileMode, 'b') != NULL)
	{
		md |= O_BINARY;
	}

	if (strchr(pszFileMode, '+') != NULL)
	{
		md &= ~O_RDONLY;
		md |= O_RDWR;
		share = SH_DENYRD;
	}

	int fd = _sopen(pszFileName, md, share, S_IREAD | S_IWRITE);
	if (fd < 0)
	{
		int count = 1;
        if ( WFile::Exists( pszFileName ) )
		{
            ::Sleep(WAIT_TIME);
			fd = _sopen(pszFileName, md, share, S_IREAD | S_IWRITE);
			while ( ( fd < 0 && errno == EACCES ) && count < TRIES )
			{
                ::Sleep(WAIT_TIME);
				//if ( GetSession()->GetGlobalDebugLevel() > 0 )
				//{
				//	std::cout << "\rWaiting to access " << pszFileName << " " << TRIES - count << ".  \r";
				//}
				count++;
				fd = _sopen( pszFileName, md, share, S_IREAD | S_IWRITE );
			}
			//if ( fd < 0 && GetSession()->GetGlobalDebugLevel() > 0 )
			//{
			//	std::cout << "\rThe file " << pszFileName << " is busy.  Try again later.\r\n";
			//}
		}
	}

	if ( fd > 0 )
	{
		if ( strchr( pszFileMode, 'a' ) )
		{
			_lseek( fd, 0L, SEEK_END );
		}

		hFile = _fdopen( fd, pszFileMode );
		if ( !hFile )
		{
			_close( fd );
		}
	}
	else
	{
		hFile = NULL;
	}

	//if ( GetSession()->GetGlobalDebugLevel() > 1 )
	//{
	//	std::cout << "\rfsh_open " << pszFileName << ", access=" << pszFileMode << ".\r\n";
	//}

    return hFile;
}
