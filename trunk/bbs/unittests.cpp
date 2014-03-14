/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2014, WWIV Software Services             */
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
#if defined ( _DEBUG )

#include "wwiv.h"
#include "WOutStreamBuffer.h"
#include "WStringUtils.h"
//#include "unittests/XferTest.h"
//#include "unittests/StuffInTest.h"
//#include "unittests/StringsTest.h"

using std::string;
using std::cout;
using std::endl;

bool RunUnitTests( const string& suiteName ) {
	GetSession()->localIO()->LocalCls();
	if (!suiteName.empty()) {
		cout << "Running Unit Test Suite: " << suiteName << endl;
	}

	/*
	    CppUnit::TextUi::TestRunner runner;
	    runner.addTest( XferTest::suite() );
	    runner.addTest( StuffInTest::suite() );
	    runner.addTest( StringsTest::suite() );
	    runner.run();
	*/

	return true;
}


#endif // _DEBUG