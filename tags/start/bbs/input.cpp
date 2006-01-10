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


static const unsigned char *valid_letters =
( unsigned char * ) "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZÄÅÇÉÑÖÜáàâäãåçéèêíìîïñóòôö°¢£§•";


/**
 * This will input a line of data, maximum maxlen characters long, terminated
 * by a C/R.  if (lc) is non-zero, lowercase is allowed, otherwise all
 * characters are converted to uppercase.
 * @param pszOutText the text entered by the user (output value)
 * @param maxlen Maximum length to allow for the input text
 * @param lc The case to return, this can be UPPER, MIXED, PROPER, or FILE_NAME
 * @param crend Add a CR to the end of the input text
 * @param bAutoMpl Call mpl(maxlen) automatically.
 */
void input1( char *pszOutText, int maxlen, int lc, bool crend, bool bAutoMpl )
{
    if ( bAutoMpl )
    {
        mpl( maxlen );
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
                case UPPER:
                    chCurrent = upcase( chCurrent );
                    break;
                case MIXED:
                    break;
                case PROPER:
                    chCurrent = upcase( chCurrent );
                    if (curpos)
                    {
                        unsigned char *ss = reinterpret_cast<unsigned char*>( strchr( reinterpret_cast<const char*>( valid_letters ), pszOutText[curpos - 1] ) );
                        if ((ss != NULL) || (pszOutText[curpos - 1] == 39))
                        {
                            if ( ( curpos < 2 ) || ( pszOutText[curpos - 2] != 77 ) || ( pszOutText[curpos - 1] != 99 ) )
                            {
                                chCurrent = locase( chCurrent );
                            }
                        }
                    }
                    break;
                case FILE_NAME:
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
                if ( curpos < maxlen && chCurrent )
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
							nl();
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
                        nl();
                    }
                    break;
                case CW:                          // Ctrl-W
                    if ( curpos )
                    {
                        do
                        {
                            curpos--;
                            BackSpace();
                            if ( pszOutText[curpos] == CZ )
                            {
                                BackSpace();
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
                        BackSpace();
                        if ( pszOutText[curpos] == CZ )
                        {
                            BackSpace();
                        }
                    }
                    break;
                case CU:
                case CX:
                    while ( curpos )
                    {
                        curpos--;
                        BackSpace();
                        if ( pszOutText[curpos] == CZ )
                        {
                            BackSpace();
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


void input1( std::string &strOutText, int maxlen, int lc, bool crend, bool bAutoMpl )
{
    char szTempBuffer[ 255 ];
    input1( szTempBuffer, maxlen, lc, crend, bAutoMpl );
    strOutText.assign( szTempBuffer );
}


void input( char *pszOutText, int len, bool bAutoMpl )
// This will input an upper-case string
{
    input1( pszOutText, len, UPPER, true, bAutoMpl );
}


void input( std::string &strOutText, int len, bool bAutoMpl )
// This will input an upper-case string
{
    char szTempBuffer[ 255 ];
    input( szTempBuffer, len, bAutoMpl );
    strOutText.assign( szTempBuffer );
}


void inputl( char *pszOutText, int len, bool bAutoMpl )
// This will input an upper or lowercase string of characters
{
    input1( pszOutText, len, MIXED, true, bAutoMpl );
}


void inputl( std::string &strOutText, int len, bool bAutoMpl )
// This will input an upper or lowercase string of characters
{
    char szTempBuffer[ 255 ];
    inputl( szTempBuffer, len, bAutoMpl );
    strOutText.assign( szTempBuffer );
}


void input_password( const char *pszPromptText, char *pszOutPassword, int len )
{
    sess->bout << pszPromptText;
    mpl( len );
    echo = false;
    input1( pszOutPassword, len, UPPER, true );
}


void input_password( const char *pszPromptText, std::string &strOutPassword, int len )
{
    sess->bout << pszPromptText;
    mpl( len );
    echo = false;
    input1( strOutPassword, len, UPPER, true );
}


//==================================================================
// Function: Input1
//
// Purpose:  Input a line of text of specified length using a ansi
//           formatted input line
//
// Parameters:  *pszOutText		= variable to save the input to
//              *pszOrigText	= line to edit.  appears in edit box
//              maxlen			= max characters allowed
//              bInsert			= insert mode false = off, true = on
//              mode			= formatting mode.
//								  UPPER  MIXED  PROPER  FILE_NAME  DATE  PHONE
//
// Returns: length of string
//==================================================================

int Input1(char *pszOutText, const char *pszOrigText, int maxlen, bool bInsert, int mode)
{
    char szTemp[ 255 ];
    int len, pos, i;
    const unsigned char dash = '-';
    const unsigned char slash = '/';

#if defined( _UNIX )
	input1(szTemp, maxlen, mode, true);
    strcpy(pszOutText, szTemp);
    return strlen(szTemp);
#endif // _UNIX

    if ( !okansi() )
    {
		input1(szTemp, maxlen, mode, true);
        strcpy(pszOutText, szTemp);
        return static_cast<unsigned char>( strlen( szTemp ) );
    }
	int nTopDataSaved = sess->topdata;
	if ( sess->topdata != WLocalIO::topdataNone )
    {
        sess->topdata = WLocalIO::topdataNone;
		app->localIO->UpdateTopScreen();
    }
    if ( mode == DATE || mode == PHONE )
    {
        bInsert = 0;
    }
    int nTopLineSaved = sess->topline;
    sess->topline = 0;
    pos = len = 0;
    szTemp[0] = '\0';
    if (maxlen > 80)
    {
        maxlen = 80;
    }
    ansic( 4 );
    int x = app->localIO->WhereX() + 1;
    int y = app->localIO->WhereY() + 1;

    goxy(x, y);
    for (i = 0; i < maxlen; i++)
    {
        sess->bout << "±";
    }
    goxy( x, y );
    if ( pszOrigText[0] )
    {
        strcpy( szTemp, pszOrigText );
        sess->bout << szTemp;
        goxy( x, y );
        pos = len = strlen(szTemp);
    }
    x = app->localIO->WhereX() + 1;

    bool done = false;
    do
    {
        goxy(pos + x, y);

        int c = get_kb_event( NUMBERS );

        switch (c)
        {
		case CX:								// Control-X
        case ESC:								// ESC
            if (len)
            {
                goxy(len + x, y);
                while (len--)
                {
                    goxy(len + x, y);
                    bputch('±');
                }
                len = pos = szTemp[0] = 0;
            }
            break;
        case COMMAND_LEFT:                    // Left Arrow
        case CS:
            if ((mode != DATE) && (mode != PHONE))
            {
                if (pos)
                    pos--;
            }
            break;
        case COMMAND_RIGHT:                   // Right Arrow
        case CD:
            if ((mode != DATE) && (mode != PHONE))
            {
                if ((pos != len) && (pos != maxlen))
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
            pos = len;
            break;
        case COMMAND_INSERT:                  // Insert
            if (mode == UPPER)
			{
                bInsert ^= 1;
			}
            break;
        case COMMAND_DELETE:                  // Delete
        case CG:
            if ((pos == len) || (mode == DATE) || (mode == PHONE))
			{
                break;
			}
            for (i = pos; i < len; i++)
			{
                szTemp[i] = szTemp[i + 1];
			}
            len--;
            for (i = pos; i < len; i++)
			{
                bputch( szTemp[i] );
			}
            bputch('±');
            break;
        case BACKSPACE:                               // Backspace
            if (pos)
            {
                if (pos != len)
                {
                    if ((mode != DATE) && (mode != PHONE))
                    {
                        for (i = pos - 1; i < len; i++)
						{
                            szTemp[i] = szTemp[i + 1];
						}
                        pos--;
                        len--;
                        goxy(pos + x, y);
                        for (i = pos; i < len; i++)
						{
                            bputch( szTemp[i] );
						}
                        sess->bout << "±";
                    }
                }
                else
                {
                    goxy(pos - 1 + x, y);
                    sess->bout << "±";
                    pos = --len;
                    if (((mode == DATE) && ((pos == 2) || (pos == 5))) ||
                        ((mode == PHONE) && ((pos == 3) || (pos == 7))))
                    {
                        goxy(pos - 1 + x, y);
                        sess->bout << "±";
                        pos = --len;
                    }
                }
            }
            break;
        case RETURN:                              // Enter
            done = true;
            break;
        default:                              // All others < 256
            if ( c < 255 && c > 31 && ( ( bInsert && len < maxlen ) || ( !bInsert && pos < maxlen) ) )
            {
                if (mode != MIXED)
                {
					c = wwiv::UpperCase<unsigned char> ( static_cast< unsigned char >( c ) );
                }
                if (mode == FILE_NAME)
				{
                    if (strchr("/\\+ <>|*?.,=\";:[]", c))
					{
                        c = 0;
					}
                }
                if ( mode == PROPER && pos )
                {
                    char *ss = strchr( reinterpret_cast<char*>( const_cast<unsigned char*>( valid_letters ) ), c );
                    // if it's a valid char and the previous char was a space
                    if ( ss != NULL && szTemp[pos - 1] != 32 )
                    {
                        c = locase( static_cast< unsigned char >( c ) );
                    }
                }
                if ( mode == DATE && ( pos == 2 || pos == 5 ) )
                {
                    bputch(slash);
                    for (i = len++; i >= pos; i--)
					{
                        szTemp[i + 1] = szTemp[i];
					}
                    szTemp[pos++] = slash;
                    goxy( pos + x, y );
                    sess->bout << &szTemp[pos];
                }
                if ( mode == PHONE && ( pos == 3 || pos == 7 ) )
                {
                    bputch(dash);
                    for (i = len++; i >= pos; i--)
					{
                        szTemp[i + 1] = szTemp[i];
					}
                    szTemp[pos++] = dash;
                    goxy(pos + x, y);
                    sess->bout << &szTemp[pos];
                }
                if ( ( mode == DATE  && c != slash ||
					   mode == PHONE && c != dash ) ||
                    ( mode != DATE && mode != PHONE && c != 0 ) )
                {
                    if ( !bInsert || pos == len )
                    {
                        bputch( static_cast< unsigned char >( c ) );
                        szTemp[pos++] = static_cast< char > ( c );
                        if (pos > len)
                        {
                            len++;
                        }
                    }
					else
                    {
                        bputch( ( unsigned char ) c );
                        for (i = len++; i >= pos; i--)
						{
                            szTemp[i + 1] = szTemp[i];
						}
                        szTemp[pos++] = ( char ) c;
                        goxy(pos + x, y);
                        sess->bout << &szTemp[pos];
                    }
                }
            }
            break;
    }
    szTemp[len] = '\0';
    CheckForHangup();
  } while ( !done && !hangup );
  if (len)
  {
      strcpy( pszOutText, szTemp );
  }
  else
  {
      pszOutText[0] = '\0';
  }

  sess->topdata = nTopDataSaved;
  sess->topline = nTopLineSaved;

  ansic( 0 );
  return len;
}


int Input1( std::string &strOutText, const char *pszOrigText, int maxlen, bool bInsert, int mode )
{
    char szTempBuffer[ 255 ];
    int nLength = Input1( szTempBuffer, pszOrigText, maxlen, bInsert, mode );
    strOutText.assign( szTempBuffer );
    return nLength;
}

