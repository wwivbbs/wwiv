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


void pausescr()
// This will pause output, displaying the [PAUSE] message, and wait
// a key to be hit.
{
    int i1, i3, warned;
	int i = 0;
    char s[81];
    char ch;
    double ttotal;
    time_t tstart, tstop;

    if ( x_only )
	{
        return;
	}

    nsp = 0;

    int oiia = setiia( 0 );

    char * ss = str_pause;
    int i2 = i1 = strlen(ss);

    bool com_freeze = incom;

    if ( !incom && outcom )
	{
        incom = true;
	}

    if ( okansi() )
    {
        GetSession()->bout.ResetColors();

        i1 = strlen( reinterpret_cast<char*>( stripcolors( ss ) ) );
        i = curatr;
        GetSession()->bout.SystemColor( GetSession()->GetCurrentUser()->hasColor() ? GetSession()->GetCurrentUser()->GetColor( 3 ) :
              GetSession()->GetCurrentUser()->GetBWColor( 3 ) );
        GetSession()->bout << ss << "\x1b[" << i1 << "D";
        GetSession()->bout.SystemColor( i );
    }
    else
    {
        if ( okansi() )
        {
            i = curatr;
            GetSession()->bout.SystemColor( GetSession()->GetCurrentUser()->hasColor() ? GetSession()->GetCurrentUser()->GetColor( 3 ) : GetSession()->GetCurrentUser()->GetBWColor( 3 ) );
        }
        for (i3 = 0; i3 < i2; i3++)
        {
            if ( ( ss[i3] == CC ) && i1 > 1 )
            {
                i1 -= 2;
            }
			GetSession()->bout << ss;
            for ( i3 = 0; i3 < i1; i3++ )
            {
                BackSpace();
            }
            if ( okansi() )
            {
                GetSession()->bout.SystemColor( i );
            }
        }
    }

    if ( okansi() )
    {
        time( &tstart );

        lines_listed = 0;
        warned = 0;
        do
        {
            while ( !bkbhit() && !hangup )
            {
                time( &tstop );
                ttotal = difftime( tstop, tstart );
                if ( ttotal == 120 )
                {
                    if ( !warned )
                    {
                        warned = 1;
                        bputch( CG );
                        GetSession()->bout.SystemColor( GetSession()->GetCurrentUser()->hasColor() ? GetSession()->GetCurrentUser()->GetColor( 6 ) :
                        GetSession()->GetCurrentUser()->GetBWColor( 6 ) );
                        GetSession()->bout << ss;
                        for ( i3 = 0; i3 < i2; i3++ )
                        {
                            if ( s[i3] == 3 && i1 > 1 )
                            {
                                i1 -= 2;
                            }
                        }
                        GetSession()->bout << "\x1b[" << i1 << "D";
                        GetSession()->bout.SystemColor( i );
                    }
                }
                else
                {
                    if ( ttotal > 180 )
                    {
                        bputch( CG );
                        for ( i3 = 0; i3 < i1; i3++ )
                        {
                            bputch( ' ' );
                        }
                        GetSession()->bout << "\x1b[" << i1 << "D";
                        GetSession()->bout.SystemColor( i );
                        setiia( oiia );
                        return;
                    }
                }
                WWIV_Delay( 100 );
                WWIV_Delay( 0 );
                CheckForHangup();
            }
            ch = bgetch();
			int nKey = wwiv::UpperCase< int >( ch );
			switch ( nKey )
            {
            case ESC:
            case 'Q':
            case 'N':
                if ( !bkbhit() )
                {
                    nsp = -1;
                }
                break;
            case 'C':
            case '=':
                if ( GetSession()->GetCurrentUser()->hasPause() )
                {
                    nsp = 1;
                    GetSession()->GetCurrentUser()->toggleStatusFlag( WUser::pauseOnPage );
                }
                break;
            default:
                break;
            }
        } while ( !ch && !hangup );
        for ( i3 = 0; i3 < i1; i3++ )
        {
            bputch(' ');
        }
        GetSession()->bout << "\x1b[" << i1 << "D";
        GetSession()->bout.SystemColor( i );
        setiia( oiia );
    }
    if ( !com_freeze )
    {
        incom = false;
    }
}


