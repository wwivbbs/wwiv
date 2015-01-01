/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*           Copyright (C)2014-2015 WWIV Software Services                */
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
#include "core/wwivport.h"
#include "core_test/file_helper.h"

#include "gtest/gtest.h"
#include "sdk/filenames.h"
#include "sdk/vardec.h"

using namespace std;
using namespace wwiv::strings;


SdkHelper::SdkHelper() : saved_dir_(File::current_directory()), root_(files_.CreateTempFilePath("bbs")) {
  const string msgs = CreatePath("msgs");
  const string gfiles = CreatePath("gfiles");
  const string menus = CreatePath("menus");
  data_ = CreatePath("data");
  const string dloads = CreatePath("dloads");

  configrec c;
  strcpy(c.msgsdir, msgs.c_str());
  strcpy(c.gfilesdir, gfiles.c_str());
  strcpy(c.menudir, menus.c_str());
  strcpy(c.datadir, data_.c_str());
  strcpy(c.dloadsdir, dloads.c_str());

  File cfile(root_, CONFIG_DAT);
  if (!cfile.Open(File::modeBinary|File::modeCreateFile|File::modeWriteOnly)) {
    throw "failed to create config.dat";
  }
  cfile.Write(&c, sizeof(configrec));
  cfile.Close();
}

std::string SdkHelper::CreatePath(const string& name) {
  const string  path = files_.CreateTempFilePath(StrCat("bbs/", name));
  File::mkdirs(path);
  return path;
}

SdkHelper::~SdkHelper() {
  chdir(saved_dir_.c_str());
}
