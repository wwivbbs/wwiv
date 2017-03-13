/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2016-2017, WWIV Software Services             */
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
#ifndef __INCLUDED_NETWORKB_FILE_MANAGER_H__
#define __INCLUDED_NETWORKB_FILE_MANAGER_H__

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "networkb/transfer_file.h"
#include "networkb/remote.h"
#include "sdk/net.h"

namespace wwiv {
namespace net {
  
class FileManager {
public:
  explicit FileManager(const std::string& root_directory, const net_networks_rec& net)
    : root_directory_(root_directory), net_(net) {}
  virtual ~FileManager() {}

  std::vector<TransferFile*> CreateTransferFileList(const Remote& remote);
  void ReceiveFile(const std::string& filename);
  const std::vector<std::string>& received_files() const { return received_files_; }
  void rename_wwivnet_pending_files();
  void rename_ftn_pending_files();

private:
  std::vector<TransferFile*> CreateWWIVnetTransferFileList(uint16_t destination_node);
  std::vector<TransferFile*> CreateFtnTransferFileList(const std::string& address);

  const net_networks_rec net_;
  const std::string root_directory_;
  const std::string network_directory_;
  std::vector<std::string> received_files_;
};

}  // namespace net
}  // namespace wwiv

#endif  // __INCLUDED_NETWORKB_FILE_MANAGER_H__
