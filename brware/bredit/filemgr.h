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
#include "FEDITIO.H"

#ifndef FEDITFIL_DOT_H

#define FILE_NOT_FOUND 1
#define FILE_OK 0
#define LINE_LENGTH 81

typedef struct ls
{
	unsigned int Length : 7;
	int Wrap : 1;
	char Text[LINE_LENGTH];
	color_t Color[LINE_LENGTH];
	ls *Next;
} LineStruct;


class EditorFileClass
{
public:

	char *Filename;
	int FileLines;

	LineStruct *FirstLine;
	BREditArguments *BREditArgs;

	EditorFileClass(char *fn, BREditArguments *FA);
	~EditorFileClass(void);


	void ExportLine(FILE *stream, LineStruct *Line, int FromColumn, int ToColumn);
	int ImportFile(char *Filename, int x, int AtLine);
	void SaveFile(void);
	void ExportFile(char *Filename,
									int BeginX, int BeginLine, int EndX, int EndLine);
	LineStruct *MakeNewLine(int LineNumber);
	void RemoveLine(int LineNumber);
	void Next(LineStruct *(&CurrentLine));
	int BreakLine(int BreakBeginX, int LineNumber, int Forced);
	int JoinLines(int LineNumber);
	void DeleteCharacter(int x, int LineNum);
	void AddAtX(char ch, int x, int Color, int LineNumber, int InsertMode);
	LineStruct *GetLine(int LineNumber);
};

#define FEDITFIL_DOT_H

#endif
