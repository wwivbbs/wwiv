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


//
// Local function prototypes
//
void AddLineToSysopLogImpl(int cmd, const std::string& text);

#define LOG_STRING 0
#define LOG_CHAR   4

/*
* Creates sysoplog filename in s, from datestring.
*/

void GetSysopLogFileName(const char *d, char *pszLogFileName)
{
	sprintf(pszLogFileName, "%c%c%c%c%c%c.log", d[6], d[7], d[0], d[1], d[3], d[4]);
}

/*
* Returns instance (temporary) sysoplog filename in s.
*/

void GetTemporaryInstanceLogFileName(char *pszInstanceLogFileName)
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

	GetTemporaryInstanceLogFileName(szInstanceBaseName);
	sprintf(szInstanceLogFileName, "%s%s", syscfg.gfilesdir, szInstanceBaseName);

	if (WFile::Exists(szInstanceLogFileName))
	{
	    char szLogFileBaseName[MAX_PATH];

        GetSysopLogFileName(date(), szLogFileBaseName);
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
void AddLineToSysopLogImpl(int cmd, const std::string& text)
{
	static std::string::size_type midline = 0;
	static char s_szLogFileName[MAX_PATH];

    if ( !(syscfg.gfilesdir ) )
    {
        // TODO Use error log.
        return;
    }

	if (&syscfg.gfilesdir[0] == NULL)
	{
		// If we try to write we will throw a NPE.
		return;
	}

	if (!s_szLogFileName[0])
	{
		strcpy(s_szLogFileName, syscfg.gfilesdir);
		GetTemporaryInstanceLogFileName(s_szLogFileName + strlen(s_szLogFileName));
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
			std::string logLine;
		    if ( midline > 0 )
		    {
				logLine = "\r\n";
				logLine += text;
			    midline = 0;
		    }
		    else
		    {
				logLine = text;
		    }
			logLine += "\r\n";
			logFile.Write( logLine );
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
			std::string logLine;
		    if ( midline == 0 || ( midline + 2 + text.length() ) > 78 )
		    {
				logLine = (midline) ? "\r\n   " : "  ";
				midline = 3 + text.length();
		    }
		    else
		    {
				logLine = ", ";
				midline += 2 + text.length();
		    }
			logLine += text;
            logFile.Write( logLine );
		    logFile.Close();
        }
		break;
	default:
		{
			std::ostringstream os;
			os << "Invalid Command passed to sysoplog::AddLineToSysopLogImpl, Cmd = " << cmd;
			AddLineToSysopLogImpl( LOG_STRING, os.str() );
		} break;
	}
}

/*
* Writes a string to the sysoplog, if user online and EffectiveSl < 255.
*/

void sysopchar(const std::string text)
{
	if ( ( incom || GetSession()->GetEffectiveSl() != 255 ) && !text.empty() )
	{
		AddLineToSysopLogImpl( LOG_CHAR, text );
	}
}

/*
* Writes a string to the sysoplog, if EffectiveSl < 255 and user online,
* indented a few spaces.
*/

void sysoplog( const std::string text, bool bIndent )
{
	if ( bIndent )
	{
		std::ostringstream os;
		os << "   " << text;
		AddLineToSysopLogImpl( LOG_STRING, os.str() );
	}
	else
	{
		AddLineToSysopLogImpl( LOG_STRING, text );
	}
}

// printf style function to write to the sysop log
void sysoplogf( const char *pszFormat, ... )
{
    va_list ap;
    char szBuffer[2048];

    va_start( ap, pszFormat );
    WWIV_VSNPRINTF( szBuffer, sizeof( szBuffer ), pszFormat, ap );
    va_end( ap );
    sysoplog( szBuffer );
}


// printf style function to write to the sysop log
void sysoplogfi( bool bIndent, const char *pszFormat, ... )
{
    va_list ap;
    char szBuffer[2048];

    va_start( ap, pszFormat );
    WWIV_VSNPRINTF( szBuffer, sizeof( szBuffer ), pszFormat, ap );
    va_end( ap );
    sysoplog( szBuffer, bIndent );
}

