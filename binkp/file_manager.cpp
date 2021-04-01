/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
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
/*                                                                        */
/**************************************************************************/
#include "binkp/file_manager.h"

#include "binkp/wfile_transfer_file.h"
#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "sdk/fido/fido_address.h"
#include "sdk/fido/fido_util.h"
#include "sdk/files/tic.h"
#include <map>
#include <string>
#include <vector>

using namespace wwiv::core;
using namespace wwiv::strings;
using namespace wwiv::sdk::fido;
using namespace wwiv::sdk::net;

namespace wwiv::net {

std::vector<TransferFile*> FileManager::CreateWWIVnetTransferFileList(int destination_node) const {
  std::vector<TransferFile*> result;
  const auto node_net = fmt::format("s{}.net", destination_node);
  const auto path = FilePath(dirs_.net_dir(), node_net);
  VLOG(2) << "       CreateWWIVnetTransferFileList: search_path: " << path;
  if (File::Exists(path)) {
    const auto fn = path.filename().string();
    result.push_back(
        new WFileTransferFile(fn, std::make_unique<File>(FilePath(dirs_.net_dir(), fn))));
    VLOG(1) << "       CreateWWIVnetTransferFileList: found file: " << fn;
  }
  return result;
}

std::vector<TransferFile*> FileManager::CreateFtnTransferFileList(const std::string& address) const {
  VLOG(1) << "CreateFtnTransferFileList: " << address;

  std::vector<fido_bundle_status_t> statuses{
      fido_bundle_status_t::crash,
      fido_bundle_status_t::normal,
      fido_bundle_status_t::direct, 
      fido_bundle_status_t::immediate
  };

  std::map<std::string, TransferFile*> result_map;

  FidoAddress dest(address);
  const auto dir = dirs_.outbound_dir();
  VLOG(2) << "CreateFtnTransferFileList: " << dir;
  for (const auto& st : statuses) {
    const auto name = flo_name(dest, st);
    const auto path = FilePath(dir, name);
    VLOG(2) << "Looking for FLO file named: " << path.string();
    if (File::Exists(path)) {
      LOG(INFO) << "Found FLO file: " << dir << "; name: " << name;
      FloFile flo(net_, path);
      if (!flo.Load()) {
        continue;
      }
      for (const auto& e : flo.flo_entries()) {
        File f(e.first);
        const auto basename = f.path().filename().string();
        auto* w = new WFileTransferFile(basename, std::make_unique<File>(e.first));
        w->set_flo_file(std::make_unique<FloFile>(net_, path));
        // emplace won't add another entry if one exists already.
        result_map.emplace(basename, w);
      }
    }
  }
  // Return a vector of the transfer files.  This way we don't
  // have duplicate entries.
  std::vector<TransferFile*> result;
  for (const auto& e : result_map) {
    result.push_back(e.second);
  }
  return result;
}

FileManager::FileManager(const wwiv::sdk::Config& config, const Network& net,
                         const std::string& receive_dir)
  : config_(config), net_(net), dirs_(config.root_directory(), net, receive_dir) {
  const auto dir = dirs_.receive_dir();
  VLOG(1) << "FileManager: receive_dir: " << dir;
  if (!File::Exists(dir)) {
    LOG(INFO) << "Creating receive directory for session: '" << dir << "'";
    if (!File::mkdirs(dir)) {
      LOG(ERROR) << "Failed to create receive directory: " << dir;
    }
  }
}

std::vector<TransferFile*> FileManager::CreateTransferFileList(const Remote& remote) const {
  if (net_.type == network_type_t::wwivnet) {
    return CreateWWIVnetTransferFileList(remote.wwivnet_node());
  }
  if (net_.type == network_type_t::ftn) {
    return CreateFtnTransferFileList(remote.ftn_address());
  }
  return{};
}

void FileManager::ReceiveFile(const std::string& filename) {
  VLOG(1) << "FileManager::ReceiveFile: " << filename;
  received_files_.push_back(filename);
}

static void rename_wwivnet_pend(const std::string& receive_directory, const std::string& net_dir, const std::string& filename) {
  const auto pend_filename = FilePath(receive_directory, filename);
  if (!File::Exists(pend_filename)) {
    LOG(ERROR) << " pending file does not exist: " << pend_filename;
    return;
  }
  const auto num = filename.substr(1);
  const std::string prefix = (to_number<int>(num)) ? "1" : "0";

  for (auto i = 0; i < 1000; i++) {
    const auto new_basename = fmt::format("p{}-0-{}.net", prefix, i);
    const auto new_filename = FilePath(net_dir, new_basename);
    VLOG(2) << new_filename;
    if (File::Rename(pend_filename, new_filename)) {
      LOG(INFO) << "renamed file: '" << pend_filename << "' to: '" << new_filename << "'";
      return;
    }
  }
  LOG(ERROR) << "all attempts failed to rename_wwivnet_pend";
}

// ReSharper disable once CppMemberFunctionMayBeConst
void FileManager::rename_wwivnet_pending_files() {
  VLOG(1) << "STATE: rename_wwivnet_pending_files";
  for (const auto& file : received_files()) {
    const auto rdir = dirs_.receive_dir();
    const auto net_dir = dirs_.net_dir();
    VLOG(1) << "       renaming_pending_file: dir: " << rdir << "; file: " << file;
    rename_wwivnet_pend(rdir, net_dir, file);
  }
}

static bool is_tic_file(const std::string& fn) {
  const auto idx = fn.rfind('.');
  if (idx == std::string::npos) {
    return false;
  }
  const auto ext = ToStringUpperCase(fn.substr(idx + 1));
  return ext == "TIC";
}

static bool move_without_overrite(const std::filesystem::path& src,
                                  const std::filesystem::path& dest, const std::string& msg) {
  LOG(INFO) << "       Attempting to move " << src.string() << " => " << dest.string();
  if (!File::Exists(dest)) {
    return File::Move(src, dest);
  }
  LOG(ERROR) << "       Skipping Move since file already exists: " << dest.string();
  if (!msg.empty()) {
    LOG(ERROR) << msg;
  }
  return false;
}

void FileManager::rename_ftn_pending_files(const Remote& remote) {
  VLOG(1) << "STATE: rename_ftn_pending_files";
  const auto rdir = dirs_.receive_dir();
  const sdk::files::TicParser tic_parser(rdir);
  for (const auto& file : received_files()) {
    const auto ipath = FilePath(dirs_.inbound_dir(), file);
    const auto rpath = FilePath(rdir, file);
    const auto upath = FilePath(dirs_.unknown_dir(), file);
    if (!File::Exists(rpath)) {
      VLOG(1) << "[pass 1] rfile does not exist: " << rpath;
      continue;
    }
    if (is_bundle_file(file) || is_packet_file(file)) {
      LOG(INFO) << "       renaming_pending_file: dir: " << rdir << "; file: " << file;
      move_without_overrite(rpath, ipath,
                            "already exists in fido inbound dir. Please move manually.");
      continue;
    }
    if (is_tic_file(file) && net_.fido.process_tic) {
      // TODO: here is where we can do the TIC support.
      // If TIC file, process tic file and move it and archive to the net_dir
      // then pass to networkt to process. For now HACK - let's just move all unknown
      // to the tick dir.
      const auto ticpath = FilePath(rdir, file);
      auto otick = tic_parser.parse(file);
      if (!otick) {
        LOG(ERROR) << "Unable to parse TIC file: " << ticpath;
        continue;
      }
      auto tic = otick.value();
      if (!tic.IsValid()) {
        LOG(ERROR) << "       Tic file " << ticpath.string() << " IS NOT VALID.";
        continue;
      }
      LOG(INFO) << "Tic file " << ticpath.string() << " is valid.";
      FidoCallout callout(config_, remote.network());
      std::string expected_tic_pw;
      try {
        FidoAddress address(remote.ftn_address());
        auto packet_config = callout.packet_config_for(address);
        expected_tic_pw = packet_config.tic_password;
      } catch (const bad_fidonet_address&) {
      }
      if (!expected_tic_pw.empty()) {
        // We expect a password.
        if (!iequals(expected_tic_pw, tic.pw)) {
          LOG(ERROR) << "       Tic Password was not correct for tic: " << ticpath.string()
                     << "; skipping.";
          continue;
        }
      } else {
        LOG(INFO) << "No password configured for node: " << remote.ftn_address();
      }
      move_without_overrite(FilePath(rdir, tic.file), FilePath(dirs_.tic_dir(), tic.file),
                            "File (from TIC) file already exists in TIC path.");
      move_without_overrite(rpath, FilePath(dirs_.tic_dir(), file),
                            "TIC file already exists in TIC path.");
    } else {
      LOG(ERROR) << "       tic processing disabled and unknown file received: '" << file << "' moving to TIC dir.";
    }
  }
  // pass 2: anything remaining move to unknown dir.
  // note that things missing were already processed.
  for (const auto& file : received_files()) {
    const auto rpath = FilePath(rdir, file);
    if (File::Exists(rpath)) {
      const auto upath = FilePath(dirs_.unknown_dir(), file);
      File::Move(rpath, upath);
    }
  }
}

} // namespace wwiv::net
