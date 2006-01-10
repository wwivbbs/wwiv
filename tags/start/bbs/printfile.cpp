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


/**
 * Prints the file pszFileName.  Returns true if the file exists and is not
 * zero length.  Returns false if the file does not exist or is zero length
 *
 * @param pszFileName Name of the file to display
 * @param bAbortable If true, a keyboard input may abort the display
 * @param bForcePause Should pauses be used even for ANSI files - Normally
 *        pause on screen is disabled for ANSI files.
 *
 * @return true if the file exists and is not zero length
 */
bool printfile( const char *pszFileName, bool bAbortable, bool bForcePause )
{
	char szFileName[ MAX_PATH ];

	WWIV_ASSERT( pszFileName );

	if ( WFile::Exists( pszFileName ) )
	{
		strcpy( szFileName, pszFileName );
	}
	else
	{
		sprintf( szFileName, "%s%s", sess->pszLanguageDir, pszFileName );
		if (strchr(szFileName, '.') == NULL)
		{
			char* pszTempFileName = szFileName + strlen(szFileName);
			if ( sess->thisuser.hasAnsi() )
			{
				if ( sess->thisuser.hasColor() )
				{
					strcpy(pszTempFileName, ".ans");
					if (!WFile::Exists(szFileName))
					{
						*pszTempFileName = '\0';
					}
				}
				if (!(*pszTempFileName))
				{
					strcpy(pszTempFileName, ".b&w");
					if (!WFile::Exists(szFileName))
					{
						strcpy(pszTempFileName, ".msg");
					}
				}
			}
			else
			{
				strcpy(pszTempFileName, ".msg");
			}
		}
		if ( !WFile::Exists( pszFileName ) )
		{
			sprintf( szFileName, "%s%s", syscfg.gfilesdir, pszFileName );
		}
		if (strchr(szFileName, '.') == NULL)
		{
			char* pszTempFileName2 = szFileName + strlen(szFileName);
			if ( sess->thisuser.hasAnsi() )
			{
				if ( sess->thisuser.hasColor() )
				{
					strcpy(pszTempFileName2, ".ans");
					if (!WFile::Exists(szFileName))
					{
						pszTempFileName2[0] = 0;
					}
				}
				if (!pszTempFileName2[0])
				{
					strcpy(pszTempFileName2, ".b&w");
					if (!WFile::Exists(szFileName))
					{
						strcpy(pszTempFileName2, ".msg");
					}
				}
			}
			else
			{
				strcpy(pszTempFileName2, ".msg");
			}
		}
	}

	long lFileSize;
	char* ss = get_file(szFileName, &lFileSize);

	if ( ss != NULL )
	{
		long lCurPos = 0;
    	bool bHasAnsi = false;
		while (lCurPos < lFileSize && !hangup)
		{
			if ( ss[lCurPos] == ESC )
			{
				// if we have an ESC, then this file probably contains
                // an ansi sequence
				bHasAnsi = true;
			}
			if ( bHasAnsi && !bForcePause )
			{
				// If this is an ANSI file, then don't pause
                // (since we may be moving around
				// on the screen, unless the caller tells us to pause anyway)
				lines_listed = 0;
			}
			bputch( ss[lCurPos++], true );
            if ( bAbortable )
            {
                bool next = false, abort = false;
                checka( &abort, &next );
                if ( abort )
                {
                    break;
                }
			    if ( bkbhit() )
			    {
				    char ch = bgetch();
				    if (ch == ' ')
				    {
					    break;
				    }
			    }
            }
		}
        FlushOutComChBuffer();
		BbsFreeMemory( ss );
		// If the file is empty, lets' return false here since nothing
        // was displayed.
		return ( lFileSize > 0 ) ? true : false;
	}
	return false;
}


/**
 * Displays a file locally, using LIST util if so defined in WWIV.INI,
 * otherwise uses normal TTY output.
 */
void print_local_file(const char *ss, const char *ss1)
{
    char szCmdLine[ MAX_PATH ];
	char szCmdLine2[ MAX_PATH ];

	WWIV_ASSERT( ss );

    char *pszTempSS = strdup( ss );
	char *bs = strchr( pszTempSS, WWIV_FILE_SEPERATOR_CHAR );
	if ( ( syscfg.sysconfig & sysconfig_list ) && !incom )
	{
		if (!bs)
		{
            char * pszTempSS1 = strdup( ss1 );
			sprintf(szCmdLine, "%s %s%s", "LIST", syscfg.gfilesdir, ss);
			if (ss1[0])
			{
				bs = strchr( pszTempSS1, WWIV_FILE_SEPERATOR_CHAR);
				if (!bs)
				{
					sprintf( szCmdLine2, "%s %s%s", szCmdLine,
                             syscfg.gfilesdir, ss1);
				}
				else
				{
					sprintf(szCmdLine2, "%s %s", szCmdLine, ss1);
				}
				strcpy(szCmdLine, szCmdLine2);
			}
            BbsFreeMemory( pszTempSS1 );
		}
		else
		{
			sprintf(szCmdLine, "%s %s", "LIST", ss);
			if (ss1[0])
			{
                char * pszTempSS1 = strdup( ss1 );
				bs = strchr(pszTempSS1, WWIV_FILE_SEPERATOR_CHAR);
				if (!bs)
				{
					sprintf( szCmdLine2, "%s %s%s", szCmdLine,
                             syscfg.gfilesdir, ss1);
				}
				else
				{
					sprintf(szCmdLine2, "%s %s", szCmdLine, ss1);
				}
				strcpy(szCmdLine, szCmdLine2);
                BbsFreeMemory ( pszTempSS1 );
			}
		}
		ExecuteExternalProgram( szCmdLine, EFLAG_NONE );
		if ( sess->IsUserOnline() )
		{
			app->localIO->LocalCls();
			app->localIO->UpdateTopScreen();
		}
	}
	else
	{
		printfile(ss);
		nl( 2 );
		pausescr();
	}
    BbsFreeMemory( pszTempSS );
}

