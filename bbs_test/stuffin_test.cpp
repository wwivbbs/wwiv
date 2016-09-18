/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*           Copyright (C)2007-2016, WWIV Software Services               */
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

#include "bbs/stuffin.h"
#include "bbs/vars.h"
#include "bbs_test/bbs_helper.h"
#include "core/strings.h"
#include "sdk/filenames.h"

using std::cout;
using std::endl;
using std::ostringstream;
using std::string;

using namespace wwiv::strings;


class StuffInTest : public testing::Test {
protected:
    virtual void SetUp() {
      helper.SetUp();
      incom = false;
      com_speed = 0;
      modem_speed = 0;
    }

public:
    const std::string t(const std::string name) {
      return StrCat(session()->temp_directory(), name);
    }

    BbsHelper helper;
private:
  string gfiles_dir_;
};

TEST_F(StuffInTest, SimpleCase) {
    const string actual = stuff_in("foo %1 %c %2 %k", "one", "two", "", "", "");

    ostringstream os;
    os << "foo one " << t("chain.txt")
      << " two " << syscfg.gfilesdir << COMMENT_TXT;
    string expected = os.str();

    EXPECT_EQ(expected, actual);
}

TEST_F(StuffInTest, Empty) {
    const string actual = stuff_in("", "", "", "", "", "");
    EXPECT_EQ(0, actual.length());
}

// Param     Description                       Example
// ---------------------------------------------------------------------
//  %%       A single '%'                      "%"
//  %1-%5    Specified passed-in parameter
TEST_F(StuffInTest, AllNumbers) {
    const string actual = stuff_in("%0%1%2%3%4%5%6%%", "1", "2", "3", "4", "5");
    string expected = "12345%";

    EXPECT_EQ(expected, actual);
}

// Param     Description                       Example
// ---------------------------------------------------------------------
//  %A       callinfo full pathname            "c:\wwiv\temp\callinfo.bbs"
//  %C       chain.txt full pathname           "c:\wwiv\temp\chain.txt"
//  %D       doorinfo full pathname            "c:\wwiv\temp\dorinfo1.def"
//  %E       door32.sys full pathname          "C:\wwiv\temp\door32.sys"    string in = "foo %1 %c %2 %k";
//  %O       pcboard full pathname             "c:\wwiv\temp\pcboard.sys"
//  %R       door full pathname                "c:\wwiv\temp\door.sys"
TEST_F(StuffInTest, AllDropFiles) {
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

// Param     Description                       Example
// ---------------------------------------------------------------------
//  %N       Instance number                   "1"
//  %P       Com port number                   "1"
TEST_F(StuffInTest, PortAndNode) {
  incom = false;
  EXPECT_EQ(string("0"), stuff_in("%P", "", "", "", "", ""));
    
  incom = true;
  EXPECT_EQ(string("1"), stuff_in("%P", "", "", "", "", ""));

  EXPECT_EQ(string("42"), stuff_in("%N", "", "", "", "", ""));
}

// Param     Description                       Example
// ---------------------------------------------------------------------
//  %M       Modem baud rate                   "14400"
//  %S       Com port baud rate                "38400"
TEST_F(StuffInTest, Speeds) {
  EXPECT_EQ(string("0"), stuff_in("%M", "", "", "", "", ""));
  EXPECT_EQ(string("0"), stuff_in("%S", "", "", "", "", ""));

  modem_speed = 38400;
  EXPECT_EQ(string("38400"), stuff_in("%M", "", "", "", "", ""));

  com_speed = 38400;
  EXPECT_EQ(string("38400"), stuff_in("%S", "", "", "", "", ""));

  com_speed = 115200;
  EXPECT_EQ(string("115200"), stuff_in("%S", "", "", "", "", ""));
}


