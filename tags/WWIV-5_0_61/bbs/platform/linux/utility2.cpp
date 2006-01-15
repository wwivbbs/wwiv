/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2006, WWIV Software Services             */
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

char *WWIV_make_abs_cmd( char *pszOutBuffer )
{
  if ( strchr( pszOutBuffer, '/' ) )
  {
    return pszOutBuffer;
  }

  char s[ MAX_PATH ];
  strcpy( s, pszOutBuffer );
  snprintf( pszOutBuffer, MAX_PATH, "%s%s", GetApplication()->GetHomeDir(), s );

  return pszOutBuffer;
}


// Reverses a string
char *strrev( char *pszBufer )
{
	WWIV_ASSERT( pszBufer );
	char szTempBuffer[255];
	int str = strlen( pszBufer );
	WWIV_ASSERT( str <= 255 );

	for ( int i = str; i >- 1; i-- )
	{
		pszBufer[i] = szTempBuffer[str-i];
	}
	strcpy( pszBufer, szTempBuffer );
	return pszBufer;
}

#define LAST(s) s[strlen(s)-1]

int WWIV_make_path(const char *s)
{
  char current_path[MAX_PATH], *p, *flp;

  p = flp = WWIV_STRDUP(s);
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
