/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.0x                             */
/*               Copyright (C)2014, WWIV Software Services                */
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
/**************************************************************************/
#include "sdk/config.h"

#include "core/file.h"
#include "sdk/filenames.h"
#include "sdk/vardec.h"

namespace wwiv {
namespace sdk {

Config::Config() : initialized_(false), config_(new configrec{}) {
  //TODO(rushfan): Do we need to change directories first if env vars are set?
  File configFile(CONFIG_DAT);
  int file_mode = File::modeReadOnly | File::modeBinary;
  if (!configFile.Open(file_mode)) {
    std::clog << CONFIG_DAT << " NOT FOUND.\r\n";
  }
  configFile.Read(config_.get(), sizeof(configrec));
  configFile.Close();
  initialized_ = true;
}

Config::~Config() {}


}
}
