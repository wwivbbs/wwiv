#include "gtest/gtest.h"
#include "core/strings.h"
#include "core_test/file_helper.h"
#include "networkb/binkp.h"
#include "networkb/binkp_commands.h"
#include "networkb/binkp_config.h"
#include "networkb/callout.h"
#include "networkb/transfer_file.h"
#include "networkb_test/fake_connection.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <thread>

using std::clog;
using std::endl;
using std::string;
using std::thread;
using std::unique_ptr;
using namespace wwiv::net;
using namespace wwiv::strings;

static const int ANSWERING_ADDRESS = 1;
static const int ORIGINATING_ADDRESS = 2;

class BinkTest : public testing::Test {
protected:
  void Start() {
    files_.Mkdir("network");
    const string line("@1 example.com");
    files_.CreateTempFile("binkp.net", line);
    const string network_dir = files_.DirName("network");
    BinkConfig* dummy_config = new BinkConfig(ORIGINATING_ADDRESS, "Dummy", network_dir);
    Callout dummy_callout(network_dir);
    BinkP::received_transfer_file_factory_t null_factory = [](const string& network_name, const string& filename) { 
      return new InMemoryTransferFile(filename, "");
    };
    std::map<const std::string, Callout> callouts = {{"", dummy_callout}};
//    callouts.emplace("", dummy_callout);
    binkp_.reset(new BinkP(&conn_, dummy_config, callouts, BinkSide::ANSWERING, ANSWERING_ADDRESS, null_factory));
    thread_ = thread([&]() {binkp_->Run(); });
  } 

  void Stop() {
    thread_.join();
  }

  unique_ptr<BinkP> binkp_;
  FakeConnection conn_;
  std::thread thread_;
  FileHelper files_;
};

TEST_F(BinkTest, ErrorAbortsSession) {
  Start();
  conn_.ReplyCommand(BinkpCommands::M_ERR, "Doh!");
  Stop();
  
  while (conn_.has_sent_packets()) {
    clog << conn_.GetNextPacket().debug_string() << endl;
  }
}

TEST(NodeFromAddressTest, SingleAddress) {
  const string address = "20000:20000/1234@foonet";
  EXPECT_EQ(1234, node_number_from_address_list(address, "foonet"));
  EXPECT_EQ(-1, node_number_from_address_list(address, "wwivnet"));
}

TEST(NodeFromAddressTest, MultipleAddresses) {
  const string address = "1:369/23@fidonet 20000:20000/1234@foonet 20000:369/24@dorknet";
  EXPECT_EQ(1234, node_number_from_address_list(address, "foonet"));
  EXPECT_EQ(-1, node_number_from_address_list(address, "wwivnet"));
  EXPECT_EQ(-1, node_number_from_address_list(address, "fidonet"));
  EXPECT_EQ(-1, node_number_from_address_list(address, "dorknet"));
}

TEST(NetworkNameFromAddressTest, SingleAddress) {
  const string address = "1:369/23@fidonet";
  EXPECT_EQ("fidonet", network_name_from_single_address(address));
}

// string expected_password_for(Callout* callout, int node)
TEST(ExpectedPasswordTest, Basic) {
  char opts[] = "opts";
  net_call_out_rec n{ 1234, 1, options_sendback, 2, 3, 4, "pass", 5, 6, 7, opts };
  Callout callout({ n });
  string actual = expected_password_for(&callout, 1234);
  EXPECT_EQ("pass", actual);
}

TEST(ExpectedPasswordTest, WrongNode) {
  char opts[] = "opts";
  net_call_out_rec n{ 1234, 1, options_sendback, 2, 3, 4, "pass", 5, 6, 7, opts };
  Callout callout({ n });
  string actual = expected_password_for(&callout, 12345);
  EXPECT_EQ("-", actual);
}
