/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
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

namespace wwiv {
namespace net {
  
class FileManager {
public:
  explicit FileManager(const std::string& network_directory): network_directory_(network_directory) {}
  virtual ~FileManager() {}

  std::vector<TransferFile*> CreateTransferFileList(uint16_t destination_node);

private:
  const std::string network_directory_;
};

}  // namespace net
}  // namespace wwiv

#endif  // __INCLUDED_NETWORKB_FILE_MANAGER_H__
