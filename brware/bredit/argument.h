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
#ifndef FEDITARG_DOT_H

class BREditArguments
{
public:
	int ScreenLines, MaxColumns, MaxLines, EnhancedMode,
			DoEnhancedIO, WWIVColorChanges, SysOpMode,
			TitleChangeable, AnonymousStatus, EnableTag;
	char Title[81], Destination[81];
	char *EditFilename, HelpFile[81];
	BREditArguments(int argc, char **argv);
	~BREditArguments(void);
};

#define OFF  1
#define ON   2
#define AUTO 3

#define ANONYMOUS_UNDEFINED 0
#define ANONYMOUS_YES 1
#define ANONYMOUS_NO 2

#define FEDITARG_DOT_H

#endif
