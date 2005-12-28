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


static const unsigned char *valid_letters =
( unsigned char * ) "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ€‚ƒ„…†‡ˆ‰Š‹ŒŽ’“”•–—˜™š¡¢£¤¥";


/**
 * This will input a line of data, maximum nMaxLength characters long, terminated
 * by a C/R.  if (lc) is non-zero, lowercase is allowed, otherwise all
 * characters are converted to uppercase.
 * @param pszOutText the text entered by the user (output value)
 * @param nMaxLength Maximum length to allow for the input text
 * @param lc The case to return, this can be INPUT_MODE_FILE_UPPER, INPUT_MODE_FILE_MIXED, INPUT_MODE_FILE_PROPER, or INPUT_MODE_FILE_NAME
 * @param crend Add a CR to the end of the input text
 * @param bAutoMpl Call GetSession()->bout.ColorizedInputField(nMaxLength) automatically.
 */
void input1( char *pszOutText, int nMaxLength, int lc, bool crend, bool bAutoMpl )
{
    if ( bAutoMpl )
    {
        GetSession()->bout.ColorizedInputField( nMaxLength );
    }

    int curpos = 0, in_ansi = 0;
    static unsigned char chLastChar;
	bool done = false;

    while ( !done && !hangup )
    {
        unsigned char chCurrent = getkey();

        if (curpos)
        {
            bChatLine = true;
        }
        else
        {
            bChatLine = false;
        }

        if (in_ansi)
        {
            if ( in_ansi == 1 &&  chCurrent != '[' )
            {
                in_ansi = 0;
            }
            else
            {
                if ( in_ansi == 1 )
                {
                    in_ansi = 2;
                }
                else
                {
                    if ( ( chCurrent < '0' || chCurrent > '9' ) && chCurrent != ';' )
                    {
                        in_ansi = 3;
                    }
                    else
                    {
                        in_ansi = 2;
                    }
                }
            }
        }
        if ( !in_ansi )
        {
            if ( chCurrent > 31 )
            {
                switch ( lc )
                {
                case INPUT_MODE_FILE_UPPER:
                    chCurrent = upcase( chCurrent );
                    break;
                case INPUT_MODE_FILE_MIXED:
                    break;
                case INPUT_MODE_FILE_PROPER:
                    chCurrent = upcase( chCurrent );
                    if ( curpos )
                    {
                        const char *ss = strchr( reinterpret_cast<const char*>( valid_letters ), pszOutText[curpos - 1] );
                        if ( ss != NULL || pszOutText[curpos - 1] == 39 )
                        {
                            if ( curpos < 2 || pszOutText[curpos - 2] != 77 || pszOutText[curpos - 1] != 99 )
                            {
                                chCurrent = locase( chCurrent );
                            }
                        }
                    }
                    break;
                case INPUT_MODE_FILE_NAME:
                    if ( strchr( "/\\+ <>|*?.,=\";:[]", chCurrent ) )
					{
                        chCurrent = 0;
					}
                    else
					{
                        chCurrent = upcase( chCurrent );
					}
                    break;
                }
                if ( curpos < nMaxLength && chCurrent )
                {
                    pszOutText[curpos++] = chCurrent;
                    bputch( chCurrent );
                }
            }
            else
            {
                switch ( chCurrent )
                {
				case SOFTRETURN:
					if ( chLastChar != RETURN )
					{
						//
						// Handle the "one-off" case where UNIX telnet clients only return
						// '\n' (#10) instead of '\r' (#13).
						//
						pszOutText[curpos] = '\0';
						done = true;
                        echo = true;
						if ( newline || crend )
						{
							GetSession()->bout.NewLine();
						}
					}
					break;
                case CN:
                case RETURN:
                    pszOutText[curpos] = '\0';
					done = true;
                    echo = true;
                    if ( newline || crend )
                    {
                        GetSession()->bout.NewLine();
                    }
                    break;
                case CW:                          // Ctrl-W
                    if ( curpos )
                    {
                        do
                        {
                            curpos--;
                            GetSession()->bout.BackSpace();
                            if ( pszOutText[curpos] == CZ )
                            {
                                GetSession()->bout.BackSpace();
                            }
                        } while ( curpos && pszOutText[curpos - 1] != SPACE );
                    }
                    break;
                case CZ:
                    break;
                case BACKSPACE:
                    if ( curpos )
                    {
                        curpos--;
                        GetSession()->bout.BackSpace();
                        if ( pszOutText[curpos] == CZ )
                        {
                            GetSession()->bout.BackSpace();
                        }
                    }
                    break;
                case CU:
                case CX:
                    while ( curpos )
                    {
                        curpos--;
                        GetSession()->bout.BackSpace();
                        if ( pszOutText[curpos] == CZ )
                        {
                            GetSession()->bout.BackSpace();
                        }
                    }
                    break;
                case ESC:
                    in_ansi = 1;
                    break;
                }
				chLastChar = chCurrent;
            }
        }
        if ( in_ansi == 3 )
        {
            in_ansi = 0;
        }
    }
    if ( hangup )
    {
        pszOutText[0] = '\0';
    }
}


void input1( std::string &strOutText, int nMaxLength, int lc, bool crend, bool bAutoMpl )
{
    char szTempBuffer[ 255 ];
    WWIV_ASSERT( nMaxLength < sizeof( szTempBuffer ) );
    input1( szTempBuffer, nMaxLength, lc, crend, bAutoMpl );
    strOutText.assign( szTempBuffer );
}


void input( char *pszOutText, int nMaxLength, bool bAutoMpl )
// This will input an upper-case string
{
    input1( pszOutText, nMaxLength, INPUT_MODE_FILE_UPPER, true, bAutoMpl );
}


void input( std::string &strOutText, int nMaxLength, bool bAutoMpl )
// This will input an upper-case string
{
    char szTempBuffer[ 255 ];
    input( szTempBuffer, nMaxLength, bAutoMpl );
    strOutText.assign( szTempBuffer );
}


void inputl( char *pszOutText, int nMaxLength, bool bAutoMpl )
// This will input an upper or lowercase string of characters
{
    input1( pszOutText, nMaxLength, INPUT_MODE_FILE_MIXED, true, bAutoMpl );
}


void inputl( std::string &strOutText, int nMaxLength, bool bAutoMpl )
// This will input an upper or lowercase string of characters
{
    char szTempBuffer[ 255 ];
    WWIV_ASSERT( nMaxLength < sizeof( szTempBuffer ) );
    inputl( szTempBuffer, nMaxLength, bAutoMpl );
    strOutText.assign( szTempBuffer );
}

void input_password( std::string promptText, std::string &strOutPassword, int nMaxLength )
{
    GetSession()->bout << promptText;
    GetSession()->bout.ColorizedInputField( nMaxLength );
    echo = false;
    input1( strOutPassword, nMaxLength, INPUT_MODE_FILE_UPPER, true );
}


//==================================================================
// Function: Input1
//
// Purpose:  Input a line of text of specified length using a ansi
//           formatted input line
//
// Parameters:  *pszOutText		= variable to save the input to
//              *orgiText    	= line to edit.  appears in edit box
//              nMaxLength			= max characters allowed
//              bInsert			= insert mode false = off, true = on
//              mode			= formatting mode.
//								  INPUT_MODE_FILE_UPPER  INPUT_MODE_FILE_MIXED  INPUT_MODE_FILE_PROPER  INPUT_MODE_FILE_NAME  INPUT_MODE_DATE  INPUT_MODE_PHONE
//
// Returns: length of string
//==================================================================

int Input1(char *pszOutText, std::string origText, int nMaxLength, bool bInsert, int mode)
{
    char szTemp[ 255 ];
    int nLength, pos, i;
    const char dash = '-';
    const char slash = '/';

#if defined( _UNIX )
	input1(szTemp, nMaxLength, mode, true);
    strcpy(pszOutText, szTemp);
    return strlen(szTemp);
#endif // _UNIX

    if ( !okansi() )
    {
		input1(szTemp, nMaxLength, mode, true);
        strcpy(pszOutText, szTemp);
        return static_cast<unsigned char>( strlen( szTemp ) );
    }
	int nTopDataSaved = GetSession()->topdata;
	if ( GetSession()->topdata != WLocalIO::topdataNone )
    {
        GetSession()->topdata = WLocalIO::topdataNone;
		GetApplication()->UpdateTopScreen();
    }
    if ( mode == INPUT_MODE_DATE || mode == INPUT_MODE_PHONE )
    {
        bInsert = false;
    }
    int nTopLineSaved = GetSession()->localIO()->GetTopLine();
    GetSession()->localIO()->SetTopLine( 0 );
    pos = nLength = 0;
    szTemp[0] = '\0';
	
	nMaxLength = std::max<int>(nMaxLength, 80);
    GetSession()->bout.Color( 4 );
    int x = GetSession()->localIO()->WhereX() + 1;
    int y = GetSession()->localIO()->WhereY() + 1;

    GetSession()->bout.GotoXY(x, y);
    for (i = 0; i < nMaxLength; i++)
    {
        GetSession()->bout << "±";
    }
    GetSession()->bout.GotoXY( x, y );
    if ( !origText.empty() )
    {
        strcpy( szTemp, origText.c_str() );
        GetSession()->bout << szTemp;
        GetSession()->bout.GotoXY( x, y );
        pos = nLength = strlen(szTemp);
    }
    x = GetSession()->localIO()->WhereX() + 1;

    bool done = false;
    do
    {
        GetSession()->bout.GotoXY(pos + x, y);

        int c = get_kb_event( NUMBERS );

        switch (c)
        {
		case CX:								// Control-X
        case ESC:								// ESC
            if (nLength)
            {
                GetSession()->bout.GotoXY(nLength + x, y);
                while (nLength--)
                {
                    GetSession()->bout.GotoXY(nLength + x, y);
                    bputch('±');
                }
                nLength = pos = szTemp[0] = 0;
            }
            break;
        case COMMAND_LEFT:                    // Left Arrow
        case CS:
            if ((mode != INPUT_MODE_DATE) && (mode != INPUT_MODE_PHONE))
            {
                if (pos)
                    pos--;
            }
            break;
        case COMMAND_RIGHT:                   // Right Arrow
        case CD:
            if ((mode != INPUT_MODE_DATE) && (mode != INPUT_MODE_PHONE))
            {
                if ((pos != nLength) && (pos != nMaxLength))
                    pos++;
            }
            break;
        case CA:
        case COMMAND_HOME:                    // Home
        case CW:
            pos = 0;
            break;
        case CE:
        case COMMAND_END:                     // End
        case CP:
            pos = nLength;
            break;
        case COMMAND_INSERT:                  // Insert
            if (mode == INPUT_MODE_FILE_UPPER)
			{
                bInsert = !bInsert;
			}
            break;
        case COMMAND_DELETE:                  // Delete
        case CG:
            if ((pos == nLength) || (mode == INPUT_MODE_DATE) || (mode == INPUT_MODE_PHONE))
			{
                break;
			}
            for (i = pos; i < nLength; i++)
			{
                szTemp[i] = szTemp[i + 1];
			}
            nLength--;
            for (i = pos; i < nLength; i++)
			{
                bputch( szTemp[i] );
			}
            bputch('±');
            break;
        case BACKSPACE:                               // Backspace
            if (pos)
            {
                if (pos != nLength)
                {
                    if ((mode != INPUT_MODE_DATE) && (mode != INPUT_MODE_PHONE))
                    {
                        for (i = pos - 1; i < nLength; i++)
						{
                            szTemp[i] = szTemp[i + 1];
						}
                        pos--;
                        nLength--;
                        GetSession()->bout.GotoXY(pos + x, y);
                        for (i = pos; i < nLength; i++)
						{
                            bputch( szTemp[i] );
						}
                        GetSession()->bout << "±";
                    }
                }
                else
                {
                    GetSession()->bout.GotoXY(pos - 1 + x, y);
                    GetSession()->bout << "±";
                    pos = --nLength;
                    if (((mode == INPUT_MODE_DATE) && ((pos == 2) || (pos == 5))) ||
                        ((mode == INPUT_MODE_PHONE) && ((pos == 3) || (pos == 7))))
                    {
                        GetSession()->bout.GotoXY(pos - 1 + x, y);
                        GetSession()->bout << "±";
                        pos = --nLength;
                    }
                }
            }
            break;
        case RETURN:                              // Enter
            done = true;
            break;
        default:                              // All others < 256
            if ( c < 255 && c > 31 && ( ( bInsert && nLength < nMaxLength ) || ( !bInsert && pos < nMaxLength) ) )
            {
                if (mode != INPUT_MODE_FILE_MIXED)
                {
					c = wwiv::UpperCase<unsigned char> ( static_cast< unsigned char >( c ) );
                }
                if (mode == INPUT_MODE_FILE_NAME)
				{
                    if (strchr("/\\+ <>|*?.,=\";:[]", c))
					{
                        c = 0;
					}
                }
                if ( mode == INPUT_MODE_FILE_PROPER && pos )
                {
                    const char *ss = strchr( reinterpret_cast<char*>( const_cast<unsigned char*>( valid_letters ) ), c );
                    // if it's a valid char and the previous char was a space
                    if ( ss != NULL && szTemp[pos - 1] != 32 )
                    {
                        c = locase( static_cast< unsigned char >( c ) );
                    }
                }
                if ( mode == INPUT_MODE_DATE && ( pos == 2 || pos == 5 ) )
                {
                    bputch(slash);
                    for (i = nLength++; i >= pos; i--)
					{
                        szTemp[i + 1] = szTemp[i];
					}
                    szTemp[pos++] = slash;
                    GetSession()->bout.GotoXY( pos + x, y );
                    GetSession()->bout << &szTemp[pos];
                }
                if ( mode == INPUT_MODE_PHONE && ( pos == 3 || pos == 7 ) )
                {
                    bputch(dash);
                    for (i = nLength++; i >= pos; i--)
					{
                        szTemp[i + 1] = szTemp[i];
					}
                    szTemp[pos++] = dash;
                    GetSession()->bout.GotoXY(pos + x, y);
                    GetSession()->bout << &szTemp[pos];
                }
                if ( ( mode == INPUT_MODE_DATE  && c != slash ||
					   mode == INPUT_MODE_PHONE && c != dash ) ||
                    ( mode != INPUT_MODE_DATE && mode != INPUT_MODE_PHONE && c != 0 ) )
                {
                    if ( !bInsert || pos == nLength )
                    {
                        bputch( static_cast< unsigned char >( c ) );
                        szTemp[pos++] = static_cast< char > ( c );
                        if (pos > nLength)
                        {
                            nLength++;
                        }
                    }
					else
                    {
                        bputch( ( unsigned char ) c );
                        for (i = nLength++; i >= pos; i--)
						{
                            szTemp[i + 1] = szTemp[i];
						}
                        szTemp[pos++] = ( char ) c;
                        GetSession()->bout.GotoXY(pos + x, y);
                        GetSession()->bout << &szTemp[pos];
                    }
                }
            }
            break;
    }
    szTemp[nLength] = '\0';
    CheckForHangup();
  } while ( !done && !hangup );
  if (nLength)
  {
      strcpy( pszOutText, szTemp );
  }
  else
  {
      pszOutText[0] = '\0';
  }

  GetSession()->topdata = nTopDataSaved;
  GetSession()->localIO()->SetTopLine( nTopLineSaved );

  GetSession()->bout.Color( 0 );
  return nLength;
}


int Input1( std::string &strOutText, std::string origText, int nMaxLength, bool bInsert, int mode )
{
    char szTempBuffer[ 255 ];
    WWIV_ASSERT( nMaxLength < sizeof( szTempBuffer ) );

    int nLength = Input1( szTempBuffer, origText, nMaxLength, bInsert, mode );
    strOutText.assign( szTempBuffer );
    return nLength;
}

