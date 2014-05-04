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
#ifndef QUOTE_DOT_H

#include "FILEMGR.H"
#include "FEDITIO.H"
#include "ARGUMENT.H"
#include <dos.h>
#include <dir.h>
#include <string.h>
#include <ctype.h>

class QuoteClass
{
	char *QuoteFileName;
	int QuoteBrowseLine, BottomLineUsed;
	IOClass *IO;
	BREditArguments *BREditArgs;
	EditorFileClass *MainFile;
	int InputNumber(char *s);
	void HighlightInput(int Number, int State, char *);


public:
	int QuotesAvailable;
	void ShowQuotePage(EditorFileClass *QuoteFile);
	void ShowLine(LineStruct *Line);

	void Quote(int LineNumber);
	void ShowTopInfo(void);
	QuoteClass(char *QuoteFileName, IOClass *InputOutput, EditorFileClass *MainEditorFile, BREditArguments *BREditArgs);
};

#define QUOTE_DOT_H

#endif
