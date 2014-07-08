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
#include "gtest/gtest.h"

#include "bbs/vars.h"

using std::cout;
using std::endl;
using std::ostringstream;
using std::string;

//TODO(rushfan): create stuffin.h
const string stuff_in(const std::string commandline, const std::string arg1,
		       const std::string arg2, const std::string arg3, const std::string arg4, const std::string arg5);

class StuffInTest : public testing::Test {
protected:
    virtual void SetUp() {
        incom = false;
        com_speed = 0;
        modem_speed = 0;
        syscfgovr.primaryport = 0;
        syscfgovr.tempdir[0] = 0;
        gfiles_dir_ = "C:\\temp";
        syscfg.gfilesdir = const_cast<char*>(gfiles_dir_.c_str());
    }
public:
    const std::string t(const std::string name) {
        ostringstream os;
        os << syscfgovr.tempdir << name;
        return string(os.str());
    }
private:
  string gfiles_dir_;
};

TEST_F(StuffInTest, SimpleCase) {
    const string actual = stuff_in("foo %1 %c %2 %k", "one", "two", "", "", "");

    ostringstream expected;
    expected << "foo one " << t("chain.txt") << " two " << syscfg.gfilesdir << "comment.txt";

    EXPECT_EQ(expected.str(), actual);
}


TEST_F(StuffInTest, Empty) {
    const string actual = stuff_in("", "", "", "", "", "");
    EXPECT_EQ(0, actual.length());
}

TEST_F(StuffInTest, AllNumbers) {
// Param     Description                       Example
// ---------------------------------------------------------------------
//  %%       A single '%'                      "%"
//  %1-%5    Specified passed-in parameter
    const string actual = stuff_in("%0%1%2%3%4%5%6%%", "1", "2", "3", "4", "5");
    string expected = "12345%";

    EXPECT_EQ(expected, actual);
}

TEST_F(StuffInTest, AllDropFiles) {
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

    EXPECT_EQ(expected.str(), actual_lower);
    EXPECT_EQ(expected.str(), actual_upper);
}

TEST_F(StuffInTest, PortAndNode) {
// Param     Description                       Example
// ---------------------------------------------------------------------
//  %N       Instance number                   "1"
//  %P       Com port number                   "1"
    EXPECT_EQ(string("0"), stuff_in("%P", "", "", "", "", ""));
    
    incom = true;
    syscfgovr.primaryport = 1;
    EXPECT_EQ(string("1"), stuff_in("%P", "", "", "", "", ""));

    // TODO(Rushfan): Figure out how to get GetApplication() working in tests and
    // reenable this one.
//    EXPECT_EQ(string("1"), stuff_in("%N", "", "", "", "", ""));
}

TEST_F(StuffInTest, Speeds) {
// Param     Description                       Example
// ---------------------------------------------------------------------
//  %M       Modem baud rate                   "14400"
//  %S       Com port baud rate                "38400"
    EXPECT_EQ(string("0"), stuff_in("%M", "", "", "", "", ""));
    EXPECT_EQ(string("0"), stuff_in("%S", "", "", "", "", ""));

    modem_speed = 38400;
    EXPECT_EQ(string("38400"), stuff_in("%M", "", "", "", "", ""));

    com_speed = 38400;
    EXPECT_EQ(string("38400"), stuff_in("%S", "", "", "", "", ""));

    com_speed = 1;
    EXPECT_EQ(string("115200"), stuff_in("%S", "", "", "", "", ""));
}


