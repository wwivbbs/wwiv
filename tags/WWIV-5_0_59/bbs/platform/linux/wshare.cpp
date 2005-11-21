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


#include "wwiv.h"
#include <sys/file.h>


#define SHARE_LEVEL 10
#define WAIT_TIME 10
#define TRIES 100


/*
 * Debug Levels:
 * ==========================================================================
 *	0 turns all debug operations off
 *	1 or greater shows file information if the file must be waited upon.
 *	2 or greater shows file information when an attempt is made to open a file.
 *	3 or greater shows file information BEFORE any attempt is made to open a file.
 *	4 or greater waits for key from console with each file open.
 *
 */

FILE *fsh_open(const char *path, char *mode)
{
  	FILE *f = fopen(path, mode);

	if (f != NULL)
  	{
		flock(fileno(f), (strpbrk(mode, "wa+")) ? LOCK_EX : LOCK_SH);
	}

	return f;
}


void fsh_close(FILE *f)
{
	if (f != NULL)
	{
		flock(fileno(f), LOCK_UN);
		fclose(f);
	}
}

size_t fsh_read(void *ptr, size_t size, size_t n, FILE *stream)
{
	if (stream == NULL)
	{
		sysoplog("\r\nAttempted to fread from closed file.\r\n");
		return 0;
	}
	return(fread(ptr, size, n, stream));
}


size_t fsh_write(const void *ptr, size_t size, size_t n, FILE *stream)
{
	if (stream == NULL)
	{
		sysoplog("\r\nAttempted to fwrite to closed file.\r\n");
		return 0;
	}
	return(fwrite(ptr, size, n, stream));
}

