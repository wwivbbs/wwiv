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


#define STOP_LIST 0
#define MAX_SCREEN_LINES_TO_SHOW 24

#define NORMAL_HIGHLIGHT   (YELLOW+(BLACK<<4))
#define NORMAL_MENU_ITEM   (CYAN+(BLACK<<4))
#define CURRENT_HIGHLIGHT  (RED+(LIGHTGRAY<<4))
#define CURRENT_MENU_ITEM  (BLACK+(LIGHTGRAY<<4))
// Undefine this so users can not toggle the sysop sub on and off
// #define NOTOGGLESYSOP


//
// Local functions
//
void reset_user_colors_to_defaults();
char *DisplayColorName( int c );


void select_editor()
{
    if ( GetSession()->GetNumberOfEditors() == 0 )
    {
        GetSession()->bout << "\r\nNo full screen editors available.\r\n\n";
        return;
    }
    else if ( GetSession()->GetNumberOfEditors() == 1 )
    {
        if ( GetSession()->thisuser.GetDefaultEditor() == 0 )
        {
            GetSession()->thisuser.SetDefaultEditor( 1 );
        }
        else
        {
            GetSession()->thisuser.SetDefaultEditor( 0 );
            GetSession()->thisuser.clearStatusFlag( WUser::autoQuote );
        }
        return;
    }
    for ( int i1 = 0; i1 < 5; i1++ )
    {
        odc[ i1 ] = '\0';
    }
    GetSession()->bout << "0. Normal non-full screen editor\r\n";
    for ( int i = 0; i < GetSession()->GetNumberOfEditors(); i++ )
    {
		GetSession()->bout << i + 1 << ". " << editors[i].description  << wwiv::endl;
        if ( ( ( i + 1 ) % 10 ) == 0 )
        {
            odc[ ( i + 1 ) / 10 - 1 ] = static_cast<char>( ( i + 1 ) / 10 );
        }
    }
    nl();
	GetSession()->bout << "|#9Which editor (|131-" << GetSession()->GetNumberOfEditors() << ", <C/R>=leave as is|#9) ? ";
    char *ss = mmkey( 2 );
    int nEditor = atoi( ss );
    if ( nEditor >= 1 && nEditor <= GetSession()->GetNumberOfEditors() )
    {
        GetSession()->thisuser.SetDefaultEditor( nEditor );
    }
    else if ( wwiv::stringUtils::IsEquals( ss, "0") )
    {
        GetSession()->thisuser.SetDefaultEditor( 0 );
        GetSession()->thisuser.clearStatusFlag( WUser::autoQuote );
    }
}


const char* GetMailBoxStatus( char* pszStatusOut )
{
    if ( GetSession()->thisuser.GetForwardSystemNumber() == 0 &&
         GetSession()->thisuser.GetForwardUserNumber() == 0 )
    {
        strcpy( pszStatusOut, "Normal" );
        return pszStatusOut;
    }
    if ( GetSession()->thisuser.GetForwardSystemNumber() != 0 )
    {
        if ( GetSession()->thisuser.GetForwardUserNumber() != 0 )
        {
            sprintf( pszStatusOut, "Forward to #%u @%u.%s.",
                        GetSession()->thisuser.GetForwardUserNumber(),
                        GetSession()->thisuser.GetForwardSystemNumber(),
                        net_networks[ GetSession()->thisuser.GetForwardNetNumber() ].name );
        }
        else
        {
            char szForwardUserName[80];
            read_inet_addr( szForwardUserName, GetSession()->usernum );
            sprintf( pszStatusOut, "Forwarded to Internet %s", szForwardUserName );
        }
        return pszStatusOut;
    }

    if ( GetSession()->thisuser.GetForwardUserNumber() == 65535 )
    {
        strcpy( pszStatusOut, "Closed" );
        return pszStatusOut;
    }

    WUser ur;
    GetApplication()->GetUserManager()->ReadUser( &ur, GetSession()->thisuser.GetForwardUserNumber() );
    if ( ur.isUserDeleted() )
    {
        GetSession()->thisuser.SetForwardUserNumber( 0 );
        strcpy( pszStatusOut, "Normal" );
        return pszStatusOut;
    }
    sprintf( pszStatusOut, "Forward to %s", ur.GetUserNameAndNumber( GetSession()->thisuser.GetForwardUserNumber() ) );
    return pszStatusOut;
}


void print_cur_stat()
{
    char s1[255], s2[255];
    ClearScreen();
    GetSession()->bout << "|10Your Preferences\r\n\n";
    sprintf( s1, "|#11|#9) Screen size       : |#2%d X %d", GetSession()->thisuser.GetScreenChars(), GetSession()->thisuser.GetScreenLines() );
    sprintf(s2, "|#12|#9) ANSI              : |#2%s", GetSession()->thisuser.hasAnsi() ?
        ( GetSession()->thisuser.hasColor() ? "Color" : "Monochrome") : "No ANSI" );
    bprintf( "%-48s %-45s\r\n", s1, s2 );

    sprintf(s1, "|#13|#9) Pause on screen   : |#2%s", GetSession()->thisuser.hasPause() ? "On" : "Off");
    char szMailBoxStatus[81];
    sprintf(s2, "|#14|#9) Mailbox           : |#2%s", GetMailBoxStatus( szMailBoxStatus ) );
    bprintf( "%-48s %-45s\r\n", s1, s2 );

    sprintf( s1, "|#15|#9) Configured Q-scan");
    sprintf( s2, "|#16|#9) Change password");
    bprintf( "%-45s %-45s\r\n", s1, s2 );

    if ( okansi() )
    {
        sprintf( s1, "|#17|#9) Update macros");
        sprintf( s2, "|#18|#9) Change colors");
        bprintf( "%-45s %-45s\r\n", s1, s2 );
        int nEditorNum = GetSession()->thisuser.GetDefaultEditor();
        sprintf( s1, "|#19|#9) Full screen editor: |#2%s",
                 ( ( nEditorNum > 0 ) && ( nEditorNum <= GetSession()->GetNumberOfEditors() ) ) ?
                 editors[ nEditorNum - 1 ].description : "None" );
        sprintf( s2, "|#1A|#9) Extended colors   : |#2%s", YesNoString( GetSession()->thisuser.isUseExtraColor() ) );
        bprintf( "%-48.48s %-45s\r\n", s1, s2 );
    }
    else
    {
        GetSession()->bout << "|#17|#9) Update macros\r\n";
    }
    sprintf( s1, "|#1B|#9) Optional lines    : |#2%d", GetSession()->thisuser.GetOptionalVal() );
    sprintf( s2, "|#1C|#9) Conferencing      : |#2%s", YesNoString( GetSession()->thisuser.isUseConference() ) );
    bprintf( "%-48s %-45s\r\n", s1, s2 );
	GetSession()->bout << "|#1I|#9) Internet Address  : |#2" << ( ( GetSession()->thisuser.GetEmailAddress()[0] == '\0') ? "None." : GetSession()->thisuser.GetEmailAddress() ) << wwiv::endl;
    GetSession()->bout << "|#1K|#9) Configure Menus\r\n";
    if (GetSession()->num_languages > 1)
    {
        sprintf( s1, "|#1L|#9) Language          : |#2%s", cur_lang_name );
        bprintf( "%-48s ", s1 );
    }
    if (num_instances() > 1)
    {
        sprintf(s1, "|#1M|#9) Allow user msgs   : |#2%s", YesNoString( GetSession()->thisuser.isIgnoreNodeMessages() ? false : true ) );
        bprintf( "%-48s", s1 );
    }
    nl();
    sprintf( s1, "|#1S|#9) Cls Between Msgs? : |#2%s", YesNoString( GetSession()->thisuser.isUseClearScreen() ) );
    sprintf( s2, "|#1T|#9) 12hr or 24hr clock: |#2%s", GetSession()->thisuser.isUse24HourClock() ? "24hr" : "12hr" );
    bprintf( "%-48s %-45s\r\n", s1, s2 );
    sprintf( s1, "|#1U|#9) Use Msg AutoQuote : |#2%s", YesNoString( GetSession()->thisuser.isUseAutoQuote() ) );

    char szWWIVRegNum[80];
    if ( GetSession()->thisuser.GetWWIVRegNumber() )
    {
        sprintf( szWWIVRegNum, "%ld", GetSession()->thisuser.GetWWIVRegNumber() );
    }
    else
    {
        strcpy( szWWIVRegNum, "(None)" );
    }
    sprintf( s2, "|#1W|#9) WWIV reg num      : |#2%s", szWWIVRegNum );
    bprintf( "%-48s %-45s\r\n", s1, s2 );

    GetSession()->bout << "|#1Q|#9) Quit to main menu\r\n";
}


char *DisplayColorName( int c )
{
    static char szColorName[20];

    if ( checkcomp( "Ami" ) || checkcomp( "Mac" ) )
    {
        sprintf( szColorName, "Color #%d", c );
        return szColorName;
    }

    switch ( c )
    {
    case 0:
        return( "Black" );
    case 1:
        return( "Blue" );
    case 2:
        return( "Green" );
    case 3:
        return( "Cyan" );
    case 4:
        return( "Red" );
    case 5:
        return( "Magenta" );
    case 6:
        return( "Yellow" );
    case 7:
        return( "White" );
    default:
        return( "" );
    }
}


const char *DescribeColorCode( int nColorCode )
{
    static char szColorDesc[81];

    if ( GetSession()->thisuser.hasColor() )
    {
        sprintf( szColorDesc, "%s on %s", DisplayColorName( nColorCode & 0x07 ), DisplayColorName( ( nColorCode >> 4 ) & 0x07 ) );
    }
    else
    {
        strcpy( szColorDesc, ( ( nColorCode & 0x07 ) == 0 ) ? "Inversed" : "Normal" );
    }
    if (nColorCode & 0x08)
    {
        strcat( szColorDesc, ( checkcomp( "Ami" ) || checkcomp( "Mac" ) ) ? ", Bold" : ", Intense" );
    }
    if (nColorCode & 0x80)
    {
        if (checkcomp("Ami"))
        {
            strcat(szColorDesc, ", Italicized");
        }
        else if (checkcomp("Mac"))
        {
            strcat(szColorDesc, ", Underlined");
        }
        else
        {
            strcat(szColorDesc, ", Blinking");
        }
    }
    return szColorDesc;
}


void color_list()
{
    nl( 2 );
    for ( int i = 0; i < 8; i++ )
    {
        setc( static_cast< unsigned char >( (i == 0) ? 0x70 : i ) );
		GetSession()->bout << i << ". " << DisplayColorName( static_cast< char >( i ) ) << "|#0" << wwiv::endl;
    }
}


void change_colors()
{
    char szColorDesc[81], nc;

    bool done = false;
    nl();
    do
    {
        ClearScreen();
        GetSession()->bout << "|10Customize Colors:";
        nl( 2 );
        if ( !GetSession()->thisuser.hasColor() )
        {
            strcpy( szColorDesc, "Monochrome base color : " );
            if ( ( GetSession()->thisuser.GetBWColor( 1 ) & 0x70 ) == 0 )
            {
                strcat(szColorDesc, DisplayColorName(GetSession()->thisuser.GetBWColor( 1 ) & 0x07));
            }
            else
            {
                strcat(szColorDesc, DisplayColorName((GetSession()->thisuser.GetBWColor( 1 ) >> 4) & 0x07));
            }
            GetSession()->bout << szColorDesc;
            nl( 2 );
        }
        for (int i = 0; i < 10; i++)
        {
            ansic(i);
            sprintf( szColorDesc, "%d"".", i );
            switch (i)
            {
            case 0:
                strcat(szColorDesc, "Default           ");
                break;
            case 1:
                strcat(szColorDesc, "Yes/No            ");
                break;
            case 2:
                strcat(szColorDesc, "Prompt            ");
                break;
            case 3:
                strcat(szColorDesc, "Note              ");
                break;
            case 4:
                strcat(szColorDesc, "Input line        ");
                break;
            case 5:
                strcat(szColorDesc, "Yes/No Question   ");
                break;
            case 6:
                strcat(szColorDesc, "Notice!           ");
                break;
            case 7:
                strcat(szColorDesc, "Border            ");
                break;
            case 8:
                strcat(szColorDesc, "Extra color #1    ");
                break;
            case 9:
                strcat(szColorDesc, "Extra color #2    ");
                break;
            }
            if ( GetSession()->thisuser.hasColor() )
            {
                strcat( szColorDesc, DescribeColorCode( GetSession()->thisuser.GetColor( i ) ) );
            }
            else
            {
                strcat( szColorDesc, DescribeColorCode( GetSession()->thisuser.GetBWColor( i ) ) );
            }
            GetSession()->bout << szColorDesc;
			nl();
        }
        GetSession()->bout << "\r\n|#9[|#2R|#9]eset Colors to Default Values, [|#2Q|#9]uit\r\n";
        GetSession()->bout << "|#9Enter Color Number to Modify (|#20|#9-|#29|#9,|#2R|#9,|#2Q|#9): ";
        char ch = onek( "RQ0123456789", true );
        if (ch == 'Q')
        {
            done = true;
        }
        else if ( ch == 'R' )
        {
            reset_user_colors_to_defaults();
        }
        else
        {
            int nColorNum = ch - '0';
            if ( GetSession()->thisuser.hasColor() )
            {
                color_list();
                ansic( 0 );
                nl();
                GetSession()->bout << "|#9(Q=Quit) Foreground? ";
                ch = onek("Q01234567");
                if ( ch == 'Q' )
                {
                    continue;
                }
                nc = static_cast< char >( ch - '0' );
                GetSession()->bout << "|#9(Q=Quit) Background? ";
                ch = onek("Q01234567");
                if ( ch == 'Q' )
                {
                    continue;
                }
                nc = static_cast< char >( nc | ((ch - '0') << 4) );
            }
            else
            {
                nl();
                GetSession()->bout << "|#9Inversed? ";
                if (yesno())
                {
                    if ((GetSession()->thisuser.GetBWColor( 1 ) & 0x70) == 0)
                    {
                        nc = static_cast< char >( 0 | ((GetSession()->thisuser.GetBWColor( 1 ) & 0x07) << 4) );
                    }
                    else
                    {
                        nc = static_cast< char >(GetSession()->thisuser.GetBWColor( 1 ) & 0x70);
                    }
                }
                else
                {
                    if ((GetSession()->thisuser.GetBWColor( 1 ) & 0x70) == 0)
                    {
                        nc = static_cast< char >( 0 | (GetSession()->thisuser.GetBWColor( 1 ) & 0x07) );
                    }
                    else
                    {
                        nc = static_cast< char >( (GetSession()->thisuser.GetBWColor( 1 ) & 0x70) >> 4 );
                    }
                }
            }
            if ( checkcomp( "Ami" ) || checkcomp( "Mac" ) )
            {
                GetSession()->bout << "|#9Bold? ";
            }
            else
            {
                GetSession()->bout << "|#9Intensified? ";
            }
            if (yesno())
            {
                nc |= 0x08;
            }

            if (checkcomp("Ami"))
            {
                GetSession()->bout << "|#9Italicized? ";
            }
            else if (checkcomp("Mac"))
            {
				GetSession()->bout << "|#9Underlined? ";
            }
            else
            {
                GetSession()->bout << "|#9Blinking? ";
            }

            if (yesno())
            {
                nc |= 0x80;
            }

            nl( 2 );
            setc( nc );
            GetSession()->bout << DescribeColorCode( nc );
            ansic( 0 );
            nl( 2 );
            GetSession()->bout << "|#8Is this OK? ";
            if ( yesno() )
            {
                GetSession()->bout << "\r\nColor saved.\r\n\n";
                if ( GetSession()->thisuser.hasColor() )
                {
                    GetSession()->thisuser.SetColor( nColorNum, nc );
                }
                else
                {
                    GetSession()->thisuser.SetBWColor( nColorNum, nc );
                }
            }
            else
            {
                GetSession()->bout << "\r\nNot saved, then.\r\n\n";
            }
        }
    } while ( !done && !hangup );
}



void l_config_qscan()
{
    bool abort = false;
	GetSession()->bout << "\r\n|#9Boards to q-scan marked with '*'|#0\r\n\n";
    for (int i = 0; (i < GetSession()->num_subs) && (usub[i].subnum != -1) && !abort; i++)
    {
        char szBuffer[81];
        sprintf(szBuffer, "%c %s. %s",
            (qsc_q[usub[i].subnum / 32] & (1L << (usub[i].subnum % 32))) ? '*' : ' ',
            usub[i].keys,
            subboards[usub[i].subnum].name);
        pla(szBuffer, &abort);
    }
    nl( 2 );
}


void config_qscan()
{
    if ( okansi() )
    {
        config_scan_plus(QSCAN);
        return;
    }

    int oc = GetSession()->GetCurrentConferenceMessageArea();
    int os = usub[GetSession()->GetCurrentMessageArea()].subnum;

    bool done = false;
    bool done1 = false;
    do
    {
        char ch;
        if ( okconf( &GetSession()->thisuser ) && uconfsub[1].confnum != -1 )
        {
            char szConfList[MAX_CONFERENCES + 2];
            bool abort = false;
            strcpy(szConfList, " ");
            GetSession()->bout << "\r\nSelect Conference: \r\n\n";
            int i = 0;
            while ( i < subconfnum && uconfsub[i].confnum != -1 && !abort )
            {
                char szBuffer[120];
                sprintf( szBuffer, "%c) %s", subconfs[uconfsub[i].confnum].designator,
                         stripcolors( reinterpret_cast<char*>( subconfs[uconfsub[i].confnum].name ) ) );
                pla(szBuffer, &abort);
                szConfList[i + 1] = subconfs[uconfsub[i].confnum].designator;
                szConfList[i + 2] = 0;
                i++;
            }
            nl();
            GetSession()->bout << "Select [" << &szConfList[1] << ", <space> to quit]: ";
            ch = onek(szConfList);
        }
        else
        {
            ch = '-';
        }
        switch (ch)
        {
      case ' ':
          done1 = true;
          break;
      default:
          if ( okconf( &GetSession()->thisuser )  && uconfsub[1].confnum != -1 )
          {
              int i = 0;
              while ((ch != subconfs[uconfsub[i].confnum].designator) && (i < subconfnum))
              {
                  i++;
              }

              if (i >= subconfnum)
              {
                  break;
              }

              setuconf(CONF_SUBS, i, -1);
          }
          l_config_qscan();
          done = false;
          do
          {
              nl();
              GetSession()->bout << "|#2Enter message base number (|#1C=Clr All, Q=Quit, S=Set All|#2): ";
              char* s = mmkey( 0 );
              if (s[0])
              {
                  for (int i = 0; (i < GetSession()->num_subs) && (usub[i].subnum != -1); i++)
                  {
                      if ( wwiv::stringUtils::IsEquals( usub[i].keys, s ) )
                      {
                          qsc_q[usub[i].subnum / 32] ^= (1L << (usub[i].subnum % 32));
                      }
                      if ( wwiv::stringUtils::IsEquals( s, "S" ) )
                      {
                          qsc_q[usub[i].subnum / 32] |= (1L << (usub[i].subnum % 32));
                      }
                      if ( wwiv::stringUtils::IsEquals( s, "C" ) )
                      {
                          qsc_q[usub[i].subnum / 32] &= ~(1L << (usub[i].subnum % 32));
                      }
                  }
                  if ( wwiv::stringUtils::IsEquals( s, "Q" ) )
                  {
                      done = true;
                  }
                  if ( wwiv::stringUtils::IsEquals( s, "?" ) )
                  {
                      l_config_qscan();
                  }
              }
          } while ( !done && !hangup );
          break;
        }
        if ( !okconf( &GetSession()->thisuser ) || uconfsub[1].confnum == -1 )
        {
            done1 = true;
        }

    } while ( !done1 && !hangup );

    if ( okconf(&GetSession()->thisuser ) )
    {
        setuconf( CONF_SUBS, oc, os );
    }
}

void make_macros()
{
    char szMacro[255], ch;
    bool done = false;

    do
    {
        bputch( CL );
        GetSession()->bout << "|#4Macro A: \r\n";
        list_macro( GetSession()->thisuser.GetMacro( 2 ) );
        nl();
        GetSession()->bout << "|#4Macro D: \r\n";
        list_macro( GetSession()->thisuser.GetMacro( 0 ) );
        nl();
        GetSession()->bout << "|#4Macro F: \r\n";
        list_macro( GetSession()->thisuser.GetMacro( 1 ) );
        nl( 2 );
		GetSession()->bout << "|#9Macro to edit or Q:uit (A,D,F,Q) : |#0";
        ch = onek("QADF");
        szMacro[0] = 0;
        switch (ch)
        {
        case 'A':
            macroedit( szMacro );
            if (szMacro[0])
            {
                GetSession()->thisuser.SetMacro( 2, szMacro );
            }
            break;
        case 'D':
            macroedit(szMacro);
            if (szMacro[0])
            {
                GetSession()->thisuser.SetMacro( 0, szMacro );
            }
            break;
        case 'F':
            macroedit(szMacro);
            if (szMacro[0])
            {
                GetSession()->thisuser.SetMacro( 1, szMacro );
            }
            break;
        case 'Q':
            done = true;
            break;
        }
    } while ( !done && !hangup );
}


void list_macro(const char *pszMacroText)
{
    int i = 0;

    while ((i < 80) && (pszMacroText[i] != 0))
    {
        if (pszMacroText[i] >= 32)
        {
            bputch(pszMacroText[i]);
        }
        else
        {
            if (pszMacroText[i] == 16)
            {
                ansic(pszMacroText[++i] - 48);
            }
            else
            {
                switch (pszMacroText[i])
                {
                case RETURN:
                    bputch('|');
                    break;
                case TAB:
                    bputch( 'ù' );
                    break;
                default:
                    bputch('^');
                    bputch( static_cast< unsigned char >( pszMacroText[i] + 64 ) );
                    break;
                }
            }
        }
        ++i;
    }
    nl();
}

char *macroedit( char *pszMacroText )
{
	*pszMacroText = '\0';
	nl();
	GetSession()->bout << "|#5Enter your macro, press |#7[|#1CTRL-Z|#7]|#5 when finished.\r\n\n";
	okskey = false;
	ansic( 0 );
	bool done = false;
	int i = 0;
	bool toggle = false;
	int textclr = 0;
	do
	{
		char ch = getkey();
		switch ( ch )
		{
		case CZ:
			done = true;
			break;
		case BACKSPACE:
			BackSpace();
			i--;
			if (i < 0)
			{
				i = 0;
			}
			pszMacroText[i] = '\0';
			break;
		case CP:
			pszMacroText[i++] = ch;
			toggle = true;
			break;
		case RETURN:
			pszMacroText[i++] = ch;
			ansic( 0 );
			bputch( '|' );
			ansic( textclr );
			break;
		case TAB:
			pszMacroText[i++] = ch;
			ansic( 0 );
			bputch( 'ù' ) ;
			ansic( textclr );
			break;
		default:
			pszMacroText[i++] = ch;
			if ( toggle )
			{
				toggle = false;
				textclr = ch - 48;
				ansic( textclr );
			}
			else
			{
				bputch(ch);
			}
			break;
		}
		pszMacroText[i + 1] = 0;
	} while ( !done && i < 80 && !hangup );
	okskey = true;
	ansic( 0 );
	nl();
	GetSession()->bout << "|#9Is this okay? ";
	if (!yesno())
	{
		*pszMacroText = '\0';
	}
	return pszMacroText;
}


void change_password()
{
    nl();
    GetSession()->bout << "|#9Change password? ";
    if ( !yesno() )
    {
        return;
    }

    std::string password, password2;
    nl();
    input_password( "|#9You must now enter your current password.\r\n|#7: ", password, 8 );
    if ( password != GetSession()->thisuser.GetPassword() )
    {
        GetSession()->bout << "\r\nIncorrect.\r\n\n";
        return;
    }
    nl( 2 );
    input_password( "|#9Enter your new password, 3 to 8 characters long.\r\n|#7: ", password, 8 );
    nl( 2 );
    input_password( "|#9Repeat password for verification.\r\n|#7: ", password2, 8 );
    if ( password == password2 )
    {
        if ( password2.length() < 3 )
        {
            nl();
			GetSession()->bout << "|#6Password must be 3-8 characters long.\r\n";
			GetSession()->bout << "|#6Password was not changed.\r\n\n";
        }
        else
        {
            GetSession()->thisuser.SetPassword( password.c_str() );
			GetSession()->bout << "\r\n|#1Password changed.\r\n\n";
            sysoplog("Changed Password.");
        }
    }
    else
    {
		GetSession()->bout << "\r\n|#6VERIFY FAILED.\r\n|#6Password not changed.\r\n\n";
    }
}


void modify_mailbox()
{
    char s[81];

    nl();

    GetSession()->bout << "|#9Do you want to close your mailbox? ";
    if (yesno())
    {
        GetSession()->bout << "|#5Are you sure? ";
        if (yesno())
        {
            GetSession()->thisuser.SetForwardSystemNumber( 0 );
            GetSession()->thisuser.SetForwardUserNumber( 65535 );  // (-1)
            return;
        }
    }
    GetSession()->bout << "|#5Do you want to forward your mail? ";
    if (!yesno())
    {
        GetSession()->thisuser.SetForwardSystemNumber( 0 );
        GetSession()->thisuser.SetForwardUserNumber( 0 );
        return;
    }
    if (GetSession()->thisuser.GetSl() >= syscfg.newusersl)
    {
        int nNetworkNumber = getnetnum( "FILEnet" );
        GetSession()->SetNetworkNumber( nNetworkNumber );
        if ( nNetworkNumber != -1)
        {
            set_net_num( GetSession()->GetNetworkNumber() );
            GetSession()->bout << "|#5Do you want to forward to your Internet address? ";
            if ( yesno() )
            {
                GetSession()->bout << "|13Enter the Internet E-Mail Address.\r\n|#9:";
                Input1( s, GetSession()->thisuser.GetEmailAddress(), 75, true, MIXED );
                if ( check_inet_addr( s ) )
                {
                    GetSession()->thisuser.SetEmailAddress( s );
                    write_inet_addr( s, GetSession()->usernum );
                    GetSession()->thisuser.SetForwardNetNumber( GetSession()->GetNetworkNumber() );
                    GetSession()->thisuser.SetForwardSystemNumber( 32767 );
                    GetSession()->bout << "\r\nSaved.\r\n\n";
                }
                return;
            }
        }
    }
    nl();
    GetSession()->bout << "|#2Forward to? ";
    input(s, 40);

    int nTempForwardUser, nTempForwardSystem;
    parse_email_info( s, &nTempForwardUser, &nTempForwardSystem );
    GetSession()->thisuser.SetForwardUserNumber( nTempForwardUser );
    GetSession()->thisuser.SetForwardSystemNumber( nTempForwardSystem );
    if ( GetSession()->thisuser.GetForwardSystemNumber() != 0 )
    {
        GetSession()->thisuser.SetForwardNetNumber( GetSession()->GetNetworkNumber() );
        if ( GetSession()->thisuser.GetForwardUserNumber() == 0 )
        {
            GetSession()->thisuser.SetForwardSystemNumber( 0 );
            GetSession()->thisuser.SetForwardNetNumber( 0 );
            GetSession()->bout << "\r\nCan't forward to a user name, must use user number.\r\n\n";
        }
    }
    else if ( GetSession()->thisuser.GetForwardUserNumber() == GetSession()->usernum )
    {
        GetSession()->bout << "\r\nCan't forward to yourself.\r\n\n";
        GetSession()->thisuser.SetForwardUserNumber( 0 );
    }

    nl();
    if ( GetSession()->thisuser.GetForwardUserNumber() == 0 && GetSession()->thisuser.GetForwardSystemNumber() == 0 )
    {
        GetSession()->thisuser.SetForwardNetNumber( 0 );
        GetSession()->bout << "Forwarding reset.\r\n";
    }
    else
    {
        GetSession()->bout << "Saved.\r\n";
    }
    nl();
}


void optional_lines()
{
    char szNumLines[81];

    GetSession()->bout << "|#9You may specify your optional lines value from 0-10,\r\n" ;
    GetSession()->bout << "|#20 |#9being all, |#210 |#9being none.\r\n";
    GetSession()->bout << "|#2What value? ";
    input( szNumLines, 2 );

    int nNumLines = atoi( szNumLines );
    if ( szNumLines[0] && nNumLines >= 0 && nNumLines < 11 )
    {
        GetSession()->thisuser.SetOptionalVal( nNumLines );
    }

}


void enter_regnum()
{
    char szRegNum[81];

    GetSession()->bout << "|#7Enter your WWIV registration number, or enter '|#20|#7' for none.\r\n|#0:";
    input( szRegNum, 5, true );

    long lRegNum = atol( szRegNum );
    if ( szRegNum[0] && lRegNum >= 0 )
    {
        GetSession()->thisuser.SetWWIVRegNumber( lRegNum );
        changedsl();
    }
}


void defaults( MenuInstanceData * MenuData )
{
    bool done = false;
    do
    {
        print_cur_stat();
        GetApplication()->GetLocalIO()->tleft( true );
        if (hangup)
        {
            return;
        }
        nl();
        char ch;
        if ( okansi() )
        {
            GetSession()->bout << "|#9Defaults: (1-9,A-C,I,K,L,M,S,T,U,W,?,Q) : ";
            ch = onek( "Q?123456789ABCIKLMSTUW", true );
        }
        else
        {
            GetSession()->bout << "|#9Defaults: (1-7,B,C,I,K,L,M,S,T,U,W,?,Q) : ";
            ch = onek( "Q?1234567BCIKLMTUW", true );
        }
        switch (ch)
        {
        case 'Q':
            done = true;
            break;
        case '?':
            print_cur_stat();
            break;
        case '1':
            input_screensize();
            break;
        case '2':
            input_ansistat();
            break;
        case '3':
            GetSession()->thisuser.toggleStatusFlag( WUser::pauseOnPage );
            break;
        case '4':
            modify_mailbox();
            break;
        case '5':
            config_qscan();
            break;
        case '6':
            change_password();
            break;
        case '7':
            make_macros();
            break;
        case '8':
            change_colors();
            break;
        case '9':
            select_editor();
            break;
        case 'A':
            GetSession()->thisuser.toggleStatusFlag( WUser::extraColor );
            break;
        case 'B':
            optional_lines();
            break;
        case 'C':
            GetSession()->thisuser.toggleStatusFlag( WUser::conference );
            changedsl();
            break;

        case 'I':
            {
                std::string internetAddress;
                nl();
                GetSession()->bout << "|#9Enter your Internet mailing address.\r\n|#7:";
                inputl( internetAddress, 65, true );
                if ( !internetAddress.empty() )
                {
                    if ( check_inet_addr( internetAddress.c_str() ) )
                    {
                        GetSession()->thisuser.SetEmailAddress( internetAddress.c_str() );
                        write_inet_addr( internetAddress.c_str(), GetSession()->usernum );
                    }
                    else
                    {
						GetSession()->bout << "\r\n|#6Invalid address format.\r\n\n";
                        pausescr();
                    }
                }
                else
                {
                    GetSession()->bout << "|#5Delete Internet address? ";
                    if (yesno())
                    {
                        GetSession()->thisuser.SetEmailAddress( "" );
                    }
                }
            }
            break;
        case 'K':
            ConfigUserMenuSet();
            MenuData->nFinished = 1;
            MenuData->nReload = 1;
            break;
        case 'L':
            if ( GetSession()->num_languages > 1 )
            {
                input_language();
            }
            break;
        case 'M':
            if ( num_instances() > 1 )
            {
                GetSession()->thisuser.clearStatusFlag( WUser::noMsgs );
                nl();
                GetSession()->bout << "|#5Allow messages sent between instances? ";
                if (!yesno())
                {
                    GetSession()->thisuser.setStatusFlag( WUser::noMsgs );
                }
            }
            break;
        case 'S':
            GetSession()->thisuser.toggleStatusFlag( WUser::clearScreen );
            break;
        case 'T':
            GetSession()->thisuser.toggleStatusFlag( WUser::twentyFourHourClock );
            break;
        case 'U':
            GetSession()->thisuser.toggleStatusFlag( WUser::autoQuote );
            break;
        case 'W':
            enter_regnum();
            break;
        }
    } while ( !done && !hangup );
}



// private function used by list_config_scan_plus and drawscan
int GetMaxLinesToShowForScanPlus()
{
#ifdef MAX_SCREEN_LINES_TO_SHOW
    return ( GetSession()->thisuser.GetScreenLines() - (4 + STOP_LIST) >
        MAX_SCREEN_LINES_TO_SHOW - (4 + STOP_LIST) ?
        MAX_SCREEN_LINES_TO_SHOW - (4 + STOP_LIST) :
    GetSession()->thisuser.GetScreenLines() - (4 + STOP_LIST));
#else
    return GetSession()->thisuser.GetScreenLines() - (4 + STOP_LIST);
#endif
}


void list_config_scan_plus(int first, int *amount, int type)
{
    char s[101];

    bool bUseConf = ( subconfnum > 1 && okconf( &GetSession()->thisuser ) ) ? true : false;

    ClearScreen();
    lines_listed = 0;

    if ( bUseConf )
    {
        strncpy( s, type == 0 ? stripcolors( reinterpret_cast<char*>( subconfs[uconfsub[GetSession()->GetCurrentConferenceMessageArea()].confnum].name ) ) : stripcolors( reinterpret_cast<char*>( dirconfs[uconfdir[GetSession()->GetCurrentConferenceFileArea()].confnum].name ) ), 26 );
        s[26] = '\0';
        bprintf( "|#1Configure |#2%cSCAN |#9-- |#2%-26s |#9-- |#1Press |#7[|#2SPACE|#7]|#1 to toggle a %s\r\n", type == 0 ? 'Q' : 'N', s, type == 0 ? "sub" : "dir" );
    }
    else
    {
        bprintf("|#1Configure |#2%cSCAN                                   |#1Press |#7[|#2SPACE|#7]|#1 to toggle a %s\r\n", type == 0 ? 'Q' : 'N', type == 0 ? "sub" : "dir");
    }
    repeat_char( 'Ä', 79, 7, true );

    int max_lines = GetMaxLinesToShowForScanPlus();

    if ( type == 0 )
    {
        for ( int this_sub = first; (this_sub < GetSession()->num_subs) && (usub[this_sub].subnum != -1) &&
              *amount < max_lines * 2; this_sub++ )
        {
                lines_listed = 0;
                sprintf(s, "|#7[|#1%c|#7] |#9%s",
                    (qsc_q[usub[this_sub].subnum / 32] & (1L << (usub[this_sub].subnum % 32))) ? '\xFE' : ' ',
                    subboards[usub[this_sub].subnum].name);
                s[44] = 0;
                if (*amount >= max_lines)
                {
                    goxy(40, 3 + *amount - max_lines);
					GetSession()->bout << s;
                }
                else
                {
                    GetSession()->bout << s;
					nl();
                }
                ++*amount;
            }
    }
    else
    {
        for ( int this_dir = first; (this_dir < GetSession()->num_dirs) && (udir[this_dir].subnum != -1) &&
              *amount < max_lines * 2; this_dir++ )
        {
                lines_listed = 0;
                int alias_dir = udir[this_dir].subnum;
                sprintf( s, "|#7[|#1%c|#7] |#2%s", qsc_n[alias_dir / 32] & (1L << (alias_dir % 32)) ? '\xFE' : ' ',
                            directories[alias_dir].name );
                s[44] = 0;
                if ( *amount >= max_lines )
                {
                    goxy( 40, 3 + *amount - max_lines );
                    GetSession()->bout << s;
                }
                else
                {
                    GetSession()->bout << s;
					nl();
                }
                ++*amount;
            }
    }
    nl();
    lines_listed = 0;
}


void config_scan_plus(int type)
{
    char **menu_items, s[50];
    int i, command, this_dir, this_sub, ad;
    int sysdir = 0, top = 0, amount = 0, pos = 0, side_pos = 0;
    struct side_menu_colors smc =
    {
        NORMAL_HIGHLIGHT,
        NORMAL_MENU_ITEM,
        CURRENT_HIGHLIGHT,
        CURRENT_MENU_ITEM
    };

    int useconf = ( subconfnum > 1 && okconf( &GetSession()->thisuser ) );
    GetSession()->topdata = WLocalIO::topdataNone;
    GetApplication()->GetLocalIO()->UpdateTopScreen();

    menu_items = static_cast<char **>( BbsAlloc2D( 10, 10, sizeof( char ) ) );
    strcpy(menu_items[0], "Next");
    strcpy(menu_items[1], "Previous");
    strcpy(menu_items[2], "Toggle");
    strcpy(menu_items[3], "Clear All");
    strcpy(menu_items[4], "Set All");

    if ( type == 0 )
    {
        strcpy( menu_items[5], "Read New" );
    }
    else
    {
        strcpy( menu_items[5], "List" );
    }

    if ( useconf )
    {
        strcpy( menu_items[6], "{ Conf" );
        strcpy( menu_items[7], "} Conf" );
        strcpy( menu_items[8], "Quit" );
        strcpy( menu_items[9], "?" );
        menu_items[9][0] = 0;
    }
    else
    {
        strcpy( menu_items[6], "Quit" );
        strcpy( menu_items[7], "?" );
        menu_items[7][0] = 0;
    }
    bool done = false;
    while ( !done && !hangup )
    {
        amount = 0;
        list_config_scan_plus( top, &amount, type );
        if ( !amount )
        {
            top = 0;
            list_config_scan_plus( top, &amount, type );
            if ( !amount )
            {
                done = true;
            }
        }
        if ( !done )
        {
            drawscan( pos, type ? is_inscan( top + pos ) :
        qsc_q[usub[top + pos].subnum / 32] & ( 1L << ( usub[top + pos].subnum % 32 ) ) );
        }
        bool redraw = true;
        bool menu_done = false;
        while ( !menu_done && !hangup && !done )
        {
            command = side_menu( &side_pos, redraw, menu_items, 1,
#ifdef MAX_SCREEN_LINES_TO_SHOW
                GetSession()->thisuser.GetScreenLines() - STOP_LIST > MAX_SCREEN_LINES_TO_SHOW - STOP_LIST ?
                MAX_SCREEN_LINES_TO_SHOW - STOP_LIST :
                GetSession()->thisuser.GetScreenLines() - STOP_LIST, &smc);
#else
                GetSession()->thisuser.GetScreenLines() - STOP_LIST, &smc);
#endif
            lines_listed = 0;
            redraw = true;
            ansic( 0 );
            if ( do_sysop_command( command ) )
            {
                menu_done = true;
                amount = 0;
            }
            switch ( command )
            {
            case '?':
            case CO:
                ClearScreen();
                printfile(SCONFIG_HLP);
                pausescr();
                menu_done = true;
                amount = 0;
                break;
            case COMMAND_DOWN:
                undrawscan(pos, type ? is_inscan(top + pos) :
                qsc_q[usub[top + pos].subnum / 32] & (1L << (usub[top + pos].subnum % 32)));
                ++pos;
                if (pos >= amount)
                {
                    pos = 0;
                }
                drawscan(pos, type ? is_inscan(top + pos) :
                qsc_q[usub[top + pos].subnum / 32] & (1L << (usub[top + pos].subnum % 32)));
                redraw = false;
                break;
            case COMMAND_UP:
                undrawscan(pos, type ? is_inscan(top + pos) :
                qsc_q[usub[top + pos].subnum / 32] & (1L << (usub[top + pos].subnum % 32)));
                if (!pos)
                {
                    pos = amount - 1;
                }
                else
                {
                    --pos;
                }
                drawscan(pos, type ? is_inscan(top + pos) :
                qsc_q[usub[top + pos].subnum / 32] & (1L << (usub[top + pos].subnum % 32)));
                redraw = false;
                break;
            case SPACE:
                if (type == 0)
                {
#ifdef NOTOGGLESYSOP
                    if (usub[top + pos].subnum == 0)
                    {
                        qsc_q[usub[top + pos].subnum / 32] |= (1L << (usub[top + pos].subnum % 32));
                    }
                    else
#endif
                        qsc_q[usub[top + pos].subnum / 32] ^= (1L << (usub[top + pos].subnum % 32));
                }
                else
                {
                    if ( wwiv::stringUtils::IsEquals( udir[0].keys, "0" ) )
                    {
                        sysdir = 1;
                    }
                    for (this_dir = 0; (this_dir < GetSession()->num_dirs); this_dir++)
                    {
                        sprintf(s, "%d", sysdir ? top + pos : top + pos + 1);
                        if ( wwiv::stringUtils::IsEquals( s, udir[this_dir].keys ) )
                        {
                            ad = udir[this_dir].subnum;
                            qsc_n[ad / 32] ^= (1L << (ad % 32));
                        }
                    }
                }
                drawscan(pos, type ? is_inscan(top + pos) :
                qsc_q[usub[top + pos].subnum / 32] & (1L << (usub[top + pos].subnum % 32)));
                redraw = false;
                break;
            case EXECUTE:
                if (!useconf && side_pos > 5)
                {
                    side_pos += 2;
                }
                switch (side_pos)
                {
                case 0:
                    top += amount;
                    if (type == 0)
                    {
                        if (top >= GetSession()->num_subs)
                        {
                            top = 0;
                        }
                    }
                    else
                    {
                        if (top >= GetSession()->num_dirs)
                        {
                            top = 0;
                        }
                    }
                    pos = 0;
                    menu_done = true;
                    amount = 0;
                    break;
                case 1:
                    if (top > GetSession()->thisuser.GetScreenLines() - 4)
                    {
                        top -= GetSession()->thisuser.GetScreenLines() - 4;
                    }
                    else
                    {
                        top = 0;
                    }
                    pos = 0;
                    menu_done = true;
                    amount = 0;
                    break;
                case 2:
                    if (type == 0)
                    {
                        qsc_q[usub[top + pos].subnum / 32] ^= (1L << (usub[top + pos].subnum % 32));
                    }
                    else
                    {
                        int this_dir, sysdir = 0;
                        int ad;
                        if ( wwiv::stringUtils::IsEquals( udir[0].keys, "0" ) )
                        {
                            sysdir = 1;
                        }
                        for (this_dir = 0; (this_dir < GetSession()->num_dirs); this_dir++)
                        {
                            sprintf(s, "%d", sysdir ? top + pos : top + pos + 1);
                            if ( wwiv::stringUtils::IsEquals( s, udir[this_dir].keys ) )
                            {
                                ad = udir[this_dir].subnum;
                                qsc_n[ad / 32] ^= (1L << (ad % 32));
                            }
                        }
                    }
                    drawscan(pos, type ? is_inscan(top + pos) :
                    qsc_q[usub[top + pos].subnum / 32] & (1L << (usub[top + pos].subnum % 32)));
                    redraw = false;
                    break;
                case 3:
                    if (type == 0)
                    {
                        for (this_sub = 0; this_sub < GetSession()->num_subs; this_sub++)
                        {
                            if (qsc_q[usub[this_sub].subnum / 32] & (1L << (usub[this_sub].subnum % 32)))
                            {
                                qsc_q[usub[this_sub].subnum / 32] ^= (1L << (usub[this_sub].subnum % 32));
                            }
                        }
                    }
                    else
                    {
                        for (this_dir = 0; this_dir < GetSession()->num_dirs; this_dir++)
                        {
                            if (qsc_n[udir[this_dir].subnum / 32] & (1L << (udir[this_dir].subnum % 32)))
                            {
                                qsc_n[udir[this_dir].subnum / 32] ^= 1L << (udir[this_dir].subnum % 32);
                            }
                        }
                    }
                    pos = 0;
                    menu_done = true;
                    amount = 0;
                    break;
                case 4:
                    if (type == 0)
                    {
                        for (this_sub = 0; this_sub < GetSession()->num_subs; this_sub++)
                        {
                            if (!(qsc_q[usub[this_sub].subnum / 32] & (1L << (usub[this_sub].subnum % 32))))
                            {
                                qsc_q[usub[this_sub].subnum / 32] ^= (1L << (usub[this_sub].subnum % 32));
                            }
                        }
                    }
                    else
                    {
                        for (this_dir = 0; this_dir < GetSession()->num_dirs; this_dir++)
                        {
                            if (!(qsc_n[udir[this_dir].subnum / 32] & (1L << (udir[this_dir].subnum % 32))))
                            {
                                qsc_n[udir[this_dir].subnum / 32] ^= 1L << (udir[this_dir].subnum % 32);
                            }
                        }
                    }
                    pos = 0;
                    menu_done = true;
                    amount = 0;
                    break;
                case 5:
                    if (type == 0)
                    {
                        express = false;
                        expressabort = false;
                        qscan(top + pos, &i);
                    }
                    else
                    {
                        i = GetSession()->GetCurrentFileArea();
                        GetSession()->SetCurrentFileArea( top + pos );
                        GetSession()->tagging = 1;
                        listfiles();
                        GetSession()->tagging = 0;
                        GetSession()->SetCurrentFileArea( i );
                    }
                    menu_done = true;
                    amount = 0;
                    break;
                case 6:
                    if ( okconf( &GetSession()->thisuser ) )
                    {
                        if ( type == 0 )
                        {
                            if ( GetSession()->GetCurrentConferenceMessageArea() > 0 )
                            {
                                GetSession()->SetCurrentConferenceMessageArea( GetSession()->GetCurrentConferenceMessageArea() - 1);
                            }
                            else
                            {
                                while ((uconfsub[GetSession()->GetCurrentConferenceMessageArea() + 1].confnum >= 0) && (GetSession()->GetCurrentConferenceMessageArea() < subconfnum - 1))
                                {
                                    GetSession()->SetCurrentConferenceMessageArea( GetSession()->GetCurrentConferenceMessageArea() + 1 );
                                }
                            }
                            setuconf( CONF_SUBS, GetSession()->GetCurrentConferenceMessageArea(), -1 );
                        }
                        else
                        {
                            if ( GetSession()->GetCurrentConferenceFileArea() > 0 )
                            {
                                GetSession()->SetCurrentConferenceFileArea( GetSession()->GetCurrentConferenceFileArea() - 1 );
                            }
                            else
                            {
                                while ((uconfdir[GetSession()->GetCurrentConferenceFileArea() + 1].confnum >= 0) && (GetSession()->GetCurrentConferenceFileArea() < dirconfnum - 1))
                                {
                                    GetSession()->SetCurrentConferenceFileArea( GetSession()->GetCurrentConferenceFileArea() + 1 );
                                }
                            }
                            setuconf( CONF_DIRS, GetSession()->GetCurrentConferenceFileArea(), -1 );
                        }
                        pos = 0;
                        menu_done = true;
                        amount = 0;
                    }
                    break;
                case 7:
                    if ( okconf( &GetSession()->thisuser ) )
                    {
                        if (type == 0)
                        {
                            if ((GetSession()->GetCurrentConferenceMessageArea() < subconfnum - 1) && (uconfsub[GetSession()->GetCurrentConferenceMessageArea() + 1].confnum >= 0))
                            {
                                GetSession()->SetCurrentConferenceMessageArea( GetSession()->GetCurrentConferenceMessageArea() + 1 );
                            }
                            else
                            {
                                GetSession()->SetCurrentConferenceMessageArea( 0 );
                            }
                            setuconf( CONF_SUBS, GetSession()->GetCurrentConferenceMessageArea(), -1 );
                        }

                        else {
                            if ((GetSession()->GetCurrentConferenceFileArea() < dirconfnum - 1) && (uconfdir[GetSession()->GetCurrentConferenceFileArea() + 1].confnum >= 0))
                            {
                                GetSession()->SetCurrentConferenceFileArea( GetSession()->GetCurrentConferenceFileArea() + 1 );
                            }
                            else
                            {
                                GetSession()->SetCurrentConferenceFileArea( 0 );
                            }
                            setuconf( CONF_DIRS, GetSession()->GetCurrentConferenceFileArea(), -1 );
                        }
                        pos = 0;
                        menu_done = true;
                        amount = 0;
                    }
                    break;
                case 8:
                    menu_done = true;
                    done = true;
                    break;
                case 9:
                    ClearScreen();
                    printfile(SCONFIG_HLP);
                    pausescr();
                    menu_done = true;
                    amount = 0;
                    if (!useconf)
                    {
                        side_pos -= 2;
                    }
                    break;
                }
                break;
            case GET_OUT:
                menu_done = true;
                done = true;
                break;
            }
        }
    }
    lines_listed = 0;
    nl();
    BbsFree2D(menu_items);
}


void drawscan(int filepos, long tagged)
{
    int max_lines = GetMaxLinesToShowForScanPlus();
    if (filepos >= max_lines)
    {
        goxy(40, 3 + filepos - max_lines);
    }
    else
    {
        goxy(1, filepos + 3);
    }

    setc( BLACK + ( CYAN << 4 ) );
    bprintf("[%c]", tagged ? '\xFE' : ' ');
    setc( YELLOW + ( BLACK << 4 ) );

    if ( filepos >= max_lines )
    {
        goxy( 41, 3 + filepos - max_lines );
    }
    else
    {
        goxy( 2, filepos + 3 );
    }
}


void undrawscan(int filepos, long tagged)
{
    int max_lines = GetMaxLinesToShowForScanPlus();

    if ( filepos >= max_lines )
    {
        goxy( 40, 3 + filepos - max_lines );
    }
    else
    {
        goxy( 1, filepos + 3 );
    }
    bprintf( "|#7[|#1%c|#7]", tagged ? '\xFE' : ' ' );
}


long is_inscan( int dir )
{
    bool sysdir = false;
    if ( wwiv::stringUtils::IsEquals( udir[0].keys, "0" ) )
    {
        sysdir = true;
    }

    for ( int this_dir = 0; ( this_dir < GetSession()->num_dirs ); this_dir++ )
    {
        char szDir[50];
        sprintf( szDir, "%d", sysdir ? dir : dir + 1 );
        if ( wwiv::stringUtils::IsEquals( szDir, udir[this_dir].keys ) )
        {
            int ad = udir[this_dir].subnum;
            return ( qsc_n[ad / 32] & ( 1L << ad % 32 ) );
        }
    }

    return 0;
}


void reset_user_colors_to_defaults()
{
    for( int i = 0; i <= 9; i++ )
    {
        GetSession()->thisuser.SetColor( i, GetSession()->newuser_colors[ i ] );
        GetSession()->thisuser.SetBWColor( i, GetSession()->newuser_bwcolors[ i ] );
    }
}

