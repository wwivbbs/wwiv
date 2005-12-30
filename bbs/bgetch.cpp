/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2006, WWIV Software Services             */
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
// Local data structures
//

static int MACRO_KEY_TABLE[] = { 0, 2, 0, 0, 0, 0, 1 };

//
// Local functions
//

void HandleControlKey( char *ch );
void PrintTime();
void RedrawCurrentLine();


char bgetch()
/* This function checks both the local keyboard, and the remote terminal
* (if any) for input.  If there is input, the key is returned.  If there
* is no input, a zero is returned.  Function keys hit are interpreted as
* such within the routine and not returned.
*/
{
    char ch = 0;
    static int qpointer = 0, cpointer;

    if (bquote)
    {
        if (!qpointer)
        {
            charbuffer[1] = '0';
            charbuffer[2] = RETURN;
            charbuffer[3] = '\0';
            cpointer = 0;
            qpointer = 1;
            while (qpointer < bquote + 2)
            {
                if ( quotes_ind[cpointer++] == SOFTRETURN )
                {
                    if (quotes_ind[cpointer] != CD)
                    {
                        ++qpointer;
                    }
                }
            }
            charbufferpointer = 1;
        }
        while (quotes_ind[cpointer] == CD )
        {
            while (quotes_ind[cpointer++] != SOFTRETURN)
                // Do nothing...
                ;
        }
        if (quotes_ind[cpointer] == SOFTRETURN)
        {
            ++qpointer;
            if (qpointer > equote + 2)
            {
                qpointer = 0;
                bquote = 0;
                equote = 0;
                return CP;
            }
            else
            {
                ++cpointer;
            }
        }
        if (quotes_ind[cpointer] == CC)
        {
            ++cpointer;
            return CP;
        }
        if (quotes_ind[cpointer] == 0)
        {
            qpointer = 0;
            bquote = 0;
            equote = 0;
            return RETURN;
        }
        return quotes_ind[cpointer++];
    }
    if (x_only)
    {
        return 0;
    }

    if (charbufferpointer)
    {
        if (!charbuffer[charbufferpointer])
        {
            charbufferpointer = charbuffer[0] = 0;
        }
        else
        {
            if ((charbuffer[charbufferpointer]) == CC)
            {
                charbuffer[charbufferpointer] = CP;
            }
            return charbuffer[charbufferpointer++];
        }
    }
    if ( GetSession()->localIO()->LocalKeyPressed() )
    {
        ch = GetSession()->localIO()->getchd1();
        GetSession()->SetLastKeyLocal( true );
        if (!(g_flags & g_flag_allow_extended))
        {
            if (!ch)
            {
                ch = GetSession()->localIO()->getchd1();
                GetSession()->localIO()->skey(ch);
                ch = static_cast< char >(((ch == F10) || (ch == CF10)) ? 2 : 0);
            }
        }
        timelastchar1 = timer1();
    }
    else if (incom && bkbhitraw())
    {
        ch = bgetchraw();
        GetSession()->SetLastKeyLocal( false );
    }

    if (!(g_flags & g_flag_allow_extended))
    {
        HandleControlKey( &ch );
    }

    return ch;
}


void HandleControlKey( char *ch )
{
    char c = *ch;

    if ( c == CBACKSPACE )
    {
        c = BACKSPACE;
    }
    if ( okskey )
    {
        switch ( c )
        {
          case CA:   // CTRL-A
          case CD:   // CTRL-D
          case CF:   // CTRL-F
              if (okmacro && (!charbufferpointer))
              {
				  int macroNum = MACRO_KEY_TABLE[(int)c];
				  strncpy(charbuffer, &(GetSession()->GetCurrentUser()->GetMacro(macroNum)[0]), sizeof(charbuffer)-1);
				  c = charbuffer[0];
                  if (c)
				  {
                      charbufferpointer = 1;
				  }
              }
              break;
          case CT:  // CTRL - T
              if ( echo )
              {
                  PrintTime();
              }
              break;
		  case CU:	// CTRL-U
			  if ( echo )
			  {
                  char xl[81], cl[81], atr[81], cc;
				  GetSession()->localIO()->SaveCurrentLine(cl, atr, xl, &cc);
				  GetSession()->bout.Color( 0 );
				  GetSession()->bout.NewLine( 2 );
				  multi_instance();
				  GetSession()->bout.NewLine();
			      RestoreCurrentLine(cl, atr, xl, &cc);
			  }
			  break;
          case CR:
              if ( echo )
              {
                  RedrawCurrentLine();
              }
              break;
          case CL:  // CTRL - L
              if (so())
              {
                  toggle_invis();
              }
              break;
          case CN:  // CTRL - N
              toggle_avail();
              break;
          case CY:
              GetSession()->GetCurrentUser()->ToggleStatusFlag( WUser::pauseOnPage );
              break;
        }
    }
    *ch = c;
}


void PrintTime()
{
    char xl[81], cl[81], atr[81], cc;

    GetSession()->localIO()->SaveCurrentLine( cl, atr, xl, &cc );

    GetSession()->bout.Color( 0 );
    GetSession()->bout.NewLine( 2 );
	time_t l = time( NULL );
	std::string currentTime = asctime( localtime( &l ) );

	//Remove the ending \n character.
	currentTime.erase( currentTime.find_last_of( "\r\n" ) );

	GetSession()->bout << "|#2" << currentTime << wwiv::endl;
    if ( GetSession()->IsUserOnline() )
    {
		GetSession()->bout << "|#9Time on   = |#1" << ctim( timer() - timeon ) << wwiv::endl;
		GetSession()->bout << "|#9Time left = |#1" << ctim( nsl() ) << wwiv::endl;
    }
    GetSession()->bout.NewLine();

    RestoreCurrentLine( cl, atr, xl, &cc );
}


void RedrawCurrentLine()
{
    char xl[81], cl[81], atr[81], cc, ansistr_1[81];

    int ansiptr_1 = ansiptr;
    ansiptr = 0;
    ansistr[ansiptr_1] = 0;
    strncpy(ansistr_1, ansistr, sizeof(ansistr_1)-1);

    GetSession()->localIO()->SaveCurrentLine(cl, atr, xl, &cc);
    GetSession()->bout.NewLine();
    RestoreCurrentLine(cl, atr, xl, &cc);

    strcpy(ansistr, ansistr_1);
    ansiptr = ansiptr_1;
}


