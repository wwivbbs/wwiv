/**************************************************************************/
/*                                                                        */
/*                 WWIV Initialization Utility Version 5.0                */
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
#include "wwivinit.h"

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
 *	4 or greater waits for key from console with each file open.
 */

int sh_open(const char *path, int file_access, unsigned mode)
{
	int handle, count, share;
	char drive[MAX_DRIVE], dir[MAX_DIR], file[MAX_FNAME], ext[MAX_EXT];
	
	if ((file_access & O_RDWR) || (file_access & O_WRONLY) || (mode & S_IWRITE)) 
	{
		share = SH_DENYRW;
	} 
	else 
	{
		share = SH_DENYWR;
	}
	handle = _sopen(path, file_access, share, mode);
	if (handle < 0) 
	{
		count = 1;
		_splitpath(path, drive, dir, file, ext);
		if (_access(path, 0) != -1) 
		{
			Sleep(WAIT_TIME);
			handle = _sopen(path, file_access, share, mode);
			while (((handle < 0) && (errno == EACCES)) && (count < TRIES)) 
			{
				if (count % 2)
					Sleep(WAIT_TIME);
				else
					Sleep(0);
				count++;
				handle = _sopen(path, file_access, share, mode);
			}
		}
	}
	return handle;
}


int sh_open1(const char *path, int access)
{
	unsigned mode = 0;
	if ((access & O_RDWR) || (access & O_WRONLY))
	{
		mode |= S_IWRITE;
	}
	if ((access & O_RDWR) || (access & O_RDONLY))
	{
		mode |= S_IREAD;
	}

	return sh_open( path, access, mode );
}


FILE *fsh_open(const char *path, char *mode)
{
	FILE *f;
	int count, fd;
	char drive[MAX_DRIVE], dir[MAX_DIR], file[MAX_FNAME], ext[MAX_EXT];
	
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
	
	fd = _sopen(path, md, share, S_IREAD | S_IWRITE);
	if (fd < 0) 
	{
		count = 1;
		_splitpath(path, drive, dir, file, ext);
		if (_access(path, 0) != -1) 
		{
			WWIV_Delay(WAIT_TIME);
			fd = _sopen(path, md, share, S_IREAD | S_IWRITE);
			while (((fd < 0) && (errno == EACCES)) && (count < TRIES)) 
			{
				WWIV_Delay(WAIT_TIME);
				count++;
				fd = _sopen(path, md, share, S_IREAD | S_IWRITE);
			}
		}
	}
	
	if (fd > 0) 
	{
		if (strchr(mode, 'a'))
		{
			_lseek(fd, 0L, SEEK_END);
		}			
		
		f = _fdopen(fd, mode);
		if (!f) 
		{
			_close(fd);
		}
	} 
	else
	{
		f = 0;
	}
	
	return f;
}


int sh_close(int f)
{
	if (f != -1)
	{
		_close(f);
	}
	return -1;
}


void fsh_close(FILE * f)
{
	fclose(f);
}


long sh_lseek(int handle, long offset, int fromwhere)
{
	
	if (handle == -1) 
	{
		return -1L;
	}
	return _lseek( handle, offset, fromwhere );
}


int sh_read(int handle, void *buf, unsigned len)
{
	
	if (handle == -1) 
	{
		return -1;
	}
	return _read( handle, buf, len );
}


int sh_write(int handle, const void *buf, unsigned len)
{
	if (handle == -1) 
	{
		return -1;
	}
	return _write( handle, buf, len );
}


size_t fsh_read(void *ptr, size_t size, size_t n, FILE * stream)
{
	if (stream == NULL) 
	{
		return 0;
	}
	return fread( ptr, size, n, stream );
}


size_t fsh_write(const void *ptr, size_t size, size_t n, FILE * stream)
{
	if (stream == NULL) 
	{
		return 0;
	}
	return fwrite( ptr, size, n, stream );
}


int sh_open25(const char *path, int file_access, int share, unsigned mode)
{
	int count;
	char drive[MAX_DRIVE], dir[MAX_DIR], file[MAX_FNAME], ext[MAX_EXT];
	
	int handle = _sopen(path, file_access, share, mode);
	if (handle < 0) 
	{
		count = 1;
		_splitpath(path, drive, dir, file, ext);
		if (_access(path, 0) != -1) 
		{
			WWIV_Delay(WAIT_TIME);
			handle = _sopen(path, file_access, share, mode);
			while (((handle < 0) && (errno == EACCES)) && (count < TRIES)) 
			{
				if (count % 2)
				{
					Sleep(WAIT_TIME);
				}
				else
				{
					Sleep(0);
				}
				count++;
				handle = _sopen(path, file_access, share, mode);
			}
		}
	}

	return handle;
}


