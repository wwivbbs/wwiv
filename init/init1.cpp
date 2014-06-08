/**************************************************************************/
/*                                                                        */
/*                 WWIV Initialization Utility Version 5.0                */
/*             Copyright (C)1998-2014, WWIV Software Services             */
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
#include "wwivinit.h"
#include "common.h"
#include <string>

#ifdef __unix__
#include <termios.h>
#endif  // __unix__

extern char configdat[];
extern char bbsdir[];

extern char **mdm_desc;
extern int mdm_count, mdm_cur;

extern autosel_data *autosel_info;
extern int num_autosel;

extern resultrec *result_codes;
extern int num_result_codes;

extern net_networks_rec *net_networks;

extern int inst;

static int wfc = 0;
static int useron = 0;

void init()
{
	curatr=0x03;
	hangup = false;  // TODO(rushfan): Remove this from init
	local_echo = true;
	thisuser.sysstatus=0;
    daylight = 0; // C Runtime Variable -- WHY?
}


void *malloca( unsigned long nbytes )
{
	void *pBuffer = bbsmalloc( nbytes + 1 );
	if ( pBuffer == NULL ) 
	{
		Printf( "\r\nNot enough memory, needed %ld bytes.\r\n\n", nbytes );
	}
	return pBuffer;
}

/****************************************************************************/


/* This will pause output, displaying the [PAUSE] message, and wait for
* a key to be hit.
*/
void pausescr()
{
	textattr(COLOR_MAGENTA);
	Puts( "[PAUSE]" );
	textattr(COLOR_CYAN);
	getkey();
	for (int i = 0; i < 7; i++) {
		backspace();
	}
}


/* This function executes a backspace, space, backspace sequence. */
void backspace()
{
	bool oldecho = local_echo;
	local_echo = true;
	Printf( "\b \b" );
	local_echo = oldecho;
}

/* This converts a character to uppercase */
int upcase(int ch)
{
	if ((ch > '`') && (ch < '{')) {
		ch = ch - 32;
	}
	return ch;
}


/* This function returns one character from either the local keyboard or
* remote com port (if applicable).  After 1.5 minutes of inactivity, a
* beep is sounded.  After 3 minutes of inactivity, the user is hung up.
*/
int getkey()
{
    return (getch());
}



/* This will input a line of data, maximum nMaxLength characters long, terminated
* by a C/R.  if (bAllowLowerCase) is true lowercase is allowed, otherwise all
* characters are converted to uppercase.
*/
void input1(char *pszOutText, int nMaxLength, bool bAllowLowerCase )
{
	int curpos=0, in_ansi=0;
    bool done = false;
	unsigned char ch;
	
	while (!done && !hangup) 
	{
		ch = getkey();
		if (in_ansi) 
		{
			if ((in_ansi==1) && (ch!='['))
			{
				in_ansi=0;
			}
			else 
			{
				if (in_ansi==1)
				{
					in_ansi=2;
				}
				else if (((ch<'0') || (ch>'9')) && (ch!=';'))
				{
					in_ansi=3;
				}
				else
				{
					in_ansi=2;
				}
			}
		}
		if (!in_ansi) 
		{
			if (ch > 31) 
			{
				if (curpos < nMaxLength) 
				{
					if ( !bAllowLowerCase )
					{
						ch = upcase(ch);
					}
					pszOutText[curpos++] = ch;
					app->localIO->LocalPutch(local_echo ? ch : '\xFE');
				}
			} 
			else switch(ch) 
			{
			  case 14:
				  // 13 on Win32
			  case 13:
				  // 10 on unix
			  case 10:
				  pszOutText[curpos] = 0;
				  done = true;
				  local_echo = true;
				  break;
			  case 23: // Ctrl-W 
				  if (curpos) 
				  {
					  do 
					  {
						  curpos--;
						  backspace();
						  if (pszOutText[curpos]==26)
						  {
							  backspace();
						  }
					  } while ((curpos) && (pszOutText[curpos-1]!=32));
				  }
				  break;
			  case 26:
				  break;
			  case 8:
				  if (curpos) 
				  {
					  curpos--;
					  backspace();
					  if (pszOutText[curpos] == 26)
					  {
						  backspace();
					  }
				  }
				  break;
			  case 21:
			  case 24:
				  while (curpos) 
				  {
					  curpos--;
					  backspace();
					  if (pszOutText[curpos] == 26)
					  {
						  backspace();
					  }
				  }
				  break;
			  case 27:
				  in_ansi=1;
				  break;
			}
		}
		if (in_ansi==3)
		{
			in_ansi=0;
		}
	}
	if (hangup)
	{
		pszOutText[0] = '\0';
	}
}

/* This will input an upper-case string */
void input(char *pszOutText, int nMaxLength)
{
	input1( pszOutText, nMaxLength, false );
}

/* The keyboard is checked for either a Y, N, or C/R to be hit.  C/R is
* assumed to be the same as a N.  Yes or No is output, and yn is set to
* zero if No was returned, and yn() is non-zero if Y was hit.
*/
int yn()
{
	char ch=0;
	const char *str_yes="Yes";
	const char *str_no="No";

	while (!hangup &&
		((ch = upcase(getkey())) != *str_yes) &&
		(ch != *str_no) &&
		(ch != 13) && (ch!=27))
		;
	Puts((ch == *str_yes) ? str_yes : str_no);
	nlx();
	return (ch == *str_yes);
}

char onek(const char *pszKeys)
{
	char ch = 0;
	
	while (!strchr(pszKeys, ch = upcase(getkey())) && !hangup)
		;
	if (hangup)
	{
		ch = pszKeys[0];
	}
	app->localIO->LocalPutch( ch );
	nlx();
	return ch;
}


/* This (obviously) outputs a string TO THE SCREEN ONLY */
void OutputStringRaw(const char *pszText) {
	for (int i=0; pszText[i]!=0; i++) 
	{
		char ch=pszText[i];
		app->localIO->LocalPutch(ch);
	}
}

int background_character = 0xb0;

int editlinestrlen( char *pszText )
{
	int i = strlen( pszText );
	while ( i >= 0 && (static_cast<unsigned char>( pszText[i-1] ) == background_character)) {
		--i;
	}
	return i;
}

void editline(std::string* s, int len, int status, int *returncode, const char *ss) {
    char pszBuffer[255];
    strcpy(pszBuffer, s->c_str());
    editline(pszBuffer, len, status, returncode, ss);
    s->assign(pszBuffer);
}

/* editline edits a string, doing I/O to the screen only. */
void editline(char *s, int len, int status, int *returncode, const char *ss)
{
    int i;
    int oldatr = curatr;
    int cx=app->localIO->WhereX();
    int cy=app->localIO->WhereY();
    for ( i = strlen( s ); i < len; i++ )
    {
        s[i] = static_cast<char>(background_character);
    }
    s[len] = '\0';
    textattr((16 * COLOR_BLUE) + COLOR_WHITE);
    OutputStringRaw(s);
    app->localIO->LocalGotoXY(77,0);
    OutputStringRaw("OVR");
    app->localIO->LocalGotoXY(cx,cy);
    bool done=false;
    int pos = 0;
    bool bInsert = false;
    do {
        int ch = getkey();
        switch ( ch ) {
        case KEY_F(1): // curses
            done = true;
            *returncode=DONE;
            break;
    	case KEY_HOME: // curses
            pos=0; 
            app->localIO->LocalGotoXY( cx, cy ); 
            break;        
        case KEY_END: // curses
            pos=editlinestrlen( s ); 
            app->localIO->LocalGotoXY( cx + pos, cy ); 
            break;  
        case KEY_RIGHT: // curses
            if (pos < len) {                       //right
                int nMaxPos = editlinestrlen( s );
                if (pos < nMaxPos) {
                    pos++;
                    app->localIO->LocalGotoXY( cx + pos, cy );
                }
            }
            break;
        case KEY_LEFT: // curses
            if ( pos > 0 ) 
            {                         //left
                pos--;
                app->localIO->LocalGotoXY( cx + pos, cy );
            }
            break;
        case CO:                                      //return
        case KEY_UP: // curses
            done = true;
            *returncode=PREV;
            break;
        case KEY_DOWN: // curses
            done = true;
            *returncode=NEXT;
            break;
        case KEY_IC: // curses
            if (status!=SET) {
                if (bInsert) {
                    bInsert = false;
                    app->localIO->LocalGotoXY(77,0);
                    OutputStringRaw("OVR");
                    app->localIO->LocalGotoXY(cx+pos,cy);
                } else {
                    bInsert = true;
                    app->localIO->LocalGotoXY(77,0);
                    OutputStringRaw("INS");
                    app->localIO->LocalGotoXY(cx+pos,cy);
                }
            }
            break;
	case KEY_DC: // curses
	case CD: // control-d
            if (status!=SET) {
                for ( i = pos; i < len; i++ ) {
                    s[i]=s[i+1];
                }
                s[len-1] = static_cast<char>(background_character);
                app->localIO->LocalGotoXY(cx,cy);
                OutputStringRaw(s);
                app->localIO->LocalGotoXY(cx+pos,cy);
            }
            break;
        default:
            if (ch>31) {
                if (status==UPPER_ONLY) {
                    ch=upcase( ch );
                }
                if ( status == SET ) {
                    ch = upcase( ch );
                    if (ch!=' ')  {
                        int i1 = 1;
                        for ( i = 0; i < len; i++ ) {
                            if ( ch == ss[i] && i1 ) {
                                i1=0;
                                pos=i;
                                app->localIO->LocalGotoXY(cx+pos,cy);
                                if (s[pos]==' ') {
                                    ch=ss[pos];
                                } else {
                                    ch=' ';
                                }
                            }
                            if ( i1 ) {
                                ch=ss[pos];
                            }
                        }
                    }
                }
                if ((pos<len)&&((status==ALL) || (status==UPPER_ONLY) || (status==SET) ||
                    ((status==NUM_ONLY) && (((ch>='0') && (ch<='9')) || (ch==' '))))) {
                    if ( bInsert )  {
                        for (i=len-1; i>pos; i--) {
                            s[i]=s[i-1];
                        }
                        s[pos++]=ch;
                        app->localIO->LocalGotoXY(cx,cy);
                        OutputStringRaw(s);
                        app->localIO->LocalGotoXY(cx+pos,cy);
                    }  else  {
                        s[pos++]=ch;
                        app->localIO->LocalPutch(ch);
                    }
                }
            }
            break;
        case RETURN:                                        //return
        case TAB:
            done = true;
            *returncode=NEXT;
            break;
        case ESC:                                    //esc
            done = true;
            *returncode=DONE;
            break;
        case 0x7f:  // yet some other delete key
        case KEY_BACKSPACE:  // curses
        case BACKSPACE:  //backspace
            if (pos>0) {
                for (i=pos-1; i<len; i++) {
                    s[i]=s[i+1];
                }
                s[len-1] = static_cast<char>(background_character);
                pos--;
                app->localIO->LocalGotoXY(cx,cy);
                OutputStringRaw(s);
                app->localIO->LocalGotoXY(cx+pos,cy);
            }
            break;
        case CA: // control-a
            pos=0; 
            app->localIO->LocalGotoXY(cx,cy); 
            break; 
        case CE: // control-e
            pos=editlinestrlen(s); 
            app->localIO->LocalGotoXY(cx+pos,cy); 
            break;
        }


    } while ( !done );

    int z = strlen( s );
    while (z >= 0 && static_cast<unsigned char>( s[z-1] ) == background_character) {
        --z;
    }
    s[z] = '\0';

    char szFinishedString[ 260 ];
    sprintf( szFinishedString, "%-255s", s );
    szFinishedString[ len ] = '\0';
    app->localIO->LocalGotoXY( cx, cy ); 
    curatr=oldatr;
    OutputStringRaw( szFinishedString );
    app->localIO->LocalGotoXY( cx, cy ); 
}


int toggleitem(int value, const char **strings, int num, int *returncode)
{
    if ( value < 0 || value >= num )
    {
		value=0;
    }
	
	int oldatr=curatr;
	int cx=app->localIO->WhereX();
	int cy=app->localIO->WhereY();
	int curatr=0x1f;
	OutputStringRaw(strings[value]);
	app->localIO->LocalGotoXY(cx,cy);
	bool done = false;
	do 
    {
		int ch = app->localIO->getchd();
		if ( ch == 0 || ch == 224) 
        {
			ch=app->localIO->getchd();
			switch (ch) 
            {
			case 59: // F1
				done = true;
				*returncode=DONE;
				break;
			case 72: // UP
			case 15: // SHIFT-TAB
				done = true;
				*returncode=PREV;
				break;
			case 80: // DOWN
				done = true;
				*returncode=NEXT;
				break;
			}
		} 
        else 
        {
			if ( ch > 31 ) 
            {
				if ( ch == 32 ) 
                {
					value= ( value + 1 ) % num;
					OutputStringRaw( strings[value] );
					app->localIO->LocalGotoXY( cx, cy );
				}
			} 
            else 
            {
				switch( ch ) 
                {
				case RETURN:
				case TAB:
					done = true;
					*returncode=NEXT;
					break;
				case ESC:
					done = true;
					*returncode=DONE;
					break;
				}
			}
		}
	} while ( !done );
	app->localIO->LocalGotoXY(cx,cy);
	curatr=oldatr;
	OutputStringRaw(strings[value]);
	app->localIO->LocalGotoXY(cx,cy);
	return value;
}


int GetNextSelectionPosition( int nMin, int nMax, int nCurrentPos, int nReturnCode )
{
    switch( nReturnCode ) 
    {
    case PREV:
        --nCurrentPos;
        if ( nCurrentPos < nMin )
        {
            nCurrentPos = nMax;
        }
        break;
    case NEXT:
        ++nCurrentPos;
        if ( nCurrentPos > nMax )
        {
            nCurrentPos = nMin;
        }
        break;
    case DONE:
        nCurrentPos = nMin;
        break;
    }
    return nCurrentPos;
}


/* This function returns the time, in seconds since midnight. */
double timer()
{
	time_t ti = time( NULL );
	struct tm *t  = localtime(&ti);
	
	return ( t->tm_hour * 3600.0 ) + ( t->tm_min * 60.0 ) + ( t->tm_sec );
}


#define TICKS_PER_SECOND 18.2


/* This function returns the time, in ticks since midnight. */
long timer1()
{
	return static_cast<long>( timer() * TICKS_PER_SECOND );
}


void fix_user_rec(userrec *u)
{
	u->name[sizeof(u->name)-1]=0;
	u->realname[sizeof(u->realname)-1]=0;
	u->callsign[sizeof(u->callsign)-1]=0;
	u->phone[sizeof(u->phone)-1]=0;
	u->pw[sizeof(u->pw)-1]=0;
	u->laston[sizeof(u->laston)-1]=0;
	u->note[sizeof(u->note)-1]=0;
	u->macros[0][sizeof(u->macros[0])-1]=0;
	u->macros[1][sizeof(u->macros[1])-1]=0;
	u->macros[2][sizeof(u->macros[2])-1]=0;
}

int number_userrecs()
{
    WFile file(syscfg.datadir, "user.lst");
    if (file.Open(WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile,
        WFile::shareDenyReadWrite, WFile::permRead)) {
        return static_cast<int>(file.GetLength());
    }
    WWIV_ASSERT(false);
    return -1;
}


void read_user(unsigned int un, userrec *u)
{
    if (((useron) && ((long) un==initinfo.usernum)) || ((wfc) && (un==1))) 
	{
		*u=thisuser;
		fix_user_rec(u);
		return;
	}

    WFile file(syscfg.datadir, "user.lst");
    if (!file.Open(WFile::modeReadWrite|WFile::modeBinary|WFile::modeCreateFile, WFile::shareDenyReadWrite, WFile::permReadWrite)) {
		u->inact=inact_deleted;
		fix_user_rec(u);
		return;
    }

    int nu = static_cast<int>((file.GetLength() / syscfg.userreclen) - 1);
	
	if ((int) un>nu) 
	{
        file.Close();
		u->inact=inact_deleted;
		fix_user_rec(u);
		return;
	}
	long pos = ((long) syscfg.userreclen) * ((long) un);
    file.Seek(pos, WFile::seekBegin);
    file.Read(u, syscfg.userreclen);
    file.Close();
	fix_user_rec(u);
}


void write_user(unsigned int un, userrec *u)
{
	if ((un<1) || (un>syscfg.maxusers))
	{
		return;
	}
	
	if (((useron) && ((long) un==initinfo.usernum)) || ((wfc) && (un==1))) 
	{
		thisuser=*u;
	}
	
    WFile file(syscfg.datadir, "user.lst");
    if (file.Open(WFile::modeReadWrite|WFile::modeBinary|WFile::modeCreateFile, WFile::shareUnknown, WFile::permReadWrite)) {
        long pos = un * syscfg.userreclen;
        file.Seek(pos, WFile::seekBegin);
        file.Write(u, syscfg.userreclen);
        file.Close();
    }
	
    user_config SecondUserRec = { 0 };
	strcpy(SecondUserRec.name, (char *) thisuser.name);
	strcpy(SecondUserRec.szMenuSet, "WWIV");
	SecondUserRec.cHotKeys = 1;
	SecondUserRec.cMenuType = 0;
	
    WFile userdat(syscfg.datadir, "user.dat");
    if (userdat.Open(WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile, WFile::shareDenyNone, WFile::permReadWrite)) {
        userdat.Seek(un * sizeof(user_config), WFile::seekBegin);
        userdat.Write(&SecondUserRec, sizeof(user_config));
        userdat.Close();
    }

}


/****************************************************************************/


int qscn_file=-1;

int open_qscn()
{
	char szFileName[MAX_PATH];
	
	if (qscn_file==-1) 
	{
		sprintf(szFileName,"%suser.qsc",syscfg.datadir);
		qscn_file=open(szFileName,O_RDWR|O_BINARY|O_CREAT, S_IREAD|S_IWRITE);
		if (qscn_file<0) 
		{
			qscn_file=-1;
			return 0;
		}
	}
	return 1;
}


void close_qscn()
{
	if (qscn_file!=-1) 
	{
		close(qscn_file);
		qscn_file=-1;
	}
}


void read_qscn(unsigned int un, unsigned long *qscn, int stayopen)
{
	if (((useron) && ((long) un==initinfo.usernum)) || ((wfc) && (un==1))) 
	{
		for ( int i=(syscfg.qscn_len/4)-1; i>=0; i-- )
        {
			qscn[i]=qsc[i];
        }
		return;
	}
	
	if (open_qscn()) 
	{
		long pos=((long)syscfg.qscn_len)*((long)un);
		if (pos + (long)syscfg.qscn_len <= filelength(qscn_file)) 
		{
			lseek(qscn_file,pos,SEEK_SET);
			read(qscn_file,qscn,syscfg.qscn_len);
			if (!stayopen)
			{
				close_qscn();
			}
			return;
		}
	}
	
	if (!stayopen)
	{
		close_qscn();
	}
	
	memset(qsc, 0, syscfg.qscn_len);
	*qsc=999;
	memset(qsc+1,0xff,((syscfg.max_dirs+31)/32)*4);
	memset(qsc+1+(syscfg.max_dirs+31)/32,0xff,((syscfg.max_subs+31)/32)*4);
}

void write_qscn(unsigned int un, unsigned long *qscn, int stayopen)
{

	if (((useron) && ((long) un==initinfo.usernum)) || ((wfc) && (un==1))) 
	{
		for (int i = (syscfg.qscn_len/4)-1; i>=0; i-- )
        {
			qsc[i]=qscn[i];
        }
	}
	
	if (open_qscn()) 
	{
		long pos=((long)syscfg.qscn_len)*((long)un);
		lseek(qscn_file,pos,SEEK_SET);
		write(qscn_file,qscn,syscfg.qscn_len);
		if (!stayopen)
		{
			close_qscn();
		}
	}
}


void wait1(long l)
{
	long l1 = timer1()+l;
	while (timer1()<l1)
		;
}


int exist(const char *pszFileName)
{
	WFindFile fnd;
	return ((fnd.open(pszFileName, 0) == false) ? 0 : 1);
}


void save_status()
{
	char szFileName[MAX_PATH];
	
	sprintf(szFileName,"%sstatus.dat",syscfg.datadir);
	int statusfile=open(szFileName,O_RDWR | O_BINARY | O_CREAT, S_IREAD|S_IWRITE);
	write(statusfile, (void *)(&status), sizeof(statusrec));
	close(statusfile);
}


char *date()
{
	static char ds[9];
	time_t t;
	time(&t);
	struct tm * pTm = localtime(&t);
	
	sprintf( ds, "%02d/%02d/%02d", pTm->tm_mon+1, pTm->tm_mday, pTm->tm_year % 100 );
	return ds;
}


char *times()
{
	static char ti[9];
	
	time_t tim = time(NULL);
	struct tm *t = localtime(&tim);
	sprintf(ti,"%02d:%02d:%02d",t->tm_hour,t->tm_min,t->tm_sec);
	return ti;
}

/** returns true if status.dat is read correctly */
bool read_status()
{
	char szFileName[81];
	
	sprintf(szFileName,"%sstatus.dat",syscfg.datadir);
	int statusfile=open(szFileName,O_RDWR | O_BINARY);
	if (statusfile>=0) 
	{
		read(statusfile,(void *)(&status), sizeof(statusrec));
		close(statusfile);
        return true;
	}
    return false;
}


int save_config()
{
	int configfile,n;
	
	if (inst==1) 
	{
		if (syscfgovr.primaryport<5) 
		{
			syscfg.primaryport=syscfgovr.primaryport;
			syscfg.com_ISR[syscfg.primaryport]=syscfgovr.com_ISR[syscfgovr.primaryport];
			syscfg.com_base[syscfg.primaryport]=syscfgovr.com_base[syscfgovr.primaryport];
			strcpy(syscfg.modem_type, syscfgovr.modem_type);
			strcpy(syscfg.tempdir, syscfgovr.tempdir);
			strcpy(syscfg.batchdir, syscfgovr.batchdir);
			if (syscfgovr.comflags & comflags_buffered_uart)
			{
				syscfg.sysconfig |= sysconfig_high_speed;
			}
			else
			{
				syscfg.sysconfig &= ~sysconfig_high_speed;
			}
		}
	}
	
	configfile=open(configdat,O_RDWR | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);
	write(configfile,(void *) (&syscfg), sizeof(configrec));
	close(configfile);
	
	configfile=open("config.ovr",O_RDWR | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);
	if (configfile>0) 
	{
		n=filelength(configfile)/sizeof(configoverrec);
		while (n<(inst-1)) 
		{
			lseek(configfile, 0L, SEEK_END);
			write(configfile, &syscfgovr, sizeof(configoverrec));
			n++;
		}
		lseek(configfile, sizeof(configoverrec)*(inst-1), SEEK_SET);
		write(configfile,&syscfgovr, sizeof(configoverrec));
		close(configfile);
	}
	
	return(0);
}


void Puts( const char *pszText )
{
	int i = 0;
	char szBuffer[ 255 ];
	
	while ( *pszText )
	{
		if (*pszText=='\n') 
		{
			szBuffer[i] = '\0';
			app->localIO->LocalPuts( szBuffer );
			i = 0;
			app->localIO->LocalPuts( "\r\n" );
			app->localIO->LocalClrEol();
		} 
		else if ( *pszText != '\r' )
		{
			szBuffer[ i++ ] = *pszText;
		}
		++pszText;
	}
	
	if ( i ) 
	{
		szBuffer[ i ] = '\0';
		app->localIO->LocalPuts( szBuffer );
	}
}


void nlx( int numLines )
{
	for ( int i=0; i < numLines; i++ )
	{
		Puts( "\r\n" );
	}
}

/**
 * Printf sytle output function.  Most init output code should use this.
 */
void Printf( const char *pszFormat, ... )
{
    va_list ap;
    char szBuffer[ 2048 ];
    
    va_start( ap, pszFormat );
    vsnprintf( szBuffer, 2048, pszFormat, ap );
    va_end( ap );
    Puts( szBuffer );
}

void create_text(const char *pszFileName)
{
	char szFullFileName[MAX_PATH];
    char szMessage[ 255 ];
	
    sprintf( szFullFileName, "gfiles%c%s", WWIV_FILE_SEPERATOR_CHAR, pszFileName );
	int hFile = open( szFullFileName, O_RDWR | O_BINARY | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE );
    sprintf( szMessage, "This is %s.\r\nEdit to suit your needs.\r\n\x1a", pszFileName);
	write( hFile, szMessage, strlen( szMessage ) );
	close( hFile );
}

void cvtx(unsigned short sp, char *rc)
{
	if (*rc) 
	{
	    resultrec *rr = &(result_codes[num_result_codes++]);
		std::string s = std::to_string(sp);
		strcpy(rr->curspeed, s.c_str());
		strcpy(rr->return_code, rc);
		rr->modem_speed = sp;
		if ((syscfg.sysconfig & sysconfig_high_speed) &&
			(syscfgovr.primaryport < MAX_ALLOWED_PORT))
		{
			rr->com_speed=syscfg.baudrate[syscfgovr.primaryport];
		}
		else
		{
			rr->com_speed=sp;
		}
	}
}

void convert_result_codes()
{
	num_result_codes=1;
	strcpy(result_codes[0].curspeed,"No Carrier");
	strcpy(result_codes[0].return_code,syscfg.no_carrier);
	result_codes[0].modem_speed=0;
	result_codes[0].com_speed=0;
	cvtx(300,syscfg.connect_300);
	cvtx(300,syscfg.connect_300_a);
	cvtx(1200,syscfg.connect_1200);
	cvtx(1200,syscfg.connect_1200_a);
	cvtx(2400,syscfg.connect_2400);
	cvtx(2400,syscfg.connect_2400_a);
	cvtx(9600,syscfg.connect_9600);
	cvtx(9600,syscfg.connect_9600_a);
	cvtx(19200,syscfg.connect_19200);
	cvtx(19200,syscfg.connect_19200_a);
    
    char szFileName[ MAX_PATH ];
    sprintf(szFileName,"%sresults.dat",syscfg.datadir);
	int hFile = open( szFileName,O_RDWR | O_BINARY | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE );
	write( hFile, result_codes, num_result_codes * sizeof( resultrec ) );
	close( hFile );
}

/****************************************************************************/

#define OFFOF(x) (short) (((long)(&(thisuser.x))) - ((long)&thisuser))

void init_files()
{
	int i;
	char s2[81], *env;
	valrec v;
	slrec sl;
	subboardrec s1;
	directoryrec d1;
	
	textattr(11);
	Puts("Creating Data Files.");
	textattr(COLOR_CYAN);
	
	memset(&syscfg, 0, sizeof(configrec));
	
	strcpy(syscfg.systempw,"SYSOP");
	Printf(".");
	sprintf(syscfg.msgsdir,"%smsgs%c", bbsdir, WWIV_FILE_SEPERATOR_CHAR);
	Printf(".");
	sprintf(syscfg.gfilesdir,"%sgfiles%c", bbsdir, WWIV_FILE_SEPERATOR_CHAR);
	Printf(".");
	sprintf(syscfg.datadir,"%sdata%c", bbsdir, WWIV_FILE_SEPERATOR_CHAR);
	Printf(".");
	sprintf(syscfg.dloadsdir,"%sdloads%c", bbsdir, WWIV_FILE_SEPERATOR_CHAR);
	Printf(".");
	sprintf(syscfg.tempdir,"%stemp1%c", bbsdir, WWIV_FILE_SEPERATOR_CHAR);
	Printf(".");
	sprintf(syscfg.menudir,"%sgfiles%cmenus%c", bbsdir, WWIV_FILE_SEPERATOR_CHAR, WWIV_FILE_SEPERATOR_CHAR);
	Printf(".");
	strcpy(syscfg.batchdir, syscfg.tempdir);
	Printf(".");
	strcpy(syscfg.bbs_init_modem,"ATS0=0M0Q0V0E0S2=1S7=20H0{");
	Printf(".");
	strcpy(syscfg.answer,"ATA{");
	strcpy(syscfg.connect_300,"1");
	strcpy(syscfg.connect_1200,"5");
	Printf(".");
	strcpy(syscfg.connect_2400,"10");
	strcpy(syscfg.connect_9600,"13");
	strcpy(syscfg.connect_19200,"50");
	Printf(".");
	strcpy(syscfg.no_carrier,"3");
	strcpy(syscfg.ring,"2");
	strcpy(syscfg.hangupphone,"ATH0{");
	Printf(".");
	strcpy(syscfg.pickupphone,"ATH1{");
	strcpy(syscfg.terminal,"");
	strcpy(syscfg.systemname,"My WWIV BBS");
	Printf(".");
	strcpy(syscfg.systemphone,"   -   -    ");
	strcpy(syscfg.sysopname,"The New Sysop");	
	Printf(".");
	
	syscfg.newusersl=10;
	syscfg.newuserdsl=0;
	syscfg.maxwaiting=50;
	Printf(".");
	for (i=0; i<5; i++) 
	{
		syscfg.baudrate[i]=300;
	}
	Printf(".");
	syscfg.com_ISR[0]=0;
	syscfg.com_ISR[1]=4;
	syscfg.com_ISR[2]=3;
	syscfg.com_ISR[3]=4;
	syscfg.com_ISR[4]=3;
	Printf(".");
	syscfg.com_base[0]=0;
	syscfg.com_base[1]=0x3f8;
	syscfg.com_base[2]=0x2f8;
	syscfg.com_base[3]=0x3e8;
	syscfg.com_base[4]=0x2e8;
	Printf(".");
	syscfg.comport[1]=1;
	syscfg.primaryport=1;
	Printf(".");
	syscfg.newuploads=0;
	syscfg.maxusers=500;
	syscfg.newuser_restrict=restrict_validate;
	Printf(".");
	syscfg.req_ratio=0.0;
	syscfg.newusergold=100.0;
	v.ar=0;
	v.dar=0;
	v.restrict=0;
	v.sl=10;
	v.dsl=0;
	Printf(".");
	for (i=0; i<10; i++)
	{
		syscfg.autoval[i]=v;
	}
	for (i=0; i<256; i++) 
	{
		sl.time_per_logon=(i / 10) * 10;
		sl.time_per_day=static_cast<unsigned short>( ((float)sl.time_per_logon) * 2.5 );
		sl.messages_read=(i / 10) * 100;
		if (i<10)
		{
			sl.emails=0;
		}
		else if (i<=19)
		{
			sl.emails=5;
		}
		else
		{
			sl.emails=20;
		}

		if (i<=10)
		{
			sl.posts=10;
		}
		else if (i<=25)
		{
			sl.posts=10;
		}
		else if (i<=39)
		{
			sl.posts=4;
		}
		else if (i<=79)
		{
			sl.posts=10;
		}
		else
		{
			sl.posts=25;
		}

		sl.ability=0;
		
		if (i>=150)
		{
			sl.ability |= ability_cosysop;
		}
		if (i>=100)
		{
			sl.ability |= ability_limited_cosysop;
		}
		if (i>=90)
		{
			sl.ability |= ability_read_email_anony;
		}
		if (i>=80)
		{
			sl.ability |= ability_read_post_anony;
		}
		if (i>=70)
		{
			sl.ability |= ability_email_anony;
		}
		if (i>=60)
		{
			sl.ability |= ability_post_anony;
		}
		if (i==255) 
		{
			sl.time_per_logon=255;
			sl.time_per_day=255;
			sl.posts=255;
			sl.emails=255;
		}
		syscfg.sl[i]=sl;
	}
	
	syscfg.userreclen = static_cast<short>( sizeof( thisuser ) );
	syscfg.waitingoffset=OFFOF(waiting);
	syscfg.inactoffset=OFFOF(inact);
	syscfg.sysstatusoffset=OFFOF(sysstatus);
	syscfg.fuoffset=OFFOF(forwardusr);
	syscfg.fsoffset=OFFOF(forwardsys);
	syscfg.fnoffset=OFFOF(net_num);
	
	syscfg.max_subs=64;
	syscfg.max_dirs=64;
	
	Printf(".");
	syscfg.qscn_len=4*(1+syscfg.max_subs+((syscfg.max_subs+31)/32)+((syscfg.max_dirs+31)/32));
	
	strcpy(syscfg.dial_prefix,"ATDT");
	syscfg.post_call_ratio=0.0;
	strcpy(syscfg.modem_type,"H2400");
	
	for (i=0; i<4; i++) 
	{
		syscfgovr.com_ISR[i+1]=syscfg.com_ISR[i+1];
		syscfgovr.com_base[i+1]=syscfg.com_base[i+1];
		syscfgovr.com_ISR[i+5]=syscfg.com_ISR[i+1];
		syscfgovr.com_base[i+5]=syscfg.com_base[i+1];
	}
	syscfgovr.primaryport=syscfg.primaryport;
	strcpy(syscfgovr.modem_type, syscfg.modem_type);
	strcpy(syscfgovr.tempdir, syscfg.tempdir);
	strcpy(syscfgovr.batchdir, syscfg.batchdir);
	
	save_config();
	
	create_arcs();
	
	Printf(".");
	
	memset(&status, 0, sizeof(statusrec));
	
	strcpy(status.date1,date());
	strcpy(status.date2,"00/00/00");
	strcpy(status.date3,"00/00/00");
	strcpy(status.log1,"000000.LOG");
	strcpy(status.log2,"000000.LOG");
	strcpy(status.gfiledate,date());
	Printf(".");
	status.callernum=65535;
	status.qscanptr=2;
	status.net_bias=0.001f;
	status.net_req_free=3.0;
	
	qsc=(unsigned long *)bbsmalloc(syscfg.qscn_len);
	Printf(".");
	memset(qsc,0,syscfg.qscn_len);
	
	save_status();
	memset(&thisuser, 0, sizeof(thisuser));
	write_user(0,&thisuser);
	write_qscn(0,qsc,0);
	thisuser.inact = inact_deleted;
	write_user(1, &thisuser);
	write_qscn(1,qsc,0);
	Printf(".");
	int hFile = open("data/names.lst", O_RDWR | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);
	close(hFile);
	
	Printf(".");
	memset(&s1, 0, sizeof(subboardrec));
	
	strcpy(s1.name,"General");
	strcpy(s1.filename,"GENERAL");
	s1.readsl=10;
	s1.postsl=20;
	s1.maxmsgs=50;
	s1.storage_type=2;
	hFile=open("data/subs.dat",O_RDWR | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);
	write(hFile, (void *) (&s1), sizeof(subboardrec));
	close(hFile);
	
	memset(&d1, 0, sizeof(directoryrec));
	
	Printf(".");
	strcpy(d1.name, "Sysop");
	strcpy(d1.filename, "SYSOP");
    sprintf(d1.path, "dloads%csysop%c", WWIV_FILE_SEPERATOR_CHAR, WWIV_FILE_SEPERATOR_CHAR);
	mkdir(d1.path);
	d1.dsl=100;
	d1.maxfiles=50;
	d1.type=65535;
	Printf(".");
	hFile=open("data/dirs.dat",O_RDWR | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);
	write(hFile, &d1, sizeof(directoryrec));
	
	memset(&d1, 0, sizeof(directoryrec));
	
	strcpy(d1.name, "Miscellaneous");
	strcpy(d1.filename, "misc");
	sprintf(d1.path, "dloads%cmisc%c", WWIV_FILE_SEPERATOR_CHAR, WWIV_FILE_SEPERATOR_CHAR);
	mkdir(d1.path);
	d1.dsl=10;
	d1.age=0;
	d1.dar=0;
	d1.maxfiles=50;
	d1.mask=0;
	d1.type=0;
	write(hFile, (void *) (&d1), sizeof(directoryrec));
	close(hFile);
	Printf(".\n\n");
	////////////////////////////////////////////////////////////////////////////
	textattr(11);
	Puts("Copying String and Miscellaneous files.");
	textattr(COLOR_CYAN);
	
	Printf(".");
	rename("wwivini.500","wwiv.ini");
	Printf(".");
    char szDestination[MAX_PATH];
    sprintf(szDestination, "data%cmenucmds.dat", WWIV_FILE_SEPERATOR_CHAR);
	rename("menucmds.dat",szDestination);
	Printf(".");
    sprintf(szDestination, "data%cregions.dat", WWIV_FILE_SEPERATOR_CHAR);
	rename("regions.dat", szDestination);
	Printf(".");
    sprintf(szDestination, "data%cwfc.dat", WWIV_FILE_SEPERATOR_CHAR);
	rename("wfc.dat", szDestination);
	Printf(".");
    sprintf(szDestination, "data%cmodems.mdm", WWIV_FILE_SEPERATOR_CHAR);
	rename("modems.500", szDestination);
	Printf(".");
    // Create the sample files.
	create_text("welcome.msg");
	create_text("newuser.msg");
	create_text("feedback.msg");
	create_text("system.msg");
	create_text("logon.msg");
	create_text("logoff.msg");
	create_text("logoff.mtr");
	create_text("comment.txt");
	
	WFindFile fnd;
	fnd.open("*.mdm", 0);
	while (fnd.next()) 
	{
		sprintf(s2,"data%c%s", WWIV_FILE_SEPERATOR_CHAR, fnd.GetFileName());
		rename(fnd.GetFileName(), s2);
	}
	
	if ((env = getenv("TZ")) == NULL) 
	{
		putenv("TZ=EST5EDT");
	}
	
	Printf(".\n\n");
	
	////////////////////////////////////////////////////////////////////////////
	textattr(11);
	Puts("Decompressing archives.  Please wait");
	textattr(COLOR_CYAN);
	if (exist("en-menus.zip")) 
	{
        char szDestination[MAX_PATH];
		Printf(".");
		system("unzip -qq -o EN-menus.zip -dgfiles ");
		Printf(".");
        sprintf(szDestination, "dloads%csysop%cen-menus.zip", 
            WWIV_FILE_SEPERATOR_CHAR, WWIV_FILE_SEPERATOR_CHAR);
		rename("en-menus.zip", szDestination);
		Printf(".");
	}
	if (exist("regions.zip")) 
	{
        char szDestination[MAX_PATH];
		Printf(".");
		system("unzip -qq -o regions.zip -ddata");
		Printf(".");
        sprintf(szDestination, "dloads%csysop%cregions.zip", 
            WWIV_FILE_SEPERATOR_CHAR, WWIV_FILE_SEPERATOR_CHAR);
		rename("regions.zip", szDestination);
		Printf(".");
	}
	if (exist("zip-city.zip")) 
	{
        char szDestination[MAX_PATH];
		Printf(".");
		system("unzip -qq -o zip-city.zip -ddata");
		Printf(".");
        sprintf(szDestination, "dloads%csysop%czip-city.zip", 
            WWIV_FILE_SEPERATOR_CHAR, WWIV_FILE_SEPERATOR_CHAR);
		rename("zip-city.zip", szDestination);
		Printf(".");
	}
	// we changed the environment, clear the change
	if (env == NULL) 
	{
		putenv("TZ=");
	}
	
	textattr(COLOR_CYAN);
	Printf(".\n\n");
}


void convert_modem_info(const char *fn)
{
	char szFileName[MAX_PATH];
	FILE *pFile;
	int i;
	
	sprintf(szFileName,"%s%s.mdm",syscfg.datadir,fn);
	pFile = fopen(szFileName,"w");
	if (!pFile) 
	{
		Printf( "Couldn't open '%s' for writing.\n", szFileName );
		textattr(COLOR_WHITE);
		exit( 2 );
	}
	
	fprintf(pFile,"NAME: \"Local defaults\"\n");
	fprintf(pFile,"INIT: \"%s\"\n",syscfg.bbs_init_modem);
	fprintf(pFile,"SETU: \"\"\n");
	fprintf(pFile,"ANSR: \"%s\"\n",syscfg.answer);
	fprintf(pFile,"PICK: \"%s\"\n",syscfg.pickupphone);
	fprintf(pFile,"HANG: \"%s\"\n",syscfg.hangupphone);
	fprintf(pFile,"DIAL: \"%s\"\n",syscfg.dial_prefix);
	fprintf(pFile,"SEPR: \"\"\n");
	fprintf(pFile,"DEFL: MS=%u CS=%u EC=N DC=N AS=N FC=%c\n",
		syscfg.baudrate[syscfgovr.primaryport],
		syscfg.baudrate[syscfgovr.primaryport],
		/* syscfg.sysconfig & sysconfig_flow_control?'Y':'N' */ 'X' );
	fprintf(pFile,"RESL: \"0\"     \"Normal\"       NORM\n");
	fprintf(pFile,"RESL: \"OK\"    \"Normal\"       NORM\n");
	fprintf(pFile,"RESL: \"%s\"    \"Ring\"         RING\n",syscfg.ring);
	
	for (i=0; i<num_result_codes; i++) 
	{
		if (result_codes[i].modem_speed) 
		{
			fprintf( pFile,"RESL: \"%s\"    \"%s\"         CON MS=%u CS=%u\n",
 				     result_codes[i].return_code, result_codes[i].curspeed,
				     result_codes[i].modem_speed, result_codes[i].com_speed );
		} 
		else 
		{
			fprintf( pFile,"RESL: \"%s\"    \"%s\"         DIS\n",
				     result_codes[i].return_code, result_codes[i].curspeed);
		}
	}
	
	fclose( pFile );
}


void init_modem_info()
{
	get_descriptions( syscfg.datadir, &mdm_desc, &mdm_count, &autosel_info, &num_autosel );
	
    // The modem type isn't used much, and should be used less under Win32
    syscfgovr.primaryport = 1;
	if (!set_modem_info("LOCAL", true)) 
	{
		result_codes = (resultrec *) bbsmalloc( 40 * sizeof( resultrec ) );
		convert_result_codes();
		convert_modem_info("LOCAL");
		BbsFreeMemory(result_codes);
		set_modem_info("LOCAL", true);
	}
}


void new_init()
{
	const int ENTRIES = 12;
	const char *dirname[] = 
	{ 
		"attach",
		"data",
		"data/regions",
		"data/zip-city",
		"gfiles",
		"gfiles/menus",
		"msgs",
		"dloads",
		"dloads/misc",
		"dloads/sysop",
		"temp1",
		"temp2",
		0L,
	};
	textattr(COLOR_YELLOW);
	Puts("\r\n\r\nNow performing installation.  Please wait...\r\n\r\n");
	textattr(COLOR_CYAN);
	////////////////////////////////////////////////////////////////////////////
	textattr(11);
	Puts("Creating Directories");
	textattr(11);
	for (int i = 0; i < ENTRIES; ++i) {
		textattr(11);
		Printf(".");
		int nRet = chdir(dirname[i]);
		if ( nRet ) 
		{
			if ( mkdir( dirname[i] ) ) 
			{
				textattr(COLOR_RED);
				Printf("\n\nERROR!!! Couldn't make '%s' Sub-Dir.\nExiting...", dirname[i]);
				textattr(COLOR_WHITE);
				exit( 2 );
			}
		} 
		else 
		{
			WWIV_ChangeDirTo(bbsdir);
		}
	}
	Printf(".\n\n");
	
	init_files();
	
	init_modem_info();
}

int verify_inst_dirs(configoverrec *co, int inst)
{
	int i, rc = 0;
	configoverrec tco;
	char szMessage[255];
	
	int hFile = open( "config.ovr", O_RDONLY | O_BINARY );
	if ( hFile > 0 )
	{
		int n = filelength( hFile ) / sizeof(configoverrec);
		for (i = 0; i < n; i++) 
		{
			if ((i + 1) != inst) 
			{
				lseek( hFile, sizeof( configoverrec ) * i, SEEK_SET );
				read( hFile, &tco, sizeof( configoverrec ) );
				
				if ((strcmp(co->tempdir, tco.tempdir) == 0) ||
					(strcmp(co->tempdir, tco.batchdir) == 0)) 
				{
					rc++;
					sprintf(szMessage,"The Temporary directory: %s is in use on Instance #%d.",
						co->tempdir, i + 1);
				}
				if ((strcmp(co->batchdir, tco.batchdir) == 0) ||
					(strcmp(co->batchdir, tco.tempdir) == 0)) 
				{
					rc++;
					sprintf(szMessage, "The Batch directory: %s is in use on Instance #%d.",
						co->batchdir, i + 1);
				}
			}
		}
		close( hFile );
	}
	if (rc) {
		app->localIO->LocalGotoXY(0, 8);
		textattr(COLOR_RED);
		app->localIO->LocalPuts(szMessage);
		for (i = 0; i < (int) strlen(szMessage); i++)
		{
			Printf("\b \b");
		}
		
		textattr(COLOR_YELLOW);
		app->localIO->LocalPuts("<ESC> when done.");
		textattr(COLOR_CYAN);
	}
	return 0;
}

