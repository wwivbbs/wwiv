/**************************************************************************/
/*                                                                        */
/*                                BREdit                                  */
/*             Copyright 1999 Art Johnston, BRWare Authors                */
/*               Based on the code from FEditby Rob Raper                 */ 
/*                 Portions Copyright 1993-1999 Rob Raper                 */
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
// FEDIT 2.0 Intelligent Optimizing ANSI IO routines
// with Enhanced ANSI support.  Enhanced ANSI commands are
// actually supported by VT100, but since VT100 is actually
// a superset of ANSI, most term programs allow the commands
// to go through.   If not, all the user must do is turn
// VT100 emulation on.  Definitely worth while for the speed increase
//
// (C) 1990-1993 by Rob Raper

#include "FEDITIO.H"
#include <conio.h>
#include <mem.h>

//#define DEBUG

IOClass::IOClass(BREditArguments *FA)
{
        // Rushfan add - tidy up 1st
        textattr(0x07);
        clrscr();

	BREditArgs=FA;
	Screen = (int *)MK_FP((peekb(0x0, 0x449)==0x07) ? 0xb000 : 0xb800, 0);
	int21 = (void (interrupt *)())getvect(0x21);
	if (FA->EnhancedMode==AUTO)
		Output("\x1bZ");  // Tell VT100 type terminal to return some value
}

IOClass::~IOClass()
{
        // EANSI_DefineRegion();
        EANSI_DefineRegion(0, 24);
}

void IOClass::ClearScreen(void)
{
	XTracking=0;
	YTracking=0;
	Output("\x1b[2J");
}


void IOClass::MoveTo(int x, int y)

// Optimizing MoveTo command.  Finds the smallest possible
// code to perform the function necessary.  Note that if the
// screen scrolls inadvertently, tracking becomes incorrect, however
// FEDIT assumes a never-scrolling screen.

{
	char s[20];

	if (y==YTracking && x==XTracking)
		return;
	s[0]=0;
	if (x==0)
		Output('\r');
	if (y==YTracking+1)
		Output('\n');
	if (y==YTracking+1)
		Output('\n');
	if (y==YTracking && x==XTracking)
		return;
	if (y==YTracking) {
		if (x>XTracking)
			OutputMovementCode(x-XTracking, 'C');
		else if (x<XTracking)
			OutputMovementCode(XTracking-x, 'D');
	} else
		sprintf(s, "\x1b[%d;%dH", y+1, x+1);
	Output(s);
	XTracking=x;
	YTracking=y;
}

void IOClass::OutputMovementCode(int num, char code)
{
	Output('\x1b');
	Output('[');
	if (num > 1)
		Output(num);
	Output(code);
}

void IOClass::SaveCursorPosition(void)
{
	XTrackingSave=XTracking;
	YTrackingSave=YTracking;
	Output("\x1b[s");
}

void IOClass::RestoreCursorPosition(void)
{
	Output("\x1b[u");
	XTracking=XTrackingSave;
	YTracking=YTrackingSave;
}

#define SCREEN_LOCATION(x,y) (x*80+y)

void IOClass::EraseToEnd(void)
{
	SetColor(0);
	Output("\x1b[K");
}


void IOClass::EANSI_InsertCharacter(void)
{
#ifndef DEBUG
	Output("\x1b[@");
#endif
	if (BREditArgs->DoEnhancedIO) {

		int Location;

		Location=SCREEN_LOCATION(XTracking, YTracking);
		memmove(&Screen[Location+1], &Screen[Location], (79-XTracking) * sizeof(int));
		Screen[Location]=0x0700;
	}
}

void IOClass::EANSI_DeleteCharacter(void)
{
#ifndef DEBUG
	Output("\x1b[P");
#endif
	if (BREditArgs->DoEnhancedIO) {

		int Location;

		Location=SCREEN_LOCATION(XTracking, YTracking);
		memmove(&Screen[Location], &Screen[Location+1], (79-XTracking) * sizeof(int));
	}
}


void IOClass::EANSI_DefineRegion(void)
{
	SaveCursorPosition();
	Output("\x1b[r");
	RestoreCursorPosition();
	CurScrollStart=0;
	CurScrollEnd=BREditArgs->ScreenLines-1;
}

void IOClass::EANSI_DefineRegion(int from, int to)
{
	char s[81];

#ifdef DEBUG
	sound(200);
	delay(10);
	nosound();
#endif
        // Change (wrapped in DoEnhancedIO)
	if (BREditArgs->DoEnhancedIO) {
                SaveCursorPosition();
                sprintf(s,"\x1b[%d;%dr", from+1, to+1);
                Output(s);
                RestoreCursorPosition();
                CurScrollStart=from;
                CurScrollEnd=to;
        }
}

void IOClass::EANSI_ScrollRegionUp(void)
{
	int y;
#ifndef DEBUG
	MoveTo(0, CurScrollEnd);
	if (BREditArgs->DoEnhancedIO)
		gotoxy(1, CurScrollEnd);
	Output('\n');
#endif
	if (BREditArgs->DoEnhancedIO) {
		union REGS regs;

		regs.h.ah = 6;
		regs.h.al = 1;
		regs.h.ch = CurScrollStart;
		regs.h.cl = 0;
		regs.h.dh = CurScrollEnd;
		regs.h.dl = 79;
		regs.h.bh = 0x07;
		int86(0x10, &regs, &regs);
	}
}


void IOClass::EANSI_ScrollRegionDown(void)
{
	char s[81];
	int y;

#ifndef DEBUG
	MoveTo(0, CurScrollStart);
	Output("\x1bM");
#endif
	if (BREditArgs->DoEnhancedIO) {
		union REGS regs;

		regs.h.ah = 7;
		regs.h.al = 1;
		regs.h.ch = CurScrollStart;
		regs.h.cl = 0;
		regs.h.dh = CurScrollEnd;
		regs.h.dl = 79;
		regs.h.bh = 0x07;
		int86(0x10, &regs, &regs);
	}
}

void IOClass::SetColor(color_t NewColor)

// Num is a color number from 0-36.  0-9 are represented as digits,
// 10-36 are represented as A-Z.  If a color is not supported (i.e. standard
// DOS mode is enabled.  Set_color attempts to remember the last
// color set.  It will not attempt to change the color again unless
// it differs from the last value.

{
	static int LastColor=255;   // Force first color to be different

	char *AnsiColors[] = {
				 "\x1b[0m",
				 "\x1b[0;1;36m",
				 "\x1b[0;1;33m",
				 "\x1b[0;35m",
				 "\x1b[0;1;44m",
				 "\x1b[0;0;32m",
				 "\x1b[0;1;5;31m",
				 "\x1b[0;1;34m"};

	if (NewColor==LastColor)
		return;
	if (!BREditArgs->WWIVColorChanges) {
		if (NewColor<8)
			Output(AnsiColors[NewColor]);
	} else {
		Output(COLOR_CHANGE_CODE);
		if (NewColor<10)
			Output(NewColor);
		else
			Output('A'+(char)(NewColor-10));
	}
	LastColor=NewColor;
}


void IOClass::Output(char ch)
{
	static int TrackingHold;

	switch(TrackingHold) {
	case WAITING_ANSI_CODE:
		if (ch!='[' && ch!=';' && !isdigit(ch))
			TrackingHold=0;
		break;
	case WAITING_WWIV_CODE:
		TrackingHold=0;
		break;
	case NOT_WAITING:
		switch(ch) {
		case ESC:
			TrackingHold=WAITING_ANSI_CODE;
			break;
		case CTRL_C:
			TrackingHold=WAITING_WWIV_CODE;
			break;
		case ENTER:
			XTracking=0;
			break;
		case BACKSPACE:
			XTracking--;
			break;
		case LINEFEED:
			YTracking++;
			break;
		case BEEP:
      break;
		default:
			XTracking++;
			break;
		}
	}
	enable();
	_AH = 2;
	_DL = ch;
	int21();
}


void IOClass::Output(int n)
{
	char s[21];

	itoa(n, s, 10);
	Output(s);
}



void IOClass::Output(char *s)
{
	int i;

	for(i=0;s[i];i++)
		Output(s[i]);
}


int IOClass::GetChar(void)
{
	clock_t t;
	unsigned char ch;
	static char ForcedCh=0;

	if (ForcedCh) {
		ch=ForcedCh;
		ForcedCh=0;
		return(ch);
	}
	switch((ch=GetRawChar())) {
	case 0:
		switch(GetRawChar()) {
		case 71:
			return KB_HOME;
		case 72:
			return KB_UP;
		case 73:
			return KB_PGUP;
		case 75:
			return KB_LEFT;
		case 77:
			return KB_RIGHT;
		case 79:
			return KB_END;
		case 80:
			return KB_DOWN;
		case 81:
			return KB_PGDN;
		case 82:
			return KB_INS;
		case 83:
			return KB_DEL;
		}
		break;
	case 27:
		t=clock()+8;
		while(clock()<t && !IsKeyWaiting());
		ch=0;
		if (!IsKeyWaiting() || (ch=GetRawChar()) != '[') {
			ForcedCh=ch;
			return 27;
		}
		if (ch=='[') switch(GetRawChar()) {
		case 'A':
			return KB_UP;
		case 'B':
			return KB_DOWN;
		case 'C':
			return KB_RIGHT;
		case 'D':
			return KB_LEFT;
		case 'H':
			return KB_HOME;
		case 'K':
			return KB_END;
		case '?':  // Got VT100 EANSI support
			while(GetRawChar()!='c');
			BREditArgs->EnhancedMode=ON;
			return GOT_EANSI;
		}
		break;
	default:
		return(ch);
	}
	return(0);
}


unsigned char IOClass::GetRawChar(void)
{
	enable();
	_AH=8;
	int21();
	return (_AL);
}


int IOClass::IsKeyWaiting(void)
{
  return(kbhit());
}


void IOClass::InputLine(char *s, int MaxChars)
{
	int CurrentLength=0, ch, i;

	SetColor(4);
	SaveCursorPosition();
	for(i=0;i<MaxChars;i++)
		Output(' ');
	RestoreCursorPosition();
	for(;;) {
		switch(ch=GetChar()) {
		case BACKSPACE:
			if (CurrentLength>0) {
				Output("\b \b");
				CurrentLength--;
			}
			break;
		case ENTER:
			s[CurrentLength]=0;
			return;
		default:
			if (ch>31 && CurrentLength<MaxChars) {
				s[CurrentLength++]=(char)ch;
				Output((char)ch);
			}
			break;
		}
	}
}
