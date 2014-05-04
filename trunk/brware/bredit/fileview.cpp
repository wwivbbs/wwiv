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
#include "fileview.h"

#define HELP_TOP_LINE 2
#define HELP_DISPLAY_LINES ((BREditArgs->ScreenLines-1)-HELP_TOP_LINE)


void FileView::ShowTopInfo(void)
{
	IO->MoveTo(0,0);
	IO->EraseToEnd();
	IO->SetColor(1);
	IO->Output(Title);
	IO->Output(" - Use arrows to move up and down, any other key to exit");
}


void FileView::ShowLine(LineStruct *Line)
{
	int i;

	if (Line) {
		i=0;
		if (strnicmp(Line->Text, "/c:", 3)==0) {
			IO->SetColor(0);
			for(i=0;i<40-(Line->Length-3)/2;i++)
				IO->Output(' ');
			i=3;
		}
		for(;i<Line->Length;i++) {
			IO->SetColor(Line->Color[i]);
			IO->Output(Line->Text[i]);
		}
	}
	IO->EraseToEnd();
}


void FileView::ShowHelpPage(EditorFileClass *File)
{
	int i;
	LineStruct *Line;

	Line=File->GetLine(HelpBrowseLine);
	for(i=0;i<=HELP_DISPLAY_LINES;i++) {
		if (Line || i<=BottomLineUsed) {
			IO->MoveTo(0, i+HELP_TOP_LINE);
			ShowLine(Line);
			if (Line)
				BottomLineUsed=i;
			File->Next(Line);
		}
	}
}

int FileView::GetHelpKey()
{
	int ch;

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
			default:
				return(ch);
			}
		}
		return(ch);
	default:
		return(ch);
	}
}



FileView::FileView(IOClass *InputOutput, BREditArguments *FA, EditorFileClass *File, char *Description)
{
	IO=InputOutput;
	BREditArgs=FA;
   strcpy(Title, Description);
	HelpBrowseLine=0;
	BottomLineUsed=0;
	IO->SetColor(0);
	IO->ClearScreen();
	ShowTopInfo();
	IO->SetColor(7);
	IO->MoveTo(0, 1);
	int i;
	for(i=1;i<BREditArgs->MaxColumns;i++)
		IO->Output('Ä');
	ShowHelpPage(File);
	if (BREditArgs->EnhancedMode==ON)
		IO->EANSI_DefineRegion(HELP_TOP_LINE, HELP_DISPLAY_LINES+HELP_TOP_LINE);
	int Done=0;
	while(!Done) {
		switch(GetHelpKey()) {
		case KB_PGUP:
			if (HelpBrowseLine>0) {
				HelpBrowseLine-=HELP_DISPLAY_LINES-3;
				if (HelpBrowseLine<0)
					HelpBrowseLine=0;
				ShowHelpPage(File);
			}
			break;
		case KB_PGDN:
			if (HelpBrowseLine+HELP_DISPLAY_LINES < File->FileLines-1) {
				HelpBrowseLine+=HELP_DISPLAY_LINES-3;
				if (HelpBrowseLine+HELP_DISPLAY_LINES >= File->FileLines)
					HelpBrowseLine=File->FileLines-1-HELP_DISPLAY_LINES;
				ShowHelpPage(File);
			}
			break;
					 // NOTE: EnhancedMode must be on for these keys   //
					 // to be generated.  InputNumber translates these //
					 // keys into PGUP and PGDN for non enhanced mode  //
					 // users.                                         //
		case KB_UP:
			if (HelpBrowseLine>0) {
				HelpBrowseLine--;
				IO->EANSI_ScrollRegionDown();
				IO->MoveTo(0, HELP_TOP_LINE);
				ShowLine(File->GetLine(HelpBrowseLine));
			}
			break;
		case KB_DOWN:
			if (HelpBrowseLine+HELP_DISPLAY_LINES < File->FileLines-1) {
				HelpBrowseLine++;
				IO->EANSI_ScrollRegionUp();
				IO->MoveTo(0, HELP_TOP_LINE+HELP_DISPLAY_LINES);
				ShowLine(File->GetLine(HelpBrowseLine+HELP_DISPLAY_LINES));
			}
			break;
		default:
			Done=1;
			break;
		}
	}
}




