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
#include "WStringUtils.h"

/**
 * The default list of computer types
 */
static const char *default_ctypes[] =
{
    "IBM PC (8088)",
    "IBM PS/2",
    "IBM AT (80286)",
    "IBM AT (80386)",
    "IBM AT (80486)",
    "Pentium",
    "Apple 2",
    "Apple Mac",
    "Commodore Amiga",
    "Commodore",
    "Atari",
    "Other",
    0L,
};


static const int MAX_DEFAULT_CTYPE_VALUE = 11;


/**
 * Clears the local and remote screen using ANSI (if enabled), otherwise DEC 12
 */
void ClearScreen()
{
    if ( okansi() )
    {
        GetSession()->bout << "\x1b[2J";
        goxy( 1, 1 );
    }
    else
    {
		bputch( CL );
    }
}



/**
 * Display character x repeated amount times in nColor, and if bAddNL is true
 * display a new line character.
 * @param x The Character to repeat
 * @param amount The number of times to repeat x
 * @param nColor the color in which to display the string
 * @param bAddNL if true, add a new line character at the end.
 */
void repeat_char( char x, int amount, int nColor, bool bAddNL )
{
    ansic( nColor );
    GetSession()->bout << charstr( amount, x );
    if ( bAddNL )
    {
        nl();
    }
}



/**
 * Returns the computer type string for computer type number num.
 *
 * @param num The computer type number for which to return the name
 *
 * @return The text describing computer type num
 */
const char *ctypes(int num)
{
    static char szCtype[81];

    if (ini_init("WWIV.INI", "CTYPES", NULL))
    {
        char szCompType[ 100 ];
        sprintf(szCompType, "COMP_TYPE[%d]", num+1);
        char* ss =ini_get(szCompType, -1, NULL);
        if (ss && *ss)
        {
            strcpy(szCtype, ss);
            if (ss)
            {
                ini_done();
                return(szCtype);
            }
        }
        ini_done();
        return NULL;
    }
    if ( ( num < 0 ) || ( num > MAX_DEFAULT_CTYPE_VALUE ) )
    {
        return NULL;
    }
    return(default_ctypes[num]);
}


/**
 * Displays s which checking for abort and next
 * @see checka
 * <em>Note: osan means Output String And Next</em>
 *
 * @param pszText The text to display
 * @param abort The abort flag (Output Parameter)
 * @param next The next flag (Output Parameter)
 */
void osan(const char *pszText, bool *abort, bool *next)
{
	CheckForHangup();
	checka(abort, next);

    int nCurPos = 0;
	while ( pszText[nCurPos] && !(*abort) && !hangup )
	{
		bputch( pszText[nCurPos++], true );   // RF20020927 use buffered bputch
		checka( abort, next );
	}
    FlushOutComChBuffer();
}


/**
 * Displays pszText in color nWWIVColor which checking for abort and next with a nl
 * @see checka
 * <em>Note: osan means Output String And Next</em>
 *
 * @param nWWIVColor The WWIV color code to use.
 * @param pszText The text to display
 * @param abort The abort flag (Output Parameter)
 * @param next The next flag (Output Parameter)
 */
void plan(int nWWIVColor, const char *pszText, bool *abort, bool *next)
{
    ansic( nWWIVColor );
    osan( pszText, abort, next );
	if (!(*abort))
	{
		nl();
	}
}

/**
 * @todo Document this
 */
char *strip_to_node( const char *txt, char *pszOutBuffer )
{
	if (strstr(txt, "@") != NULL)
    {
		bool ok = true;
		for (int i = 0; i < wwiv::stringUtils::GetStringLength(txt); i++)
        {
			if (ok)
            {
				pszOutBuffer[i] = txt[i];
				pszOutBuffer[i + 1] = 0;
			}
			if (txt[i + 2] == '#')
            {
				ok = false;
            }
		}
		return pszOutBuffer;
	}
	else if (strstr(txt, "AT") != NULL)
    {
		bool ok = true;
		for (int i = 2; i < wwiv::stringUtils::GetStringLength(txt); i++)
        {
			if (ok)
            {
				pszOutBuffer[i - 2] = txt[i];
				pszOutBuffer[i - 1] = 0;
			}
			if (txt[i + 1] == '`')
            {
				ok = false;
            }
		}
		return pszOutBuffer;
	}
    strcpy( pszOutBuffer, txt );
	return pszOutBuffer;
}
