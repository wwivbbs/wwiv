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
#ifndef __INCLUDED_STRINGS_TEST_H__
#define __INCLUDED_STRINGS_TEST_H__

#ifdef _MSC_VER
#pragma once
#endif

#include "cppunit/extensions/HelperMacros.h"
#include "cppunit/TestCase.h"
#include "cppunit/ui/text/TestRunner.h"
#include "cppunit/TestCaller.h"

class StringsTest : public CppUnit::TestCase {
   CPPUNIT_TEST_SUITE( StringsTest );
   CPPUNIT_TEST( testStripColors );
   CPPUNIT_TEST( testProperize );
   CPPUNIT_TEST_SUITE_END( );

protected:
    void testStripColors();
    void testProperize();
};

#endif
