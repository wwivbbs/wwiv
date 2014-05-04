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
#ifndef FEDITIO_DOT_H

#include "ARGUMENT.H"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <dos.h>
#include <time.h>

#define KB_UP       -1
#define KB_DOWN     -2
#define KB_LEFT     -3
#define KB_RIGHT    -4
#define KB_PGUP     -5
#define KB_PGDN     -6
#define KB_HOME     -7
#define KB_END      -8
#define KB_INS      -9
#define KB_DEL      -10
#define KB_ESC      -11
#define GOT_EANSI   -100

#define NOT_WAITING         0
#define WAITING_ANSI_CODE   1
#define WAITING_WWIV_CODE   2

enum CTRL_CODES
{
    UNKNOWN=0, CTRL_A=1,CTRL_B, CTRL_C, CTRL_D, CTRL_E, CTRL_F,
    CTRL_G, BACKSPACE, TAB, CTRL_J, CTRL_K, CTRL_L,
    ENTER, CTRL_N, CTRL_O, CTRL_P, CTRL_Q, CTRL_R,
    CTRL_S, CTRL_T, CTRL_U, CTRL_V, CTRL_W, CTRL_X,
    CTRL_Y, CTRL_Z, ESC
};


#define LINEFEED            '\x0a'
#define BEEP                '\x07'
#define COLOR_CHANGE_CODE   '\x03'

#define color_t unsigned char

class IOClass
{
	int XTracking, YTracking;
	int XTrackingSave, YTrackingSave;
	int CurScrollStart, CurScrollEnd;
	int far *Screen;
	BREditArguments *BREditArgs;
	void far interrupt(*int21) ();
	unsigned char GetRawChar(void);
	void OutputMovementCode(int num, char code);

public:
	IOClass(BREditArguments *FA);
	~IOClass();
	void Output(char ch);
	void Output(char *s);
	void Output(int n);
	int GetChar(void);
	int IsKeyWaiting(void);
	void SetColor(color_t num);
	void ClearScreen(void);
	void MoveTo(int x, int y);
	void EraseToEnd(void);
	void SaveCursorPosition();
	void RestoreCursorPosition();
	void EANSI_DefineRegion(void);
	void EANSI_DefineRegion(int start, int end);
	void EANSI_InsertCharacter(void);
	void EANSI_DeleteCharacter(void);
	void EANSI_ScrollRegionUp(void);
	void EANSI_ScrollRegionDown(void);
	void InputLine(char *s, int MaxChars);
};

#define FEDITIO_DOT_H

#endif
