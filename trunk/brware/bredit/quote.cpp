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
#include "QUOTE.H"

#define QUOTE_TOP_LINE 3
#define QUOTE_DISPLAY_LINES ((BREditArgs->ScreenLines-1)-QUOTE_TOP_LINE)

QuoteClass::QuoteClass(char *QuoteFN, IOClass *InputOutput, EditorFileClass *MainEditorFile, BREditArguments *FA)
{
	ffblk ff;

	BREditArgs=FA;
	QuoteFileName=QuoteFN;
	MainFile=MainEditorFile;
	QuotesAvailable=(findfirst(QuoteFileName, &ff, 0)==0) ? 1 : 0;
	IO=InputOutput;
	QuoteBrowseLine=0;
}

void QuoteClass::ShowLine(LineStruct *Line)
{
	int i;

	if (Line) {
		for(i=0;i<Line->Length;i++) {
			IO->SetColor(Line->Color[i]);
			IO->Output(Line->Text[i]);
		}
	}
	IO->EraseToEnd();
}

void QuoteClass::ShowTopInfo(void)
{
	IO->MoveTo(0,0);
	IO->EraseToEnd();
	IO->SetColor(2);
	IO->Output("Quote from line : ");
	IO->SetColor(1);
	IO->MoveTo(58,0);
	IO->Output("Use arrows to browse,");
	IO->MoveTo(8,1);
	IO->EraseToEnd();
	IO->SetColor(2);
	IO->Output("to line : ");
	IO->MoveTo(58, 1);
	IO->SetColor(1);
	IO->Output("ESC to abort quote");
}

void QuoteClass::ShowQuotePage(EditorFileClass *QuoteFile)
{
	int i;
	LineStruct *Line;

	Line=QuoteFile->GetLine(QuoteBrowseLine);
	for(i=0;i<=QUOTE_DISPLAY_LINES;i++) {
		if (Line || i<=BottomLineUsed) {
			IO->MoveTo(0, i+QUOTE_TOP_LINE);
			ShowLine(Line);
			if (Line)
				BottomLineUsed=i;
			QuoteFile->Next(Line);
		}
	}
}

int QuoteClass::InputNumber(char *s)
{
	int ch, CurrentStringLength;

	for(;;) {
		CurrentStringLength=strlen(s);
		ch=IO->GetChar();
		switch(ch) {
		case KB_UP:
		case KB_DOWN:
		case KB_PGUP:
		case KB_PGDN:
		case ENTER:
		case ESC:
			if (BREditArgs->EnhancedMode!=ON) {
				switch(ch) {
				case KB_UP:
					return KB_PGUP;
				case KB_DOWN:
					return KB_PGDN;
				}
			}
			return(ch);
		case BACKSPACE:
			if (CurrentStringLength>0) {
				s[--CurrentStringLength]=0;
				IO->Output("\b \b");
			}
			break;
		default:
			if (CurrentStringLength<3 && isdigit(ch)) {
				s[CurrentStringLength++]=(char)ch;
				s[CurrentStringLength]=0;
				IO->SetColor(4);
				IO->Output((char)ch);
			}
			break;
		}
	}
}

#define HIGHLIGHT 1
#define UNHIGHLIGHT 0

#define INPUT_X 18

void QuoteClass::HighlightInput(int Number, int State, char *Data)
{
	char s[10];

	IO->SetColor((State==HIGHLIGHT) ? 4 : 0);
	sprintf(s,"%-3s", Data);
	IO->MoveTo(INPUT_X, Number);
	IO->Output(s);
}

#define NOT_DONE 0
#define DO_QUOTE 1
#define ABORT_QUOTE 2

#define ARROW_SIZE 5

#define FROM 0
#define TO 1

void QuoteClass::Quote(int LineNumber)
{
	int i;
	LineStruct *Line, *NextLine;

	EditorFileClass QuoteFile(QuoteFileName, BREditArgs);

	if (strchr(QuoteFile.GetLine(0)->Text, '#')!=NULL) { // Quote file contains author info
		QuoteFile.RemoveLine(0);
		QuoteFile.RemoveLine(0);
	}
	BottomLineUsed=0;
	for(i=0;i<QuoteFile.FileLines;i++) {
		Line=QuoteFile.MakeNewLine(i);
		sprintf(Line->Text, "%-3d> ", i+1);
		memset(Line->Color, 2, sizeof(color_t) * 3);
		Line->Length=ARROW_SIZE;
		Line->Color[3]=7;
		Line->Color[4]=0;
		Line->Wrap=1;
		NextLine=QuoteFile.GetLine(i+1);
		if (NextLine->Length+(ARROW_SIZE+1)>BREditArgs->MaxColumns)
			NextLine->Length=BREditArgs->MaxColumns-(ARROW_SIZE+1);
  /*		if (NextLine->Length+(ARROW_SIZE+1)>BREditArgs->MaxColumns) {
			int Count, NoSpaces=1;
			for(Count=BREditArgs->MaxColumns-(ARROW_SIZE+1);(Count>=0 && NoSpaces);Count--)
				if (NextLine->Text[Count]==' ')
					NoSpaces=0;
			if (NoSpaces)
				NextLine->Length=BREditArgs->MaxColumns-(ARROW_SIZE+1);
		}    */
		QuoteFile.JoinLines(i);
	}
	IO->SetColor(0);
	IO->ClearScreen();
	ShowTopInfo();
	IO->SetColor(7);
	IO->MoveTo(0, 2);
	for(i=1;i<BREditArgs->MaxColumns;i++)
		IO->Output('Ä');
	ShowQuotePage(&QuoteFile);
	if (BREditArgs->EnhancedMode==ON)
		IO->EANSI_DefineRegion(QUOTE_TOP_LINE, QUOTE_DISPLAY_LINES+QUOTE_TOP_LINE);

	int QuoteAction=NOT_DONE, CurrentInputField=0;
	char QuoteData[2][4];

	for(i=0;i<2;i++)
		QuoteData[i][0]=0;

	HighlightInput(FROM, HIGHLIGHT, QuoteData[FROM]);
	while(QuoteAction==NOT_DONE) {
		IO->MoveTo(INPUT_X+strlen(QuoteData[CurrentInputField]), CurrentInputField);
		switch(InputNumber(QuoteData[CurrentInputField])) {
		case ESC:
			QuoteAction=ABORT_QUOTE;
			break;
		case ENTER:
			if (strlen(QuoteData[CurrentInputField])==0) {
				QuoteAction=ABORT_QUOTE;
				break;
			}
			i=atoi(QuoteData[CurrentInputField]);
			if (i<1 || i>QuoteFile.FileLines ||
				 (CurrentInputField==1 && i<atoi(QuoteData[FROM]))) {
				IO->Output(BEEP);
				for(i=0;i<2;i++)
					QuoteData[i][0]=0;
				HighlightInput(TO, UNHIGHLIGHT, QuoteData[TO]);
				HighlightInput(FROM, HIGHLIGHT, QuoteData[FROM]);
				CurrentInputField=FROM;
			} else if (CurrentInputField==FROM) {
				CurrentInputField=TO;
				HighlightInput(FROM, UNHIGHLIGHT, QuoteData[FROM]);
				HighlightInput(TO, HIGHLIGHT, QuoteData[TO]);
			} else
				QuoteAction=DO_QUOTE;
			break;
		case KB_PGUP:
			if (QuoteBrowseLine>0) {
				QuoteBrowseLine-=QUOTE_DISPLAY_LINES-3;
				if (QuoteBrowseLine<0)
					QuoteBrowseLine=0;
				ShowQuotePage(&QuoteFile);
			}
			break;
		case KB_PGDN:
			if (QuoteBrowseLine+QUOTE_DISPLAY_LINES < QuoteFile.FileLines-1) {
				QuoteBrowseLine+=QUOTE_DISPLAY_LINES-3;
				if (QuoteBrowseLine+QUOTE_DISPLAY_LINES >= QuoteFile.FileLines)
					QuoteBrowseLine=QuoteFile.FileLines-1-QUOTE_DISPLAY_LINES;
				ShowQuotePage(&QuoteFile);
			}
			break;
					 // NOTE: EnhancedMode must be on for these keys   //
					 // to be generated.  InputNumber translates these //
					 // keys into PGUP and PGDN for non enhanced mode  //
					 // users.                                         //
		case KB_UP:
			if (QuoteBrowseLine>0) {
				QuoteBrowseLine--;
				IO->EANSI_ScrollRegionDown();
				IO->MoveTo(0, QUOTE_TOP_LINE);
				ShowLine(QuoteFile.GetLine(QuoteBrowseLine));
			}
			break;
		case KB_DOWN:
			if (QuoteBrowseLine+QUOTE_DISPLAY_LINES < QuoteFile.FileLines-1) {
				QuoteBrowseLine++;
				IO->EANSI_ScrollRegionUp();
				IO->MoveTo(0, QUOTE_TOP_LINE+QUOTE_DISPLAY_LINES);
				ShowLine(QuoteFile.GetLine(QuoteBrowseLine+QUOTE_DISPLAY_LINES));
			}
		}
	}
	if (QuoteAction==DO_QUOTE) {

		LineStruct *NewMainLine;
		int j;

		for(i=atoi(QuoteData[FROM])-1;i<=atoi(QuoteData[TO])-1;i++) {
			NewMainLine=MainFile->MakeNewLine(LineNumber);
			Line=QuoteFile.GetLine(i);
			for(j=3;j<Line->Length;j++) {
				NewMainLine->Text[j-3]=Line->Text[j];
				NewMainLine->Color[j-3]=Line->Color[j];
				NewMainLine->Length=Line->Length-3;
			}
			LineNumber++;
		}
		NewMainLine=MainFile->MakeNewLine(LineNumber);
		NewMainLine->Length=0;
	}
}
