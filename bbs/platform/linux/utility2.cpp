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


void show_files( const char *pszFileName, const char *pszDirectoryName )
// Displays list of files matching filespec pszFileName in directory pszDirectoryName.
{
	char s[ 255 ], s1[ 255 ];

	char c = ( okansi() ) ? '\xCD' : '=';
	nl();

    sprintf( s, "|#7[|#4FileSpec: %s    Dir: %s  |#7]", stripfn( pszFileName ), pszFileName );

	int i = ( sess->thisuser.GetScreenChars() - 1 ) / 2 - strlen( stripcolors( s ) ) / 2;
	sess->bout << "|#7" << charstr( i, c ) << s;

	i = sess->thisuser.GetScreenChars() - 1 - i - strlen( stripcolors( s ) );
	sess->bout << "|#7%s" << charstr( i, c );

	sprintf(s1,"%s%s",pszDirectoryName,stripfn(pszFileName));
	WFindFile fnd;
	bool bFound = fnd.open(s1, 0);
	while (bFound)
	{
		strcpy(s,fnd.GetFileName());
		align(s);
		sprintf(s1,"|#7[|#2%s|#7]|#1 ",s);
		if ( app->localIO->WhereX()> ( sess->thisuser.GetScreenChars() - 15 ) )
		{
			nl();
		}
		sess->bout << s1;
		bFound = fnd.next();
	}

	nl();
	ansic( 7 );
	sess->bout << charstr( sess->thisuser.GetScreenChars() - 1, c );
	nl( 2 );
}


char *WWIV_make_abs_cmd(char *out)
{
  if (strchr(out, '/'))
  {
    return out;
  }

  char s[MAX_PATH];
  strcpy(s, out);
  sprintf(out, "%s%s", app->GetHomeDir(), s);

  return(out);
}


// Reverses a string
char *strrev(char *s)
{
	WWIV_ASSERT( s );
	char s1[255];
	int str = strlen( s );
	WWIV_ASSERT( str <= 255 );

	for ( int i = str; i >- 1; i-- )
	{
		s[i] = s1[str-i];
	}
	strcpy( s,s1 );
	return s;
}

#define LAST(s) s[strlen(s)-1]

int WWIV_make_path(char *s)
{
  char current_path[MAX_PATH], *p, *flp;

  p = flp = strdup(s);
  getcwd(current_path, MAX_PATH);
  if(LAST(p) == WWIV_FILE_SEPERATOR_CHAR)
    LAST(p) = 0;
  if(*p == WWIV_FILE_SEPERATOR_CHAR) {
    chdir(WWIV_FILE_SEPERATOR_STRING);
    p++;
  }
  for(; (p = strtok(p, WWIV_FILE_SEPERATOR_STRING)) != 0; p = 0) {
    if(chdir(p)) {
      if(mkdir(p)) {
        chdir(current_path);
        return -1;
      }
      chdir(p);
    }
  }
  chdir(current_path);
  if(flp)
  {
    BbsFreeMemory(flp);
  }
  return 0;
}

#if defined (LAST)
#undef LAST
#endif

void WWIV_Delay(unsigned long usec)
{
	if(usec)
	{
		usleep(usec);
	}
}

void WWIV_OutputDebugString( const char *pszString )
{
	//std::cout << pszString;
}
