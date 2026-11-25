/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                Copyright (C)2026, WWIV Software Services               */
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
#include "bbs/dropfile.h"

#include "bbs/bbs_helper.h"
#include "core/file.h"
#include "gtest/gtest.h"

#include <fstream>
#include <iterator>
#include <string>

using namespace wwiv::core;

class DropFileTest : public ::testing::Test {
protected:
  void SetUp() override {
    helper.SetUp();
    ASSERT_TRUE(helper.files().Mkdir("temp"));
    helper.sess().dirs().temp_directory(helper.files().DirName("temp"));
    a()->init_status_and_instances_for_test();
  }

  pcboard_sys_rec CreatePCBoardSys() {
    CreatePCBoardSysDropFile();
    pcboard_sys_rec pcb{};
    File f(create_dropfile_filename(drop_file_t::PCBOARD_SYS));
    EXPECT_TRUE(f.Open(File::modeReadOnly | File::modeBinary));
    EXPECT_EQ(static_cast<File::size_type>(sizeof(pcb)), f.length());
    EXPECT_EQ(static_cast<File::size_type>(sizeof(pcb)), f.Read(&pcb, sizeof(pcb)));
    return pcb;
  }

  BbsHelper helper;
};

// Fixed width fields are not null terminated, so compare the whole field.
template <size_t N> static std::string field(const char (&f)[N]) { return std::string(f, N); }

TEST_F(DropFileTest, PCBoardSys_Names) {
  helper.user()->set_name("JOHN SMITH");
  const auto pcb = CreatePCBoardSys();

  EXPECT_EQ("JOHN           ", field(pcb.firstname));
  EXPECT_EQ(std::string("JOHN SMITH") + std::string(15, '\0'), field(pcb.name));
  EXPECT_EQ(std::string("XXX") + std::string(9, '\0'), field(pcb.password));
}

// The first name, user name and language used to be written with sprintf or
// strcpy, which wrote past the end of their fields.
TEST_F(DropFileTest, PCBoardSys_LongValuesAreTruncatedToTheirFields) {
  const std::string name = "ABCDEFGHIJKLMNOPQRSTUVWXYZABCD"; // 30 chars, the maximum.
  helper.user()->set_name(name);
  helper.sess().current_language("english");
  const auto pcb = CreatePCBoardSys();

  EXPECT_EQ(name.substr(0, 15), field(pcb.firstname));
  EXPECT_EQ(name.substr(0, 25), field(pcb.name));
  EXPECT_EQ("engl", field(pcb.slanguage));
  // The fields after the name are intact.
  EXPECT_EQ(pcb.time_limit, pcb.sminsleft);
  EXPECT_EQ("01:00", field(pcb.seventtime));
}

TEST_F(DropFileTest, PCBoardSys_EmptyName) {
  helper.user()->set_name("");
  const auto pcb = CreatePCBoardSys();

  EXPECT_EQ(std::string(15, ' '), field(pcb.firstname));
  EXPECT_EQ(std::string(25, '\0'), field(pcb.name));
}

TEST_F(DropFileTest, PCBoardSys_OpenBps) {
  a()->modem_speed_ = 2400;
  EXPECT_EQ("2400 ", field(CreatePCBoardSys().openbps));

  a()->modem_speed_ = 115200;
  EXPECT_EQ("11520", field(CreatePCBoardSys().openbps));
}

static std::string ReadBinary(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

static bool HasBareLF(const std::string& s) {
  for (size_t i = 0; i < s.size(); i++) {
    if (s[i] == '\n' && (i == 0 || s[i - 1] != '\r')) {
      return true;
    }
  }
  return false;
}

// DOS doors read these, so every line ends in CR LF on every platform.
TEST_F(DropFileTest, DosDropFiles_UseCRLF) {
  [[maybe_unused]] auto _ = create_chain_file();
  for (const auto t : {drop_file_t::CHAIN_TXT, drop_file_t::DORINFO_DEF,
                       drop_file_t::CALLINFO_BBS, drop_file_t::DOOR_SYS}) {
    const auto path = create_dropfile_filename(t);
    const auto contents = ReadBinary(path);
    EXPECT_FALSE(contents.empty()) << path;
    EXPECT_NE(std::string::npos, contents.find("\r\n")) << path;
    EXPECT_FALSE(HasBareLF(contents)) << path;
  }
}

#ifndef _WIN32
// DOOR32.SYS is for 32-bit doors, so it uses the platform's line endings.
TEST_F(DropFileTest, Door32Sys_UsesPlatformLineEndings) {
  [[maybe_unused]] auto _ = create_chain_file();
  const auto contents = ReadBinary(create_dropfile_filename(drop_file_t::DOOR32_SYS));
  EXPECT_FALSE(contents.empty());
  EXPECT_EQ(std::string::npos, contents.find('\r'));
}
#endif
