/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*           Copyright (C)2014-2022, WWIV Software Services               */
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
#include "sdk/sdk_helper.h"

#include "core/datetime.h"
#include "core/file.h"
#include "core/strings.h"
#include "core/version.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/net/net.h"
#include "sdk/vardec.h"

#include "gtest/gtest.h"
#include <string>
#include "sdk_helper.h"


using namespace std;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::net;
using namespace wwiv::strings;

static statusrec_t create_status() {
  statusrec_t s = {};
  memset(&s, 0, sizeof(statusrec_t));
  const auto now{DateTime::now().to_string("%m/%d/%y")};
  to_char_array(s.date1, now);
  to_char_array(s.date2, "00/00/00");
  to_char_array(s.date3, "00/00/00");
  to_char_array(s.log1, "000000.log");
  to_char_array(s.log2, "000000.log");
  to_char_array(s.gfiledate, now);
  s.callernum = 65535;
  s.qscanptr = 2;
  s.net_bias = 0.001f;
  s.net_req_free = 3.0;
  return s;
}

SdkHelper::SdkHelper()
    : saved_dir_(File::current_directory()), 
      root_(files_.CreateTempFilePath("bbs")) {
  data_ = CreatePath("data");
  msgs_ = CreatePath("msgs");
  gfiles_ = CreatePath("gfiles");
  menus_ = CreatePath("menus");
  scripts_ = CreatePath("scripts");
  dloads_ = CreatePath("dloads");
  logs_ = CreatePath("logs");
  scratch_ = CreatePath("e/scratch");
  CreatePath("e/scratch/1");
  CreatePath("e/scratch/2");
  CreatePath("e/scratch/3");
  CreatePath("e/scratch/4");

  {
    wwiv::sdk::config_t c{};
    c.msgsdir = "msgs";
    c.gfilesdir = "gfiles";
    c.menudir = "menus";
    c.datadir = "data";
    c.logdir = "logs";
    c.dloadsdir = "dloads";
    c.tempdir_format = "e/temp/%n";
    c.scratchdir_format = "e/scratch/%n";

    // Add header version.
    // TODO(rushfan): This really should all be done in the Config class and also used
    // by wwivconfig from there.
    c.header.config_revision_number = 0;
    c.userreclen = sizeof(userrec);
    c.header.written_by_wwiv_num_version = wwiv_config_version();

    // Set some more defaults various tests need
    c.maxusers = 100;
    c.max_dirs = 64;
    c.max_subs = 64;

    config_ = std::make_unique<wwiv::sdk::Config>(root_, c);
    config_->set_paths_for_test(datadir(), msgsdir(), gfilesdir(), menudir(), dloadsdir(), scriptdir());

    // Force this to be read-write since we're in a test environment (same as
    // in the upgrade case)
    config_->set_readonly(false);
    if (!config_->Save()) {
      throw std::runtime_error("failed to create config.json");
    }
  }

  {
    File sfile(FilePath(data_, STATUS_DAT));
    if (!sfile.Open(File::modeBinary | File::modeCreateFile | File::modeWriteOnly)) {
      throw std::runtime_error("failed to create status.dat");
    }
    auto s = create_status();
    sfile.Write(&s, sizeof(statusrec_t));
    sfile.Close();
  }
}

std::filesystem::path SdkHelper::CreatePath(const string& name) {
  auto path = files_.CreateTempFilePath(FilePath("bbs", name).string());
  File::mkdirs(path);
  return path;
}

wwiv::sdk::Config& SdkHelper::config() const { 
  return *config_; 
}

wwiv::sdk::net::Network SdkHelper::CreateTestNetwork(network_type_t net_type) {
  const auto dir = CreatePath("network");
  const uint16_t node = net_type == network_type_t::ftn ? FTN_FAKE_OUTBOUND_NODE : 1;
  wwiv::sdk::net::Network n(net_type, "TestNET", dir, node);
  if (net_type == network_type_t::ftn) {
    n.fido.inbound_dir = "in";
    n.fido.outbound_dir = "out";
    n.fido.temp_inbound_dir = "tempin";
    n.fido.temp_outbound_dir = "tempout";
    n.fido.unknown_dir = "unknown";
    n.fido.bad_packets_dir = "bad";
    n.fido.netmail_dir = "netmail";
    n.fido.fido_address = "21:12/2112";
  }
  return n;
}

SdkHelper::~SdkHelper() {
  try {
    const auto dir = saved_dir_.string();
    (void) chdir(dir.c_str());
  } catch (...) {
    //
  }
}
