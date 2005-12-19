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


//
// Allows local-only editing of some of the user data in a shadowized
// window.
//


void OnlineUserEditor()
{
#if !defined (_UNIX)
	char sl[4], dsl[4], exempt[4], sysopsub[4], ar[17], dar[17], restrict[17], rst[17], uk[8], dk[8], up[6], down[6], posts[6], banktime[6], gold[10], ass[6], logons[6];
	int cp, i, rc = ABORTED;

	GetSession()->DisplaySysopWorkingIndicator( true );
	GetApplication()->GetLocalIO()->savescreen(&screensave);
	curatr = GetSession()->GetUserEditorColor();
	int wx = 5;
	int wy = 3;
	GetApplication()->GetLocalIO()->MakeLocalWindow(wx, wy-2, 70, 16 + 2);
    char szBar[ 255 ];
	sprintf( szBar, "Ã%s´", charstr( 70 - wx + 3, 'Ä' ) );
	GetApplication()->GetLocalIO()->LocalXYPrintf( wx, wy, szBar );
	GetApplication()->GetLocalIO()->LocalXYPrintf( wx, wy + 4, szBar );
	GetApplication()->GetLocalIO()->LocalXYPrintf( wx, wy + 7, szBar );
	GetApplication()->GetLocalIO()->LocalXYPrintf( wx, wy + 11, szBar );
	GetApplication()->GetLocalIO()->LocalXYPrintf( wx, wy + 13, szBar);
    sprintf( sl, "%u", GetSession()->GetCurrentUser()->GetSl() );
	sprintf( dsl, "%u", GetSession()->GetCurrentUser()->GetDsl() );
	sprintf( exempt, "%u", GetSession()->GetCurrentUser()->GetExempt() );
	if (*qsc > 999)
	{
		*qsc = 999;
	}
	sprintf( sysopsub, "%lu", *qsc );
    sprintf( uk, "%lu", GetSession()->GetCurrentUser()->GetUploadK() );
	sprintf( dk, "%lu", GetSession()->GetCurrentUser()->GetDownloadK() );
    sprintf( up, "%u", GetSession()->GetCurrentUser()->GetFilesUploaded() );
    sprintf( down, "%u", GetSession()->GetCurrentUser()->GetFilesDownloaded() );
    sprintf( posts, "%u", GetSession()->GetCurrentUser()->GetNumMessagesPosted() );
    sprintf( banktime, "%u", GetSession()->GetCurrentUser()->GetTimeBankMinutes() );
    sprintf( logons, "%u", GetSession()->GetCurrentUser()->GetNumLogons() );
    sprintf( ass, "%u", GetSession()->GetCurrentUser()->GetAssPoints() );

	_gcvt( GetSession()->GetCurrentUser()->GetGold(), 5, gold );
	strcpy( rst, restrict_string );
	for (i = 0; i <= 15; i++)
	{
		if ( GetSession()->GetCurrentUser()->hasArFlag(1 << i))
		{
			ar[i] = ( char ) ( 'A' + i );
		}
		else
		{
			ar[i] = SPACE;
		}
		if (GetSession()->GetCurrentUser()->hasDarFlag(1 << i))
		{
			dar[i] = ( char ) ( 'A' + i );
		}
		else
		{
			dar[i] = SPACE;
		}
		if ( GetSession()->GetCurrentUser()->hasRestrictionFlag ( 1 << i ) )
		{
			restrict[i] = rst[i];
		}
		else
		{
			restrict[i] = SPACE;
		}
	}
	dar[16]	= '\0';
	ar[16]	= '\0';
	restrict[16] = '\0';
	cp = 0;
	bool done = false;

    // heading
    char szLocalName[ 255 ];
    sprintf( szLocalName, "[%s]", GetSession()->GetCurrentUser()->GetUserNameAndNumber( GetSession()->usernum ) );
	GetApplication()->GetLocalIO()->LocalXYAPrintf( wx + 1, wy - 1, 31, " %-29.29s%s ", "WWIV User Editor", StringJustify( szLocalName, 37, SPACE, JUSTIFY_RIGHT ) );

    GetApplication()->GetLocalIO()->LocalXYAPrintf( wx + 2,  wy + 1, 3,   "Security Level(SL): %s", sl );
	GetApplication()->GetLocalIO()->LocalXYAPrintf( wx + 36, wy + 1, 3,   "  Message AR: %s", ar );
	GetApplication()->GetLocalIO()->LocalXYAPrintf( wx + 2,  wy + 2, 3,   "DL Sec. Level(DSL): %s", dsl );
	GetApplication()->GetLocalIO()->LocalXYAPrintf( wx + 36, wy + 2, 3,   " Download AR: %s", dar );
	GetApplication()->GetLocalIO()->LocalXYAPrintf( wx + 2,  wy + 3, 3,   "   User Exemptions: %s", exempt );
	GetApplication()->GetLocalIO()->LocalXYAPrintf( wx + 36, wy + 3, 3,   "Restrictions: %s", restrict );
	GetApplication()->GetLocalIO()->LocalXYAPrintf( wx + 2,  wy + 5, 3,   "         Sysop Sub: %s", sysopsub );
	GetApplication()->GetLocalIO()->LocalXYAPrintf( wx + 36, wy + 5, 3,   "   Time Bank: %s", banktime );
	GetApplication()->GetLocalIO()->LocalXYAPrintf( wx + 2,  wy + 6, 3,   "        Ass Points: %s", ass );
	GetApplication()->GetLocalIO()->LocalXYAPrintf( wx + 36, wy + 6, 3,   " Gold Points: %s", gold );
	GetApplication()->GetLocalIO()->LocalXYAPrintf( wx + 2,  wy + 8, 3,   "       KB Uploaded: %s", uk );
	GetApplication()->GetLocalIO()->LocalXYAPrintf( wx + 35, wy + 8, 3,   "KB Downloaded: %s", dk );
	GetApplication()->GetLocalIO()->LocalXYAPrintf( wx + 2,  wy + 9, 3,   "    Files Uploaded: %s", up );
	GetApplication()->GetLocalIO()->LocalXYAPrintf( wx + 32, wy + 9, 3,   "Files Downloaded: %s", down );
	GetApplication()->GetLocalIO()->LocalXYAPrintf( wx + 2,  wy + 10, 3,  "   Messages Posted: %s", posts );
	GetApplication()->GetLocalIO()->LocalXYAPrintf( wx + 32, wy + 10, 3,  "Number of Logons: %s", logons );
	GetApplication()->GetLocalIO()->LocalXYAPrintf( wx + 2,  wy + 12, 3,  "Note: %s", GetSession()->GetCurrentUser()->GetNote() );
	GetApplication()->GetLocalIO()->LocalXYAPrintf( wx + 1, wy + 14, 31,  "    (ENTER) Next Field   (UP-ARROW) Previous Field    (ESC) Exit    ");
    curatr = 3;
	while ( !done )
	{
		switch (cp)
		{
		case 0:
			GetApplication()->GetLocalIO()->LocalGotoXY(wx + 22, wy + 1);
			GetApplication()->GetLocalIO()->LocalEditLine(sl, 3, NUM_ONLY, &rc, "");
			GetSession()->GetCurrentUser()->SetSl( atoi( sl ) );
            sprintf( sl, "%d", GetSession()->GetCurrentUser()->GetSl() );
			GetApplication()->GetLocalIO()->LocalPrintf( "%-3s", sl );
			break;
		case 1:
			GetApplication()->GetLocalIO()->LocalGotoXY( wx + 50, wy + 1 );
			GetApplication()->GetLocalIO()->LocalEditLine( ar, 16, SET, &rc, "ABCDEFGHIJKLMNOP " );
			GetSession()->GetCurrentUser()->SetAr( 0 );
			for (i = 0; i <= 15; i++)
			{
				if (ar[i] != SPACE)
				{
                    GetSession()->GetCurrentUser()->SetArFlag(1 << i);
				}
			}
			break;
		case 2:
			GetApplication()->GetLocalIO()->LocalGotoXY(wx + 22, wy + 2);
			GetApplication()->GetLocalIO()->LocalEditLine(dsl, 3, NUM_ONLY, &rc, "");
			GetSession()->GetCurrentUser()->SetDsl( atoi( dsl ) );
            sprintf( dsl, "%d", GetSession()->GetCurrentUser()->GetDsl() );
			GetApplication()->GetLocalIO()->LocalPrintf( "%-3s", dsl );
			break;
		case 3:
			GetApplication()->GetLocalIO()->LocalGotoXY(wx + 50, wy + 2);
			GetApplication()->GetLocalIO()->LocalEditLine(dar, 16, SET, &rc, "ABCDEFGHIJKLMNOP ");
			GetSession()->GetCurrentUser()->SetDar( 0 );
			for (i = 0; i <= 15; i++)
			{
				if (dar[i] != SPACE)
				{
                    GetSession()->GetCurrentUser()->SetDarFlag( 1 << i );
				}
			}
			break;
		case 4:
			GetApplication()->GetLocalIO()->LocalGotoXY(wx + 22, wy + 3);
			GetApplication()->GetLocalIO()->LocalEditLine(exempt, 3, NUM_ONLY, &rc, "");
			GetSession()->GetCurrentUser()->SetExempt( atoi( exempt ) );
            sprintf( exempt, "%u", GetSession()->GetCurrentUser()->GetExempt() );
			GetApplication()->GetLocalIO()->LocalPrintf( "%-3s", exempt );
			break;
		case 5:
			GetApplication()->GetLocalIO()->LocalGotoXY(wx + 50, wy + 3);
			GetApplication()->GetLocalIO()->LocalEditLine(restrict, 16, SET, &rc, rst);
			GetSession()->GetCurrentUser()->SetRestriction( 0 );
			for (i = 0; i <= 15; i++)
			{
				if (restrict[i] != SPACE)
				{
					GetSession()->GetCurrentUser()->setRestrictionFlag( 1 << i );
				}
			}
			break;
		case 6:
			GetApplication()->GetLocalIO()->LocalGotoXY(wx + 22, wy + 5);
			GetApplication()->GetLocalIO()->LocalEditLine(sysopsub, 3, NUM_ONLY, &rc, "");
			*qsc = atoi( sysopsub );
            sprintf( sysopsub, "%lu", *qsc );
			GetApplication()->GetLocalIO()->LocalPrintf( "%-3s", sysopsub );
			break;
		case 7:
			GetApplication()->GetLocalIO()->LocalGotoXY(wx + 50, wy + 5);
			GetApplication()->GetLocalIO()->LocalEditLine(banktime, 5, NUM_ONLY, &rc, "");
			GetSession()->GetCurrentUser()->SetTimeBankMinutes( atoi( banktime ) );
            sprintf( banktime, "%u", GetSession()->GetCurrentUser()->GetTimeBankMinutes() );
			GetApplication()->GetLocalIO()->LocalPrintf( "%-5s", banktime );
			break;
		case 8:
			GetApplication()->GetLocalIO()->LocalGotoXY(wx + 22, wy + 6);
			GetApplication()->GetLocalIO()->LocalEditLine(ass, 5, NUM_ONLY, &rc, "");
			GetSession()->GetCurrentUser()->SetAssPoints( atoi( ass ) );
            sprintf( ass, "%u", GetSession()->GetCurrentUser()->GetAssPoints() );
			GetApplication()->GetLocalIO()->LocalPrintf( "%-5s", ass );
			break;
		case 9:
			GetApplication()->GetLocalIO()->LocalGotoXY(wx + 50, wy + 6);
			GetApplication()->GetLocalIO()->LocalEditLine( gold, 5, NUM_ONLY, &rc, "" );
			GetSession()->GetCurrentUser()->SetGold( static_cast<float>( atof( gold ) ) );
			_gcvt( GetSession()->GetCurrentUser()->GetGold(), 5, gold );
			GetApplication()->GetLocalIO()->LocalPrintf( "%-5s", gold );
			break;
		case 10:
			GetApplication()->GetLocalIO()->LocalGotoXY(wx + 22, wy + 8);
			GetApplication()->GetLocalIO()->LocalEditLine(uk, 7, NUM_ONLY, &rc, "");
			GetSession()->GetCurrentUser()->SetUploadK( atol( uk ) );
            sprintf( uk, "%lu", GetSession()->GetCurrentUser()->GetUploadK() );
			GetApplication()->GetLocalIO()->LocalPrintf( "%-7s", uk );
			break;
		case 11:
			GetApplication()->GetLocalIO()->LocalGotoXY(wx + 50, wy + 8);
			GetApplication()->GetLocalIO()->LocalEditLine(dk, 7, NUM_ONLY, &rc, "");
			GetSession()->GetCurrentUser()->SetDownloadK( atol( dk ) );
            sprintf( dk, "%lu", GetSession()->GetCurrentUser()->GetDownloadK() );
            GetApplication()->GetLocalIO()->LocalPrintf( "%-7s", dk );
			break;
		case 12:
			GetApplication()->GetLocalIO()->LocalGotoXY(wx + 22, wy + 9);
			GetApplication()->GetLocalIO()->LocalEditLine(up, 5, NUM_ONLY, &rc, "");
			GetSession()->GetCurrentUser()->SetFilesUploaded( atoi( up ) );
            sprintf( up, "%u", GetSession()->GetCurrentUser()->GetFilesUploaded() );
			GetApplication()->GetLocalIO()->LocalPrintf( "%-5s", up );
			break;
		case 13:
			GetApplication()->GetLocalIO()->LocalGotoXY( wx + 50, wy + 9 );
			GetApplication()->GetLocalIO()->LocalEditLine( down, 5, NUM_ONLY, &rc, "" );
			GetSession()->GetCurrentUser()->SetFilesDownloaded( atoi( down ) );
            sprintf( down, "%u", GetSession()->GetCurrentUser()->GetFilesDownloaded() );
			GetApplication()->GetLocalIO()->LocalPrintf( "%-5s", down );
			break;
		case 14:
			GetApplication()->GetLocalIO()->LocalGotoXY(wx + 22, wy + 10);
			GetApplication()->GetLocalIO()->LocalEditLine(posts, 5, NUM_ONLY, &rc, "");
			GetSession()->GetCurrentUser()->SetNumMessagesPosted( atoi( posts ) );
            sprintf( posts, "%u", GetSession()->GetCurrentUser()->GetNumMessagesPosted() );
			GetApplication()->GetLocalIO()->LocalPrintf( "%-5s", posts );
			break;
		case 15:
			GetApplication()->GetLocalIO()->LocalGotoXY(wx + 50, wy + 10);
			GetApplication()->GetLocalIO()->LocalEditLine(logons, 5, NUM_ONLY, &rc, "");
            GetSession()->GetCurrentUser()->SetNumLogons( atoi( logons ) );
            sprintf( logons, "%u", GetSession()->GetCurrentUser()->GetNumLogons() );
			GetApplication()->GetLocalIO()->LocalPrintf( "%-5s", logons );
			break;
		case 16:
            {
                char szNote[ 81 ];
			    GetApplication()->GetLocalIO()->LocalGotoXY( wx + 8, wy + 12 );
                strcpy( szNote, GetSession()->GetCurrentUser()->GetNote() );
			    GetApplication()->GetLocalIO()->LocalEditLine( szNote, 60, ALL, &rc, "" );
			    StringTrimEnd( szNote );
                GetSession()->GetCurrentUser()->SetNote( szNote );
            }
			break;
    }
    switch (rc)
	{
	case ABORTED:
        done = true;
        break;
	case DONE:
        done = true;
        break;
	case NEXT:
        cp = (cp + 1) % 17;
        break;
	case PREV:
        cp--;
        if ( cp < 0 )
		{
			cp = 16;
		}
        break;
    }
  }
  GetApplication()->GetLocalIO()->restorescreen(&screensave);
  GetSession()->ResetEffectiveSl();
  changedsl();
  GetSession()->DisplaySysopWorkingIndicator( false );
#endif // !defined (_UNIX)
}



/**
 * This function prints out a string, with a user-specifiable delay between
 * each character, and a user-definable pause after the entire string has
 * been printed, then it backspaces the string. The color is also definable.
 * The parameters are as follows:
 * <p>
 * <em>Note: ANSI is not required.</em>
 * <p>
 * Example:
 * <p>
 * BackPrint("This is an example.",3,20,500);
 *
 * @param pszText  The string to print
 * @param nColorCode The color of the string
 * @param nCharDelay Delay between each character, in milliseconds
 * @param nStringDelay Delay between completion of string and backspacing
 */
void BackPrint( const char *pszText, int nColorCode, int nCharDelay, int nStringDelay )
{
	WWIV_ASSERT( pszText );

	bool oecho = echo;
	echo = true;
	int nLength = strlen( pszText );
	GetSession()->bout.Color( nColorCode );
	WWIV_Delay( nCharDelay );
	int nPos = 0;
	while ( pszText[nPos]  && !hangup )
	{
		bputch( pszText[nPos] );
		nPos++;
		WWIV_Delay( nCharDelay );
	}

	WWIV_Delay( nStringDelay );
	for ( int i = 0; i < nLength && !hangup; i++ )
	{
		BackSpace();
		WWIV_Delay( 5 );
	}
	echo = oecho;
}


/**
 * This function will reposition the cursor i spaces to the left, or if the
 * cursor is on the left side of the screen already then it will not move.
 * If the user has no ANSI then nothing happens.
 * @param nNumberOfChars Number of characters to move to the left
 */
void MoveLeft( int nNumberOfChars )
{
	if ( okansi() )
	{
		GetSession()->bout << "\x1b[" << nNumberOfChars << "D";
	}
}


/**
 * Moves the cursor to the end of the line using ANSI sequences.  If the user
 * does not have ansi, this this function does nothing.
 */
void ClearEOL()
{
	if ( okansi() )
	{
		GetSession()->bout << "\x1b[K";
	}
}


/**
 * This function will print out a string, making each character "spin"
 * using the / - \ | sequence. The color is definable and is the
 * second parameter, not the first. If the user does not have ANSI
 * then the string is simply printed normally.
 * @param
 */
void SpinPuts( const char *pszText, int nColorCode )
{
	bool oecho	= echo;
	echo		= true;

	WWIV_ASSERT( pszText );

	if ( okansi() )
	{
		GetSession()->bout.Color( nColorCode );
	    const int dly = 30;
	    int nPos = 0;
		while ( pszText[nPos] && !hangup )
		{
			WWIV_Delay(dly);
			GetSession()->bout << "/";
			MoveLeft( 1 );
			WWIV_Delay(dly);
			GetSession()->bout << "-";
			MoveLeft( 1 );
			WWIV_Delay(dly);
			GetSession()->bout << "\\";
			MoveLeft( 1 );
			WWIV_Delay(dly);
			GetSession()->bout << "|";
			MoveLeft( 1 );
			WWIV_Delay(dly);
			bputch(pszText[nPos]);
			nPos++;
		}
	}
	else
	{
		GetSession()->bout << pszText;
	}
	echo = oecho;
}
