/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*           Copyright (C)2016-2021, WWIV Software Services               */
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
#include "sdk/fido/fido_directories.h"

#include "core/file.h"
#include "core/log.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include <string>
#include <utility>
#include <vector>

using namespace wwiv::core;
using namespace wwiv::stl;
using namespace wwiv::strings;

namespace wwiv::sdk::fido {

  
FtnDirectories::FtnDirectories(const std::filesystem::path& bbsdir, const Network& net)
  : FtnDirectories(bbsdir, net, net.dir) {}

static void md(const std::vector<std::filesystem::path>& paths) {
  for (const auto& p : paths) {
    if (!File::Exists(p)) {
      File::mkdirs(p);
    }
  }
}
// Receive dirs is relative to BBS home.
FtnDirectories::FtnDirectories(std::filesystem::path bbsdir, const Network& net,
                               std::filesystem::path receive_dir)
    : bbsdir_(std::move(bbsdir)), net_(net), net_dir_(net.dir),
      inbound_dir_(File::absolute(net_dir_, net_.fido.inbound_dir)),
      temp_inbound_dir_(File::absolute(net_dir_, net_.fido.temp_inbound_dir)),
      temp_outbound_dir_(File::absolute(net_dir_, net_.fido.temp_outbound_dir)),
      outbound_dir_(File::absolute(net_dir_, net_.fido.outbound_dir)),
      netmail_dir_(File::absolute(net_dir_, net_.fido.netmail_dir)),
      bad_packets_dir_(File::absolute(net_dir_, net_.fido.bad_packets_dir)),
      receive_dir_(std::move(receive_dir)),
      tic_dir_(File::absolute(net_dir_, net_.fido.tic_dir)),
      unknown_dir_(File::absolute(net_dir_, net_.fido.unknown_dir)) {
  VLOG(1) << "FtnDirectories: receive_dir: " << receive_dir_;
  md({inbound_dir_, temp_inbound_dir_, temp_outbound_dir(), netmail_dir_, bad_packets_dir_,
     receive_dir_, tic_dir_, unknown_dir_});
}

FtnDirectories::~FtnDirectories() = default;

std::string FtnDirectories::net_dir() const { return net_dir_.string(); }
std::string FtnDirectories::inbound_dir() const { return inbound_dir_.string(); }
std::string FtnDirectories::temp_inbound_dir() const { return temp_inbound_dir_.string(); }
std::string FtnDirectories::temp_outbound_dir() const { return temp_outbound_dir_.string(); }
std::string FtnDirectories::outbound_dir() const { return outbound_dir_.string(); }
std::string FtnDirectories::netmail_dir() const { return netmail_dir_.string(); }
std::string FtnDirectories::bad_packets_dir() const { return bad_packets_dir_.string(); }
std::string FtnDirectories::receive_dir() const { return receive_dir_.string(); }

std::string FtnDirectories::tic_dir() const { return tic_dir_.string(); }
std::string FtnDirectories::unknown_dir() const { return unknown_dir_.string(); }


}