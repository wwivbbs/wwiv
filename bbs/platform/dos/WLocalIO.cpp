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

#define SCROLL_UP(t,b,l) \
  _CH=t;\
  _DH=b;\
  _BH=curatr;\
  _AL=l;\
  _CL=0;\
  _DL=79;\
  _AH=6;\
  my_video_int();

void my_video_int()
{
  static unsigned short sav_bp;

  __emit__(0x56, 0x57);                     /* push si, push di */
  sav_bp = _BP;
  geninterrupt(0x10);
  _BP = sav_bp;
  __emit__(0x5f, 0x5e);                     /* pop di, pop si */
#endif
}


/*
 * Sets screen attribute at screen pos x,y to attribute contained in a.
 */

void WLocalIO::set_attr_xy(int x, int y, int a)
{
  union REGS r;
  int xx, yy, oc, oa;

   xx = WhereX();
   yy = WhereY();
   read_char_xy(x, y, &oc, &oa);
   movecsr(x, y);
   r.h.ah = 0x09;
   r.h.al = (unsigned char) (oc);
   r.h.bh = 0;
   r.h.bl = (unsigned char) (a);
   r.x.cx = 1;
   int86(0x10, &r, &r);
   movecsr(xx, yy);
}




WLocalIO::WLocalIO() 
{ 
	ExtendedKeyWaiting = 0; 
	wx = 0; 
	m_nWfcStatus = 0;
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
  _BH = 0x00;
  _DH = y;
  _DL = x;
  _AH = 0x02;
  my_video_int();
}




int WLocalIO::WhereX()
/* This function returns the current X cursor position, as the number of
* characters from the left hand side of the screen.  An X position of zero
* means the cursor is at the left-most position
*/
{
    if (x_only)
    {
        return(wx);
    }
    return(wherex());    
}



int WLocalIO::WhereY()
/* This function returns the Y cursor position, as the line number from
* the top of the logical window.  The offset due to the protected top
* of the screen display is taken into account.  A WhereY() of zero means
* the cursor is at the top-most position it can be at.
*/
{
  return(wherey());
}



void WLocalIO::LocalLf()
/* This function performs a linefeed to the screen (but not remotely) by
* either moving the cursor down one line, or scrolling the logical screen
* up one line.
*/
{
  _BH = 0x00;
  _AH = 0x03;
  my_video_int();
  tempio = _DL;
  if (_DH >= screenbottom) {
    SCROLL_UP(topline, screenbottom, 1);
    _DL = tempio;
    _DH = screenbottom;
    _BH = 0;
    _AH = 0x02;
    my_video_int();
  } else {
    tempio = _DH + 1;
    _DH = tempio;
    _AH = 0x02;
    my_video_int();
  }
}



void WLocalIO::LocalCr()
/* This short function returns the local cursor to the left-most position
* on the screen.
*/
{
  _BH = 0x00;
  _AH = 0x03;
  my_video_int();
  _DL = 0x00;
  _AH = 2;
  my_video_int();
}

void WLocalIO::LocalCls()
/* This clears the local logical screen */
{
  SCROLL_UP(topline, screenbottom, 0);
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
  _BH = 0;
  _AH = 3;
  my_video_int();
  if (_DL == 0) {
    if (_DH != topline) {
      _DL = 79;
      tempio = _DH - 1;
      _DH = tempio;
      _AH = 2;
      my_video_int();
    }
  } else {
    _DL--;
    _AH = 2;
    my_video_int();
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
  _BL = curatr;
  _BH = 0x00;
  _CX = 0x01;
  _AL = ch;
  _AH = 0x09;
  my_video_int();
  _BH = 0x00;
  _AH = 0x03;
  my_video_int();
  ++_DL;
  if (_DL == 80) {
    _DL = 0;
    if (_DH == screenbottom) {
      SCROLL_UP(topline, screenbottom, 1);
      _DH = screenbottom;
      _DL = 0;
      _BH = 0;
      _AH = 0x02;
      my_video_int();
    } else {
      tempio = _DH + 1;
      _DH = tempio;
      _AH = 0x02;
      my_video_int();
    }
    lines_listed++;
  } else {
    _AH = 0x02;
    my_video_int();
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
  int i, i1;

  i1 = (WhereY() * 80 + WhereX()) * 2;
  for (i = 0; s[i] != 0; i++) {
    scrn[i * 2 + i1 + 1] = curatr;
    scrn[i * 2 + i1] = s[i];
  }
  LocalGotoXY(WhereX() + strlen(s), WhereY());
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
            setc((GetSession()->thisuser.sysstatus & sysstatus_color) ? GetSession()->thisuser.colors[3] :
            GetSession()->thisuser.bwcolors[3]);
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
        _CH = topline;
        _DH = screenbottom + 1;
        _AL = l - topline;
        _CL = 0;
        _DL = 79;
        _BH = 0x07;
        _AH = 7;
        my_video_int();
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
            curatr = ((GetSession()->thisuser.sysstatus & sysstatus_color) ? GetSession()->thisuser.colors[0] :
            GetSession()->thisuser.bwcolors[0]);
            SCROLL_UP(l, topline - 1, 0);
            curatr = sa;
            oldy += (topline - l);
        }
    }
    topline = l;
    if (using_modem)
        screenlinest = GetSession()->thisuser.screenlines;
    else
        screenlinest = defscreenbottom + 1 - topline;
}


void WLocalIO::savescreen(screentype * s)
{
  if (!s->scrn1)
    s->scrn1 = (char *) malloca(screenlen);

  if (s->scrn1)
    memmove(s->scrn1, scrn, screenlen);
    
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
        memmove(scrn, s->scrn1, screenlen);
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
                    GetApplication()->GetComm()->dtr(false);
                    break;
                case 88:                          /* Shift-F5 */
                    i1 = (rand() % 20) + 10;
                    for (i = 0; i < i1; i++)
                    {
                        outchr(rand() % 256);
                    }
                    hangup = 1;
                    GetApplication()->GetComm()->dtr(false);
                    break;
                case 98:                          /* Ctrl-F5 */
                    nl();
                    pl("Call back later when you are there.");
                    nl();
                    hangup = 1;
                    GetApplication()->GetComm()->dtr(false);
                    break;
                case 64:                          /* F6 */
                    ToggleSysopAlert();
                      tleft(false);
                    break;
                case 65:                          /* F7 */
                    GetSession()->thisuser.extratime -= 5.0 * 60.0;
                    tleft(false);
                    break;
                case 66:                          /* F8 */
                    GetSession()->thisuser.extratime += 5.0 * 60.0;
                    tleft(false);
                    break;
                case 67:                          /* F9 */
                    if (GetSession()->thisuser.sl != 255) 
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
        
        if ((GetSession()->thisuser.sl != 255) && (actsl == 255)) 
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
            strcpy(tl, (char *) GetSession()->thisuser.pw);
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
        GetApplication()->statusMgr->Read();
        LocalGotoXY(0, 0);
        sprintf(ol, "%-50s  Activity for %8s:      ", syscfg.systemname, status.date1);
        LocalFastPuts(ol);
        
        LocalGotoXY(0, 1);
        sprintf(ol, "Users: %4u       Total Calls: %5lu      Calls Today: %4u    Posted      :%3u ",
            status.users, status.callernum1, status.callstoday, status.localposts);
        LocalFastPuts(ol);
        
        LocalGotoXY(0, 2);
        sprintf(ol, "%-36s      %-4u min   /  %2u%%    E-mail sent :%3u ",
            nam(&GetSession()->thisuser, usernum), status.activetoday,
            (int) (10 * status.activetoday / 144), status.emailtoday);
        LocalFastPuts(ol);
        
        LocalGotoXY(0, 3);
        sprintf(ol, "SL=%3u   DL=%3u               FW=%3u      Uploaded:%2u files    Feedback    :%3u ",
            GetSession()->thisuser.sl, GetSession()->thisuser.dsl, fwaiting, status.uptoday, status.fbacktoday);
        LocalFastPuts(ol);
        break;
        
    case 2:
        
        strcpy(rst, restrict_string);
        for (i = 0; i <= 15; i++) 
        {
            if (GetSession()->thisuser.ar & (1 << i))
            {
                ar[i] = 'A' + i;
            }
            else
            {
                ar[i] = 32;
            }
            if (GetSession()->thisuser.dar & (1 << i))
            {
                dar[i] = 'A' + i;
            }
            else
            {
                dar[i] = 32;
            }
            if (GetSession()->thisuser.restrict & (1 << i))
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
        if (strcmp((char *) GetSession()->thisuser.laston, date()))
        {
            strcpy(lo, (char *) GetSession()->thisuser.laston);
        }
        else
        {
            sprintf(lo, "Today:%2d", GetSession()->thisuser.ontoday);
        }
        
        LocalGotoXY(0, 0);
        sprintf(ol, "%-35s W=%3u UL=%4u/%6lu SL=%3u LO=%5u PO=%4u",
            nam(&GetSession()->thisuser, usernum), GetSession()->thisuser.waiting, GetSession()->thisuser.uploaded,
            GetSession()->thisuser.uk, GetSession()->thisuser.sl, GetSession()->thisuser.logons, GetSession()->thisuser.msgpost);
        LocalFastPuts(ol);
        
        LocalGotoXY(0, 1);
        if (GetSession()->thisuser.wwiv_regnum)
        {
            sprintf(calls, "%lu", GetSession()->thisuser.wwiv_regnum);
        }
        else
        {
            strcpy(calls, (char *) GetSession()->thisuser.callsign);
        }
        sprintf(ol, "%-20s %12s  %-6s DL=%4u/%6lu DL=%3u TO=%5.0lu ES=%4u",
            GetSession()->thisuser.realname, GetSession()->thisuser.phone, calls,
            GetSession()->thisuser.downloaded, GetSession()->thisuser.dk, GetSession()->thisuser.dsl,
            ((long) ((GetSession()->thisuser.timeon + timer() - timeon) / 60.0)),
            GetSession()->thisuser.emailsent + GetSession()->thisuser.emailnet);
        LocalFastPuts(ol);
        
        LocalGotoXY(0, 2);
        sprintf(ol, "ARs=%-16s/%-16s R=%-16s EX=%3u %-8s FS=%4u",
            ar, dar, restrict, GetSession()->thisuser.exempt, lo, GetSession()->thisuser.feedbacksent);
        LocalFastPuts(ol);
        
        LocalGotoXY(0, 3);
        sprintf(ol, "%-40.40s %c %2u %-17s          FW= %3u",
            GetSession()->thisuser.note, GetSession()->thisuser.sex, GetSession()->thisuser.age,
            ctypes(GetSession()->thisuser.comp_type), fwaiting);
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
    int i, i1;
    
    *cc = (char) curatr;
    strcpy(xl, endofline);
    i = ((WhereY() + topline) * 80) * 2;
    for (i1 = 0; i1 < WhereX(); i1++) 
    {
        cl[i1] = scrn[i + (i1 * 2)];
        atr[i1] = scrn[i + (i1 * 2) + 1];
    }
    cl[GetApplication()->GetLocalIO()->WhereX()] = 0;
    atr[GetApplication()->GetLocalIO()->WhereX()] = 0;
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
#if defined (__OS2__)

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
	puttext(GetApplication()->GetLocalIO()->WhereX(), GetApplication()->GetLocalIO()->WhereY(), 80, GetApplication()->GetLocalIO()->WhereY(), (void *) &EOLBuffer);

#elif defined (_WIN32)

	CONSOLE_SCREEN_BUFFER_INFO ConInfo;
	DWORD cb;
	int len = 80 - GetApplication()->GetLocalIO()->WhereX();

	GetConsoleScreenBufferInfo(hConOut,&ConInfo);

	FillConsoleOutputCharacter(hConOut, ' ', len, ConInfo.dwCursorPosition, &cb);
	FillConsoleOutputAttribute(hConOut, (WORD) curatr, len, ConInfo.dwCursorPosition, &cb);

#endif
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
#if defined (_WIN32)

	return (coninfo.dwSize.Y - 1);

#elif defined (__OS2__)

	return (vmi.row - 1);

#endif
}
