#include "gtest/gtest.h"
#include "core_test/file_helper.h"
#include "networkb/binkp.h"
#include "networkb_test/fake_connection.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <thread>

using std::string;
using std::thread;
using std::unique_ptr;
using namespace wwiv::net;

static const int ANSWERING_ADDRESS = 1;
static const int ORIGINATING_ADDRESS = 2;


class BinkTest : public testing::Test {
protected:
  void Start() {
    binkp_.reset(new BinkP(&conn_, BinkSide::ANSWERING, ANSWERING_ADDRESS, ORIGINATING_ADDRESS));
    thread_ = thread([&]() {binkp_->Run(); });
  } 

  void Stop() {
    thread_.join();
  }

  unique_ptr<BinkP> binkp_;
  FakeConnection conn_;
  std::thread thread_;
};

TEST_F(BinkTest, ErrorAbortsSession) {
  Start();
  conn_.ReplyCommand(M_ERR, "Doh!");
  Stop();
  
  conn_.GetNextPacket();
};
