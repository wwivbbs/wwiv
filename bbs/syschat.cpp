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


//////////////////////////////////////////////////////////////////////////////
//
//
// module private functions
//
//

void chatsound(int sf, int ef, int uf, int dly1, int dly2, int rp);


//////////////////////////////////////////////////////////////////////////////
//
// static variables for use in two_way_chat
//

#define MAXLEN 160
#define MAXLINES_SIDE 13

static int wwiv_x1, wwiv_y1, wwiv_x2, wwiv_y2, cp0, cp1;
static char (*side0)[MAXLEN], (*side1)[MAXLEN];


//////////////////////////////////////////////////////////////////////////////
//
// Makes various (local-only) sounds based upon input params. The params are:
//     sf = starting frequency, in hertz
//     ef = ending frequency, in hertz
//     uf = frequency change, in hertz, for each step
//   dly1 = delay, in milliseconds, between each step, when going from sf
//             to ef
//   dly2 = delay, in milliseconds, between each repetition of the sound
//             sequence
//     rp = number of times to play the whole sound sequence
//

void chatsound(int sf, int ef, int uf, int dly1, int dly2, int rp)
{
	for (int i1 = 0; i1 < rp; i1++)
	{
		if (sf < ef)
		{
			for (int i = sf; i < ef; i += uf)
			{
				WWIV_Sound(i, dly1);
			}
		}
		else
		{
			for (int i = ef; i > sf; i -= uf)
			{
				WWIV_Sound(i, dly1);
			}
		}
		WWIV_Delay(dly2);
	}
}

//////////////////////////////////////////////////////////////////////////////
//
//
// Function called when user requests chat w/sysop.
//

void RequestChat()
{
	nl( 2 );
	if ( sysop2() && !GetSession()->thisuser.isRestrictionChat() )
	{
		if (chatcall)
		{
			chatcall = false;
			GetSession()->bout << "Chat call turned off.\r\n";
			GetApplication()->GetLocalIO()->UpdateTopScreen();
		}
		else
		{
			GetSession()->bout << "|#9Enter Reason for chat: \r\n|#0:";
            char szReason[ 81 ];
			inputl( szReason, 70, true );
			if (szReason[0])
			{
				if ( !play_sdf( CHAT_NOEXT, false ) )
				{
					chatsound(100, 800, 10, 10, 25, 5);
				}
				chatcall = true;
                char szChatReason[81];
				sprintf( szChatReason, "%s: %s", "Chat", szReason );
				nl();
				sysoplog( szChatReason );
				for (int nTemp = strlen( szChatReason ); nTemp < 80; nTemp++)
				{
					szChatReason[nTemp] = SPACE;
				}
				szChatReason[80] = '\0';
                GetApplication()->GetLocalIO()->SetChatReason( szChatReason );
				GetApplication()->GetLocalIO()->UpdateTopScreen();
				GetSession()->bout << "Chat call turned ON.\r\n";
				nl();
			}
		}
	}
	else
	{
		GetSession()->bout << "|#6" << syscfg.sysopname <<
					  " is not available.\r\n\n|#5Try sending feedback instead.\r\n";
		strcpy(irt, "|#1Tried Chatting");
		irt_name[0] = '\0';
		imail( 1, 0 );
	}
}


//////////////////////////////////////////////////////////////////////////////
//
//
// Allows selection of a name to "chat as". Returns selected string in *s.
//

void select_chat_name(char *pszSysopName)
{
	GetApplication()->GetLocalIO()->pr_Wait( 1 );
	GetApplication()->GetLocalIO()->savescreen(&screensave);
	strcpy(pszSysopName, syscfg.sysopname);
	curatr = GetSession()->GetChatNameSelectionColor();
	GetApplication()->GetLocalIO()->MakeLocalWindow(20, 5, 43, 3);
	GetApplication()->GetLocalIO()->LocalXYPuts( 22, 6, "Chat As: " );
	curatr = GetSession()->GetEditLineColor();
	GetApplication()->GetLocalIO()->LocalXYPuts( 31, 6, charstr( 30, SPACE ) );

    int rc;
	GetApplication()->GetLocalIO()->LocalGotoXY( 31, 6 );
    GetApplication()->GetLocalIO()->LocalEditLine( pszSysopName, 30, ALL, &rc, pszSysopName );
	if (rc != ABORTED)
	{
		StringTrimEnd( pszSysopName );
		int nUserNumber = atoi( pszSysopName );
		if ( nUserNumber > 0 && nUserNumber <= syscfg.maxusers )
		{
        	WUser tu;
            GetApplication()->GetUserManager()->ReadUser( &tu, nUserNumber );
            strcpy( pszSysopName, tu.GetUserNameAndNumber( nUserNumber ) );
		}
		else
		{
			if (!pszSysopName[0])
            {
				strcpy( pszSysopName, syscfg.sysopname );
            }
		}
	}
	else
	{
		strcpy( pszSysopName, "" );
	}
	GetApplication()->GetLocalIO()->restorescreen(&screensave);
	GetApplication()->GetLocalIO()->pr_Wait( 0 );
}


// Allows two-way chatting until sysop aborts/exits chat. or the end of line is hit,
// then chat1 is back in control.
void two_way_chat(char *pszRollover, int maxlen, bool crend, char *pszSysopName)
{
  char s2[100], temp1[100];
  int i, i1;

  int cm = chatting;
  int begx = GetApplication()->GetLocalIO()->WhereX();
  if (pszRollover[0] != 0)
  {
    if (charbufferpointer)
	{
      char szTempBuffer[255];
      strcpy(szTempBuffer, pszRollover);
      strcat(szTempBuffer, &charbuffer[charbufferpointer]);
      strcpy(&charbuffer[1], szTempBuffer);
      charbufferpointer = 1;
    }
	else
	{
      strcpy(&charbuffer[1], pszRollover);
      charbufferpointer = 1;
    }
    pszRollover[0] = 0;
  }
  bool done = false;
  int side = 0;
  unsigned char ch = 0;
  do
  {
    ch = getkey();
    if ( GetSession()->IsLastKeyLocal() )
	{
      if (GetApplication()->GetLocalIO()->WhereY() == 11)
	  {
        GetSession()->bout << "\x1b[12;1H";
        for (int screencount = 0; screencount < GetSession()->thisuser.GetScreenChars(); screencount++)
		{
          s2[screencount] = '\xCD';
		}
        sprintf( temp1, "|B1|#2 %s chatting with %s |B0|#1", pszSysopName, GetSession()->thisuser.GetUserNameAndNumber( GetSession()->usernum ) );
        int cnt = (((GetSession()->thisuser.GetScreenChars() - strlen(stripcolors(temp1))) / 2));
        if (cnt)
		{
          strncpy(&s2[cnt - 1], temp1, (strlen(temp1)));
		}
        else
		{
          strcpy(s2, charstr(205, GetSession()->thisuser.GetScreenChars() - 1));
		}
        s2[GetSession()->thisuser.GetScreenChars()] = '\0';
        GetSession()->bout << s2;
        s2[0] = '\0';
        temp1[0] = '\0';
        for (int cntr = 1; cntr < 12; cntr++)
		{
          sprintf(s2, "\x1b[%d;%dH", cntr, 1);
          GetSession()->bout << s2;
          if ((cntr >= 0) && (cntr < 5))
		  {
            ansic( 1 );
            GetSession()->bout << side0[cntr + 6];
          }
          GetSession()->bout << "\x1b[K";
          s2[0] = 0;
        }
        sprintf(s2, "\x1b[%d;%dH", 5, 1);
        GetSession()->bout << s2;
        s2[0] = 0;
      }
      else if (GetApplication()->GetLocalIO()->WhereY() > 11)
	  {
        wwiv_x2 = (GetApplication()->GetLocalIO()->WhereX() + 1);
        wwiv_y2 = (GetApplication()->GetLocalIO()->WhereY() + 1);
        sprintf(s2, "\x1b[%d;%dH", wwiv_y1, wwiv_x1);
        GetSession()->bout << s2;
        s2[0] = 0;
      }
      side = 0;
      ansic( 1 );
    }
	else
	{
      if (GetApplication()->GetLocalIO()->WhereY() >= 23)
	  {
        for (int cntr = 13; cntr < 25; cntr++)
		{
          sprintf(s2, "\x1b[%d;%dH", cntr, 1);
          GetSession()->bout << s2;
          if ((cntr >= 13) && (cntr < 17))
		  {
            ansic( 5 );
            GetSession()->bout << side1[cntr - 7];
          }
          GetSession()->bout << "\x1b[K";
          s2[0] = '\0';
        }
        sprintf(s2, "\x1b[%d;%dH", 17, 1);
        GetSession()->bout << s2;
        s2[0] = '\0';
      }
	  else if ( GetApplication()->GetLocalIO()->WhereY() < 12 && side == 0 )
	  {
        wwiv_x1 = (GetApplication()->GetLocalIO()->WhereX() + 1);
        wwiv_y1 = (GetApplication()->GetLocalIO()->WhereY() + 1);
        sprintf(s2, "\x1b[%d;%dH", wwiv_y2, wwiv_x2);
        GetSession()->bout << s2;
        s2[0] = 0;
      }
      side = 1;
      ansic( 5 );
    }
    if ( cm )
	{
      if ( chatting == 0 )
	  {
        ch = RETURN;
	  }
	}
    if ( ch >= SPACE )
	{
      if ( side == 0 )
	  {
        if ( GetApplication()->GetLocalIO()->WhereX() < (GetSession()->thisuser.GetScreenChars() - 1 ) && cp0 < maxlen )
		{
          if ( GetApplication()->GetLocalIO()->WhereY() < 11 )
		  {
            side0[GetApplication()->GetLocalIO()->WhereY()][cp0++] = ch;
            bputch( ch );
          }
		  else
		  {
            side0[GetApplication()->GetLocalIO()->WhereY()][cp0++] = ch;
            side0[GetApplication()->GetLocalIO()->WhereY()][cp0] = 0;
            for (int cntr = 0; cntr < 12; cntr++)
			{
              sprintf(s2, "\x1b[%d;%dH", cntr, 1);
              GetSession()->bout << s2;
              if ((cntr >= 0) && (cntr < 6))
			  {
                ansic( 1 );
                GetSession()->bout << side0[cntr + 6];
                wwiv_y1 = GetApplication()->GetLocalIO()->WhereY() + 1;
                wwiv_x1 = GetApplication()->GetLocalIO()->WhereX() + 1;
              }
              GetSession()->bout << "\x1b[K";
              s2[0] = 0;
            }
            sprintf(s2, "\x1b[%d;%dH", wwiv_y1, wwiv_x1);
            GetSession()->bout << s2;
            s2[0] = 0;
          }
          if (GetApplication()->GetLocalIO()->WhereX() == (GetSession()->thisuser.GetScreenChars() - 1))
		  {
            done = true;
		  }
        }
		else
		{
          if (GetApplication()->GetLocalIO()->WhereX() >= (GetSession()->thisuser.GetScreenChars() - 1))
		  {
            done = true;
		  }
        }
      }
	  else
	  {
        if ((GetApplication()->GetLocalIO()->WhereX() < (GetSession()->thisuser.GetScreenChars() - 1)) && (cp1 < maxlen))
		{
          if (GetApplication()->GetLocalIO()->WhereY() < 23)
		  {
            side1[GetApplication()->GetLocalIO()->WhereY() - 13][cp1++] = ch;
            bputch(ch);
          }
		  else
		  {
            side1[GetApplication()->GetLocalIO()->WhereY() - 13][cp1++] = ch;
            side1[GetApplication()->GetLocalIO()->WhereY() - 13][cp1] = 0;
            for (int cntr = 13; cntr < 25; cntr++)
			{
              sprintf(s2, "\x1b[%d;%dH", cntr, 1);
              GetSession()->bout << s2;
              if ( cntr >= 13 && cntr < 18 )
			  {
                ansic( 5 );
                GetSession()->bout << side1[cntr - 7];
                wwiv_y2 = GetApplication()->GetLocalIO()->WhereY() + 1;
                wwiv_x2 = GetApplication()->GetLocalIO()->WhereX() + 1;
              }
              GetSession()->bout << "\x1b[K";
              s2[0] = '\0';
            }
            sprintf(s2, "\x1b[%d;%dH", wwiv_y2, wwiv_x2);
            GetSession()->bout << s2;
            s2[0] = '\0';
          }
          if (GetApplication()->GetLocalIO()->WhereX() == (GetSession()->thisuser.GetScreenChars() - 1))
		  {
            done = true;
		  }
        }
		else
		{
          if (GetApplication()->GetLocalIO()->WhereX() >= (GetSession()->thisuser.GetScreenChars() - 1))
		  {
            done = true;
		  }
        }
      }
    }
    else switch ( ch )
	{
        case 7:
            {
                if ( chatting && outcom )
                {
                    rputch( 7 );
                }
            }
          break;
        case RETURN:                            /* C/R */
            if (side == 0)
            {
                side0[GetApplication()->GetLocalIO()->WhereY()][cp0] = 0;
            }
            else
            {
                side1[GetApplication()->GetLocalIO()->WhereY() - 13][cp1] = 0;
            }
            done = true;
            break;
        case BACKSPACE:                             /* Backspace */
            {
                if (side == 0)
                {
                    if (cp0)
                    {
                        if (side0[GetApplication()->GetLocalIO()->WhereY()][cp0 - 2] == 3)
                        {
                            cp0 -= 2;
                            ansic( 0 );
                        }
                        else if (side0[GetApplication()->GetLocalIO()->WhereY()][cp0 - 1] == 8)
                        {
                            cp0--;
                            bputch( SPACE );
                        }
                        else
                        {
                            cp0--;
                            BackSpace();
                        }
                    }
                }
                else if (cp1)
                {
                    if (side1[GetApplication()->GetLocalIO()->WhereY() - 13][cp1 - 2] == CC)
                    {
                        cp1 -= 2;
                        ansic( 0 );
                    }
                    else  if (side1[GetApplication()->GetLocalIO()->WhereY() - 13][cp1 - 1] == BACKSPACE)
                    {
                        cp1--;
                        bputch(SPACE);
                    }
                    else
                    {
                        cp1--;
                        BackSpace();
                    }
                }
            }
          break;
        case CX:                            /* Ctrl-X */
          while (GetApplication()->GetLocalIO()->WhereX() > begx) {
            BackSpace();
            if (side == 0)
              cp0 = 0;
            else
              cp1 = 0;
          }
          ansic( 0 );
          break;
        case CW:                            /* Ctrl-W */
          if (side == 0)
		  {
            if (cp0)
			{
              do
			  {
                if (side0[GetApplication()->GetLocalIO()->WhereY()][cp0 - 2] == CC)
				{
                  cp0 -= 2;
                  ansic( 0 );
                }
				else if (side0[GetApplication()->GetLocalIO()->WhereY()][cp0 - 1] == BACKSPACE)
				{
                  cp0--;
                  bputch(SPACE);
                }
				else
				{
                  cp0--;
                  BackSpace();
                }
              } while ((cp0) && (side0[GetApplication()->GetLocalIO()->WhereY()][cp0 - 1] != SPACE) &&
                       (side0[GetApplication()->GetLocalIO()->WhereY()][cp0 - 1] != BACKSPACE) &&
                       (side0[GetApplication()->GetLocalIO()->WhereY()][cp0 - 2] != CC));
            }
          }
		  else
		  {
            if (cp1)
			{
              do
			  {
                if (side1[GetApplication()->GetLocalIO()->WhereY() - 13][cp1 - 2] == CC)
				{
                  cp1 -= 2;
                  ansic( 0 );
                }
				else if (side1[GetApplication()->GetLocalIO()->WhereY() - 13][cp1 - 1] == BACKSPACE)
				{
                  cp1--;
                  bputch(SPACE);
                }
				else
				{
                  cp1--;
                  BackSpace();
                }
              } while ((cp1) && (side1[GetApplication()->GetLocalIO()->WhereY() - 13][cp1 - 1] != SPACE) &&
                       (side1[GetApplication()->GetLocalIO()->WhereY() - 13][cp1 - 1] != BACKSPACE) &&
                       (side1[GetApplication()->GetLocalIO()->WhereY() - 13][cp1 - 2]));
            }
          }
          break;
        case CN:                            /* Ctrl-N */
          if (side == 0)
		  {
            if ((GetApplication()->GetLocalIO()->WhereX()) && (cp0 < maxlen))
			{
              bputch(BACKSPACE);
              side0[GetApplication()->GetLocalIO()->WhereY()][cp0++] = BACKSPACE;
            }
          }
		  else  if ((GetApplication()->GetLocalIO()->WhereX()) && (cp1 < maxlen))
		  {
            bputch(BACKSPACE);
            side1[GetApplication()->GetLocalIO()->WhereY() - 13][cp1++] = BACKSPACE;
            }
          break;
        case CP:                            /* Ctrl-P */
          if (side == 0)
		  {
            if (cp0 < maxlen - 1)
			{
              ch = getkey();
              if ((ch >= SPACE) && (ch <= 126))
			  {
                side0[GetApplication()->GetLocalIO()->WhereY()][cp0++] = CC;
                side0[GetApplication()->GetLocalIO()->WhereY()][cp0++] = ch;
                ansic(ch - 48);
              }
            }
          }
		  else
		  {
            if (cp1 < maxlen - 1)
			{
              ch = getkey();
              if ((ch >= SPACE) && (ch <= 126))
			  {
                side1[GetApplication()->GetLocalIO()->WhereY() - 13][cp1++] = CC;
                side1[GetApplication()->GetLocalIO()->WhereY() - 13][cp1++] = ch;
                ansic(ch - 48);
              }
            }
          }
          break;
        case TAB:                             /* Tab */
          if (side == 0)
		  {
            i = 5 - (cp0 % 5);
            if (((cp0 + i) < maxlen) && ((GetApplication()->GetLocalIO()->WhereX() + i) < GetSession()->thisuser.GetScreenChars()))
			{
              i = 5 - ((GetApplication()->GetLocalIO()->WhereX() + 1) % 5);
              for (i1 = 0; i1 < i; i1++)
			  {
                side0[GetApplication()->GetLocalIO()->WhereY()][cp0++] = SPACE;
                bputch( SPACE );
              }
            }
          }
		  else
		  {
            i = 5 - (cp1 % 5);
            if (((cp1 + i) < maxlen) && ((GetApplication()->GetLocalIO()->WhereX() + i) < GetSession()->thisuser.GetScreenChars()))
			{
              i = 5 - ((GetApplication()->GetLocalIO()->WhereX() + 1) % 5);
              for (i1 = 0; i1 < i; i1++)
			  {
                side1[GetApplication()->GetLocalIO()->WhereY() - 13][cp1++] = SPACE;
                bputch( SPACE );
              }
            }
          }
          break;
      }
  } while ( !done && !hangup );

  if ( ch != RETURN )
  {
    if (side == 0)
	{
      i = cp0 - 1;
      while ((i > 0) && (side0[GetApplication()->GetLocalIO()->WhereY()][i] != SPACE) &&
             (side0[GetApplication()->GetLocalIO()->WhereY()][i] != BACKSPACE) ||
			 (side0[GetApplication()->GetLocalIO()->WhereY()][i - 1] == CC))
        i--;
      if ((i > (GetApplication()->GetLocalIO()->WhereX() / 2)) && (i != (cp0 - 1)))
	  {
        i1 = cp0 - i - 1;
        for (i = 0; i < i1; i++)
		{
          bputch( BACKSPACE );
		}
        for (i = 0; i < i1; i++)
		{
          bputch( SPACE );
		}
        for (i = 0; i < i1; i++)
		{
          pszRollover[i] = side0[GetApplication()->GetLocalIO()->WhereY()][cp0 - i1 + i];
		}
        pszRollover[i1] = '\0';
        cp0 -= i1;
      }
      side0[GetApplication()->GetLocalIO()->WhereY()][cp0] = '\0';
    }
	else
	{
      i = cp1 - 1;
      while ((i > 0) && (side1[GetApplication()->GetLocalIO()->WhereY() - 13][i] != SPACE) &&
       (side1[GetApplication()->GetLocalIO()->WhereY() - 13][i] != BACKSPACE) ||
	   (side1[GetApplication()->GetLocalIO()->WhereY() - 13][i - 1] == CC))
	  {
        i--;
	  }
      if ((i > (GetApplication()->GetLocalIO()->WhereX() / 2)) && (i != (cp1 - 1)))
	  {
        i1 = cp1 - i - 1;
        for (i = 0; i < i1; i++)
		{
          bputch( BACKSPACE );
		}
        for (i = 0; i < i1; i++)
		{
          bputch( SPACE );
		}
        for (i = 0; i < i1; i++)
		{
          pszRollover[i] = side1[GetApplication()->GetLocalIO()->WhereY() - 13][cp1 - i1 + i];
		}
        pszRollover[i1] = '\0';
        cp1 -= i1;
      }
      side1[GetApplication()->GetLocalIO()->WhereY() - 13][cp1] = '\0';
    }
  }
  if ( crend && GetApplication()->GetLocalIO()->WhereY() != 11 && GetApplication()->GetLocalIO()->WhereY() < 23 )
  {
    nl();
  }
  if (side == 0)
  {
    cp0 = 0;
  }
  else
  {
    cp1 = 0;
  }
}

/****************************************************************************/

/*
 * High-level chat function, calls two_way_chat() if appropriate, else
 * uses normal TTY chat.
 */

void chat1(char *pszChatLine, bool two_way)
{
	char cl[81], xl[81], s[255], s1[255], atr[81], s2[81], cc, szSysopName[81];

	select_chat_name( szSysopName );
	if ( szSysopName[0] == 0 )
	{
		return;
	}

	int otag = GetSession()->tagging;
	GetSession()->tagging = 0;

	chatcall = false;
	if (two_way)
	{
		write_inst(INST_LOC_CHAT2, 0, INST_FLAGS_NONE);
		chatting = 2;
	}
	else
	{
		write_inst(INST_LOC_CHAT, 0, INST_FLAGS_NONE);
		chatting = 1;
	}
	double tc = timer();
    WFile chatFile( syscfg.gfilesdir, "chat.txt" );

	GetApplication()->GetLocalIO()->SaveCurrentLine(cl, atr, xl, &cc);
	s1[0] = 0;

	bool oe = echo;
	echo = true;
	nl( 2 );
	int nSaveTopData = GetSession()->topdata;
	if ( !okansi() )
	{
		two_way = false;
	}
	if (modem_speed == 300)
	{
		two_way = false;
	}

	if (two_way)
	{
		GetApplication()->GetLocalIO()->LocalCls();
		cp0 = 0;
		cp1 = 0;
		if (defscreenbottom == 24)
		{
			GetSession()->topdata = WLocalIO::topdataNone;
			GetApplication()->GetLocalIO()->UpdateTopScreen();
		}
		GetSession()->bout << "\x1b[2J";
		wwiv_x2 = 1;
		wwiv_y2 = 13;
		GetSession()->bout << "\x1b[1;1H";
		wwiv_x1 = GetApplication()->GetLocalIO()->WhereX();
		wwiv_y1 = GetApplication()->GetLocalIO()->WhereY();
		GetSession()->bout << "\x1b[12;1H";
        ansic( 7 );
		for (int screencount = 0; screencount < GetSession()->thisuser.GetScreenChars(); screencount++)
		{
			bputch( static_cast< unsigned char >( 205 ), true );
		}
        FlushOutComChBuffer();
		sprintf( s, " %s chatting with %s ", szSysopName, GetSession()->thisuser.GetUserNameAndNumber( GetSession()->usernum ) );
		int cnt = ((GetSession()->thisuser.GetScreenChars() - strlen(stripcolors(s))) / 2);
		cnt = std::max<int>( cnt, 0 );
		sprintf(s1, "\x1b[12;%dH", cnt);
		GetSession()->bout << s1;
		ansic( 4 );
		GetSession()->bout << s;
		GetSession()->bout << "\x1b[1;1H";
		s[0] = 0;
		s1[0] = 0;
		s2[0] = 0;
	}
    GetSession()->bout << "|#7" << szSysopName << "'s here...";
	nl( 2 );
	strcpy( s1, pszChatLine );

	if (two_way)
	{
		side0 = new char[MAXLINES_SIDE][MAXLEN];
		side1 = new char[MAXLINES_SIDE][MAXLEN];
		if (!side0 || !side1)
		{
			two_way = false;
		}
	}
	do
	{
		if (two_way)
		{
			two_way_chat(s1, MAXLEN, true, szSysopName);
		}
		else
		{
			inli(s, s1, MAXLEN, true, false);
		}
		if ( chat_file && !two_way )
		{
            if ( !chatFile.IsOpen() )
			{
				GetApplication()->GetLocalIO()->LocalFastPuts("-] Chat file opened.\r\n");
                if ( chatFile.Open( WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile,
                                    WFile::shareUnknown, WFile::permReadWrite ) )
                {
                    chatFile.Seek( 0L, WFile::seekEnd );
				    sprintf(s2, "\r\n\r\nChat file opened %s %s\r\n", fulldate(), times());
				    chatFile.Write( s2, strlen(s2) );
				    strcpy(s2, "----------------------------------\r\n\r\n");
                    chatFile.Write( s2, strlen(s2) );
                }
			}
			strcat(s, "\r\n");
            chatFile.Write( s2, strlen(s2) );
		}
        else if (chatFile.IsOpen())
		{
            chatFile.Close();
		    GetApplication()->GetLocalIO()->LocalFastPuts("-] Chat file closed.\r\n");
		}
		if (hangup)
		{
			chatting = 0;
		}
	} while (chatting);

	if (chat_file)
	{
		chat_file = false;
	}
	if (side0)
	{
		delete[] side0;
		side0 = NULL;
	}
	if (side1)
	{
		delete[] side1;
		side1 = NULL;
	}
	ansic( 0 );

	if (two_way)
	{
		GetSession()->bout << "\x1b[2J";
	}

	nl();
    GetSession()->bout << "|#7Chat mode over...\r\n\n";
	chatting = 0;
	tc = timer() - tc;
	if (tc < 0)
	{
		tc += SECONDS_PER_DAY_FLOAT;
	}
	extratimecall += tc;
	GetSession()->topdata = nSaveTopData;
	if ( GetSession()->IsUserOnline() )
	{
		GetApplication()->GetLocalIO()->UpdateTopScreen();
	}
	echo = oe;
	RestoreCurrentLine(cl, atr, xl, &cc);

	GetSession()->tagging = otag;
	if ( okansi() )
	{
		GetSession()->bout << "\x1b[K";
	}
}

