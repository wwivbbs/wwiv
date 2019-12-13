/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2016-2019, WWIV Software Services             */
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

#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "networkb/wfile_transfer_file.h"
#include "sdk/fido/fido_address.h"
#include "sdk/fido/fido_util.h"
#include <map>
#include <string>
#include <vector>

using std::map;
using std::string;
using std::vector;

using namespace wwiv::core;
using namespace wwiv::strings;
using namespace wwiv::sdk::fido;

namespace wwiv::net {

vector<TransferFile*> FileManager::CreateWWIVnetTransferFileList(uint16_t destination_node) const {
  vector<TransferFile*> result;
  const auto s_node_net = fmt::format("s{}.net", destination_node);
  const auto search_path = PathFilePath(dirs_.net_dir(), s_node_net);
  VLOG(2) << "       CreateWWIVnetTransferFileList: search_path: " << search_path;
  if (File::Exists(search_path)) {
    File file(search_path);
    const auto basename = file.path().filename().string();
    result.push_back(new WFileTransferFile(
        basename, std::make_unique<File>(PathFilePath(dirs_.net_dir(), basename))));
    LOG(INFO) << "       CreateWWIVnetTransferFileList: found file: " << basename;
  }
  return result;
}

std::vector<TransferFile*> FileManager::CreateFtnTransferFileList(const string& address) const {
  LOG(INFO) << "CreateFtnTransferFileList: " << address;

  std::vector<fido_bundle_status_t> statuses{
      fido_bundle_status_t::crash,
      fido_bundle_status_t::normal,
      fido_bundle_status_t::direct, 
      fido_bundle_status_t::immediate
  };

  map<string, TransferFile*> result_map;

  FidoAddress dest(address);
  VLOG(1) << "CreateFtnTransferFileList: " << dirs_.outbound_dir();
  for (const auto& st : statuses) {
    const auto name = flo_name(dest, st);
    VLOG(1) << "Looking for FLO file named: " << FilePath(dirs_.outbound_dir(), name);
    if (File::Exists(PathFilePath(dirs_.outbound_dir(), name))) {
      LOG(INFO) << "Found file file: " << dirs_.outbound_dir() << "; name: " << name;
      const auto path = PathFilePath(dirs_.outbound_dir(), name);
      FloFile flo(net_, path);
      if (!flo.Load()) {
        continue;
      }
      for (const auto& e : flo.flo_entries()) {
        File f(e.first);
        const auto basename = f.path().filename().string();
        auto w = new WFileTransferFile(basename, std::make_unique<File>(e.first));
        w->set_flo_file(std::make_unique<FloFile>(net_, path));
        // emplace won't add another entry if one exists already.
        result_map.emplace(basename, w);
      }
    }
  }
  // Return a vector of the transfer files.  This way we don't
  // have duplicate entries.
  vector<TransferFile*> result;
  for (const auto& e : result_map) {
    result.push_back(e.second);
  }
  return result;
}

std::vector<TransferFile*> FileManager::CreateTransferFileList(const Remote& remote) const {
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

static void rename_wwivnet_pend(const string& directory, const string& filename) {
  const auto pend_filename = PathFilePath(directory, filename);
  if (!File::Exists(pend_filename)) {
    LOG(ERROR) << " pending file does not exist: " << pend_filename;
    return;
  }
  const auto num = filename.substr(1);
  const string prefix = (to_number<int>(num)) ? "1" : "0";

  for (int i = 0; i < 1000; i++) {
    const auto new_basename = fmt::format("p{}-0-{}.net", prefix, i);
    const auto new_filename = PathFilePath(directory, new_basename);
    VLOG(2) << new_filename;
    if (File::Rename(pend_filename, new_filename)) {
      LOG(INFO) << "renamed file: '" << pend_filename << "' to: '" << new_filename << "'";
      return;
    }
  }
  LOG(ERROR) << "all attempts failed to rename_wwivnet_pend";
}

void FileManager::rename_wwivnet_pending_files() {
  VLOG(1) << "STATE: rename_wwivnet_pending_files";
  for (const auto& file : received_files()) {
    LOG(INFO) << "       renaming_pending_file: dir: " << dirs_.net_dir() << "; file: " << file;
    rename_wwivnet_pend(dirs_.net_dir(), file);
  }
}

void FileManager::rename_ftn_pending_files() {
  VLOG(1) << "STATE: rename_ftn_pending_files";
  for (const auto& file : received_files()) {
    if (is_bundle_file(file) || is_packet_file(file)) {
      LOG(INFO) << "       renaming_pending_file: dir: " << dirs_.net_dir() << "; file: " << file;
      if (!File::Exists(PathFilePath(dirs_.inbound_dir(), file))) {
        File::Move(PathFilePath(dirs_.net_dir(), file), PathFilePath(dirs_.inbound_dir(), file));
      } else {
        LOG(ERROR) << "File: " << file << " already exists in fido inbound dir. Please move manually.";
      }
    } else {
      LOG(ERROR) << "       unknown file received: " << file;
    }
  }
}


}
