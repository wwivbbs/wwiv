/****************************************************************************/
/*                                                                          */
/*                             WWIV Version 5.0x                            */
/*            Copyright (C) 1998-2003 by WWIV Software Services             */
/*                                                                          */
/*      Distribution or publication of this source code, it's individual    */
/*       components, or a compiled version thereof, whether modified or     */
/*        unmodified, without PRIOR, WRITTEN APPROVAL of WWIV Software      */
/*        Services is expressly prohibited.  Distribution of compiled       */
/*            versions of WWIV is restricted to copies compiled by          */
/*           WWIV Software Services.  Violators will be procecuted!         */
/*                                                                          */
/****************************************************************************/
#include "../../wwiv.h"



/*
 * Sets screen attribute at screen pos x,y to attribute contained in a.
 */

void WLocalIO::set_attr_xy(int x, int y, int a)
{
	COORD loc;
	DWORD cb;
	
	loc.X = x;
	loc.Y = y;
	
	m_nWfcStatus = 0;
	
	WriteConsoleOutputAttribute(hConOut,(LPWORD)&a,1,loc,&cb);
}




WLocalIO::WLocalIO() 
{ 
    VioGetMode(&vmi, 0);
    
    if (vmi.fbType & 0x02) 
    {
        printf("\n\nYou must be in text mode to run WWIV.\n\n");
        abort();
    }
    if (vmi.col != ((USHORT) 80)) 
    {
        printf("%u\n\nYou must be in 80 column mode to run WWIV.\n\n", (unsigned int) vmi.col);
        abort();
    }

	CellStr[0] = 32;    // Space
	CellStr[1] = 0;     // Nothing
	CellStr[2] = 0;     // Nothing
}



void WLocalIO::set_global_handle(int i)
{
    char s[81];
    
    if (x_only)
        return;
    
    if (i) 
    {
        if (!global_handle) 
        {
            sprintf(s, "%sGLOBAL.TXT", syscfg.gfilesdir);
            global_handle = sh_open(s, O_RDWR | O_APPEND | O_BINARY | O_CREAT,
                S_IREAD | S_IWRITE);
            global_ptr = 0;
            global_buf = (char *) malloca(GLOBAL_SIZE);
            if ((global_handle < 0) || (!global_buf)) 
            {
                global_handle = 0;
                if (global_buf) 
                {
                    bbsfree(global_buf);
                    global_buf = NULL;
                }
            }
        }
    } 
    else 
    {
        if (global_handle) 
        {
            sh_write(global_handle, global_buf, global_ptr);
            sh_close(global_handle);
            global_handle = 0;
            if (global_buf) 
            {
                bbsfree(global_buf);
                global_buf = NULL;
            }
        }
    }
}


void WLocalIO::global_char(char ch)
{
    
    if (global_buf && global_handle) 
    {
        global_buf[global_ptr++] = ch;
        if (global_ptr == GLOBAL_SIZE) 
        {
            sh_write(global_handle, global_buf, global_ptr);
            global_ptr = 0;
        }
    }
}

void WLocalIO::set_x_only(int tf, char *fn, int ovwr)
{
    static int gh;
    char s[81];
    
    if (x_only) 
    {
        if (!tf) 
        {
            if (global_handle) 
            {
                sh_write(global_handle, global_buf, global_ptr);
                sh_close(global_handle);
                global_handle = 0;
                if (global_buf) 
                {
                    bbsfree(global_buf);
                    global_buf = NULL;
                }
            }
            x_only = 0;
            set_global_handle(gh);
            gh = 0;
            express = expressabort = 0;
        }
    } 
    else 
    {
        if (tf) 
        {
            gh = global_handle;
            set_global_handle(0);
            x_only = 1;
            wx = 0;
            sprintf(s, "%s%s", syscfgovr.tempdir, fn);
            if (ovwr)
            {
                global_handle = sh_open(s, O_RDWR | O_TRUNC | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);
            }
            else
            {
                global_handle = sh_open(s, O_RDWR | O_APPEND | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);
            }
            global_ptr = 0;
            express = 1;
            expressabort = 0;
            global_buf = (char *) malloca(GLOBAL_SIZE);
            if ((global_handle < 0) || (!global_buf)) 
            {
                global_handle = 0;
                if (global_buf) 
                {
                    bbsfree(global_buf);
                    global_buf = NULL;
                }
                set_x_only(0, NULL, 0);
            }
        }
    }
    timelastchar1 = timer1();
}


void WLocalIO::LocalGotoXY(int x, int y)
// This, obviously, moves the cursor to the location specified, offset from
// the protected dispaly at the top of the screen.  Note: this function
// is 0 based, so (0,0) is the upper left hand corner.
{
    if (x < 0)
    {
        x = 0;
    }
    if (x > 79)
    {
        x = 79;
    }
    if (y < 0)
    {
        y = 0;
    }
    y += topline;
    if (y > screenbottom)
    {
        y = screenbottom;
    }
    
    if (x_only) 
    {
        wx = x;
        return;
    }
    VioSetCurPos((USHORT)y, (USHORT)x, 0);
}




int WLocalIO::WhereX()
/* This function returns the current X cursor position, as the number of
* characters from the left hand side of the screen.  An X position of zero
* means the cursor is at the left-most position
*/
{
    
    USHORT CurX, CurY;
    
    if (x_only)
    {
        return(wx);
    }
    
    VioGetCurPos(&CurY, &CurX, 0);
    return CurX;
}



int WLocalIO::WhereY()
/* This function returns the Y cursor position, as the line number from
* the top of the logical window.  The offset due to the protected top
* of the screen display is taken into account.  A WhereY() of zero means
* the cursor is at the top-most position it can be at.
*/
{
    USHORT CurX, CurY;
    
    VioGetCurPos(&CurY, &CurX, 0);
    return CurY - topline;
}



void WLocalIO::LocalLf()
/* This function performs a linefeed to the screen (but not remotely) by
* either moving the cursor down one line, or scrolling the logical screen
* up one line.
*/
{
    USHORT CurX, CurY;
    
    VioGetCurPos(&CurY, &CurX, 0);
    if (CurY >= screenbottom)
    {
        CellStr[1] = (unsigned char) curatr;
        VioScrollUp(topline, 0, screenbottom, 79, 1, CellStr, 0);
    }
    else
    {
        VioSetCurPos(CurY + 1, CurX, 0);
    }
}



void WLocalIO::LocalCr()
/* This short function returns the local cursor to the left-most position
* on the screen.
*/
{
    USHORT CurX, CurY;
    VioGetCurPos(&CurY, &CurX, 0);
    VioSetCurPos(CurY, 0, 0);
}

void WLocalIO::LocalCls()
/* This clears the local logical screen */
{
    // %%TODO: Debugging hack - REMOVE THIS!!
    CellStr[1] = (unsigned char) curatr;
    VioScrollUp(topline, 0, screenbottom, 79, 65535, CellStr, 0);

    LocalGotoXY(0, 0);
    lines_listed = 0;
}



void WLocalIO::LocalBackspace()
/* This function moves the cursor one position to the left, or if the cursor
* is currently at its left-most position, the cursor is moved to the end of
* the previous line, except if it is on the top line, in which case nothing
* happens.
*/
{
    USHORT CurX, CurY;
    
    VioGetCurPos(&CurY, &CurX, 0);
    if (CurX != 0)
    {
        VioSetCurPos(CurY, CurX - 1, 0);
    }
    else if (CurY != topline)
    {
        VioSetCurPos(CurY - 1, 79, 0);
    }
}



void WLocalIO::LocalPutchRaw(unsigned char ch)
/* This function outputs one character to the screen, then updates the
* cursor position accordingly, scolling the screen if necessary.  Not that
* this function performs no commands such as a C/R or L/F.  If a value of
* 8, 7, 13, 10, 12 (backspace, beep, C/R, L/F, TOF), or any other command-
* type characters are passed, the appropriate corresponding "graphics"
* symbol will be output to the screen as a normal character.
*/
{
    USHORT CurX, CurY;
    BYTE CharAttribute = curatr;
    
    VioGetCurPos(&CurY, &CurX, 0);
    (&ch, 1, CurY, CurX, &CharAttribute, 0);
    if (CurX != 79)
    {
        VioSetCurPos(CurY, CurX + 1, 0);
    }
    else if (CurY != screenbottom)
    {
        VioSetCurPos(CurY + 1, 0, 0);
    }
    else
    {
        CellStr[1] = (unsigned char) curatr;
        VioScrollUp(topline, 0, screenbottom, 79, 1, CellStr, 0);
        VioSetCurPos(CurY, 0, 0);
    }
}




void WLocalIO::LocalPutch(unsigned char ch)
/* This function outputs one character to the local screen.  C/R, L/F, TOF,
* BS, and BELL are interpreted as commands instead of characters.
*/
{
    if (x_only) 
    {
        if (ch > 31) 
        {
            wx = (wx + 1) % 80;
        } 
        else if ((ch == 13) || (ch == 12)) 
        {
            wx = 0;
        } 
        else if (ch == 8) 
        {
            if (wx)
            {
                wx--;
            }
        }
        return;
    }

    if (ch > 31)
    {
        LocalPutchRaw(ch);
    }
    else if (ch == 13)
    {
        LocalCr();
    }
    else if (ch == 10)
    {
        LocalLf();
    }
    else if (ch == 12) 
    {
        lines_listed = 0;
        LocalCls();
    } 
    else if (ch == 8)
    {
        LocalBackspace();
    }
    else if (ch == 7)
    {
        if (outcom == 0) 
        {
			WWIV_Sound(500, 4);
        }
    }
}


void WLocalIO::LocalPuts(const char *s)
/* This (obviously) outputs a string TO THE SCREEN ONLY */
{
    while (*s) 
    {
        LocalPutch(*s++);
    }
}

void WLocalIO::LocalFastPuts(const char *s)
/* This RAPIDLY outputs ONE LINE to the screen only*/
{
    USHORT CurX, CurY;
    BYTE CharAttribute = curatr;
    
    VioGetCurPos(&CurY, &CurX, 0);
    VioWrtCharStrAtt(s, strlen(s), CurY, CurX, &CharAttribute, 0);
}



void WLocalIO::pr_Wait(int i1)
{
    int i, i2, i3;
    char *ss = "-=[WAIT]=-";
    i2 = i3 = strlen(ss);
    for (i = 0; i < i3; i++)
    {
        if ((ss[i] == 3) && (i2 > 1))
        {
            i2 -= 2;
        }
    }
        
    if (i1) 
    {
        if (okansi()) 
        {
            i = curatr;
            setc((sess->thisuser.sysstatus & sysstatus_color) ? sess->thisuser.colors[3] :
            sess->thisuser.bwcolors[3]);
            npr(ss);
            npr("\x1b[%dD", i2);
            setc(i);
        } 
        else 
        {
            npr(ss);
        }
    } 
    else 
    {
        if (okansi()) 
        {
            for (i = 0; i < i2; i++)
            {
                outchr(' ');
            }
            npr("\x1b[%dD", i2);
        } 
        else 
        {
            for (i = 0; i < i2; i++)
            {
                backspace();
            }
        }
    }
}



void WLocalIO::set_protect(int l)
/* set_protect sets the number of lines protected at the top of the screen. */
{
    int sa;
    
    if (l != topline) 
    {
        if (l > topline) 
        {
            if ((WhereY() + topline - l) < 0) 
            {
                CellStr[1] = (unsigned char) curatr;
                VioScrollDn(topline, 0, screenbottom + 1, 79, l - topline, CellStr, 0);
                LocalGotoXY(WhereX(), WhereY() + l - topline);
            } 
            else 
            {
                oldy += (topline - l);
            }
        } 
        else 
        {
            sa = curatr;
            curatr = ((sess->thisuser.sysstatus & sysstatus_color) ? sess->thisuser.colors[0] :
            sess->thisuser.bwcolors[0]);
            CellStr[1] = (unsigned char) curatr;
            VioScrollUp(l, 0, topline - 1, 79, 65535, CellStr, 0);
            {
                COORD dest;  
                SMALL_RECT MoveRect, ClipRect;
                CHAR_INFO fill;
                
                MoveRect.Top    = topline;
                MoveRect.Bottom = screenbottom;
                ClipRect.Top    = 0;
                ClipRect.Bottom = screenbottom;
                ClipRect.Left  = MoveRect.Left   = 0;
                ClipRect.Right = MoveRect.Right  = 79;
                
                fill.Attributes = curatr;
                fill.Char.AsciiChar = ' ';
                
                dest.X = 0;
                dest.Y = l;
                
                ScrollConsoleScreenBuffer(hConOut,&MoveRect,&ClipRect,dest,&fill);
                Curpos.Y -= topline - l;
                SetConsoleCursorPosition(hConOut,Curpos);
            }
            curatr = sa;
            oldy += (topline - l);
        }
    }
    topline = l;
    if (using_modem)
        screenlinest = sess->thisuser.screenlines;
    else
        screenlinest = defscreenbottom + 1 - topline;
}


void WLocalIO::savescreen(screentype * s)
{
    int screenlen = 160 * (screenbottom + 1);
    if (!s->scrn1)
    {
        s->scrn1 = (char *) malloca(screenlen);
    }
    if (s->scrn1)
    {
        USHORT ScreenBufferSize = screenlen;
        VioReadCellStr(s->scrn1, &ScreenBufferSize, 0, 0, 0);
    }

    s->x1 = WhereX();
    s->y1 = WhereY();
    s->topline1 = topline;
    s->curatr1 = curatr;
}


void WLocalIO::restorescreen(screentype * s)
/* restorescreen restores a screen previously saved with savescreen */
{
    if (s->scrn1) 
    {
        VioWrtCellStr(s->scrn1, screenlen, 0, 0, 0);
        free(s->scrn1);
        s->scrn1 = NULL;
    }
    topline = s->topline1;
    curatr = s->curatr1;
    LocalGotoXY(s->x1, s->y1);
}


void WLocalIO::temp_cmd(char *s)
{
    int i;
    
    pr_Wait(1);
    savescreen(&screensave);
    i = topline;
    topline = 0;
    curatr = 0x07;
    LocalCls();
    extern_prog(s, EFLAG_TOPSCREEN);
    restorescreen(&screensave);
    topline = i;
    pr_Wait(0);
}


char xlate[] = 
{
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', 0, 0, 0, 0,
        'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', 0, 0, 0, 0, 0,
        'Z', 'X', 'C', 'V', 'B', 'N', 'M',
};

char WLocalIO::scan_to_char(unsigned char ch)
{
    if ((ch >= 16) && (ch <= 50))
    {
        return (xlate[ch - 16]);
    }
    else
    {
        return (0);
    }
}

void WLocalIO::alt_key(unsigned char ch)
{
    char ch1;
    char *ss, *ss1, s[81], cmd[128];
    int f, l;
    
    cmd[0] = 0;
    ch1 = scan_to_char(ch);
    if (ch1) 
    {
        sprintf(s, "%sMACROS.TXT", syscfg.datadir);
        f = sh_open1(s, O_RDONLY | O_BINARY);
        if (f > 0) 
        {
            l = filelength(f);
            ss = (char *) malloca(l + 10);
            if (ss) 
            {
                sh_read(f, ss, l);
                sh_close(f);
                
                ss[l] = 0;
                ss1 = strtok(ss, "\r\n");
                while (ss1) 
                {
                    if (upcase(*ss1) == ch1) 
                    {
                        strtok(ss1, " \t");
                        ss1 = strtok(NULL, "\r\n");
                        if (ss1 && (strlen(ss1) < 128))
                            strcpy(cmd, ss1);
                        ss1 = NULL;
                    } 
                    else
                    {
                        ss1 = strtok(NULL, "\r\n");
                    }
                }
                bbsfree(ss);
            } 
            else
            {
                sh_close(f);
            }

            if (cmd[0]) 
            {
                if (cmd[0] == '@') 
                {
                    if (okmacro && okskey && (!charbufferpointer) && (cmd[1])) 
                    {
                        for (l = strlen(cmd) - 1; l >= 0; l--) 
                        {
                            if (cmd[l] == '{')
                            {
                                cmd[l] = '\r';
                            }
                            strcpy(charbuffer, cmd);
                            charbufferpointer = 1;
                        }
                    }
                } 
                else if (m_nWfcStatus == 1)
                {
                    holdphone(true);
                    temp_cmd(cmd);
                    cleanup_net();
                    holdphone(false);
                }
                
            }
        }
    }
}

void WLocalIO::skey(char ch)
/* skey handles all f-keys and the like hit FROM THE KEYBOARD ONLY */
{
    int i, i1;
    
    if (((syscfg.sysconfig & sysconfig_no_local) == 0) && (!in_extern)) 
    {
        if (okskey) 
        {
            if ((ch >= 104) && (ch <= 113)) 
            {
                set_autoval(ch - 104);
            } 
            else 
            {
                switch ((unsigned char) ch) 
                {
                case 59:                          /* F1 */
                    val_cur_user();
                    break;
                case 84:                          /* Shift-F1 */
                    set_global_handle(!global_handle);
                    UpdateTopScreen();
                    break;
                case 94:                          /* Ctrl-F1 */
                    if (bbsshutdown)
                        bbsshutdown = 0;
                    else
                        shut_down(1);
                    break;
                case 60:                          /* F2 */
                    topdata += 1;
                    if (topdata == 3)
                    {
                        topdata = 0;
                    }
                    UpdateTopScreen();
                    break;
                case 61:                          /* F3 */
                    if (using_modem) 
                    {
                        incom = (!incom);
                        dump();
                        tleft(false);
                    }
                    break;
                case 62:                          /* F4 */
                    chatcall = 0;
                    UpdateTopScreen();
                    break;
                case 63:                          /* F5 */
                    hangup = 1;
                    app->comm->dtr(false);
                    break;
                case 88:                          /* Shift-F5 */
                    i1 = (rand() % 20) + 10;
                    for (i = 0; i < i1; i++)
                    {
                        outchr(rand() % 256);
                    }
                    hangup = 1;
                    app->comm->dtr(false);
                    break;
                case 98:                          /* Ctrl-F5 */
                    nl();
                    pl("Call back later when you are there.");
                    nl();
                    hangup = 1;
                    app->comm->dtr(false);
                    break;
                case 64:                          /* F6 */
                    ToggleSysopAlert();
                    tleft(false);
                    break;
                case 65:                          /* F7 */
                    sess->thisuser.extratime -= 5.0 * 60.0;
                    tleft(false);
                    break;
                case 66:                          /* F8 */
                    sess->thisuser.extratime += 5.0 * 60.0;
                    tleft(false);
                    break;
                case 67:                          /* F9 */
                    if (sess->thisuser.sl != 255) 
                    {
                        if (actsl != 255)
                        {
                            actsl = 255;
                        }
                        else
                        {
                            reset_act_sl();
                        }
                        changedsl();
                        tleft(false);
                    }
                    break;
                case 68:                          /* F10 */
                    if (chatting == 0) 
                    {
                        if (syscfg.sysconfig & sysconfig_2_way)
                        {
                            chat1("", 1);
                        }
                        else
                        {
                            chat1("", 0);
                        }
                    } 
                    else
                    {
                        chatting = 0;
                    }
                    break;
/* No need on a multi-tasking OS
                case 93:                          // Shift-F10 
                    temp_cmd(getenv("COMSPEC"));
                    break;
*/
                case 103:                         /* Ctrl-F10 */
                    if (chatting == 0)
                    {
                        chat1("", 0);
                    }
                    else
                    {
                        chatting = 0;
                    }
                    break;
                case 71:                          /* HOME */
                    if (chatting == 1) 
                    {
                        if (chat_file)
                        {
                            chat_file = 0;
                        }
                        else
                        {
                            chat_file = 1;
                        }
                    }
                    break;
                default:
                    alt_key((unsigned char) ch);
                    break;
                }
            }
        } 
        else 
        {
            alt_key((unsigned char) ch);
        }
    }
}


static const char * szTopScrItems[] = 
{
    "Comm Disabled",
    "Temp Sysop",
    "Capture",
    "Alert",
    "อออออออ",
    "Available",
    "อออออออออออ",
    "%s chatting with %s"
};

void WLocalIO::tleft(bool bCheckForTimeOut)
{
    int cx, cy, ctl, cc, ln, i;
    double nsln;
    char tl[30];
    static char sbuf[200];
    static char *ss[8];
    
    if (!sbuf[0]) 
    {
        ss[0] = sbuf;
        for (i = 0; i < 7; i++) 
        {
            strcpy(ss[i], szTopScrItems[i]);
            ss[i + 1] = ss[i] + strlen(ss[i]) + 1;
        }
    }
    cx = WhereX();
    cy = WhereY();
    ctl = topline;
    cc = curatr;
    curatr = sysinfo.topscreen_color;
    topline = 0;
    nsln = nsl();
    if (chatcall && (topdata == 2))
    {
        ln = 5;
    }
    else
    {
        ln = 4;
    }
    
    
    if (topdata) 
    {
        if ((using_modem) && (!incom)) 
        {
            LocalGotoXY(1, ln);
            LocalFastPuts(ss[0]);
            for (i = 19; i < (int) strlen(curspeed); i++)
                LocalPutch('อ');
        } 
        else 
        {
            LocalGotoXY(1, ln);
            LocalFastPuts(curspeed);
            for (i = WhereX(); i < 23; i++)
            {
                LocalPutch('อ');
            }
        }
        
        if ((sess->thisuser.sl != 255) && (actsl == 255)) 
        {
            LocalGotoXY(23, ln);
            LocalFastPuts(ss[1]);
        }
        if (global_handle) 
        {
            LocalGotoXY(40, ln);
            LocalFastPuts(ss[2]);
        }
        LocalGotoXY(54, ln);
        if (GetSysopAlert())
        {
            LocalFastPuts(ss[3]);
        } 
        else 
        {
            LocalFastPuts(ss[4]);
        }
        
        LocalGotoXY(64, ln);
        if (sysop1()) 
        {
            LocalFastPuts(ss[5]);
        } 
        else 
        {
            LocalFastPuts(ss[6]);
        }
    }
    switch (topdata) 
    {
    case 1:
        if (useron) 
        {
            LocalGotoXY(18, 3);
            sprintf(tl, "T-%6.2f", nsln / 60.0);
            LocalFastPuts(tl);
        }
        break;
    case 2:
        LocalGotoXY(64, 3);
        if (useron)
        {
            sprintf(tl, "T-%6.2f", nsln / 60.0);
        }
        else
        {
            strcpy(tl, (char *) sess->thisuser.pw);
        }
        LocalFastPuts(tl);
        break;
    }
    topline = ctl;
    curatr = cc;
    LocalGotoXY(cx, cy);
    if ((bCheckForTimeOut) && (useron))
    {
        if (nsln == 0.0) 
        {
            nl();
            pl("Time expired.");
            nl();
            hangup = 1;
        }
    }
}


void WLocalIO::UpdateTopScreenImpl()
{
    int cc, cx, cy, ctl, i, lll;
    char sl[81], ar[17], dar[17], restrict[17], rst[17], lo[90], ol[190], calls[20];
    
    lll = lines_listed;
    
    if (so() && !incom)
    {
        topdata = 0;
    }
    
    switch (topdata) 
    {
    case 0:
        set_protect(0);
        break;
    case 1:
        set_protect(5);
        break;
    case 2:
        if (chatcall)
        {
            set_protect(6);
        }
        else 
        {
            if (topline == 6)
            {
                set_protect(0);
            }
            set_protect(5);
        }
        break;
    }
    cx = WhereX();
    cy = WhereY();
    ctl = topline;
    cc = curatr;
    curatr = sysinfo.topscreen_color;
    topline = 0;
    for (i = 0; i < 80; i++)
    {
        sl[i] = '\xCD';
    }
    sl[80] = 0;
    
    switch (topdata) 
    {
    case 0:
        break;
    case 1:
        app->statusMgr->Read();
        LocalGotoXY(0, 0);
        sprintf(ol, "%-50s  Activity for %8s:      ", syscfg.systemname, status.date1);
        LocalFastPuts(ol);
        
        LocalGotoXY(0, 1);
        sprintf(ol, "Users: %4u       Total Calls: %5lu      Calls Today: %4u    Posted      :%3u ",
            status.users, status.callernum1, status.callstoday, status.localposts);
        LocalFastPuts(ol);
        
        LocalGotoXY(0, 2);
        sprintf(ol, "%-36s      %-4u min   /  %2u%%    E-mail sent :%3u ",
            nam(&sess->thisuser, usernum), status.activetoday,
            (int) (10 * status.activetoday / 144), status.emailtoday);
        LocalFastPuts(ol);
        
        LocalGotoXY(0, 3);
        sprintf(ol, "SL=%3u   DL=%3u               FW=%3u      Uploaded:%2u files    Feedback    :%3u ",
            sess->thisuser.sl, sess->thisuser.dsl, fwaiting, status.uptoday, status.fbacktoday);
        LocalFastPuts(ol);
        break;
        
    case 2:
        
        strcpy(rst, restrict_string);
        for (i = 0; i <= 15; i++) 
        {
            if (sess->thisuser.ar & (1 << i))
            {
                ar[i] = 'A' + i;
            }
            else
            {
                ar[i] = 32;
            }
            if (sess->thisuser.dar & (1 << i))
            {
                dar[i] = 'A' + i;
            }
            else
            {
                dar[i] = 32;
            }
            if (sess->thisuser.restrict & (1 << i))
            {
                restrict[i] = rst[i];
            }
            else
            {
                restrict[i] = 32;
            }
        }
        dar[16] = 0;
        ar[16] = 0;
        restrict[16] = 0;
        if (strcmp((char *) sess->thisuser.laston, date()))
        {
            strcpy(lo, (char *) sess->thisuser.laston);
        }
        else
        {
            sprintf(lo, "Today:%2d", sess->thisuser.ontoday);
        }
        
        LocalGotoXY(0, 0);
        sprintf(ol, "%-35s W=%3u UL=%4u/%6lu SL=%3u LO=%5u PO=%4u",
            nam(&sess->thisuser, usernum), sess->thisuser.waiting, sess->thisuser.uploaded,
            sess->thisuser.uk, sess->thisuser.sl, sess->thisuser.logons, sess->thisuser.msgpost);
        LocalFastPuts(ol);
        
        LocalGotoXY(0, 1);
        if (sess->thisuser.wwiv_regnum)
        {
            sprintf(calls, "%lu", sess->thisuser.wwiv_regnum);
        }
        else
        {
            strcpy(calls, (char *) sess->thisuser.callsign);
        }
        sprintf(ol, "%-20s %12s  %-6s DL=%4u/%6lu DL=%3u TO=%5.0lu ES=%4u",
            sess->thisuser.realname, sess->thisuser.phone, calls,
            sess->thisuser.downloaded, sess->thisuser.dk, sess->thisuser.dsl,
            ((long) ((sess->thisuser.timeon + timer() - timeon) / 60.0)),
            sess->thisuser.emailsent + sess->thisuser.emailnet);
        LocalFastPuts(ol);
        
        LocalGotoXY(0, 2);
        sprintf(ol, "ARs=%-16s/%-16s R=%-16s EX=%3u %-8s FS=%4u",
            ar, dar, restrict, sess->thisuser.exempt, lo, sess->thisuser.feedbacksent);
        LocalFastPuts(ol);
        
        LocalGotoXY(0, 3);
        sprintf(ol, "%-40.40s %c %2u %-17s          FW= %3u",
            sess->thisuser.note, sess->thisuser.sex, sess->thisuser.age,
            ctypes(sess->thisuser.comp_type), fwaiting);
        LocalFastPuts(ol);
        
        if (chatcall) 
        {
            LocalGotoXY(0, 4);
            LocalFastPuts(chatreason);
        }
        break;
    }
    if (ctl != 0) 
    {
        LocalGotoXY(0, ctl - 1);
        LocalFastPuts(sl);
    }
    topline = ctl;
    LocalGotoXY(cx, cy);
    curatr = cc;
    tleft(false);
    
    lines_listed = lll;
}


/****************************************************************************/


/**
 * IsLocalKeyPressed - returns whether or not a key been pressed at the local console.
 * 
 * @return true if a key has been pressed at the local console, false otherwise
 */
bool WLocalIO::LocalKeyPressed()
{
    return (x_only ? 0 : (kbhit() || ExtendedKeyWaiting)) ? true : false;
}

/****************************************************************************/
/*
* returns the ASCII code of the next character waiting in the
* keyboard buffer.  If there are no characters waiting in the
* keyboard buffer, then it waits for one.
*
* A value of 0 is returned for all extended keys (such as F1,
* Alt-X, etc.).  The function must be called again upon receiving
* a value of 0 to obtain the value of the extended key pressed.
*/
unsigned char WLocalIO::getchd()
{
    unsigned char rc;
    
    if (ExtendedKeyWaiting) 
    {
        ExtendedKeyWaiting = 0;
        return (unsigned char) getch();
    }
    rc = getch();
    if (rc == 0)
    {
        ExtendedKeyWaiting = 1;
    }
    return rc;
}


/****************************************************************************/
/*
* returns the ASCII code of the next character waiting in the
* keyboard buffer.  If there are no characters waiting in the
* keyboard buffer, then it returns immediately with a value
* of 255.
*
* A value of 0 is returned for all extended keys (such as F1,
* Alt-X, etc.).  The function must be called again upon receiving
* a value of 0 to obtain the value of the extended key pressed.
*/
unsigned char WLocalIO::getchd1()
{
    unsigned char rc;
    
    if (!((kbhit()) || ExtendedKeyWaiting))
    {
        return 255;
    }
    if (ExtendedKeyWaiting) 
    {
        ExtendedKeyWaiting = 0;
        return (unsigned char) getch();
    }
    rc = getch();
    if (rc == 0)
    {
        ExtendedKeyWaiting = 1;
    }
    return rc;
}


void WLocalIO::SaveCurrentLine(char *cl, char *atr, char *xl, char *cc)
{
    //  int i, i1;
    
    *cc = (char) curatr;
    strcpy(xl, endofline);
    {                                         /* add in these extra braces to
        * define more local vars */
        char buf[1024];
        USHORT CurX, CurY, BufLen = sizeof(buf);
        
        VioGetCurPos(&CurY, &CurX, 0);
        VioReadCellStr(buf, &BufLen, CurY, 0, 0);
        i = 0;
        for (i1 = 0; i1 < app->localIO->WhereX(); i1++) {
            cl[i1] = buf[i1 * 2];
            atr[i1] = buf[(i1 * 2) + 1];
        }
    }
    cl[app->localIO->WhereX()] = 0;
    atr[app->localIO->WhereX()] = 0;
}


/**
 * LocalGetChar - gets character entered at local console.  
 *                <B>Note: This is a blocking function call.</B>
 *
 * @return int value of key entered
 */
int  WLocalIO::LocalGetChar()
{
		return getch();
}


void WLocalIO::MakeLocalWindow(int x, int y, int xlen, int ylen)
{
    int xx;
    int yy;
    int i;

    // Make sure that we are within the range of {(0,0), (80,screenbottom)}
    if (xlen > 80)
    {
        xlen = 80;
    }
    if (ylen > (screenbottom + 1 - topline))
    {
        ylen = (screenbottom + 1 - topline);
    }
    if ((x + xlen) > 80)
    {
        x = 80 - xlen;
    }
    if ((y + ylen) > screenbottom + 1)
    {
        y = screenbottom + 1 - ylen;
    }
    
    xx = WhereX();
    yy = WhereY();

    // we expect to be offset by topline
    y += topline;

    // large enough to hold 80x50
    CHAR_INFO ci[4000];

    // pos is the position within the buffer
    COORD pos = {0, 0};
    COORD size;
    SMALL_RECT rect;

    // rect is the area on screen the buffer is to be drawn
    rect.Top    = y;
    rect.Left   = x;
    rect.Right  = xlen+x-1;
    rect.Bottom = ylen+y-1;

    // size of the buffer to use (lower right hand coordinate)
    size.X      = xlen;
    size.Y      = ylen;

    // our current position within the CHAR_INFO buffer
    int nCiPtr  = 0;

    //
    // Loop through Y, each time looping through X adding the right character
    //

    for (int yloop = 0; yloop<size.Y; yloop++)
    {
        for (int xloop=0; xloop<size.X; xloop++)
        {
            ci[nCiPtr].Attributes       = curatr;
            if ((yloop==0) || (yloop==size.Y-1))
            {
                ci[nCiPtr].Char.AsciiChar   = (char)(196);      // top and bottom
            }
            else
            {
                if ((xloop==0) || (xloop==size.X-1))
                {
                    ci[nCiPtr].Char.AsciiChar   = (char)(179);  // right and left sides
                }
                else
                {
                    ci[nCiPtr].Char.AsciiChar   = (char)(32);   // nothing... Just filler (space)
                }
            }
            nCiPtr++;
        }
    }

    
    //
    // sum of the lengths of the previous lines (+0) is the start of next line
    //

    ci[0].Char.AsciiChar                    = (char)(218);      // upper left
    ci[xlen-1].Char.AsciiChar               = (char)(191);      // upper right
    
    ci[xlen*(ylen-1)].Char.AsciiChar        = (char)(192);      // lower left
    ci[xlen*(ylen-1)+xlen-1].Char.AsciiChar = (char)(217);      // lower right

    //
    // Send it all to the screen with 1 WIN32 API call (Windows 95's console mode API support
    // is MUCH slower than NT/Win2k, therefore it is MUCH faster to render the buffer off
    // screen and then write it with one fell swoop.
    //

    WriteConsoleOutput(hConOut, ci, size, pos, &rect);

    //
    // Draw shadow around boxed window
    //

    for (i = 0; i < xlen; i++)
    {
        set_attr_xy(x + 1 + i, y + ylen, 0x08);
    }
    
    for (i = 0; i < ylen; i++)
    {
        set_attr_xy(x + xlen, y + 1 + i, 0x08);
    }
    
    LocalGotoXY(xx, yy);

}


void WLocalIO::SetCursor(UINT cursorStyle)
{
	CONSOLE_CURSOR_INFO cursInfo;

	switch (cursorStyle)
	{
		case _NOCURSOR:
		{
			cursInfo.dwSize = 20;
			cursInfo.bVisible = false;
		    SetConsoleCursorInfo(hConOut, &cursInfo);
		} 
		break;
		case _SOLIDCURSOR:
		{
			cursInfo.dwSize = 100;
		    cursInfo.bVisible = true;
		    SetConsoleCursorInfo(hConOut, &cursInfo);
		} 
		break;
		case _NORMALCURSOR:
		default:
		{
			cursInfo.dwSize = 20;
		    cursInfo.bVisible = true;
			SetConsoleCursorInfo(hConOut, &cursInfo);
		}
	} 
}


void WLocalIO::LocalClrEol()
{
	char EOLBuffer[160];
	int Temp;

	for (Temp = 0; Temp < sizeof(EOLBuffer); Temp++)
	{
		if (Temp % 2)
		{
			EOLBuffer[Temp] = (char) curatr;
		}
		else
		{
			EOLBuffer[Temp] = ' ';
		}
	}
	puttext(app->localIO->WhereX(), app->localIO->WhereY(), 80, app->localIO->WhereY(), (void *) &EOLBuffer);
}



void WLocalIO::LocalWriteScreenBuffer(const char *pszBuffer)
{
    CHAR_INFO ci[2000];

    COORD pos       = { 0, 0};
    COORD size      = { 80, 25 };
    SMALL_RECT rect = { 0, 0, 79, 24 };

	
	for(int i=0; i<2000; i++)
	{
        ci[i].Char.AsciiChar = (char) *(pszBuffer + ((i*2)+0));
        ci[i].Attributes     = (unsigned char) *(pszBuffer + ((i*2)+1));
	}
    WriteConsoleOutput(hConOut, ci, size, pos, &rect);
}


int WLocalIO::GetDefaultScreenBottom()
{
	return (vmi.row - 1);
}
