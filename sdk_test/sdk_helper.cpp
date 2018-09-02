/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*           Copyright (C)2014-2017, WWIV Software Services               */
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
#include "sdk_test/sdk_helper.h"
#include <sstream>

#include <algorithm>
#include <iostream>
#include <string>

#include "core/file.h"
#include "core/strings.h"
#include "core/version.h"
#include "core/wwivport.h"
#include "core_test/file_helper.h"

#include "gtest/gtest.h"
#include "core/datetime.h"
#include "sdk/filenames.h"
#include "sdk/vardec.h"

using namespace std;
using namespace wwiv::core;
using namespace wwiv::strings;

static statusrec_t create_status() {
  statusrec_t s = {};
  memset(&s, 0, sizeof(statusrec_t));
  const string now(time_t_to_mmddyy(time_t_now()));
  to_char_array(s.date1, now);
  strcpy(s.date2, "00/00/00");
  strcpy(s.date3, "00/00/00");
  strcpy(s.log1, "000000.log");
  strcpy(s.log2, "000000.log");
  to_char_array(s.gfiledate, now);
  s.callernum = 65535;
  s.qscanptr = 2;
  s.net_bias = 0.001f;
  s.net_req_free = 3.0;
  return s;
}

SdkHelper::SdkHelper() : saved_dir_(File::current_directory()), root_(files_.CreateTempFilePath("bbs")) {
  data_ = CreatePath("data");
  msgs_ = CreatePath("msgs");
  gfiles_ = CreatePath("gfiles");
  menus_ = CreatePath("menus");
  scripts_ = CreatePath("scripts");
  dloads_ = CreatePath("dloads");

  {
    configrec c = {};
    to_char_array(c.msgsdir, "msgs");
    to_char_array(c.gfilesdir, "gfiles");
    to_char_array(c.menudir, "menus");
    to_char_array(c.datadir, "data");
    to_char_array(c.dloadsdir, "dloads");

    // Add header version.
    // TODO(rushfan): This really should all be done in the Config class and also used
    // by wwivconfig from there.
    configrec_header_t h = {};
    h.config_revision_number = 0;
    h.config_size = sizeof(configrec);
    c.userreclen = sizeof(userrec);
    h.written_by_wwiv_num_version = wwiv_num_version;
    to_char_array(h.signature, "WWIV");
    c.header.header = h;

    File cfile(FilePath(root_, CONFIG_DAT));
    if (!cfile.Open(File::modeBinary|File::modeCreateFile|File::modeWriteOnly)) {
      throw std::runtime_error("failed to create config.dat");
    }
    cfile.Write(&c, sizeof(configrec));
    cfile.Close();
  }

  {
    File sfile(FilePath(data_, STATUS_DAT));
    if (!sfile.Open(File::modeBinary | File::modeCreateFile | File::modeWriteOnly)) {
      throw std::runtime_error("failed to create status.dat");
    }
    statusrec_t s = create_status();
    sfile.Write(&s, sizeof(statusrec_t));
    sfile.Close();
  }
}

std::string SdkHelper::CreatePath(const string& name) {
  const string  path = files_.CreateTempFilePath(StrCat("bbs/", name));
  File::mkdirs(path);
  return path;
}

SdkHelper::~SdkHelper() {
  chdir(saved_dir_.c_str());
}
