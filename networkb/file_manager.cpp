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

vector<TransferFile*> FileManager::CreateWWIVnetTransferFileList(uint16_t destination_node) {
  vector<TransferFile*> result;
  const string s_node_net = StringPrintf("s%d.net", destination_node);
  const string search_path = FilePath(net_.dir, s_node_net);
  VLOG(2) << "       CreateWWIVnetTransferFileList: search_path: " << search_path;
  if (File::Exists(search_path)) {
    File file(search_path);
    const string basename = file.GetName();
    result.push_back(new WFileTransferFile(basename, std::make_unique<File>(net_.dir, basename)));
    LOG(INFO) << "       CreateWWIVnetTransferFileList: found file: " << basename;
  }
  return result;
}

std::vector<TransferFile*> FileManager::CreateFtnTransferFileList(const string& address) {
  return{};
}

std::vector<TransferFile*> FileManager::CreateTransferFileList(const Remote& remote) {
  if (net_.type == network_type_t::wwivnet) {
    return CreateWWIVnetTransferFileList(remote.wwivnet_node());
  } else if (net_.type == network_type_t::ftn) {
    return CreateFtnTransferFileList(remote.ftn_address());
  }
  return{};
}

void FileManager::ReceiveFile(const std::string& filename) {
  received_files_.push_back(filename);
}

static void rename_pend(const string& directory, const string& filename) {
  File pend_file(directory, filename);
  if (!pend_file.Exists()) {
    LOG(ERROR) << " pending file does not exist: " << pend_file;
    return;
  }
  const string pend_filename(pend_file.full_pathname());
  const string num = filename.substr(1);
  const string prefix = (atoi(num.c_str())) ? "1" : "0";

  for (int i = 0; i < 1000; i++) {
    const string new_filename = StringPrintf("%sp%s-0-%u.net", directory.c_str(), prefix.c_str(), i);
    VLOG(2) << new_filename;
    if (File::Rename(pend_filename, new_filename)) {
      LOG(INFO) << "renamed file to: " << new_filename;
      return;
    }
  }
  LOG(ERROR) << "all attempts failed to rename_pend";
}

void FileManager::rename_pending_files() {
  VLOG(1) << "STATE: rename_pending_files";
  for (const auto& file : received_files()) {
    LOG(INFO) << "       renaming_pending_file: dir: " << net_.dir << "; file: " << file;
    rename_pend(net_.dir, file);
  }
}


}
}