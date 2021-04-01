/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2016-2021, WWIV Software Services             */
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
#ifndef INCLUDED_BINKP_FILE_MANAGER_H
#define INCLUDED_BINKP_FILE_MANAGER_H

#include "binkp/remote.h"
#include "binkp/transfer_file.h"
#include "sdk/fido/fido_directories.h"
#include "sdk/net/net.h"
#include <string>
#include <vector>

namespace wwiv::net {
  
class FileManager final {
public:
  FileManager(const wwiv::sdk::Config& config, const sdk::net::Network& net,
                       const std::string& receive_dir);
  ~FileManager() = default;

  [[nodiscard]] std::vector<TransferFile*> CreateTransferFileList(const Remote& remote) const;
  void ReceiveFile(const std::string& filename);
  [[nodiscard]] const std::vector<std::string>& received_files() const { return received_files_; }
  void rename_wwivnet_pending_files();
  void rename_ftn_pending_files(const Remote& remote);

  // For tests.
  [[nodiscard]] const wwiv::sdk::fido::FtnDirectories& dirs() const { return dirs_; }

private:
  [[nodiscard]] std::vector<TransferFile*> CreateWWIVnetTransferFileList(int destination_node) const;
  [[nodiscard]] std::vector<TransferFile*> CreateFtnTransferFileList(const std::string& address) const;

  const wwiv::sdk::Config& config_;
  const sdk::net::Network net_;
  const wwiv::sdk::fido::FtnDirectories dirs_;
  const std::string network_directory_;
  std::vector<std::string> received_files_;
};

}  // namespace

#endif
