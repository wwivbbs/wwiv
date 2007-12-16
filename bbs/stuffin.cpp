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
#include <vector>

static void stuff_in_num( std::string& outBuffer, char *pszFormatString, int nNumber );
static void append_filename( int filenameType, std::string& out);

// Replacable parameters:
// ~~~~~~~~~~~~~~~~~~~~~~
//
// Param     Description                       Example
// ---------------------------------------------------------------------
//  %%       A single '%'                      "%"
//  %1-%5    Specified passed-in parameter
//  %A       callinfo full pathname            "c:\wwiv\temp\callinfo.bbs"
//  %C       chain.txt full pathname           "c:\wwiv\temp\chain.txt"
//  %D       doorinfo full pathname            "c:\wwiv\temp\dorinfo1.def"
//  %E       door32.sys full pathname          "C:\wwiv\temp\door32.sys"
//  %K       gfiles comment file for archives  "c:\wwiv\gfiles\comment.txt"
//  %M       Modem baud rate                   "14400"
//  %N       Instance number                   "1"
//  %O       pcboard full pathname             "c:\wwiv\temp\pcboard.sys"
//  %P       Com port number                   "1"
//  %R       door full pathname                "c:\wwiv\temp\door.sys"
//  %S       Com port baud rate                "38400"
//  %T       Time remaining (min)              "30"
//

/**
 * @todo Document this
 */
void stuff_in( std::string& outCommandLine, const std::string& inCommandLine, const char *pszArg1, 
			   const char *pszArg2, const char *pszArg3, const char *pszArg4, const char *pszArg5 )
{
	std::vector<std::string> flags;
	flags.push_back( pszArg1 );
	flags.push_back( pszArg2 );
	flags.push_back( pszArg3 );
	flags.push_back( pszArg4 );
	flags.push_back( pszArg5 );

	std::string::const_iterator iter = inCommandLine.begin();
	while ( iter != inCommandLine.end() )
    {
        if ( *iter == '%')
        {
            ++iter;
			char ch = wwiv::UpperCase<char>(*iter);
            switch (ch)
            {
                // used: %12345ABCDIMNOPRST

                // fixed strings
            case '%':
				outCommandLine += '%';
                break;
                // replacable parameters
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
				outCommandLine += flags.at(ch - '1');
                break;
                // call-specific numbers
            case 'M':
                stuff_in_num(outCommandLine, "%u", modem_speed);
                break;
            case 'K':
                outCommandLine += syscfg.gfilesdir;
				outCommandLine += COMMENT_TXT;
                break;
            case 'P':
                stuff_in_num( outCommandLine, "%u", incom ? syscfgovr.primaryport : 0 );
                break;
            case 'N':
                stuff_in_num( outCommandLine, "%u", GetApplication()->GetInstanceNumber() );
                break;
            case 'S':
				stuff_in_num( outCommandLine, "%lu", ( com_speed == 1 ) ? 115200 : com_speed );
                break;
            case 'T':
                {
                    double d = nsl();
                    if (d < 0)
				    {
                        d += HOURS_PER_DAY_FLOAT * SECONDS_PER_HOUR_FLOAT;
				    }
                    stuff_in_num( outCommandLine, "%u", static_cast<int>( d ) / MINUTES_PER_HOUR );
                }
                break;
                // chain.txt type filenames
            case 'C':
				append_filename( CHAINFILE_CHAIN, outCommandLine );
                break;
            case 'D':
				append_filename( CHAINFILE_DORINFO, outCommandLine );
                break;
            case 'O':
				append_filename( CHAINFILE_PCBOARD, outCommandLine );
                break;
            case 'A':
				append_filename( CHAINFILE_CALLINFO, outCommandLine );
                break;
            case 'R':
				append_filename( CHAINFILE_DOOR, outCommandLine );
                break;
            case 'E':
				append_filename( CHAINFILE_DOOR32, outCommandLine );
                break;
            }
            ++iter;
        }
        else
        {
			outCommandLine += *iter++;
        }
    }
}

/**
 * @todo Document this
 */
static void stuff_in_num( std::string& outBuffer, char *pszFormatString, int nNumber )
{
    char szTemp[ MAX_PATH ];

	WWIV_ASSERT( pszFormatString );

    snprintf( szTemp, sizeof( szTemp ), pszFormatString, nNumber );
	outBuffer += szTemp;
}


static void append_filename( int filenameType, std::string& out )
{
	std::string fileName;
	create_filename( filenameType, fileName );
	out += fileName;
}
