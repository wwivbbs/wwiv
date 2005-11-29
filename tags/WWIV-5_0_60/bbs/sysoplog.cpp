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


//
// Local function prototypes
//
void sl1(int cmd, const char *pszLogText);

#define LOG_STRING 0
#define LOG_CHAR   4

/*
* Creates sysoplog filename in s, from datestring.
*/

void slname(const char *d, char *pszLogFileName)
{
	sprintf(pszLogFileName, "%c%c%c%c%c%c.log", d[6], d[7], d[0], d[1], d[3], d[4]);
}

/*
* Returns instance (temporary) sysoplog filename in s.
*/

void islname(char *pszInstanceLogFileName)
{
	sprintf(pszInstanceLogFileName, "inst-%3.3u.log", GetApplication()->GetInstanceNumber());
}

#define CAT_BUFSIZE 8192

/*
* Copies temporary/instance sysoplog to primary sysoplog file.
*/

void catsl()
{
    char szInstanceBaseName[MAX_PATH];
    char szInstanceLogFileName[MAX_PATH];

	islname(szInstanceBaseName);
	sprintf(szInstanceLogFileName, "%s%s", syscfg.gfilesdir, szInstanceBaseName);

	if (WFile::Exists(szInstanceLogFileName))
	{
	    char szLogFileBaseName[MAX_PATH];

        slname(date(), szLogFileBaseName);
        WFile wholeLogFile( syscfg.gfilesdir, szLogFileBaseName );

		char* pLogBuffer = static_cast<char *>( BbsAllocA( CAT_BUFSIZE ) );
		if ( pLogBuffer )
		{
            if ( wholeLogFile.Open( WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile, WFile::shareUnknown, WFile::permReadWrite ) )
			{
                wholeLogFile.Seek( 0, WFile::seekBegin );
                wholeLogFile.Seek( 0, WFile::seekEnd );

                WFile instLogFile( szInstanceLogFileName );
                if ( instLogFile.Open( WFile::modeReadOnly | WFile::modeBinary ) )
				{
                    int nNumRead = 0;
					do
					{
                        nNumRead = instLogFile.Read( pLogBuffer, CAT_BUFSIZE );
						if (nNumRead > 0)
						{
                            wholeLogFile.Write( pLogBuffer, nNumRead );
						}
					} while ( nNumRead == CAT_BUFSIZE );

                    instLogFile.Close();
                    instLogFile.Delete( false );
				}
                wholeLogFile.Close();
			}
			BbsFreeMemory( pLogBuffer );
		}
	}
}

/*
* Writes a line to the sysoplog.
*/
void sl1(int cmd, const char *pszLogText)
{
	static int midline = 0;
	static char s_szLogFileName[MAX_PATH];
	char szLogLine[ 255 ];

    if ( !(syscfg.gfilesdir ) )
    {
        // TODO Use error log.
        return;
    }

	WWIV_ASSERT(pszLogText);

	if (&syscfg.gfilesdir[0] == NULL)
	{
		// If we try to write we will throw a NPE.
		return;
	}

	if (!s_szLogFileName[0])
	{
		strcpy(s_szLogFileName, syscfg.gfilesdir);
		islname(s_szLogFileName + strlen(s_szLogFileName));
	}
	switch (cmd)
	{
    case LOG_STRING:    // Write line to sysop's log
        {
            WFile logFile( s_szLogFileName );
            if ( !logFile.Open( WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile, WFile::shareUnknown, WFile::permReadWrite ) )
		    {
			    return;
		    }
            if ( logFile.GetLength() )
		    {
                logFile.Seek( -1L, WFile::seekEnd );
                char chLastChar;
                logFile.Read( &chLastChar, 1 );
			    if ( chLastChar == CZ )
                {
                    logFile.Seek( -1L, WFile::seekEnd );
                }
		    }
		    if ( midline > 0 )
		    {
			    sprintf( szLogLine, "\r\n%s", pszLogText );
			    midline = 0;
		    }
		    else
		    {
			    sprintf(szLogLine, "%s", pszLogText);
		    }
		    int nLineLen = strlen(szLogLine);
		    szLogLine[nLineLen++] = '\r';
		    szLogLine[nLineLen++] = '\n';
		    szLogLine[nLineLen] = 0;
            logFile.Write( szLogLine, nLineLen );
            logFile.Close();
        }
		break;
    case LOG_CHAR:
        {
            WFile logFile( s_szLogFileName );
            if ( !logFile.Open( WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile, WFile::shareUnknown, WFile::permReadWrite ) )
		    {
			    // sysop log ?
			    return;
		    }
            if ( logFile.GetLength() )
		    {
                logFile.Seek( -1L, WFile::seekEnd );
                char chLastChar;
                logFile.Read( &chLastChar, 1 );
			    if ( chLastChar == CZ )
                {
                    logFile.Seek( -1L, WFile::seekEnd );
                }
		    }
		    if ( midline == 0 || ( midline + 2 + strlen( pszLogText ) ) > 78 )
		    {
			    strcpy( szLogLine, midline ? "\r\n   " : "  " );
			    strcat( szLogLine, pszLogText );
			    midline = 3 + strlen( pszLogText );
		    }
		    else
		    {
			    strcpy( szLogLine, ", " );
			    strcat( szLogLine, pszLogText );
			    midline += ( 2 + strlen( pszLogText ) );
		    }
		    int nLineLen = strlen( szLogLine );
            logFile.Write( szLogLine, nLineLen );
		    logFile.Close();
        }
		break;
	default:
		{
			char szTempMsg[81];
			sprintf( szTempMsg, "Invalid Command passed to sysoplog::sl1, Cmd = %d", cmd );
			sl1( LOG_STRING, szTempMsg );
		} break;
	}
}

/*
* Writes a string to the sysoplog, if user online and EffectiveSl < 255.
*/

void sysopchar(const char *pszLogText)
{
    if ( ( incom || GetSession()->GetEffectiveSl() != 255 ) && pszLogText[0] )
	{
		sl1( LOG_CHAR, pszLogText );
	}
}

/*
* Writes a string to the sysoplog, if EffectiveSl < 255 and user online,
* indented a few spaces.
*/

void sysoplog( const char *pszLogText, bool bIndent )
{
	WWIV_ASSERT( pszLogText );

	if ( bIndent )
	{
    	char szBuffer[ 255 ];
		sprintf( szBuffer, "   %s", pszLogText );
		sl1( LOG_STRING, szBuffer );
	}
	else
	{
		sl1( LOG_STRING, pszLogText );
	}
}

// printf style function to write to the sysop log
void sysoplogf( const char *pszFormat, ... )
{
    va_list ap;
    char szBuffer[2048];

    va_start( ap, pszFormat );
    vsnprintf( szBuffer, sizeof( szBuffer ), pszFormat, ap );
    va_end( ap );
    sysoplog( szBuffer );
}


// printf style function to write to the sysop log
void sysoplogfi( bool bIndent, const char *pszFormat, ... )
{
    va_list ap;
    char szBuffer[2048];

    va_start( ap, pszFormat );
    vsnprintf( szBuffer, sizeof( szBuffer ), pszFormat, ap );
    va_end( ap );
    sysoplog( szBuffer, bIndent );
}

