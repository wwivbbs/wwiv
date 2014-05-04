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
// BREdit 2.0 -- Editor Core
// Copyright (C) 1990-1993 by Rob Raper

#include "EDITOR.H"

EditorClass::EditorClass(IOClass *IOClassPtr, BREditArguments *FA)
{
	BREditArgs=FA;
	IO=IOClassPtr;

	int ch, FoundWord, i, TabSpacesRemaining=0;
	int Save=0;


	InsertMode=1;
	CursorX=0;
	CursorY=0;
	Page=0;
	Color=0;
	BottomLineUsed=0;

	EditorFileClass MainEditorFile(BREditArgs->EditFilename, BREditArgs);
	QuoteClass Quoter("QUOTES.TXT", IO, &MainEditorFile, BREditArgs);

	QuotesAvailable=Quoter.QuotesAvailable;

	MainFile=&MainEditorFile;

        IO->SetColor(1);
        IO->ClearScreen();
        IO->SetColor(0);
	DrawScreen();
        for(;;)
        {
		IO->MoveTo(CursorX, CursorY+TopLine);
		SetCurrentLine();
                if (TabSpacesRemaining)
                {
			ch=' ';
			TabSpacesRemaining--;
                }
                else
                {
                        ch=IO->GetChar();
                }
                switch(ch)
                {
		case UNKNOWN:
			break;
		case GOT_EANSI:
                        // %%TODO: Remove this
                        sound(200);
                        delay(50);
			IO->EANSI_DefineRegion(TopLine, DisplayLines+TopLine);
			break;
		case KB_LEFT:
			if (CursorX)
                        {
				CursorX--;
                        }
			break;
		case KB_RIGHT:
			if (CursorX<CurrentLine->Length)
                        {
				CursorX++;
                        }
			break;
		case KB_UP:
                        if (CurrentLineNumber>0)
                        {
				DecreaseCursorY();
				SetCurrentLine();
				if (CursorX>CurrentLine->Length)
                                {
					CursorX=CurrentLine->Length;
                                }
			}
			break;
		case KB_DOWN:
                        if (CurrentLineNumber<MainFile->FileLines-1)
                        {
				IncreaseCursorY();
				MainFile->Next(CurrentLine);
				if (CursorX>CurrentLine->Length)
                                {
                                        CursorX=CurrentLine->Length;
                                }
                        }
			break;
		case KB_HOME:
			CursorX=0;
			break;
		case KB_END:
			CursorX=CurrentLine->Length;
			break;
		case TAB:
			TabSpacesRemaining= (((CursorX + TAB_SIZE) / TAB_SIZE) * TAB_SIZE) - CursorX;
			break;
		case ENTER:
			Color=0;
                        if (CheckForCommand("/SN") || CheckForCommand("/ESN"))
                        {
				BREditArgs->AnonymousStatus=ANONYMOUS_NO;
				Save=1;
                        }
                        else if (CheckForCommand("/SY") || CheckForCommand("/ESY"))
                        {
				BREditArgs->AnonymousStatus=ANONYMOUS_YES;
				Save=1;
                        }
                        else if (CheckForCommand("/S") ||  CheckForCommand("/ES"))
                        {
				BREditArgs->AnonymousStatus=ANONYMOUS_UNDEFINED;
				Save=1;
			}
                        if (Save)
                        {
				IO->MoveTo(0, BottomLineUsed+TopLine+1);
				IO->EraseToEnd();
				MainFile->SaveFile();
				return;
                        }
                        else if (CheckForCommand("/IN"))
                        {
				ShowInfo();
				DrawScreen();
                        }
                        else if (CheckForCommand("/IM"))
                        {
                                if (BREditArgs->SysOpMode)
                                {
                                        char szImportFilename[255];
                                        IO->MoveTo(0,0);
                                        IO->Output("Filename: ");
                                        IO->InputLine(szImportFilename, 60);
                                        IO->MoveTo(0,0);
                                        IO->SetColor(1);
                                        IO->EraseToEnd();
                                        ShowTitle();
                                        IO->SetColor(Color);
                                        IO->MoveTo(CursorX, CursorY+TopLine);
                                        IO->EraseToEnd();
                                        if (szImportFilename[0])
                                        {
                                                MainFile->ImportFile(szImportFilename, 0, CurrentLineNumber);
                                        }
                                        else
                                        {
                                                ShowMessage("No filename given, File Not Imported.");
                                        }
                                }
                                else
                                {
                                        ShowMessage("You must be a sysop to use this command.");
                                }
				DrawScreen();
                        }
                        else if (CheckForCommand("/T"))
                        {
                                if (BREditArgs->TitleChangeable)
                                {
					IO->MoveTo(0,0);
					IO->Output("Title: ");
					IO->InputLine(BREditArgs->Title, 60);
					IO->MoveTo(0,0);
					IO->SetColor(1);
					IO->EraseToEnd();
					ShowTitle();
					IO->SetColor(Color);
					IO->MoveTo(CursorX, CursorY+TopLine);
					IO->EraseToEnd();
                                }
                                else
                                {
					ShowMessage("Cannot change title.");
                                }
                        }
                        else if (CheckForCommand("/Q"))
                        {
                                if (QuotesAvailable)
                                {
					int CurFileLines;

					CurFileLines=MainFile->FileLines;
					Quoter.Quote(CurrentLineNumber);
					CursorY+=MainFile->FileLines-CurFileLines;
                                        if (CursorY>=DisplayLines)
                                        {
						Page+=(CursorY-(DisplayLines-VIEW_LINES));
						CursorY-=(CursorY-(DisplayLines-VIEW_LINES));
					}
					DrawScreen();
                                }
                                else
                                {
					ShowMessage("Quotes not available");
                                }
                        }
                        else if (CheckForCommand("/L"))
                        {
				FileView *PreviewScreen;
				PreviewScreen=new FileView(IO, BREditArgs, MainFile, "Message Preview");
				delete PreviewScreen;
				DrawScreen();
                        }
                        else if (CheckForCommand("/H") || CheckForCommand("/?"))
                        {
				FileView *HelpScreen;
				EditorFileClass HelpFile(BREditArgs->HelpFile, BREditArgs);
				HelpScreen=new FileView(IO, BREditArgs, &HelpFile, "BREdit Help");
				delete HelpScreen;
				DrawScreen();
                        }
                        else if (CheckForCommand("/A"))
                        {
				IO->MoveTo(0,0);
				IO->SetColor(2);
				IO->Output("Abort edit (Y/N)? ");
				IO->EraseToEnd();
                                if (toupper(IO->GetChar())=='Y')
                                {
					IO->SetColor(1);
					IO->Output("Yes");
					IO->MoveTo(0, BottomLineUsed+TopLine+1);
					return;
				}
				ShowTitle();
				IO->SetColor(Color);
				IO->MoveTo(CursorX, CursorY+TopLine);
				IO->EraseToEnd();
                        }
                        else
                        {
				IncreaseCursorY();
				Display(BREAK_LINE, CursorY-1+Page, CursorX, 1);
				CursorX=0;
			}
			break;
		case BACKSPACE:
                        if (CursorX)
                        {
				CursorX--;
				MainFile->DeleteCharacter(CursorX, CurrentLineNumber);
                        }
                        else if (CurrentLineNumber)
                        {
				DecreaseCursorY();
				SetCurrentLine();
				CursorX=CurrentLine->Length;
				CurrentLine->Wrap=1;
			}
			Display(JOIN_LINE, CurrentLineNumber);
			break;
		case KB_INS:
			InsertMode=!InsertMode;
			ShowInsertStatus();
			break;
#ifdef MACRO_SUPPORT
		case CTRL_A:
		case CTRL_D:
		case CTRL_F:
			StuffKey(CTRL_P);
			StuffKey(ch);
			break;
#endif
		case KB_DEL:
		case CTRL_G:
			if (CursorX<CurrentLine->Length)
                        {
				MainFile->DeleteCharacter(CursorX, CurrentLineNumber);
                        }
			else if (CurrentLine->Next)
                        {
				CurrentLine->Wrap=1;
                        }
			Display(JOIN_LINE, CurrentLineNumber);
			break;
		case CTRL_R:
			DrawScreen();
			break;
		case CTRL_W:
                        if (CursorX>0)
                        {
				FoundWord=0;
                                for(i=CursorX-1;i>0;i--)
                                {
					if (CurrentLine->Text[i]!=' ')
                                        {
						FoundWord=1;
                                        }
                                        if (CurrentLine->Text[i]==' ' && FoundWord)
                                        {
						i++;
						break;
					}
				}
				FoundWord=i;
                                for (i=CursorX; i<CurrentLine->Length; i++)
                                {
					CurrentLine->Text[i-(CursorX-FoundWord)]=CurrentLine->Text[i];
					CurrentLine->Color[i-(CursorX-FoundWord)]=CurrentLine->Color[i];
				}
				CurrentLine->Length-=CursorX-FoundWord;
				CursorX=FoundWord;
				ShowOneLine(CursorX, CursorY, ERASE_END);
			}
			break;
		case CTRL_Y:
                        if (CurrentLineNumber+1 < MainFile->FileLines)
                        {
				CursorX=0;
				MainFile->RemoveLine(CurrentLineNumber);
                                if (CurrentLineNumber==MainFile->FileLines)
                                {
					DecreaseCursorY();
					ShowLines(CursorY+1);
                                }
                                else
                                        if (BREditArgs->EnhancedMode==ON)
                                        {
						IO->EANSI_DefineRegion(TopLine+CursorY, DisplayLines+TopLine);
						IO->EANSI_ScrollRegionUp();
						IO->EANSI_DefineRegion(TopLine, DisplayLines+TopLine);
						if (DisplayLines+Page<MainFile->FileLines)
							ShowOneLine(0, DisplayLines, NO_ERASE);
                                        }
                                        else
                                        {
						ShowLines(CursorY);
                                        }
				break;
			}
		case CTRL_X:
			CurrentLine->Length=0;
			CursorX=0;
			IO->MoveTo(CursorX, CursorY+TopLine);
			IO->EraseToEnd();
			break;
		case CTRL_P:
			ch=IO->GetChar();
			if (ch>='0' && ch<='9')
                        {
				Color=(ch-'0');
                        }
			if (ch>='A' && ch<='Z')
                        {
				Color=10+(ch-'A');
                        }
			break;
		default:
                        if (ch>31)
                        {
				MainFile->AddAtX(ch, CursorX, CurrentLineNumber, Color, InsertMode);
				CursorX++;
				if ((CursorX==BREditArgs->MaxColumns-1) ||
                                    (InsertMode && (CurrentLine->Length>=BREditArgs->MaxColumns-1)))
                                    {
					FoundWord=0;
                                        for(i=CurrentLine->Length-1; i>=0; i--)
                                        {
						if (CurrentLine->Text[i]!=' ')
                                                {
							FoundWord=1;
                                                }
                                                if (CurrentLine->Text[i]==' ' && FoundWord)
                                                {
							CursorX--;
							Display(BREAK_LINE, CurrentLineNumber, i+1, NOT_FORCED);
							CursorX++;
							SetCurrentLine();
                                                        if (CursorX > CurrentLine->Length)
                                                        {  // Did cursor get wrapped?
								CursorX-=CurrentLine->Length;
								IncreaseCursorY();
							}
							break;
						}
					}
                                        if (i<0)
                                        {
						CursorX--;
						Display(BREAK_LINE, CurrentLineNumber, CursorX, NOT_FORCED);
						IncreaseCursorY();
						CursorX=1;
					}
                                }
                                else
                                {
					if (InsertMode)
                                        {
						ShowOneLine(CursorX-1, CursorY, NO_ERASE);
                                        }
                                        else
                                        {
						IO->SetColor(Color);
						IO->Output((char)ch);
					}
				}
			}
			break;
		}
	}
}

void EditorClass::SetCurrentLine(void)
{
	CurrentLine=MainFile->GetLine(CurrentLineNumber);
}

#ifdef MACRO_SUPPORT

void EditorClass::StuffKey(char ch)
// Stuffs a key into the keyboard buffer.  Used for macro implementation.
{
	int i;

	disable();
	i = peekb(0, 1052);
	pokeb(0, i + 1024, ch);
	if (i == 60)
		i = 28;
	pokeb(0, 1052, i + 2);
	enable();
}

#endif

#define ADD_TO_ACCEPTABLE(x, desc) AcceptableKeys[NumAcceptable++]=x;  \
							strcat(CommandString, desc);

/*
void EditorClass::BlockCommands()
{
	char AcceptableKeys[20], CommandString[81];
	int NumAcceptable=0;


	strcpy(CommandString("Block: ");
	ADD_TO_ACCEPTABLE('B', "[B]egin ");
	ADD_TO_ACCEPTABLE('E', "[E]nd ");
	ADD_TO_ACCEPTABLE('H', "[H]ide ");
	if (BREditArgs->SysOpMode)
		ADD_TO_ACCEPTABLE('R', "[R]ead");
	if (

	IO->MoveTo(0,0);
	IO->SetColor(1);
}  */

void EditorClass::Display(int Action, int Line, int X, int Type)
{
	int CurNumLines=MainFile->FileLines, LastJoinedLine;

	if (X==NO_PARAMETER)
		X=CursorX;
        switch(Action)
        {
	case BREAK_LINE:
		LastJoinedLine=MainFile->BreakLine(X, Line, Type);
		if (LastJoinedLine<0)      // BreakLine ALWAYS modifies next line at least
			LastJoinedLine=Line+1;
		break;
	case JOIN_LINE:
		LastJoinedLine=MainFile->JoinLines(Line);
		break;
	}
	Line-=Page;
	ShowOneLine(min(X, CursorX), Line, ERASE_END);
	if (LastJoinedLine<0)
		return;
	LastJoinedLine-=Page;  // Put last joined line into page context
	if (BREditArgs->EnhancedMode==ON && CurNumLines!=MainFile->FileLines) {
		if ((CursorY+Page)<MainFile->FileLines-1) {
			IO->EANSI_DefineRegion(LastJoinedLine+TopLine, DisplayLines+TopLine);
			if (CurNumLines < MainFile->FileLines) {
				IO->EANSI_ScrollRegionDown();
				if (BottomLineUsed<DisplayLines)
					BottomLineUsed++;
			} else {
				IO->EANSI_ScrollRegionUp();
				if (DisplayLines+Page<MainFile->FileLines) {
					ShowOneLine(0, DisplayLines, NO_ERASE);
				} else
					BottomLineUsed--;
			}
			IO->EANSI_DefineRegion(TopLine, DisplayLines+TopLine);
		} else
			ShowOneLine(0, Line+1, ERASE_END);
	}
	ShowLines(Line+1, LastJoinedLine);
	if (BREditArgs->EnhancedMode!=ON && CurNumLines!=MainFile->FileLines)
		ShowLines(LastJoinedLine+1, DisplayLines);
}


int EditorClass::CheckForCommand(char *CompareTo)
{
	int i;

	for(i=0;i<strlen(CompareTo);i++)
		if (i>=CurrentLine->Length || toupper(CurrentLine->Text[i])!=CompareTo[i])
			return NO_MATCH;
	CurrentLine->Length=0;
	CursorX=0;
	return MATCH;
}


void EditorClass::IncreaseCursorY(void)
{
	LineStruct *OldCurrentLine;

	OldCurrentLine=CurrentLine;
	CursorY++;
	if (CursorY>=DisplayLines) {
		if (BREditArgs->EnhancedMode==ON) {
			Page++;
			IO->EANSI_ScrollRegionUp();
			if (DisplayLines+Page<MainFile->FileLines)
				ShowOneLine(0, DisplayLines, NO_ERASE);
			else
				BottomLineUsed--;
			CursorY--;
		} else {
			Page+=(DisplayLines-VIEW_LINES);
			CursorY-=(DisplayLines-VIEW_LINES);
			ShowLines(0);
		}
	}
	CurrentLine=OldCurrentLine;
}

void EditorClass::DecreaseCursorY(void)
{
	LineStruct *OldCurrentLine;

	OldCurrentLine=CurrentLine;
		CursorY--;
	if (CursorY<0) {
		if (BREditArgs->EnhancedMode==ON) {
			CursorY=0;
			Page--;
			IO->EANSI_ScrollRegionDown();
			if (BottomLineUsed<DisplayLines)
				BottomLineUsed++;
			ShowOneLine(0, CursorY, NO_ERASE);
		} else {
			Page-=(DisplayLines-VIEW_LINES);
			CursorY+=(DisplayLines-VIEW_LINES);
			if (Page<0) {
				CursorY-=0-Page;
				Page=0;
			}
			ShowLines(0);
		}
	}
	CurrentLine=OldCurrentLine;
}


void EditorClass::DrawScreen(void)
{
	IO->SetColor(0);
	IO->ClearScreen();
	ShowTitleAndBar();
	ShowLines(0);
}

void EditorClass::ShowLines(int FromLine, int StopLine)
{
	int ToLine, i;

	if (StopLine==NO_PARAMETER)
		StopLine=DisplayLines;

	#define LastLine (MainFile->FileLines-Page-1)

	if (BottomLineUsed>LastLine)
		ToLine=BottomLineUsed;
	else
		ToLine=LastLine;
	if (ToLine>StopLine)
		ToLine=StopLine;
	for(i=FromLine;i<=ToLine;i++)
		ShowOneLine(0, i, ERASE_END);
}

void EditorClass::ShowOneLine(int FromChar, int AtLine, int EraseMode)
{
	LineStruct *Line;
	int i;

	Line=MainFile->GetLine(Page+AtLine);
	IO->MoveTo(FromChar, AtLine+TopLine);
	if (Line) {
		for(i=FromChar;i<Line->Length;i++) {
			IO->SetColor(Line->Color[i]);
			IO->Output(Line->Text[i]);
		}
		if (AtLine>BottomLineUsed)
			BottomLineUsed=AtLine;
	}
	switch(EraseMode) {
	case NO_ERASE:
		break;
	case ERASE_END:
		IO->EraseToEnd();
		break;
	case ERASE_SINGLE:
		IO->Output(' ');
		break;
	}
}


void EditorClass::ShowTitleAndBar(void)
{
	int i;

	ShowTitle();
	IO->MoveTo(0, 1);
        IO->SetColor(7);       // 7
	for(i=0;i<(BREditArgs->MaxColumns/2)-1;i++)
                IO->Output('Ä');    // Ä
        IO->Output('Ä');            // Á
	for(i=0;i<(BREditArgs->MaxColumns/2)-1;i++)
                IO->Output('Ä');   // Ä
	IO->SetColor(0);
	TopLine=2;
	IO->EANSI_DefineRegion(TopLine, DisplayLines+TopLine);
}

void EditorClass::ShowMessage(char *Message)
{
	IO->MoveTo(0,0);
	IO->SetColor(2);
	IO->Output(Message);
	IO->Output(" - Hit a key");
	IO->EraseToEnd();
	IO->GetChar();
	ShowTitle();
	IO->SetColor(Color);
	IO->MoveTo(CursorX, CursorY+TopLine);
	IO->EraseToEnd();
}



void EditorClass::ShowTitle()
{
	IO->SetColor(1);
	IO->MoveTo(0,0);
	IO->Output("[ BREdit v");
	IO->Output(VERSION);
	IO->Output(" ù /H for help ");
	if (QuotesAvailable)
		IO->Output("ù /Q to quote ");
	IO->Output("ù Max ");
	IO->Output(BREditArgs->MaxLines);
	IO->Output(" lines ]");
	ShowInsertStatus();
}


void EditorClass::ShowInsertStatus(void)
{
	IO->MoveTo(75, 0);
	if (InsertMode) {
		IO->SetColor(2);
		IO->Output("Ins");
	} else
		IO->Output("   ");
}

void EditorClass::ShowInfoLine(char *Item, char *Setting, int &Line)
{
	IO->MoveTo(0, Line);
	IO->SetColor(0);
	IO->Output(Item);
	IO->MoveTo(30, Line);
	IO->SetColor(5);
	IO->Output(Setting);
	Line++;
}


void EditorClass::ShowInfo(void)
{
	char s[81];
	int i;

	IO->ClearScreen();
	IO->SetColor(1);
	IO->MoveTo(0,0);
	IO->Output("BREdit Version ");
	IO->Output(VERSION);
	IO->Output("  (C)Copyright 1999 by BRWare");
	IO->MoveTo(0,1);
	IO->SetColor(7);
	for(i=0;i<79;i++)
		IO->Output('Ä');
	i=3;
	ShowInfoLine("Enhanced Mode", (BREditArgs->EnhancedMode==ON) ? "ON" :
							 "OFF (Try using VT102 Emulation)", i);
	ShowInfoLine("SysOp Mode", (BREditArgs->SysOpMode) ? "ON" : "OFF", i);
	sprintf(s, "%d lines, %d columns", BREditArgs->MaxLines, BREditArgs->MaxColumns);
	ShowInfoLine("Limits", s, i);
	ShowInfoLine("Quotes", QuotesAvailable ? "Available" : "Not Available", i);
	if (BREditArgs->TitleChangeable) {
		ShowInfoLine("Title", BREditArgs->Title, i);
		ShowInfoLine("Destination", BREditArgs->Destination, i);
		ShowInfoLine("Tag Line", (BREditArgs->EnableTag) ? "Enabled" : "Disabled", i);
	} else {
		ShowInfoLine("Message Information", "Not Available", i);
	}
        ShowInfoLine("DoEnahancedIO", (BREditArgs->DoEnhancedIO) ? "ON" : "OFF", i);
	IO->GetChar();
}

