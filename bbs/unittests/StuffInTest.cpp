/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)2007, WWIV Software Services                  */
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
#include "StuffInTest.h"

using std::cout;
using std::endl;
using std::ostringstream;
using std::string;

CPPUNIT_TEST_SUITE_REGISTRATION( StuffInTest );

void StuffInTest::testSimpleCase() 
{
    const string actual = stuff_in("foo %1 %c %2 %k", "one", "two", "", "", "");

    ostringstream expected;
    expected << "foo one " << t("chain.txt") << " two " << syscfg.gfilesdir << "comment.txt";

    CPPUNIT_ASSERT_EQUAL(expected.str(), actual);
}


void StuffInTest::testEmpty() 
{
    const string actual = stuff_in("", "", "", "", "", "");

    CPPUNIT_ASSERT_EQUAL(string(""), actual);
}

void StuffInTest::testAllNumbers() 
{
// Param     Description                       Example
// ---------------------------------------------------------------------
//  %%       A single '%'                      "%"
//  %1-%5    Specified passed-in parameter
    const string actual = stuff_in("%0%1%2%3%4%5%6%%", "1", "2", "3", "4", "5");
    string expected = "12345%";

    CPPUNIT_ASSERT_EQUAL(expected, actual);
}

void StuffInTest::testAllDropFiles() 
{
// Param     Description                       Example
// ---------------------------------------------------------------------
//  %A       callinfo full pathname            "c:\wwiv\temp\callinfo.bbs"
//  %C       chain.txt full pathname           "c:\wwiv\temp\chain.txt"
//  %D       doorinfo full pathname            "c:\wwiv\temp\dorinfo1.def"
//  %E       door32.sys full pathname          "C:\wwiv\temp\door32.sys"    string in = "foo %1 %c %2 %k";
//  %O       pcboard full pathname             "c:\wwiv\temp\pcboard.sys"
//  %R       door full pathname                "c:\wwiv\temp\door.sys"
    const string actual_lower = stuff_in("%a %c %d %e %o %r ", "", "", "", "", "");
    const string actual_upper = stuff_in("%A %C %D %E %O %R ", "", "", "", "", "");

    ostringstream expected;
    expected << t("callinfo.bbs")   << " " 
             << t("chain.txt")      << " " 
             << t("dorinfo1.def")   << " " 
             << t("door32.sys")     << " "
             << t("pcboard.sys")    << " "
             << t("door.sys")       << " ";

    CPPUNIT_ASSERT_EQUAL(expected.str(), actual_lower);
    CPPUNIT_ASSERT_EQUAL(expected.str(), actual_upper);
}

void StuffInTest::testPortAndNode() 
{
// Param     Description                       Example
// ---------------------------------------------------------------------
//  %N       Instance number                   "1"
//  %P       Com port number                   "1"
    CPPUNIT_ASSERT_EQUAL(string("0"), stuff_in("%P", "", "", "", "", ""));
    
    incom = true;
    syscfgovr.primaryport = 1;
    CPPUNIT_ASSERT_EQUAL(string("1"), stuff_in("%P", "", "", "", "", ""));

    CPPUNIT_ASSERT_EQUAL(string("1"), stuff_in("%N", "", "", "", "", ""));
}

void StuffInTest::testSpeeds()
{
// Param     Description                       Example
// ---------------------------------------------------------------------
//  %M       Modem baud rate                   "14400"
//  %S       Com port baud rate                "38400"
    CPPUNIT_ASSERT_EQUAL(string("0"), stuff_in("%M", "", "", "", "", ""));
    CPPUNIT_ASSERT_EQUAL(string("0"), stuff_in("%S", "", "", "", "", ""));

    modem_speed = 38400;
    CPPUNIT_ASSERT_EQUAL(string("38400"), stuff_in("%M", "", "", "", "", ""));

    com_speed = 38400;
    CPPUNIT_ASSERT_EQUAL(string("38400"), stuff_in("%S", "", "", "", "", ""));

    com_speed = 1;
    CPPUNIT_ASSERT_EQUAL(string("115200"), stuff_in("%S", "", "", "", "", ""));
}

void StuffInTest::tearDown()
{
    incom = false;
    com_speed = 0;
    modem_speed = 0;
    syscfgovr.primaryport = 0;
}

const std::string StuffInTest::t(const std::string name)
{
    ostringstream os;
    os << syscfgovr.tempdir << name;
    return string(os.str());
}

#endif
