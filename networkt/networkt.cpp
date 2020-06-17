/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*              Copyright (C)2020, WWIV Software Services                 */
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

// WWIV5 NetworkC
#include "core/command_line.h"
#include "core/file.h"
#include "core/findfiles.h"
#include "core/log.h"
#include "core/os.h"
#include "core/scope_exit.h"
#include "core/semaphore_file.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "net_core/net_cmdline.h"
#include "sdk/callout.h"
#include "sdk/config.h"
#include "sdk/status.h"
#include "sdk/fido/fido_directories.h"
#include "sdk/files/dirs.h"
#include "sdk/files/files.h"
#include "sdk/files/tic.h"
#include "sdk/net/packets.h"
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <string>

using std::cout;
using std::endl;
using std::map;
using std::string;
using std::unique_ptr;

using namespace wwiv::core;
using namespace wwiv::net;
using namespace wwiv::strings;
using namespace wwiv::sdk;
using namespace wwiv::sdk::net;
using namespace wwiv::sdk::fido;
using namespace wwiv::stl;
using namespace wwiv::os;
using namespace wwiv::sdk::fido;

static void ShowHelp(const NetworkCommandLine& cmdline) {
  cout << cmdline.GetHelp() << endl;
  exit(1);
}

std::optional<files::directory_t> FindDir(const files::Dirs& dirs, const std::string& area_tag) {
  for (const auto& d : dirs.dirs()) {
    if (iequals(area_tag, d.area_tag)) {
      return {d};
    }
  }
  return std::nullopt;
}

bool process_ftn_tic(const Config& config, const net_networks_rec& net, bool save_tic_files, bool skip_delete) {
  if (!net.fido.process_tic) {
    LOG(ERROR) << "Exiting without attempting to process TIC files; TIC processing disabled for network: " << net.name;
    return false;
  }
  const FtnDirectories ftn_directories(config.root_directory(), net);
  files::Dirs dirs(config.datadir());
  if (!dirs.Load()) {
    LOG(ERROR) << "Unable to load directories.";
    return false;
  }
  files::FileApi api(config.datadir());

  FindFiles ff(FilePath(ftn_directories.tic_dir(), "*.tic"), FindFilesType::files);
  auto first = true;
  const files::TicParser parser(ftn_directories.tic_dir());
  for (const auto& f : ff) {
    auto ot = parser.parse(f.name);
    if (!ot) {
      continue;
    }
    auto t = ot.value();
    if (!t.IsValid()) {
      continue;
    }
    auto od = FindDir(dirs, t.area);
    if (!od) {
      LOG(ERROR) << "Unable to find AREA_TAG for tic file: TAG: " << t.area << "; file; " << f.name;
      continue;
    }
    auto d = od.value();
    auto fa = api.CreateOrOpen(d);
    if (!fa) {
      LOG(ERROR) << "Unable to open file area: " << d.filename;
      continue;
    }
    if (first) {
      LOG(INFO) << "------------------------------------------------------------------------------";
      first = false;
    }
    files::FileName fn(t.file);
    auto op = fa->FindFile(fn);
    files::FileRecord r;
    r.set_filename(fn);
    r.set_description(t.desc);
    r.set_extended_description(!t.ldesc.empty());
    r.set_numbytes(t.size());
    r.set_date(t.date());
    const auto actual_t = File::creation_time(FilePath(ftn_directories.tic_dir(), r));
    r.set_actual_date(DateTime::from_time_t(actual_t));
    const auto ext_desc = JoinStrings(t.ldesc, "\r\n");
    if (op.has_value()) {
      LOG(INFO) << "File already exists in file area";
      LOG(INFO) << "** Updating: "  << r.aligned_filename();
      if (!fa->UpdateFile(r, op.value(), ext_desc)) {
        LOG(ERROR) << "Failed to update File: " << fn.aligned_filename();
        continue;
      }
    } else {
      LOG(INFO) << "** Adding  :" << r.aligned_filename();
      if (!fa->AddFile(r, ext_desc)) {
        LOG(ERROR) << "Error adding file: " << r.aligned_filename();
        continue;
      }
      if (!fa->Save()) {
        LOG(ERROR) << "Error saving file area: " << d.filename;
        continue;
      }
    }
    // Display information about the file;
    LOG(INFO) << "File Name  : " << r.aligned_filename();
    LOG(INFO) << "Description: " << t.desc;
    LOG(INFO) << "Ext Desc   : ";
    const auto v = SplitString(ext_desc, "\r\n", true);
    for (const auto& l : v) {
      LOG(INFO) << "    " << l;
    }
    LOG(INFO) << "------------------------------------------------------------------------------";
    const auto src = FilePath(ftn_directories.tic_dir(), r);
    const auto tic = FilePath(ftn_directories.tic_dir(), f.name);
    const auto dest = FilePath(d.path, r);
    if (save_tic_files) {
      VLOG(1) << "Not moving file, just copy, --save_tic_files == true";
      File::Copy(src, dest);
    } else {
      VLOG(1) << "Moving file to: " << dest.string();
      File::Move(src, dest);
      if (!skip_delete) {
        File::Remove(tic);
      }
    }
  }
  return true;
}

int networkt_main(const NetworkCommandLine& net_cmdline) {
  try {
    const auto& net = net_cmdline.network();

    StatusMgr sm(net_cmdline.config().datadir(), [](int) {});
    const auto status = sm.GetStatus();

    switch (net.type) {
    case network_type_t::ftn: {
      const auto save_tic_files = net_cmdline.cmdline().barg("save_tic_files");
      const auto skip_delete = net_cmdline.skip_delete();
      if (!process_ftn_tic(net_cmdline.config(), net, save_tic_files, skip_delete)) {
        return 1;
      }
    } break;
    case network_type_t::wwivnet:
      LOG(ERROR) << "TIC support not implemented for WWIVnet";
      return 1;
    case network_type_t::internet:
      LOG(ERROR) << "TIC support not implemented for Internet";
      return 1;
    case network_type_t::news:
      LOG(ERROR) << "TIC support not implemented for NNTP";
      return 1;
    default:
      LOG(ERROR) << "Unknown network type: " << static_cast<int>(net.type);
      return 1;
    }

    return 0;
  } catch (const std::exception& e) {
    LOG(ERROR) << "ERROR: [network" << net_cmdline.net_cmd() << "]: " << e.what();
  }
  return 2;
}

int main(int argc, char** argv) {
  LoggerConfig config(LogDirFromConfig);
  Logger::Init(argc, argv, config);

  ScopeExit at_exit(Logger::ExitLogger);
  CommandLine cmdline(argc, argv, "net");
  cmdline.add_argument({"process_instance", "Also process pending files for BBS instance #", "0"});
  cmdline.add_argument(BooleanCommandLineArgument{
      "save_tic_files", 'S', "Save TIC files, do not delete TIC and archives", false});

  const NetworkCommandLine net_cmdline(cmdline, 't');
  if (!net_cmdline.IsInitialized() || net_cmdline.cmdline().help_requested()) {
    ShowHelp(net_cmdline);
    return 1;
  }
  try {
    auto semaphore = SemaphoreFile::try_acquire(net_cmdline.semaphore_path(),
                                                net_cmdline.semaphore_timeout());
    return networkt_main(net_cmdline);
  } catch (const semaphore_not_acquired& e) {
    LOG(ERROR) << "ERROR: [network" << net_cmdline.net_cmd()
               << "]: Unable to Acquire Network Semaphore: " << e.what();
  }
}