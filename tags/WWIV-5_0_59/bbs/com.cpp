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
// Local functions
//

void addto( char *pszAnsiString, int nNumber );


void RestoreCurrentLine(const char *cl, const char *atr, const char *xl, const char *cc)
{
    if ( GetApplication()->GetLocalIO()->WhereX() )
	{
        nl();
	}
    for ( int i = 0; cl[i] != 0; i++ )
	{
        setc( atr[i] );
        bputch( cl[i], true );
    }
    FlushOutComChBuffer();
    setc( *cc );
    strcpy( endofline, xl );
}


#define OUTCOMCH_BUFFER_SIZE 1024
static char s_szOutComChBuffer[ OUTCOMCH_BUFFER_SIZE + 1 ];
static int  s_nOutComChBufferPosition = 0;


void FlushOutComChBuffer()
{
    if ( s_nOutComChBufferPosition > 0 )
    {
        GetApplication()->GetComm()->write( s_szOutComChBuffer, s_nOutComChBufferPosition );
        s_nOutComChBufferPosition = 0;
        memset( s_szOutComChBuffer, 0, OUTCOMCH_BUFFER_SIZE + 1 );
    }
}


void rputch( char ch, bool bUseInternalBuffer )
{

    if ( ok_modem_stuff && NULL != GetApplication()->GetComm() )
    {
        if ( bUseInternalBuffer )
        {
            if ( s_nOutComChBufferPosition >= OUTCOMCH_BUFFER_SIZE )
            {
                FlushOutComChBuffer();
            }
            s_szOutComChBuffer[ s_nOutComChBufferPosition++ ] = ch;
        }
        else
        {
            GetApplication()->GetComm()->putW(ch);
        }
    }
}



// Note: if this function is called anywhere except for from the
// WFC, it will break the UNIX implementation of WComm unless
// Wiou implements some fake method of peek (i.e. caching the
// character read, and using it as the return value of read)
char rpeek_wfconly()
{
    if ( ok_modem_stuff && !global_xx )
    {
        return ( ( char ) GetApplication()->GetComm()->peek() );
    }
    return 0;
}


char bgetchraw()
{
    if ( ok_modem_stuff && !global_xx && NULL != GetApplication()->GetComm() )
    {
        if ( GetApplication()->GetComm()->incoming() )
        {
            return ( GetApplication()->GetComm()->getW() );
        }
        if ( GetApplication()->GetLocalIO()->LocalKeyPressed() )
        {
            return ( GetApplication()->GetLocalIO()->getchd1() );
        }
    }
    return 0;
}


bool bkbhitraw()
{
    if ( ok_modem_stuff && !global_xx )
    {
        return ( GetApplication()->GetComm()->incoming() || GetApplication()->GetLocalIO()->LocalKeyPressed() );
    }
    else if ( GetApplication()->GetLocalIO()->LocalKeyPressed() )
    {
        return true;
    }
    return false;
}


void dump()
{
    if ( ok_modem_stuff )
    {
		GetApplication()->GetComm()->purgeOut();
		GetApplication()->GetComm()->purgeIn();
    }
}


bool CheckForHangup()
// This function checks to see if the user logged on to the com port has
// hung up.  Obviously, if no user is logged on remotely, this does nothing.
// returns the value of hangup
{
    if ( !hangup && GetSession()->using_modem && !GetApplication()->GetComm()->carrier() )
    {
        hangup = hungup = true;
        if ( GetSession()->IsUserOnline() )
        {
            sysoplog( "Hung Up." );
            std::cout << "Hung Up!";
        }
    }
    return hangup;
}


void addto( char *pszAnsiString, int nNumber )
{
    char szBuffer[ 20 ];

	strcat( pszAnsiString, ( pszAnsiString[0]) ? ";" : "\x1b[" );
    snprintf( szBuffer, sizeof( szBuffer ), "%d", nNumber );
    strcat( pszAnsiString, szBuffer );
}


void makeansi( int attr, char *pszOutBuffer, bool forceit )
/* Passed to this function is a one-byte attribute as defined for IBM type
* screens.  Returned is a string which, when printed, will change the
* display to the color desired, from the current function.
*/
{
    char *temp = "04261537";

    int catr = curatr;
    pszOutBuffer[0] = '\0';
    if ( attr != catr )
    {
        if ( ( catr & 0x88 ) ^ ( attr & 0x88 ) )
        {
            addto(pszOutBuffer, 0);
            addto(pszOutBuffer, 30 + temp[attr & 0x07] - '0');
            addto(pszOutBuffer, 40 + temp[(attr & 0x70) >> 4] - '0');
            catr = ( attr & 0x77 );
        }
        if ((catr & 0x07) != (attr & 0x07))
        {
            addto(pszOutBuffer, 30 + temp[attr & 0x07] - '0');
        }
        if ((catr & 0x70) != (attr & 0x70))
        {
            addto(pszOutBuffer, 40 + temp[(attr & 0x70) >> 4] - '0');
        }
        if ((catr & 0x08) ^ (attr & 0x08))
        {
            addto(pszOutBuffer, 1);
        }
        if ((catr & 0x80) ^ (attr & 0x80))
        {
            if ( checkcomp( "Mac" ) )
            {
				// This is the code for Mac's underline
				// They don't have Blinking or Italics
                addto( pszOutBuffer, 4 );
            }
            else if ( checkcomp( "Ami" ) )
            {
				// Some Amiga terminals use 3 instead of
				// 5 for italics.  Using both won't hurt
                addto( pszOutBuffer, 3 );
            }
			// anything, only italics will be generated
            addto( pszOutBuffer, 5 );
        }
    }
    if ( pszOutBuffer[0] )
    {
        strcat( pszOutBuffer, "m" );
    }
    if ( !okansi() && !forceit )
    {
        pszOutBuffer[0] = '\0';
    }
}




// This function performs a CR/LF sequence to move the cursor to the next
// line.  If any end-of-line ANSI codes are set (such as changing back to
// the default color) are specified, those are executed first.
void nl( int nNumLines )
{
    for (int i = 0; i < nNumLines; i++)
    {
        if (endofline[0])
	    {
            GetSession()->bout << endofline;
            endofline[0] = 0;
        }
        bputs("\r\n");
        if ( inst_msg_waiting() && !bChatLine )
	    {
            process_inst_msgs();
	    }
    }
}

void BackSpace()
// This function executes a BACKSPACE, SPACE, BACKSPACE sequence.
{
    bool bSavedEcho = echo;
    echo = true;
    bputs("\b \b");
    echo = bSavedEcho;
}


void setc( int nColor )
/* This sets the current color (both locally and remotely) to that
* specified (in IBM format).
*/
{
    char szBuffer[30];
    makeansi( nColor, szBuffer, false );
    bputs( szBuffer );
}


void resetnsp()
{
    if ( nsp == 1 && !( GetSession()->thisuser.hasPause() ) )
    {
        GetSession()->thisuser.toggleStatusFlag( WUser::pauseOnPage );
    }
    nsp=0;
}


bool bkbhit()
{
    if ( x_only )
    {
        std::cout << "x_only set!" << std::endl;
        return false;
    }

    if ( ( GetApplication()->GetLocalIO()->LocalKeyPressed() || ( incom && bkbhitraw() ) ||
         ( charbufferpointer && charbuffer[charbufferpointer] ) ) ||
		 bquote )
    {
        return true;
    }
    return false;
}


void mpl( int nNumberOfChars )
/* This will make a reverse-video prompt line i characters long, repositioning
* the cursor at the beginning of the input prompt area.  Of course, if the
* user does not want ansi, this routine does nothing.
*/
{
    if ( okansi() )
    {
        ansic( 4 );
        for ( int i = 0; i < nNumberOfChars; i++ )
        {
            bputch( ' ', true );
        }
        FlushOutComChBuffer();
        GetSession()->bout << "\x1b[" << nNumberOfChars << "D";
    }
}


char getkey()
/* This function returns one character from either the local keyboard or
* remote com port (if applicable).  After 1.5 minutes of inactivity, a
* beep is sounded.  After 3 minutes of inactivity, the user is hung up.
*/
{
    resetnsp();
    int beepyet = 0;
    timelastchar1 = timer1();

    using namespace wwiv::stringUtils;
    long tv = ( so() || IsEqualsIgnoreCase( GetSession()->GetCurrentSpeed().c_str(), "TELNET" ) ) ? 10920L : 3276L;
    long tv1 = tv - 1092L;     // change 4.31 Build3

    if ( !GetSession()->tagging || GetSession()->thisuser.isUseNoTagging() )
    {
        lines_listed = 0;
    }

    char ch = 0;
    do
    {
        while ( !bkbhit() && !hangup )
        {
            giveup_timeslice();
            long dd = timer1();
            if ( dd < timelastchar1 && ( ( dd + 1000 ) > timelastchar1 ) )
            {
                timelastchar1 = dd;
            }
            if (labs(dd - timelastchar1) > 65536L)
            {
                timelastchar1 -= 1572480L;	// # secs per day * 18.2
            }
            if (((dd - timelastchar1) > tv1) && (!beepyet))
            {
                beepyet = 1;
                bputch( CG );
            }
            if ( GetApplication()->IsShutDownActive() )
            {
                if ((( GetApplication()->GetShutDownTime() - timer()) < 120) && ((GetApplication()->GetShutDownTime() - timer()) > 60))
                {
                    if ( GetApplication()->GetShutDownStatus() != WBbsApp::shutdownTwoMinutes )
                    {
                        shut_down( WBbsApp::shutdownTwoMinutes );
                        GetApplication()->SetShutDownStatus( WBbsApp::shutdownTwoMinutes );
                    }
                }
                if (((GetApplication()->GetShutDownTime() - timer()) < 60) && ((GetApplication()->GetShutDownTime() - timer()) > 0))
                {
                    if ( GetApplication()->GetShutDownStatus() != WBbsApp::shutdownOneMinute )
                    {
                        shut_down( WBbsApp::shutdownOneMinute );
                        GetApplication()->SetShutDownStatus( WBbsApp::shutdownOneMinute );
                    }
                }
                if ( ( GetApplication()->GetShutDownTime() - timer() ) <= 0 )
                {
                    shut_down( WBbsApp::shutdownImmediate );
                }
            }
            if (labs(dd - timelastchar1) > tv)
            {
                nl();
                GetSession()->bout << "Call back later when you are there.\r\n";
                hangup = true;
            }
            CheckForHangup();
        }
        ch = bgetch();
    } while ( !ch && !hangup );
    return ch;
}


static void print_yn(int i)
{
// TODO Add random Strings back in.
/*

    if (num_strings(i))
    {
        GetSession()->bout << getrandomstring(i);
		nl();
    }
    else
    {
*/
	switch (i)
	{
	case 2:
		GetSession()->bout << YesNoString( true );
		break;
	case 3:
		GetSession()->bout << YesNoString( false );
		break;
	}
	nl();
//    }
}


bool yesno()
/* The keyboard is checked for either a Y, N, or C/R to be hit.  C/R is
* assumed to be the same as a N.  Yes or No is output, and yn is set to
* zero if No was returned, and yesno() is non-zero if Y was hit.
*/
{
    char ch = 0;

    ansic( 1 );
    while ((!hangup) && ((ch = wwiv::UpperCase<char>(getkey())) != *(YesNoString( true ))) && (ch != *(YesNoString( false ))) && (ch != RETURN))
        ;

    if (ch == *(YesNoString( true )))
    {
        print_yn( 2 );
    }
    else
    {
        print_yn( 3 );
    }
    return (ch == *(YesNoString( true ))) ? true : false;
}


/**
 * This is the same as yesno(), except C/R is assumed to be "Y"
 */
bool noyes()
{
    char ch = 0;

    ansic( 1 );
    while ((!hangup) && ((ch = wwiv::UpperCase<char>(getkey())) != *(YesNoString( true ))) && (ch != *(YesNoString( false ))) && (ch != RETURN))
        ;

    if (ch == *(YesNoString( false )))
    {
        print_yn( 3 );
    }
    else
    {
        print_yn( 2 );
    }
    return ( ch == *(YesNoString( true )) || ch == RETURN ) ? true : false;
}


char ynq()
{
    char ch = 0;

    ansic( 1 );
    while ( !hangup &&
		    ( ch = wwiv::UpperCase<char>( getkey() ) ) != *( YesNoString( true ) ) &&
			ch != *(YesNoString( false )) &&
		    ( ch != *str_quit ) && ( ch != RETURN ) )
	{
		// NOP
		;
	}
    if ( ch == *( YesNoString( true ) ) )
    {
        ch = 'Y';
        print_yn( 2 );
    }
    else if ( ch == *str_quit )
    {
        ch = 'Q';
        GetSession()->bout << str_quit;
		nl();
    }
    else
    {
        ch = 'N';
        print_yn( 3 );
    }
    return ch;
}


void ansic(int wwivColor)
{
    unsigned char c = '\0';

    if ( wwivColor <= -1 && wwivColor >= -16 )
    {
        c = ( GetSession()->thisuser.hasColor() ?
        rescolor.resx[207 + abs(wwivColor)] : GetSession()->thisuser.GetBWColor( 0 ) );
    }
    if ( wwivColor >= 0 && wwivColor <= 9 )
    {
        c = ( GetSession()->thisuser.hasColor() ?
        GetSession()->thisuser.GetColor( wwivColor ) : GetSession()->thisuser.GetBWColor( wwivColor ) );
    }
    if ( wwivColor >= 10 && wwivColor <= 207 )
    {
        c = ( GetSession()->thisuser.hasColor() ?
        rescolor.resx[wwivColor - 10] : GetSession()->thisuser.GetBWColor( 0 ) );
    }
    if ( c == curatr )
    {
        return;
    }

    setc( c );

    makeansi( GetSession()->thisuser.hasColor() ?
        GetSession()->thisuser.GetColor( 0 ) : GetSession()->thisuser.GetBWColor( 0 ), endofline, false );
}

char onek( const char *pszAllowableChars, bool bAutoMpl )
{
    if ( bAutoMpl )
    {
        mpl( 1 );
    }
	char ch = onek1( pszAllowableChars );
    bputch(ch);
    nl();
    return ch;
}


void reset_colors()
{
    // ANSI Clear Attributes String
    GetSession()->bout << "\x1b[0m";
}

void goxy(int x, int y)
{
    if ( okansi() )
    {
		y = std::min<int>( y, GetSession()->screenlinest );	// Don't get Y get too big or mTelnet will not be happy
        GetSession()->bout << "\x1b[" << y << ";" << x << "H";
    }
}
char onek1(const char *pszAllowableChars)
{
	WWIV_ASSERT( pszAllowableChars );
    char ch = 0;

    while (!strchr(pszAllowableChars, ch = upcase(getkey())) && !hangup)
        ;

    if (hangup)
    {
        ch = pszAllowableChars[0];
    }
    return ch;
}



/** Backspaces from the current cursor position to the beginning of a line */
void BackLine()
{
    ansic( 0 );
    bputch(SPACE);
    for (int i = GetApplication()->GetLocalIO()->WhereX(); i > 0; i--)
    {
        BackSpace();
    }
}



/**
 * This function outputs a string of characters to the screen (and remotely
 * if applicable).  The com port is also checked first to see if a remote
 * user has hung up
 */
int bputs( const char *pszText )
{
    if ( !pszText || !( *pszText ) )
    {
        return 0;
    }
    int displayed = strlen( pszText );

    CheckForHangup();
    if ( !hangup )
    {
        int i = 0;

        while ( pszText[i] )
        {
            bputch( pszText[i++], true );
        }
        FlushOutComChBuffer();
    }

    return displayed;
}


/**
 * printf sytle output function.  Most code should use this when writing
 * locally + remotely.
 */
int bprintf( const char *pszFormatText,... )
{
    va_list ap;
    char szBuffer[ 2048 ];

    va_start( ap, pszFormatText );
    vsnprintf( szBuffer, sizeof( szBuffer ), pszFormatText, ap );
    va_end( ap );
    return bputs( szBuffer );
}


/**
 * Outputs title bar containing variable argument text contained in 'pszFormatText' in
 * a 'steely-bar' fashion.  Non-ANSI outputs B&W, centered on screen.
 */
void DisplayLiteBar( const char *pszFormatText,... )
{
    va_list ap;
    char s[1024], s1[1024];

    va_start( ap, pszFormatText );
    vsnprintf( s, sizeof( s ), pszFormatText, ap );
    va_end( ap );

    if ( strlen( s ) % 2 != 0 )
    {
        strcat( s, " " );
    }
    int i = ( 74 - strlen( s ) ) / 2;
    if ( okansi() )
    {
        snprintf( s1, sizeof( s1 ), "%s%s%s", charstr( i, ' ' ), stripcolors( s ), charstr( i, ' ' ) );
		GetSession()->bout << "\x1B[0;1;37m" << charstr( strlen( s1 ) + 4, 'Ü' ) << wwiv::endl;
        GetSession()->bout << "\x1B[0;34;47m  " << s1 << "  \x1B[40m\r\n";
		GetSession()->bout << "\x1B[0;1;30m" << charstr( strlen( s1 ) + 4, 'ß' ) << wwiv::endl;
    }
    else
    {
		GetSession()->bout << charstr( i, ' ' ) << s << wwiv::endl;
    }
}

