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
#include <stdio.h>
#include "EDITOR.H"
#include "FILEMGR.H"

EditorFileClass::EditorFileClass(char *Fn, BREditArguments *FA)
{
	FirstLine=NULL;
	FileLines=0;
	BREditArgs=FA;
	Filename=Fn;
	MakeNewLine(0);
	ImportFile(Filename, 0, 0);
}

EditorFileClass::~EditorFileClass(void)
{
	while(FileLines)
		RemoveLine(0);
}

int EditorFileClass::ImportFile(char *Filename, int x, int AtLine)
{
	FILE *stream;
	int CurrentColor=0, ch, i;
	int Done=0; 			// LineCount=0;
	int LineNumber=AtLine;
	LineStruct *Line;

	if ((stream=fopen(Filename, "rb"))==NULL)
		return(FILE_NOT_FOUND);
	BreakLine(x, LineNumber, 0);
	LineNumber++;
	Line=MakeNewLine(LineNumber);
	while(!feof(stream)) {
		ch=fgetc(stream);
		switch(ch) {
		case 2:
			strcpy(Line->Text, "/C:");
			memset(Line->Color, 0, 3);
			Line->Length=3;
			break;
		case 4:
			Done=0;
			while(!Done) {
				switch(fgetc(stream)) {
				case EOF:
					Done=1;
					break;
				case 13:
					Done=1;
					break;
				}
			}
			break;
		case 3:
			ch=fgetc(stream);
			if (ch>='0' && ch<='9')
				CurrentColor=ch-'0';
			if (ch>='A' && ch<='Z')
				CurrentColor=10+ch-'A';
			break;
		case 13:
			while(Line->Length && Line->Text[Line->Length-1]==' ')
				(Line->Length)--;
			LineNumber++;
			Line=MakeNewLine(LineNumber);
			CurrentColor=0;
			break;
		case 1:
			Line->Wrap=1;
			break;
		case TAB:
			if (Line->Length < BREditArgs->MaxColumns-5) {
				int AddSpaces;
				AddSpaces= (((Line->Length + TAB_SIZE) / TAB_SIZE) * TAB_SIZE) - Line->Length;
				for(i=0;i<AddSpaces;i++) {
					Line->Text[Line->Length]=' ';
					Line->Color[(Line->Length++)]=CurrentColor;
				}
			}
			break;
		case BACKSPACE:
			if (Line->Length)
				Line->Length--;
			break;
		default:
			if (ch>31 && (Line->Length) < BREditArgs->MaxColumns) {
				Line->Text[Line->Length]=ch;
				Line->Color[Line->Length]=CurrentColor;
				(Line->Length)++;
			}
			break;
		}
	}
	Line->Wrap=1;
	JoinLines(LineNumber);
	JoinLines(AtLine);
	fclose(stream);
	return(FILE_OK);
}

void EditorFileClass::ExportLine(FILE *stream, LineStruct *Line, int FromColumn, int ToColumn)
{
	int CurrentColor=0, i;

	for(i=FromColumn;i<=ToColumn;i++) {
		if (i==0 && !strnicmp("/C:", Line->Text, 3)) {
			fputc('\x02', stream);
			i=3;
		}
		if (Line->Color[i]!=CurrentColor) {
			CurrentColor=Line->Color[i];
			fputc('\x03', stream);
			if (CurrentColor<10)
				fputc(CurrentColor+'0', stream);
			else
				fputc(CurrentColor+'A'-10, stream);
		}
		fputc(Line->Text[i], stream);
	}
	if (Line->Wrap)
		fputc('\x01', stream);
	fputc('\r', stream);
	fputc('\n', stream);
}

void EditorFileClass::SaveFile()
{
	LineStruct *CurrentLine;
	FILE *stream;
	int i;

	stream=fopen(Filename, "wb");
	CurrentLine=FirstLine;
	for(i=0;i<FileLines;i++) {
		ExportLine(stream, CurrentLine, 0, CurrentLine->Length-1);
		Next(CurrentLine);
	}
	fclose(stream);
}



LineStruct *EditorFileClass::GetLine(int LineNum)
{
	LineStruct *Line;
	int i;

	Line=FirstLine;
	for(i=0;i<LineNum;i++)
		Next(Line);
	return(Line);
}

void EditorFileClass::AddAtX(char ch, int x, int LineNum, int Color, int InsertMode)
{
	LineStruct *Line;
	int i;

	Line=GetLine(LineNum);
	if (InsertMode) {
		for(i=Line->Length;i>x;i--) {
			Line->Text[i]=Line->Text[i-1];
			Line->Color[i]=Line->Color[i-1];
		}
		(Line->Length)++;
	}
	Line->Text[x]=ch;
	Line->Color[x]=Color;
	if (x==Line->Length)
		(Line->Length)++;
}

void EditorFileClass::DeleteCharacter(int x, int LineNum)
{
	LineStruct *Line;
	int i;

	Line=GetLine(LineNum);
	if (x<Line->Length) {
		for(i=x;i<Line->Length-1;i++) {
			Line->Text[i]=Line->Text[i+1];
			Line->Color[i]=Line->Color[i+1];
		}
		(Line->Length)--;
	} else
		if (Line->Next)
			Line->Wrap=1;
}



LineStruct *EditorFileClass::MakeNewLine(int LineNumber)
{
	LineStruct *NewLine, *PreviousLine, *CurrentLine;
	int i;

	NewLine=new LineStruct;
	if (NewLine==NULL) {
		printf("Out of memory!");
		exit(1);
	}
	memset(NewLine, 0, sizeof(LineStruct));
	if (LineNumber==0) {
		CurrentLine=FirstLine;
		FirstLine=NewLine;
		NewLine->Next=CurrentLine;
		CurrentLine=FirstLine;
	} else {
		CurrentLine=FirstLine;
		for(i=1;i<LineNumber;i++)
			Next(CurrentLine);
		NewLine->Next=CurrentLine->Next;
		CurrentLine->Next=NewLine;
		CurrentLine=NewLine;
	}
	FileLines++;
  return(NewLine);
}




int EditorFileClass::BreakLine(int BreakBeginX, int LineNumber, int Forced)

// Breaks a line into two parts.  "Forced" is true when
// a user presses <return> to break up two lines.  False
// when the editor breaks up the lines in the case of word
// wrap.

{
	LineStruct *NewLine, *CurrentLine;
	int i;

	CurrentLine=GetLine(LineNumber);
	if (!CurrentLine)
		return 0;
	NewLine=MakeNewLine(LineNumber+1);
	for(i=BreakBeginX; i<CurrentLine->Length; i++) {
		NewLine->Text[i-BreakBeginX]=CurrentLine->Text[i];
		NewLine->Color[i-BreakBeginX]=CurrentLine->Color[i];
	}
	NewLine->Length=CurrentLine->Length-BreakBeginX;
	CurrentLine->Length=BreakBeginX;
	NewLine->Wrap=CurrentLine->Wrap;
	CurrentLine->Wrap=(!Forced);
	return(JoinLines(LineNumber+1));
}

void EditorFileClass::RemoveLine(int LineNumber)
// Removes CurrentLine.

{
	LineStruct *Line, *DeleteLine;

	if (LineNumber==0) {
		Line=FirstLine->Next;
		delete FirstLine;
		FirstLine=Line;
	} else {
		Line=GetLine(LineNumber-1);
		DeleteLine=Line->Next;
		Line->Next=DeleteLine->Next;
		delete DeleteLine;
	}
  FileLines--;
}

int EditorFileClass::JoinLines(int LineNumber)

// Returns line number if something has changed

{
	LineStruct *NextLine, *Line;
	int i, WordLengthCount, CompleteWordLength, LineJoinComplete;
	int SomethingDone=-1;


	Line=GetLine(LineNumber);
	while(Line->Wrap && Line->Next) {
		LineNumber++;
		NextLine=Line->Next;
		LineJoinComplete=0;
		while(!LineJoinComplete) {
			if (NextLine->Length==0) {
				SomethingDone=LineNumber;
				Line->Wrap=NextLine->Wrap;
				RemoveLine(LineNumber);
				LineJoinComplete=1;
			} else {
				CompleteWordLength=0;
				for(i=0;i<NextLine->Length;i++) {
					if (NextLine->Text[i]==' ' || i==(NextLine->Length-1))
						CompleteWordLength=i+1;
					else {
						if (CompleteWordLength)
							break;
					}
				}
				if (CompleteWordLength && CompleteWordLength+Line->Length < BREditArgs->MaxColumns) {
					SomethingDone=LineNumber;
					for(i=0;i<CompleteWordLength;i++) {
						Line->Text[i+Line->Length]=NextLine->Text[i];
						Line->Color[i+Line->Length]=NextLine->Color[i];
					}
					for(i=CompleteWordLength;i<NextLine->Length; i++) {
						NextLine->Text[i-CompleteWordLength]=NextLine->Text[i];
						NextLine->Color[i-CompleteWordLength]=NextLine->Color[i];
					}
					(Line->Length)+=CompleteWordLength;
					(NextLine->Length)-=CompleteWordLength;
				} else {
					LineJoinComplete=1;
				}
			}
		}
		Next(Line);
	}
	return SomethingDone;
}


void EditorFileClass::Next(LineStruct *(&CurrentLine))
{
	if (CurrentLine==NULL)
		return;
	CurrentLine=CurrentLine->Next;
}




