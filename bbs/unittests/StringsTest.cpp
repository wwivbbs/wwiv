/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)2008, WWIV Software Services                  */
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
#include "StringsTest.h"

using std::cout;
using std::endl;
using std::ostringstream;
using std::string;

CPPUNIT_TEST_SUITE_REGISTRATION( StringsTest );


void StringsTest::testStripColors()
{
    CPPUNIT_ASSERT_EQUAL( string(""), stripcolors(string("")) );
    CPPUNIT_ASSERT_EQUAL( string("|"), stripcolors(string("|")) );
    CPPUNIT_ASSERT_EQUAL( string("|0"), stripcolors(string("|0")) );
    CPPUNIT_ASSERT_EQUAL( string("12345"), stripcolors(string("12345")) );
    CPPUNIT_ASSERT_EQUAL( string("abc"), stripcolors(string("abc")) );
    CPPUNIT_ASSERT_EQUAL( string("1 abc"), stripcolors(string("\x031 abc")) );
    CPPUNIT_ASSERT_EQUAL( string("\x03 abc"), stripcolors(string("\x03 abc")) );
    CPPUNIT_ASSERT_EQUAL( string("abc"), stripcolors(string("|15abc")) );
}

void StringsTest::testProperize() 
{
    CPPUNIT_ASSERT_EQUAL( string("Rushfan"), properize( string("rushfan") ) );
    CPPUNIT_ASSERT_EQUAL( string("Rushfan"), properize( string("rUSHFAN") ) );
    CPPUNIT_ASSERT_EQUAL( string(""), properize( string("") ) );
    CPPUNIT_ASSERT_EQUAL( string(" "), properize( string(" ") ) );
    CPPUNIT_ASSERT_EQUAL( string("-"), properize( string("-") ) );
    CPPUNIT_ASSERT_EQUAL( string("."), properize( string(".") ) );
    CPPUNIT_ASSERT_EQUAL( string("R"), properize( string("R") ) );
    CPPUNIT_ASSERT_EQUAL( string("R"), properize( string("r") ) );
    CPPUNIT_ASSERT_EQUAL( string("Ru"), properize( string("RU") ) );
    CPPUNIT_ASSERT_EQUAL( string("R.U"), properize( string("r.u") ) );
    CPPUNIT_ASSERT_EQUAL( string("R U"), properize( string("r u") ) );
    CPPUNIT_ASSERT_EQUAL( string("Rushfan"), properize( string("Rushfan") ) );
}


#endif
