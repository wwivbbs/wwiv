/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
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

#include "wwiv.h"

#if defined (_WIN32)
#define SNPRINTF _snprintf
#else
#define SNPRINTF snprintf
#endif

// Displays list of files matching filespec pszFileName in directory pszDirectoryName.
void show_files( const char *pszFileName, const char *pszDirectoryName ) {
	char s[MAX_PATH];
	char drive[MAX_DRIVE], direc[MAX_DIR], file[MAX_FNAME], ext[MAX_EXT];

	char c = ( okansi() ) ? '\xCD' : '=';
	GetSession()->bout.NewLine();
#if defined (_WIN32)
	_splitpath(pszDirectoryName, drive, direc, file, ext);
#else
	strcpy(direc, pszDirectoryName);
	strcpy(drive, "");
	strcpy(file, pszFileName);
	strcpy(ext, "");
#endif
	SNPRINTF(s, sizeof( s ), "|#7[|B1|15 FileSpec: %s    Dir: %s%s |B0|#7]", WWIV_STRUPR(stripfn(pszFileName)), drive, direc);
	int i = ( GetSession()->GetCurrentUser()->GetScreenChars() - 1 ) / 2 - strlen(stripcolors(s)) / 2;
	GetSession()->bout << "|#7" << charstr( i, c ) << s;
	i = GetSession()->GetCurrentUser()->GetScreenChars() - 1 - i - strlen(stripcolors(s));
	GetSession()->bout << "|#7" << charstr( i, c );

	char szFullPathName[ MAX_PATH ];
	SNPRINTF( szFullPathName, sizeof( szFullPathName ), "%s%s", pszDirectoryName, WWIV_STRUPR( stripfn(pszFileName ) ) );
	WFindFile fnd;
	bool bFound = fnd.open( szFullPathName, 0 );
	while (bFound) {
		strncpy(s, fnd.GetFileName(), MAX_PATH);
		align(s);
		SNPRINTF( szFullPathName, sizeof( szFullPathName ), "|#7[|#2%s|#7]|#1 ", s );
		if ( GetSession()->localIO()->WhereX() > ( GetSession()->GetCurrentUser()->GetScreenChars() - 15 ) ) {
			GetSession()->bout.NewLine();
		}
		GetSession()->bout << szFullPathName;
		bFound = fnd.next();
	}

	GetSession()->bout.NewLine();
	GetSession()->bout.Color( 7 );
	GetSession()->bout << charstr( GetSession()->GetCurrentUser()->GetScreenChars() - 1, c );
	GetSession()->bout.NewLine( 2 );
}


