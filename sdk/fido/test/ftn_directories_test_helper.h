/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2021-2022, WWIV Software Services             */
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
#ifndef INCLUDED_SDK_FIDO_TEST_FTN_DIRECTORIES_TEST_HELPER_H
#define INCLUDED_SDK_FIDO_TEST_FTN_DIRECTORIES_TEST_HELPER_H

#include "sdk/fido/fido_directories.h"
#include "sdk/net/net.h"
#include <filesystem>
#include <string>

namespace wwiv::sdk::fido::test {

wwiv::sdk::net::Network CreateTestNetwork(const std::filesystem::path& dir) { 
  wwiv::sdk::net::Network n(wwiv::sdk::net::network_type_t::ftn, "TestNET", dir,
                            FTN_FAKE_OUTBOUND_NODE);
  n.fido.inbound_dir = "in";
  n.fido.outbound_dir = "out";
  n.fido.temp_inbound_dir = "tempin";
  n.fido.temp_outbound_dir = "tempout";
  n.fido.unknown_dir = "unknown";
  n.fido.bad_packets_dir = "bad";
  n.fido.netmail_dir = "netmail";
  n.fido.fido_address = "21:12/2112";
  return n;
}

/**
 * Helper class for tests requiring local filesystem access.
 *
 * Note: This class can not use File since it is used by the tests for File.
 */
class FtnDirectoriesTestHelper {
public:
  FtnDirectoriesTestHelper(wwiv::core::test::FileHelper& h)
      : net_(CreateTestNetwork(h.Dir("network"))),
        dirs_(h.TempDir(), net_) {}

  const wwiv::sdk::fido::FtnDirectories& dirs() const { return dirs_; }
  wwiv::sdk::net::Network& net() { return net_; }

private:
  wwiv::sdk::net::Network net_;
  wwiv::sdk::fido::FtnDirectories dirs_;
};

} // namespace wwiv::sdk::fido::test

#endif
