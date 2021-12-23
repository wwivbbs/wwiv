/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*               Copyright (C)2014-2021, WWIV Software Services           */
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
#include "core/file.h"
#include "core/log.h"
#include "core/semaphore_file.h"
#include "core/strings.h"
#include "core/test/file_helper.h"
#include "gtest/gtest.h"
#include <chrono>
#include <string>

using namespace wwiv::core;
using namespace wwiv::strings;

TEST(SemaphoreFileTest, AlreadyAcqired) {
  const wwiv::core::test::FileHelper file;
  const auto& tmp = file.TempDir();
  std::filesystem::path path;
  {
    // Will throw if it can't acquire.
    const auto ok =
        SemaphoreFile::try_acquire(FilePath(tmp, "x.sem"), "", std::chrono::milliseconds(100));

    path = ok.path();
    LOG(INFO) << "fd: " << ok.fd() << "; fn: " << path;

    EXPECT_TRUE(File::Exists(path));

    try {
      auto nok =
          SemaphoreFile::try_acquire(FilePath(tmp, "x.sem"), "", std::chrono::milliseconds(10));
      FAIL() << "semaphore_not_acquired expected";
    } catch (const wwiv::core::semaphore_not_acquired&) {
      // expected to happen.
    }
  }
  EXPECT_FALSE(File::Exists(path)) << path;
}

TEST(SemaphoreFileTest, Smoke) {
  const wwiv::core::test::FileHelper file;
  const auto& tmp = file.TempDir();
  // Will throw if it can't acquire.
  const auto ok =
      SemaphoreFile::try_acquire(FilePath(tmp, "x.sem"), "", std::chrono::milliseconds(100));

  const auto fn = ok.path();
  LOG(INFO) << "fd: " << ok.fd() << "; fn: " << fn;

  EXPECT_TRUE(File::Exists(fn)) << fn;
}
