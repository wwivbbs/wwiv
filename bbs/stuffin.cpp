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


static void stuff_in_num( char *pszOutBuffer, char *pszFormatString,
                          int nNumber );
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
void stuff_in( char *pszOutCommandLine, const char *pszInCommandLine,
               const char *pszFlag1, const char *pszFlag2,
               const char *pszFlag3, const char *pszFlag4,
               const char *pszFlag5 )
{
    int nInPtr = 0, nOutPtr = 0;

	WWIV_ASSERT(pszOutCommandLine);
	WWIV_ASSERT(pszInCommandLine);

    while (pszInCommandLine[nInPtr] != '\0')
    {
        if (pszInCommandLine[nInPtr] == '%')
        {
            ++nInPtr;
            pszOutCommandLine[nOutPtr] = '\0';
            switch (wwiv::UpperCase<char>(pszInCommandLine[nInPtr]))
            {
                // used: %12345ABCDIMNOPRST

                // fixed strings
            case '%':
                {
                    strcat(pszOutCommandLine, "%");
                }
                break;
                // replacable parameters
            case '1':
				WWIV_ASSERT(pszFlag1);
				if (pszFlag1)
				{
					strcat(pszOutCommandLine, pszFlag1);
				}
                break;
            case '2':
				WWIV_ASSERT(pszFlag2);
				if (pszFlag2)
				{
					strcat(pszOutCommandLine, pszFlag2);
				}
                break;
            case '3':
				WWIV_ASSERT(pszFlag3);
				if (pszFlag3)
				{
					strcat(pszOutCommandLine, pszFlag3);
				}
                break;
            case '4':
				WWIV_ASSERT(pszFlag4);
				if (pszFlag4)
				{
					strcat(pszOutCommandLine, pszFlag4);
				}
                break;
            case '5':
				WWIV_ASSERT(pszFlag5);
				if (pszFlag5)
				{
					strcat(pszOutCommandLine, pszFlag5);
				}
                break;
                // call-specific numbers
            case 'M':
                {
                    stuff_in_num(pszOutCommandLine, "%u", modem_speed);
                }
                break;
            case 'K':
                {
                    char szFileName[MAX_PATH];
                    sprintf(szFileName,"%s%s", syscfg.gfilesdir, COMMENT_TXT);
                    strcat(pszOutCommandLine, szFileName);
                }
                break;
            case 'P':
                {
                    stuff_in_num(pszOutCommandLine, "%u", incom ? syscfgovr.primaryport : 0);
                }
                break;
            case 'N':
                {
                    stuff_in_num( pszOutCommandLine, "%u", app->GetInstanceNumber() );
                }
                break;
            case 'S':
                {
                    if (com_speed == 1)
                    {
                        strcat(pszOutCommandLine, "115200");
                    }
                    else
                    {
                        stuff_in_num(pszOutCommandLine, "%lu", com_speed);
                    }
                }
                break;
            case 'T':
                {
                    double d = nsl();
                    if (d < 0)
				    {
                        d += HOURS_PER_DAY_FLOAT * SECONDS_PER_HOUR_FLOAT;
				    }
                    stuff_in_num(pszOutCommandLine, "%u", static_cast<int>( d ) / MINUTES_PER_HOUR );
                }
                break;
                // chain.txt type filenames
            case 'C':
                {
                    create_filename(CHAINFILE_CHAIN, pszOutCommandLine + nOutPtr);
                }
                break;
            case 'D':
                {
                    create_filename(CHAINFILE_DORINFO, pszOutCommandLine + nOutPtr);
                }
                break;
            case 'O':
                {
                    create_filename(CHAINFILE_PCBOARD, pszOutCommandLine + nOutPtr);
                }
                break;
            case 'A':
                {
                    create_filename(CHAINFILE_CALLINFO, pszOutCommandLine + nOutPtr);
                }
                break;
            case 'R':
                {
                    create_filename(CHAINFILE_DOOR, pszOutCommandLine + nOutPtr);
                }
                break;
            case 'E':
                {
                    create_filename( CHAINFILE_DOOR32, pszOutCommandLine + nOutPtr );
                }
                break;
            }
            nOutPtr = strlen( pszOutCommandLine );
            nInPtr++;
        }
        else
        {
            pszOutCommandLine[nOutPtr++] = pszInCommandLine[nInPtr++];
        }
    }
    pszOutCommandLine[nOutPtr] = '\0';
}


/**
 * @todo Document this
 */
static void stuff_in_num( char *pszOutBuffer, char *pszFormatString,
                          int nNumber )
{
    char szTemp[ 255 ];

	WWIV_ASSERT( pszOutBuffer );
	WWIV_ASSERT( pszFormatString );

    sprintf( szTemp, pszFormatString, nNumber );
    strcat( pszOutBuffer, szTemp );
}


