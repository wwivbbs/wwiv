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
#include "WStringUtils.h"

#ifdef __APPLE__
#include <sys/param.h>
#include <sys/mount.h>
#else
#include <sys/vfs.h>
#endif // __APPLE__

// Returns the length of an open file handle in bytes

long filelength(int f)
{
  size_t src, len;

  src = lseek(f, 0L, SEEK_CUR);		/* No "ftell()" equiv */
  len = lseek(f, 0L, SEEK_END);
  lseek(f, src, SEEK_SET);

  return len;
}

// Changes the size of the file hanle (Either by truncating or zero-padding).

void chsize(int f, size_t size)
{
  ftruncate(f,size);
}



double WWIV_GetFreeSpaceForPath(const char * szPath)
{
  struct statfs fs;
  double fk;

  if (statfs(szPath, &fs))
  {
#ifdef _DEBUG_BBS
    fprintf(stderr, "%s: ", szPath);
#endif
    perror("freek1()");
    return(0.0);
  }

  fk = ((double) fs.f_bsize * (double) fs.f_bavail) / 1024.0;

  return(fk);
}


void WWIV_ChangeDirTo(const char *pszDirectoryName)
{
	chdir( pszDirectoryName );
}


void WWIV_GetDir( char *pszDirectoryName, bool bSlashAtEnd )
{
	getcwd( pszDirectoryName, 80 );
	if ( bSlashAtEnd )
	{
		if ( pszDirectoryName[ strlen( pszDirectoryName )-1 ]!= '/' )
		{
			strcat( pszDirectoryName, "/" );
		}
	}
}


void WWIV_GetFileNameFromPath(const char *pszPath, char *pszFileName)
{
	char *pszTemp = strdup(pszPath);
	char *pTempFn = strrchr(pszTemp, '/');
	if (pTempFn != NULL)
	{
		*pTempFn = 0;
		pTempFn++;
	}
	else
	{
		pTempFn = pszTemp;
	}
	strcpy(pszFileName, pTempFn);
	BbsFreeMemory(pszTemp);
}

bool WWIV_CopyFile(const char * szSourceFileName, const char * szDestFileName)
{
  int f1, f2, i;
  char *b;

  if ( !wwiv::stringUtils::IsEquals(szSourceFileName, szDestFileName ) &&
       WFile::Exists( szSourceFileName ) &&
	   !WFile::Exists( szDestFileName ) )
  {
    if ( ( b = static_cast<char *>( BbsAllocA( 16400 ) ) ) == NULL )
    {
      return 0;
    }
    f1 = open( szSourceFileName, O_RDONLY | O_BINARY );
    if(!f1)
    {
      BbsFreeMemory(b);
      return 0;
    }

    f2 = open( szDestFileName,
                 O_RDWR | O_BINARY | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE );
    if(!f2)
    {
      BbsFreeMemory(b);
      close(f1);
      return 0;
    }

    i = read(f1, (void *) b, 16384);

    while (i > 0)
    {
      giveup_timeslice();
      write(f2, (void *) b, i);
      i = read(f1, (void *) b, 16384);
    }

    f1=close(f1);
    f2=close(f2);
    BbsFreeMemory(b);
  }

  return 1;
}
