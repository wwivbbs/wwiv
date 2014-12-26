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
    const string line("@1 example.com -");
    files_.CreateTempFile("addresses.binkp", line);
    const string network_dir = files_.DirName("network");
    BinkConfig* dummy_config = new BinkConfig(ORIGINATING_ADDRESS, "Dummy", network_dir);
    Callout* dummy_callout = new Callout(network_dir);
    BinkP::received_transfer_file_factory_t null_factory = [](const string& filename) { return new InMemoryTransferFile(filename, ""); };
    binkp_.reset(new BinkP(&conn_, dummy_config, dummy_callout, BinkSide::ANSWERING, ANSWERING_ADDRESS, null_factory));
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
    std::clog << conn_.GetNextPacket().debug_string() << std::endl;
  }
};
