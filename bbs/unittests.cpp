/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2007, WWIV Software Services             */
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
#include "wwiv.h"
#include "WOutStreamBuffer.h"
#include "WStringUtils.h"

using std::string;
using std::cout;
using std::endl;

void simple();

bool RunUnitTests( const string& suiteName ) 
{
	cout << "Running Unit Test Suite: " << suiteName << endl;
	simple();
	return true;
}


void simple() {
	cout << "Simple" << endl;
	string in = "foo %1 %c %2 %k";
	string out;
	stuff_in(out, in, "one", "two", "", "", "");
	cout << out;
}
