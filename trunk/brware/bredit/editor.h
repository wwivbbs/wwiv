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
#ifndef EDTIOR_DOT_H

#include "FEDITIO.H"
#include "ARGUMENT.H"
#include "FILEMGR.H"
#include "QUOTE.H"
#include "FILEVIEW.H"

#include <string.h>
#include <stdlib.h>

#include "version.h"

#define VIEW_LINES 3

#define NO_PARAMETER -1

#define FORCED 1
#define NOT_FORCED 0

#define MATCH 1
#define NO_MATCH 0

#define ERASE_END 1
#define ERASE_SINGLE 2
#define NO_ERASE 0

#define TAB_SIZE 5

#define JOIN_LINE 1
#define BREAK_LINE 2

#ifndef min
#define min(a, b) ((a < b) ? a : b)
#endif

#define DisplayLines ((BREditArgs->ScreenLines-1)-TopLine)
#define CurrentLineNumber (CursorY+Page)

#define MACRO_SUPPORT

class EditorClass
{
public:
	int CursorX, CursorY, Color, Page, TopLine;
	int InsertMode, BottomLineUsed, QuotesAvailable;
	LineStruct *CurrentLine;

	IOClass *IO;
	BREditArguments *BREditArgs;
	EditorFileClass *MainFile;

	void Display(int Action, int Line, int X=NO_PARAMETER, int Type=NO_PARAMETER);
	void RedrawScreen(void);
	void ShowTitle(void);
	void ShowTitleAndBar(void);
	void ShowInsertStatus(void);
	void PrintCurrentLine(int StartX);
	void DrawScreen(void);
	void IncreaseCursorY(void);
	void DecreaseCursorY(void);
	void ShowLines(int FromLine, int ToLine=NO_PARAMETER);
	void SetCurrentLine(void);
	int CheckForCommand(char *CompareTo);
	void ShowInfoLine(char *Item, char *Setting, int &Line);
	void ShowInfo(void);
	void ShowOneLine(int Xpos, int LineNumber, int EraseMode);
	void BlockCommands();
	void ShowMessage(char *Message);
	void StuffKey(char ch);
	EditorClass(IOClass *IO, BREditArguments *FA);
};

#define EDITOR_DOT_H

#endif
