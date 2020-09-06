/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*           Copyright (C)2014-2020, WWIV Software Services               */
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
#include "bbs_helper.h"
#include <sstream>

#include <algorithm>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "bbs/bbs.h"

#include "bbs/application.h"
#include "core/wwivport.h"
#include "core/file.h"
#include "sdk/config.h"
#include "sdk/user.h"

#include "gtest/gtest.h"

using std::begin;
using std::end;
using std::move;
using std::string;
using std::make_unique;
using std::unique_ptr;
using std::replace;

using namespace wwiv::core;
using namespace wwiv::sdk;

void BbsHelper::SetUp() {
  auto temp = files_.TempDir();
  // We want the "BBS Home" to be our temp dir.
  chdir(temp.string().c_str());

  ASSERT_TRUE(files_.Mkdir("data"));
  ASSERT_TRUE(files_.Mkdir("gfiles"));
  ASSERT_TRUE(files_.Mkdir("en"));
  ASSERT_TRUE(files_.Mkdir("en/gfiles"));
  // Use our own local IO class that will capture the output.
  io_.reset(new TestIO());
  app_.reset(CreateSession(io_->local_io()));
  a()->SetCommForTest(io_->remote_io());

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

  auto sysconfig = make_unique<configrec>();

  a()->context().dirs().language_directory(dir_en_gfiles_);
  auto config = make_unique<Config>(temp);
  config->set_initialized_for_test(true);
  config->set_paths_for_test(dir_data_, dir_msgs_, dir_gfiles_, dir_menus_, dir_dloads_, dir_data_);
  a()->set_config_for_test(move(config));
  user_ = a()->user();
  // No pause in tests.
  user_->ClearStatusFlag(User::pauseOnPage);
  a()->context().num_screen_lines(std::numeric_limits<int>::max());

  // Create a reasonable default user.  Some tests (bputch/bputs tests)
  // Require a properly constructed user.
  User::CreateNewUserRecord(user_, 50, 20, 0, 0.1234f, 
  { 7, 11, 14, 13, 31, 10, 12, 9, 5, 3 }, { 7, 15, 15, 15, 112, 15, 15, 7, 7, 7 });
  user_->SetStatusFlag(User::ansi);
  user_->SetStatusFlag(User::status_color);

  a()->instance_number_ = 42;

  // Reset the color attribute to 7 between tests.
  bout.curatr(7);
  // We need this true so our bputch tests can capture remote.
  a()->context().outcom(true);
  a()->context().ok_modem_stuff(true);
}

void BbsHelper::TearDown() {
}

TestIO::TestIO() {
  local_io_ = new TestLocalIO(&this->captured_);
  remote_io_ = new TestRemoteIO(&this->rcaptured_);
}

string TestIO::captured() {
  string captured(captured_);
  captured_.clear();
  return captured;
}

string TestIO::rcaptured() {
  string captured(rcaptured_);
  rcaptured_.clear();
  return captured;
}

TestLocalIO::TestLocalIO(string* captured) : LocalIO(), captured_(captured) {}

void TestLocalIO::Putch(unsigned char ch) {
  captured_->push_back(ch);
}

TestRemoteIO::TestRemoteIO(std::string* captured) : RemoteIO(), captured_(captured) {}

unsigned int TestRemoteIO::put(unsigned char ch) {
  captured_->push_back(ch);
  return 1;
}

unsigned int TestRemoteIO::write(const char *buffer, unsigned int count, bool) {
  captured_->append(string(buffer, count));
  return count;
}

