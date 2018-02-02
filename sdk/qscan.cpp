/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*               Copyright (C)2018, WWIV Software Services                */
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
#include "sdk/qscan.h"

#include <exception>
#include <stdexcept>
#include <string>

using std::endl;
using std::string;

namespace wwiv {
namespace sdk {

UserQScan::UserQScan(const std::string& filename, int user_number, int qscan_length,
  int max_subs, int max_dirs)
  : file_(filename), qscan_(std::make_unique<uint32_t[]>(qscan_length)) {
  open_ = file_.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile);
  auto pos = user_number * qscan_length;
  if (pos + qscan_length <= file_.length()) {
    file_.Seek(pos, File::Whence::begin);
    file_.Read(qscan_.get(), qscan_length);
  }
  else {
    // Create new empty qscan record.
    memset(qscan_.get(), 0, qscan_length);
    *qscan_.get() = 999;
    memset(qscan_.get() + 1, 0xff, ((max_dirs + 31) / 32) * 4);
    memset(qscan_.get() + 1 + (max_subs + 31) / 32, 0xff, ((max_subs + 31) / 32) * 4);

  }
}

UserQScan::~UserQScan() {}

}
}
