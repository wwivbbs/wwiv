/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*           Copyright (C)2014-2021, WWIV Software Services               */
/*                                                                        */
/*    Licensed  under the  Apache License, Version  2.0 (the "License");  */
/*    you may not use this  file  except in compliance with the License.  */
/*    You may obtain a copy of the License at                             */
/*                                                                        */
/*                http://www.apache.org/licenses/LICENSE-2.0              */
/*                                                                        */
/*    Unless  required  by  applicable  law  or agreed to  in  writing,   */
/*    software  distributed  under  the  License  is  distributed on an   */
/*    "AS IS"  BASIS, WIT`OUT  WARRANTIES  OR  CONDITIONS OF ANY  KIND,   */
/*    either  express  or implied.  See  the  License for  the specific   */
/*    language governing permissions and limitations under the License.   */
/*                                                                        */
/**************************************************************************/
#include "gtest/gtest.h"

#include "common_test/common_helper.h"

#include "common/output.h"
#include "core/file.h"
#include "core/log.h"
#include "sdk/config.h"
#include "sdk/user.h"
#include <algorithm>
#include <memory>
#include <string>
#include <utility>

using std::begin;
using std::end;
using std::move;
using std::make_unique;
using std::unique_ptr;
using std::replace;

using namespace wwiv::core;
using namespace wwiv::sdk;

CommonHelper::CommonHelper(int) {
  io_.reset(new TestIO());
  const auto& temp = files_.TempDir();
  // We want the "BBS Home" to be our temp dir.
  CHECK(chdir(temp.string().c_str()) == 0);

  files_.Mkdir("data");
  files_.Mkdir("gfiles");
  files_.Mkdir("en");
  files_.Mkdir("en/gfiles");

  dir_data_ = files_.DirName("data");
  dir_gfiles_ = files_.DirName("gfiles");
  dir_en_gfiles_ = files_.DirName("en/gfiles");
  dir_menus_ = files_.DirName("menus");
  dir_msgs_ = files_.DirName("msgs");
  dir_dloads_ = files_.DirName("dloads");
#ifdef _WIN32
  dir_gfiles_ = File::FixPathSeparators(dir_gfiles_);
  dir_en_gfiles_ = File::FixPathSeparators(dir_en_gfiles_);
#endif  // _WIN32

  config_ = std::make_unique<Config>(temp);
  // Grab a raw pointer before moving it to Application
  config_->set_initialized_for_test(true);
  config_->set_paths_for_test(dir_data_, dir_msgs_, dir_gfiles_, dir_menus_, dir_dloads_,
                              dir_data_);
}

CommonHelper::CommonHelper() : CommonHelper(42) {
  // Use our own local IO class that will capture the output.
  const auto& temp = files_.TempDir();
  common_session_context_ = std::make_unique<wwiv::common::SessionContext>(io_->local_io(), temp);
  context_ = std::make_unique<TestContext>(*this);
}

void CommonHelper::SetUp() {

  sess().dirs().language_directory(dir_en_gfiles_);
  user()->ClearStatusFlag(User::pauseOnPage);
  sess().num_screen_lines(std::numeric_limits<int>::max());

  // Create a reasonable default user.  Some tests (bputch/bputs tests)
  // Require a properly constructed user.
  User::CreateNewUserRecord(user(), 50, 20, 0, 0.1234f, 
  { 7, 11, 14, 13, 31, 10, 12, 9, 5, 3 }, { 7, 15, 15, 15, 112, 15, 15, 7, 7, 7 });
  user()->SetStatusFlag(User::ansi);
  user()->SetStatusFlag(User::status_color);

  // Reset the color attribute to 7 between tests.
  bout.curatr(7);
  // We need this true so our bputch tests can capture remote.
  sess().outcom(true);
  sess().ok_modem_stuff(true);
}

Config& CommonHelper::config() const {
  return *config_;
}

wwiv::common::SessionContext& CommonHelper::sess() {
  CHECK(common_session_context_.get());
  return *common_session_context_;
}


wwiv::common::Context& CommonHelper::context() {
  return *context_;
}

TestIO::TestIO() {
  local_io_ = new TestLocalIO(&this->captured_);
  remote_io_ = new TestRemoteIO(&this->rcaptured_);
}

std::string TestIO::captured() {
  auto captured(captured_);
  captured_.clear();
  return captured;
}

std::string TestIO::rcaptured() {
  auto captured(rcaptured_);
  rcaptured_.clear();
  return captured;
}

TestLocalIO::TestLocalIO(std::string* captured) : LocalIO(), captured_(captured) {}

void TestLocalIO::Putch(unsigned char ch) {
  captured_->push_back(ch);
}

TestRemoteIO::TestRemoteIO(std::string* captured) : RemoteIO(), captured_(captured) {}

unsigned int TestRemoteIO::put(unsigned char ch) {
  captured_->push_back(ch);
  return 1;
}

unsigned int TestRemoteIO::write(const char *buffer, unsigned int count, bool) {
  captured_->append(std::string(buffer, count));
  return count;
}

