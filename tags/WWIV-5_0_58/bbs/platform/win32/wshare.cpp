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

#include "wwiv.h"

#define SHARE_LEVEL 10
#define WAIT_TIME 10
#define TRIES 100


/**
 * debug levels:
 *
 *	0 turns all debug operations off
 *	1 or greater shows file information if the file must be waited upon.
 *	2 or greater shows file information when an attempt is made to open a file.
 *	3 or greater shows file information BEFORE any attempt is made to open a file.
 */

//
// Local prototypes
//
FILE *fsh_open(const char *path, char *mode)
{
	FILE *f;

	if ( sess->GetGlobalDebugLevel() > 2 )
	{
		std::cout << "\rfsh_open " << path << ", access=" << mode << ".\r\n";
	}

	int share = SH_DENYWR;
	int md = 0;
	if (strchr(mode, 'w') != NULL)
	{
		share = SH_DENYRD;
		md = O_RDWR | O_CREAT | O_TRUNC;
	}
	else if (strchr(mode, 'a') != NULL)
	{
		share = SH_DENYRD;
		md = O_RDWR | O_CREAT;
	}
	else
	{
		md = O_RDONLY;
	}

	if (strchr(mode, 'b') != NULL)
	{
		md |= O_BINARY;
	}

	if (strchr(mode, '+') != NULL)
	{
		md &= ~O_RDONLY;
		md |= O_RDWR;
		share = SH_DENYRD;
	}

	int fd = _sopen(path, md, share, S_IREAD | S_IWRITE);
	if (fd < 0)
	{
		int count = 1;
		if (access(path, 0) != -1)
		{
			WWIV_Delay(WAIT_TIME);
			fd = _sopen(path, md, share, S_IREAD | S_IWRITE);
			while ( ( (fd < 0) && (errno == EACCES) ) && (count < TRIES) )
			{
				WWIV_Delay(WAIT_TIME);
				if ( sess->GetGlobalDebugLevel() > 0 )
				{
					std::cout << "\rWaiting to access " << path << " " << TRIES - count << ".  \r";
				}
				count++;
				fd = _sopen( path, md, share, S_IREAD | S_IWRITE );
			}
			if ( fd < 0 && sess->GetGlobalDebugLevel() > 0 )
			{
				std::cout << "\rThe file " << path << " is busy.  Try again later.\r\n";
			}
		}
	}

	if ( fd > 0 )
	{
		if ( strchr( mode, 'a' ) )
		{
			lseek( fd, 0L, SEEK_END );
		}

		f = fdopen( fd, mode );
		if ( !f )
		{
			close( fd );
		}
	}
	else
	{
		f = 0;
	}

	if ( sess->GetGlobalDebugLevel() > 1 )
	{
		std::cout << "\rfsh_open " << path << ", access=" << mode << ".\r\n";
	}

	return f;
}


void fsh_close(FILE * f)
{
	fclose( f );
}


size_t fsh_read(void *ptr, size_t size, size_t n, FILE * stream)
{
	if (stream == NULL)
	{
		sysoplog("\r\nAttempted to fread from closed file.\r\n");
		return 0;
	}
	return fread(ptr, size, n, stream);
}


size_t fsh_write(const void *ptr, size_t size, size_t n, FILE * stream)
{
    if (stream == NULL)
    {
        sysoplog("\r\nAttempted to fwrite to closed file.\r\n");
        return 0;
    }
    return fwrite(ptr, size, n, stream);
}


