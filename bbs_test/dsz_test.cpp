/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*           Copyright (C)2007-2022, WWIV Software Services               */
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

#include "bbs/dsz.h"
#include "core/log.h"
#include "core/textfile.h"
#include "core_test/file_helper.h"
#include "sdk/files/files.h"
#include <string>

TEST(DszTest, Smoke) {
  FileHelper h;
  const std::string loglines=R"(Z  46532 38400 bps 3324 cps   0 errors    66 1024 DSZ.COM 1177
Z 124087 19200 bps 1880 cps   0 errors     6 1024 MXY.TMP 1177
S 217837 57600 bps 5050 cps   0 errors     0 1024 MPT110.ZIP MPt
)";
  std::vector<std::string> fn_e{"DSZ.COM", "MXY.TMP", "MPT110.ZIP"};
  std::vector<int> cps_e{3324, 1880, 5050};
  const auto path = h.CreateTempFile("dsz.log", loglines);

  TextFile tf(path, "rt");
  auto lines = tf.ReadFileIntoVector();
  for (const auto& l : lines) {
    LOG(INFO) << l;
  }

  int dn = 0, ul = 0, e = 0;
  std::vector<std::string> fn_a;
  std::vector<int> cps_a;
  ProcessDSZLogFile(path.string(), [&](dsz_logline_t t, std::string fn, int cps)
  {
    fn_a.push_back(fn);
    cps_a.push_back(cps);
    switch (t) {
    case dsz_logline_t::download:
      ++dn;
      break;
    case dsz_logline_t::upload:
      ++ul;
      break;
    case dsz_logline_t::error:
      ++e;
      break;
    }
  });
  EXPECT_EQ(1, dn);
  EXPECT_EQ(2, ul);
  for (int i=0; i < 3; i++) {
    EXPECT_EQ(fn_a.at(i), wwiv::sdk::files::align(fn_e.at(i)));
    EXPECT_EQ(cps_a.at(i), cps_e.at(i));
  }
}
