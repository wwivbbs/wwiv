/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2021, WWIV Software Services             */
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
#include "binkp/binkp.h"
#include "binkp/binkp_commands.h"
#include "binkp/binkp_config.h"
#include "binkp/transfer_file.h"
#include "binkp_test/fake_connection.h"
#include "core/file.h"
#include "core/strings.h"
#include "core_test/file_helper.h"
#include "sdk/net/callout.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include <string>
#include <thread>

using wwiv::sdk::Callout;
using namespace wwiv::core;
using namespace wwiv::net;
using namespace wwiv::sdk::fido;
using namespace wwiv::sdk::net;
using namespace wwiv::strings;

static const std::string ANSWERING_ADDRESS = "20000/20000:1";
static const int ORIGINATING_ADDRESS = 2;

class BinkTest : public testing::Test {
protected:
  void StartBinkpReceiver() {
    CHECK(files_.Mkdir("network"));
    CHECK(files_.Mkdir("gfiles"));
    const std::string line("@1 example.com");
    files_.CreateTempFile("binkp.net", line);
    const auto network_dir = files_.DirName("network");
    const auto gfiles_dir = files_.DirName("gfiles");
    wwiv::sdk::config_t wwiv_config_{};
    wwiv_config_.systemname = "Test System";
    wwiv_config_.sysopname = "Test Sysop";
    wwiv::sdk::Config config(File::current_directory(), wwiv_config_);
    config.gfilesdir(gfiles_dir);
    Network net(network_type_t::wwivnet, "Dummy Network");
    net.dir = network_dir;
    binkp_config_ = std::make_unique<BinkConfig>(ORIGINATING_ADDRESS, config, network_dir);
    auto dummy_callout = std::make_unique<Callout>(net, 0);
    BinkP::received_transfer_file_factory_t null_factory = [](const std::string&, const std::string& filename) { 
      return new InMemoryTransferFile(filename, "");
    };
    binkp_config_->callouts()["wwivnet"] = std::move(dummy_callout);
    binkp_.reset(new BinkP(&conn_, binkp_config_.get(), BinkSide::ANSWERING, ANSWERING_ADDRESS, null_factory));
    CommandLine cmdline({ "networkb_tests.exe" }, "");
    thread_ = std::thread([&]() { binkp_->Run(cmdline); });
  } 

  void Stop() {
    thread_.join();
  }

  std::unique_ptr<BinkP> binkp_;
  std::unique_ptr<BinkConfig> binkp_config_;
  FakeConnection conn_;
  std::thread thread_;
  FileHelper files_;
};

TEST_F(BinkTest, ErrorAbortsSession) {
  StartBinkpReceiver();
  conn_.ReplyCommand(BinkpCommands::M_ERR, "Doh!");
  Stop();
  
  while (conn_.has_sent_packets()) {
    std::clog << conn_.GetNextPacket().debug_string() << std::endl;
  }
}

static int node_number_from_address_list(const std::string& addresses,
                                         const std::string& network_name) {
  const auto a = ftn_address_from_address_list(addresses, network_name);
  return wwivnet_node_number_from_ftn_address(a);
}

TEST(NodeFromAddressTest, SingleAddress) {
  const std::string address = "20000:20000/1234@foonet";
  EXPECT_EQ(1234, node_number_from_address_list(address, "foonet"));
  EXPECT_EQ(WWIVNET_NO_NODE, node_number_from_address_list(address, "wwivnet"));
}

TEST(NodeFromAddressTest, MultipleAddresses) {
  const std::string address = "1:369/23@fidonet 20000:20000/1234@foonet 20000:369/24@dorknet";
  EXPECT_EQ("20000:20000/1234@foonet", ftn_address_from_address_list(address, "foonet"));
  EXPECT_EQ(1234, node_number_from_address_list(address, "foonet"));
  EXPECT_EQ(WWIVNET_NO_NODE, node_number_from_address_list(address, "wwivnet"));
  EXPECT_EQ(WWIVNET_NO_NODE, node_number_from_address_list(address, "fidonet"));
  EXPECT_EQ(WWIVNET_NO_NODE, node_number_from_address_list(address, "dorknet"));
}

TEST(NodeFromAddressTest, MultipleAddresses_SameNetwork) {
  const std::string address = "1:369/-1@coolnet 1:369/23@coolnet";
  EXPECT_EQ("1:369/23@coolnet", ftn_address_from_address_list(address, "coolnet"));
}

// string expected_password_for(Callout* callout, int node)
TEST(ExpectedPasswordTest, Basic) {
  const net_call_out_rec n{ "20000:20000/1234", 1234, 1, unused_options_sendback, 2, 3, 4, "pass", 5, 6 };
  Callout callout({ n });
  const std::string actual = expected_password_for(&callout, 1234);
  EXPECT_EQ("pass", actual);
}

TEST(ExpectedPasswordTest, WrongNode) {
  const net_call_out_rec n{"20000:20000/1234", 1234, 1, unused_options_sendback, 2, 3, 4, "pass", 5, 6 };
  Callout callout({ n });
  const std::string actual = expected_password_for(&callout, 12345);
  EXPECT_EQ("-", actual);
}


TEST(FtnFromAddressListSet, Smoke) {
  const std::string address = "3:57/0@fidonet 3:770/1@fidonet 3:770/0@fidonet 3:772/1@fidonet "
                              "3:772/0@fidonet 21:1/100@fsxnet 21:1/3@fsxnet 21:1/2@fsxnet "
                              "21:1/1@fsxnet 21:1/0@fsxnet 39:970/0@amiganet 46:3/103@agoranet";
  const FidoAddress a("21:1/0@fsxnet");
  const std::set addresses{FidoAddress("21:1/0@fsxnet")};
  const auto known = ftn_addresses_from_address_list(address, addresses);
  EXPECT_THAT(known, testing::ElementsAre(a));
}


TEST(FtnFromAddressListSet, Smoke_Same) {
  const std::string address = "3:57/0@fidonet 3:770/1@fidonet 3:770/0@fidonet 3:772/1@fidonet "
                              "3:772/0@fidonet 21:1/100@fsxnet 21:1/3@fsxnet 21:1/2@fsxnet "
                              "21:1/1@fsxnet 21:1/0@fsxnet 39:970/0@amiganet 46:3/103@agoranet";
  const FidoAddress a("21:1/0@fsxnet");
  const std::set addresses{FidoAddress(a)};
  const auto known = ftn_addresses_from_address_list(address, addresses);
  EXPECT_THAT(known, testing::ElementsAre(a));
}

TEST(FtnFromAddressListSet, WithoutDomainInKnown) {
  const std::string address = "3:57/0@fidonet 3:770/1@fidonet 3:770/0@fidonet 3:772/1@fidonet "
                              "3:772/0@fidonet 21:1/100@fsxnet 21:1/3@fsxnet 21:1/2@fsxnet "
                              "21:1/1@fsxnet 21:1/0@fsxnet 39:970/0@amiganet 46:3/103@agoranet";
  const FidoAddress a("21:1/2");
  const std::set addresses{FidoAddress(a)};
  const auto known = ftn_addresses_from_address_list(address, addresses);
  const FidoAddress expected("21:1/2@fsxnet");
  EXPECT_THAT(known, testing::ElementsAre(expected));
}

TEST(FtnFromAddressListSet, WithoutDomainInADR) {
  const std::string address = "3:57/0 3:770/1 3:770/0 3:772/1 "
                              "3:772/0 21:1/100 21:1/3 21:1/2 "
                              "21:1/1 21:1/0 39:970/0 46:3/103";
  const FidoAddress a("21:1/2@fsxnet");
  const std::set addresses{FidoAddress(a)};
  const auto known = ftn_addresses_from_address_list(address, addresses);
  EXPECT_THAT(known, testing::ElementsAre(a));
}

TEST(FtnFromAddressListSet, WithoutDomain_NotFound) {
  const std::string address = "3:57/0@fidonet 3:770/1@fidonet 3:770/0@fidonet 3:772/1@fidonet "
                              "3:772/0@fidonet 21:1/100@fsxnet 21:1/3@fsxnet 21:1/2@fsxnet "
                              "21:1/1@fsxnet 21:1/0@fsxnet 39:970/0@amiganet 46:3/103@agoranet";
  const FidoAddress a("21:1/2112");
  const std::set addresses{FidoAddress(a)};
  const auto known = ftn_addresses_from_address_list(address, addresses);
  EXPECT_THAT(known, testing::IsEmpty());
}

TEST(FtnFromAddressListSet, WithoutDomainInADR_NotFound) {
  const std::string address = "3:57/0 3:770/1 3:770/0 3:772/1 "
                              "3:772/0 21:1/100 21:1/3 21:1/2 "
                              "21:1/1 21:1/0 39:970/0 46:3/103";
  const FidoAddress a("21:1/2001@fsxnet");
  const std::set addresses{FidoAddress(a)};
  const auto known = ftn_addresses_from_address_list(address, addresses);
  EXPECT_THAT(known, testing::IsEmpty());
}
