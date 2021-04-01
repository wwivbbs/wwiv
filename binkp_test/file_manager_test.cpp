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
#include "gtest/gtest.h"

#include "core/strings.h"
#include "core_test/file_helper.h"
#include "core/file.h"
#include "binkp/file_manager.h"
#include "core/textfile.h"
#include <string>

using namespace wwiv::core;
using namespace wwiv::net;
using namespace wwiv::sdk::net;
using namespace wwiv::strings;

class FileManagerTest : public testing::Test {
public:
  FileManagerTest() {
    const auto& root = file_helper_.TempDir();
    config_ = std::make_unique<wwiv::sdk::Config>(root);
    config_->set_initialized_for_test(true);
    net.name = "WWIVnet";
    net.fido.process_tic = true;
    net.type = network_type_t::ftn;
    net.fido.bad_packets_dir = "b";
    net.fido.inbound_dir = "in";
    net.fido.netmail_dir = "netmail";
    net.fido.outbound_dir = "out";
    net.fido.temp_inbound_dir = "tin";
    net.fido.temp_outbound_dir = "tout";
    net.fido.tic_dir = "tic";
    net.fido.unknown_dir = "unknown";
    net.dir = FilePath(root, "net").string();
    File::mkdirs(net.dir);

    const auto dir_data_ = file_helper_.DirName("data");
    const auto dir_gfiles_ = file_helper_.DirName("gfiles");
    const auto dir_menus_ = file_helper_.DirName("menus");
    const auto dir_msgs_ = file_helper_.DirName("msgs");
    const auto dir_dloads_ = file_helper_.DirName("dloads");

    config_->set_initialized_for_test(true);
    config_->set_paths_for_test(dir_data_, dir_msgs_, dir_gfiles_, dir_menus_, dir_dloads_,
                                dir_data_);

    const auto receive_dir = FilePath(root, "r");
    fm = std::make_unique<FileManager>(*config_, net, receive_dir.string());
    r = fm->dirs().receive_dir();
    t = fm->dirs().tic_dir();
    u = fm->dirs().unknown_dir();
    bink_config_ = std::make_unique<BinkConfig>(1, *config_, net.dir.string());
  }

  [[nodiscard]] bool CreateTic(const std::filesystem::path& dir, const std::string& filename, const std::string& pw) {
    // Create TIC file
    std::filesystem::path fn{filename};
    const auto tic_name = fn.replace_extension(".tic");
    TextFile out(dir / tic_name, "wt");
    out.WriteLine("File " + filename);
    out.WriteLine("Crc 00000000");
    out.WriteLine("Size 0");
    out.WriteLine("Area Foo");
    if (!pw.empty()) {
      out.WriteLine("Pw " + pw);
    }
    out.Close();

    {
      // Create filename
      File f(dir / filename);
      f.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
      f.Close();
    }
    return true;
  }

  [[nodiscard]] bool CreateTic(const std::filesystem::path& dir, const std::string& filename) {
    return CreateTic(dir, filename, "");
  }

  FileHelper file_helper_;
  Network net{};
  std::unique_ptr<FileManager> fm;
  std::unique_ptr<wwiv::sdk::Config> config_;
  std::unique_ptr<BinkConfig> bink_config_;
  std::filesystem::path r;
  std::filesystem::path t;
  std::filesystem::path u;
};

TEST_F(FileManagerTest, Basic) {
  ASSERT_TRUE(CreateTic(r, "foo.zip"));

  fm->ReceiveFile("foo.zip");
  fm->ReceiveFile("foo.tic");
  const Remote remote(bink_config_.get(), false, "1:1/1");
  fm->rename_ftn_pending_files(remote);

  EXPECT_FALSE(File::Exists(r / "foo.zip"));
  EXPECT_FALSE(File::Exists(r / "foo.tic"));

  EXPECT_TRUE(File::Exists(t / "foo.zip"));
  EXPECT_TRUE(File::Exists(t / "foo.tic"));
}

TEST_F(FileManagerTest, NoProcessTic) {
  ASSERT_TRUE(CreateTic(r, "foo.zip"));

  net.fido.process_tic = false;
  const auto receive_dir = FilePath(file_helper_.TempDir(), "r");
  fm = std::make_unique<FileManager>(*config_, net, receive_dir.string());
  r = fm->dirs().receive_dir();
  t = fm->dirs().tic_dir();
  u = fm->dirs().unknown_dir();
  fm->ReceiveFile("foo.zip");
  fm->ReceiveFile("foo.tic");

  const Remote remote(bink_config_.get(), false, "1:1/1");
  fm->rename_ftn_pending_files(remote);

  EXPECT_FALSE(File::Exists(r / "foo.zip"));
  EXPECT_FALSE(File::Exists(r / "foo.tic"));
  EXPECT_FALSE(File::Exists(t / "foo.zip"));
  EXPECT_FALSE(File::Exists(t / "foo.tic"));

  EXPECT_TRUE(File::Exists(u / "foo.zip"));
  EXPECT_TRUE(File::Exists(u / "foo.tic"));
}

TEST_F(FileManagerTest, WithPassword) {
  ASSERT_TRUE(CreateTic(r, "foo.zip", "rush"));

  fm->ReceiveFile("foo.zip");
  fm->ReceiveFile("foo.tic");
  wwiv::sdk::fido::FidoCallout callout(*config_, net);
  const wwiv::sdk::fido::FidoAddress remote_addr("1:1/1");
  fido_node_config_t node_config{};
  node_config.packet_config.tic_password = "rush";
  callout.insert(remote_addr, node_config);
  const Remote remote(bink_config_.get(), false, "1:1/1");
  fm->rename_ftn_pending_files(remote);

  EXPECT_TRUE(File::Exists(t / "foo.zip"));
  EXPECT_TRUE(File::Exists(t / "foo.tic"));
}

TEST_F(FileManagerTest, WithPassword_WrongPassword) {
  ASSERT_TRUE(CreateTic(r, "foo.zip", "styx"));

  fm->ReceiveFile("foo.zip");
  fm->ReceiveFile("foo.tic");
  wwiv::sdk::fido::FidoCallout callout(*config_, net);
  const wwiv::sdk::fido::FidoAddress remote_addr("1:1/1");
  fido_node_config_t node_config{};
  node_config.packet_config.tic_password = "rush";
  node_config.binkp_config.host = "mystic.wwivbbs.org";
  callout.insert(remote_addr, node_config);
  callout.Save();
  const Remote remote(bink_config_.get(), false, "1:1/1");
  fm->rename_ftn_pending_files(remote);

  EXPECT_TRUE(File::Exists(u / "foo.zip"));
  EXPECT_TRUE(File::Exists(u / "foo.tic"));
}