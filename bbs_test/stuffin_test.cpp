/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*           Copyright (C)2007-2021, WWIV Software Services               */
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
#include "bbs_test/bbs_helper.h"
#include "core/file.h"
#include "core/strings.h"
#include "sdk/filenames.h"

#include <string>

using namespace wwiv::core;
using namespace wwiv::strings;

class StuffInTest : public testing::Test {
protected:
  void SetUp() override {
    helper.SetUp();
    a()->sess().incom(false);
    a()->modem_speed_ = 0;
  }

public:
  static std::string t(const std::string& name) {
    return FilePath(a()->sess().dirs().temp_directory(), name).string();
  }

  BbsHelper helper;
};

TEST_F(StuffInTest, SimpleCase) {
  wwiv::bbs::CommandLine cl("foo %1 %c %2 %k");
  cl.args("one", "two");

  std::ostringstream os;
  os << "foo one " << t(DROPFILE_CHAIN_TXT) << " two " << helper.gfiles() << COMMENT_TXT;
  const auto expected = os.str();

  EXPECT_EQ(expected, cl.cmdline());
}

TEST_F(StuffInTest, Empty) {
  wwiv::bbs::CommandLine actual("");
  EXPECT_TRUE(actual.empty());
}

// Param     Description                       Example
// ---------------------------------------------------------------------
//  %%       A single '%'                      "%"
//  %1-%5    Specified passed-in parameter
TEST_F(StuffInTest, AllNumbers) {
  wwiv::bbs::CommandLine cl("%0%1%2%3%4%5%6%%");
  cl.args("1", "2", "3", "4", "5");
  const std::string expected = "12345%";

  EXPECT_EQ(expected, cl.cmdline());
}

// Param     Description                       Example
// ---------------------------------------------------------------------
//  %A       callinfo full pathname            "c:\wwiv\temp\callinfo.bbs"
//  %C       chain.txt full pathname           "c:\wwiv\temp\chain.txt"
//  %D       doorinfo full pathname            "c:\wwiv\temp\dorinfo1.def"
//  %E       door32.sys full pathname          "C:\wwiv\temp\door32.sys"    string in = "foo %1 %c
//  %2 %k"; %O       pcboard full pathname             "c:\wwiv\temp\pcboard.sys" %R       door full
//  pathname                "c:\wwiv\temp\door.sys"
TEST_F(StuffInTest, AllDropFiles) {
  wwiv::bbs::CommandLine actual_lower("%a %c %d %e %o %r ");
  wwiv::bbs::CommandLine actual_upper("%A %C %D %E %O %R ");

  std::ostringstream expected;
  expected << t("callinfo.bbs") << " " << t(DROPFILE_CHAIN_TXT) << " " << t("dorinfo1.def") << " "
           << t("door32.sys") << " " << t("pcboard.sys") << " " << t("door.sys") << " ";

  EXPECT_EQ(expected.str(), actual_lower.cmdline());
  EXPECT_EQ(expected.str(), actual_upper.cmdline());
}

// Param     Description                       Example
// ---------------------------------------------------------------------
//  %N       Instance number                   "1"
//  %P       Com port number                   "1"
TEST_F(StuffInTest, PortAndNode) {
  a()->sess().incom(false);
  wwiv::bbs::CommandLine c0("%P");
  EXPECT_EQ(std::string("0"), c0.cmdline());

  a()->sess().incom(true);
  wwiv::bbs::CommandLine c1("%P");
  EXPECT_EQ(std::string("1"), c1.cmdline());

  wwiv::bbs::CommandLine cn("%N");
  EXPECT_EQ(std::string("42"), cn.cmdline());
}

// Param     Description                       Example
// ---------------------------------------------------------------------
//  %M       Modem baud rate                   "14400"
//  %S       Com port baud rate                "38400"
TEST_F(StuffInTest, Speeds) {
  {
    wwiv::bbs::CommandLine cm("%M");
    EXPECT_EQ(std::string("0"), cm.cmdline());
    wwiv::bbs::CommandLine cs("%S");
    EXPECT_EQ(std::string("0"), cs.cmdline());
  }

  {
    a()->modem_speed_ = 38400;
    wwiv::bbs::CommandLine cm("%M");
    EXPECT_EQ(std::string("38400"), cm.cmdline());
    wwiv::bbs::CommandLine cs("%S");
    EXPECT_EQ(std::string("38400"), cs.cmdline());
  }
}
