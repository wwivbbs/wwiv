/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                Copyright (C)2016, WWIV Software Services               */
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
#include "networkb/file_manager.h"

#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "core/file.h"
#include "core/md5.h"
#include "core/log.h"
#include "core/strings.h"
#include "networkb/wfile_transfer_file.h"


using std::string;
using std::vector;

using namespace wwiv::core;
using namespace wwiv::strings;

namespace wwiv {
namespace net {

vector<TransferFile*> FileManager::CreateTransferFileList(uint16_t destination_node) {
  vector<TransferFile*> result;
  const string s_node_net = StringPrintf("s%d.net", destination_node);
  const string search_path = FilePath(network_directory_, s_node_net);
  VLOG(2) << "       CreateTransferFileList: search_path: " << search_path;
  if (File::Exists(search_path)) {
    File file(search_path);
    const string basename = file.GetName();
    result.push_back(new WFileTransferFile(basename, std::make_unique<File>(network_directory_, basename)));
    LOG(INFO) << "       CreateTransferFileList: found file: " << basename;
  }
  return result;
}


}
}
