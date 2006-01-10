/****************************************************************************/
/*                                                                          */
/*                             WWIV Version 5.0x                            */
/*            Copyright (C) 1998-2003 by WWIV Software Services             */
/*                                                                          */
/*      Distribution or publication of this source code, it's individual    */
/*       components, or a compiled version thereof, whether modified or     */
/*        unmodified, without PRIOR, WRITTEN APPROVAL of WWIV Software      */
/*        Services is expressly prohibited.  Distribution of compiled       */
/*            versions of WWIV is restricted to copies compiled by          */
/*           WWIV Software Services.  Violators will be procecuted!         */
/*                                                                          */
/****************************************************************************/
#include "../../wwiv.h"

#define SHARE_LEVEL 10
#define WAIT_TIME 10
#define TRIES 100

void sysoplog(const char *s);                     /* warn */
extern INT32 incom;

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
	
	if (debuglevel > 2)
	{
		printf("\rsh_open %s, access=%u.\r\n", path, file_access);
	}
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
		if (access(path, 0) != -1) 
		{
			Sleep(WAIT_TIME);
			handle = _sopen(path, file_access, share, mode);
			while (((handle < 0) && (errno == EACCES)) && (count < TRIES)) 
			{
				if (count % 2)
					Sleep(WAIT_TIME);
				else
					Sleep(0);
				if (debuglevel)
					printf("\rWaiting to access %s%s %d.  \r",
					file, ext, TRIES - count);
				count++;
				handle = _sopen(path, file_access, share, mode);
			}
			if ((handle < 0) && (debuglevel))
				printf("\rThe file %s%s is busy.  Try again later.\r\n", file, ext);
		}
	}
	if (debuglevel > 1)
		printf("\rsh_open %s, access=%u, handle=%d.\r\n",
		path, file_access, handle);
	if ((debuglevel > 3) && (!incom))
		getche();
	return (handle);
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

	return (sh_open(path, access, mode));
}


FILE *fsh_open(const char *path, char *mode)
{
	FILE *f;
	int count, fd;
	char drive[MAX_DRIVE], dir[MAX_DIR], file[MAX_FNAME], ext[MAX_EXT];
	
	if (debuglevel > 2)
	{
		printf("\rfsh_open %s, access=%s.\r\n", path, mode);
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
	
	fd = _sopen(path, md, share, S_IREAD | S_IWRITE);
	if (fd < 0) 
	{
		count = 1;
		_splitpath(path, drive, dir, file, ext);
		if (access(path, 0) != -1) 
		{
			WWIV_Delay(WAIT_TIME);
			fd = _sopen(path, md, share, S_IREAD | S_IWRITE);
			while (((fd < 0) && (errno == EACCES)) && (count < TRIES)) 
			{
				WWIV_Delay(WAIT_TIME);
				if (debuglevel)
				{
					printf("\rWaiting to access %s%s %d.  \r", file, ext, TRIES - count);
				}
				count++;
				fd = _sopen(path, md, share, S_IREAD | S_IWRITE);
			}
			if ((fd < 0) && (debuglevel))
			{
				printf("\rThe file %s%s is busy.  Try again later.\r\n", file, ext);
			}
		}
	}
	
	if (fd > 0) 
	{
		if (strchr(mode, 'a'))
		{
			lseek(fd, 0L, SEEK_END);
		}			
		
		f = fdopen(fd, mode);
		if (!f) 
		{
			close(fd);
		}
	} 
	else
	{
		f = 0;
	}
	
	if (debuglevel > 1)
	{
		printf("\rfsh_open %s, access=%s.\r\n",	path, mode);
	}
	
	if ((debuglevel > 3) && (!incom))
	{
		getche();
	}
	
	return (f);
}


int sh_close(int f)
{
	if (f != -1)
	{
		close(f);
	}
	return (-1);
}


void fsh_close(FILE * f)
{
	fclose(f);
}


long sh_lseek(int handle, long offset, int fromwhere)
{
	
	if (handle == -1) 
	{
		sysoplog("Attempted to seek in closed file.\r\n");
		return (-1L);
	}
	return (_lseek(handle, offset, fromwhere));
}


int sh_read(int handle, void *buf, unsigned len)
{
	
	if (handle == -1) 
	{
		sysoplog("\r\nAttempted to read from closed file.\r\n");
		return (-1);
	}
	return (read(handle, buf, len));
}


int sh_write(int handle, const void *buf, unsigned len)
{
	if (handle == -1) 
	{
		sysoplog("\r\nAttempted to write to closed file.\r\n");
		return (-1);
	}
	return (write(handle, buf, len));
}


size_t fsh_read(void *ptr, size_t size, size_t n, FILE * stream)
{
	if (stream == NULL) 
	{
		sysoplog("\r\nAttempted to fread from closed file.\r\n");
		return (0);
	}
	return (fread(ptr, size, n, stream));
}


size_t fsh_write(const void *ptr, size_t size, size_t n, FILE * stream)
{
	if (stream == NULL) 
	{
		sysoplog("\r\nAttempted to fwrite to closed file.\r\n");
		return (0);
	}
	return (fwrite(ptr, size, n, stream));
}


int sh_open25(const char *path, int file_access, int share, unsigned mode)
{
	int handle, count;
	char drive[MAX_DRIVE], dir[MAX_DIR], file[MAX_FNAME], ext[MAX_EXT];
	
	if (debuglevel > 2)
	{
		printf("\rsh_open %s, access=%u.\r\n", path, file_access);
	}
	
	handle = _sopen(path, file_access, share, mode);
	if (handle < 0) 
	{
		count = 1;
		_splitpath(path, drive, dir, file, ext);
		if (access(path, 0) != -1) 
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
				if (debuglevel)
				{
					printf("\rWaiting to access %s%s %d.  \r", file, ext, TRIES - count);
				}
				count++;
				handle = _sopen(path, file_access, share, mode);
			}
			if ((handle < 0) && (debuglevel))
			{
				printf("\rThe file %s%s is busy.  Try again later.\r\n", file, ext);
			}
		}
	}

	if (debuglevel > 1)
	{
		printf("\rsh_open %s, access=%u, handle=%d.\r\n", path, file_access, handle);
	}
	if ((debuglevel > 3) && (!incom))
	{
		getche();
	}
	return (handle);
}


