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
#ifndef FILEVIEW_DOT_H
#define FILEVIEW_DOT_H

#include "FEDITIO.H"
#include "FILEMGR.H"

class FileView
{
	int HelpBrowseLine, BottomLineUsed;
	char Title[81];
	IOClass *IO;
	BREditArguments *BREditArgs;

	int GetHelpKey();
	void ShowTopInfo();
	void ShowHelpPage(EditorFileClass *File);
	void ShowLine(LineStruct *Line);

public:
	FileView(IOClass *InputOutput, BREditArguments *BREditArguments, EditorFileClass *File, char *Description);
};

#endif