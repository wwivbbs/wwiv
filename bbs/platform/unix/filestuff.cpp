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


#include "wwiv.h"

#ifdef __APPLE__
#include <sys/param.h>
#include <sys/mount.h>
#else
#include <sys/vfs.h>
#endif // __APPLE__


double WWIV_GetFreeSpaceForPath(const char * szPath) {
	struct statfs fs;
	double fk;

	if (statfs(szPath, &fs)) {
#ifdef _DEBUG_BBS
		fprintf(stderr, "%s: ", szPath);
#endif
		perror("freek1()");
		return(0.0);
	}

	fk = ((double) fs.f_bsize * (double) fs.f_bavail) / 1024.0;

	return(fk);
}


void WWIV_ChangeDirTo(const char *pszDirectoryName) {
	chdir( pszDirectoryName );
}


void WWIV_GetDir( char *pszDirectoryName, bool bSlashAtEnd ) {
	getcwd( pszDirectoryName, 80 );
	if ( bSlashAtEnd ) {
		if ( pszDirectoryName[ strlen( pszDirectoryName )-1 ]!= '/' ) {
			strcat( pszDirectoryName, "/" );
		}
	}
}


void WWIV_GetFileNameFromPath(const char *pszPath, char *pszFileName) {
	char *pszTemp = WWIV_STRDUP(pszPath);
	char *pTempFn = strrchr(pszTemp, '/');
	if (pTempFn != NULL) {
		*pTempFn = 0;
		pTempFn++;
	} else {
		pTempFn = pszTemp;
	}
	strcpy(pszFileName, pTempFn);
	BbsFreeMemory(pszTemp);
}

