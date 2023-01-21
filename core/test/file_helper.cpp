/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*           Copyright (C)2014-2022, WWIV Software Services               */
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
#include "core/test/file_helper.h"

#include "core/command_line.h"
#include "core/datetime.h"
#include "core/file.h"
#include "core/log.h"
#include "core/os.h"
#include "core/strings.h"
#include "fmt/format.h"
#include "gtest/gtest.h"
#include <cerrno>
#include <cstdio>
#include <filesystem>
#include <string>

using namespace wwiv::core;
using namespace wwiv::strings;

namespace wwiv::core::test {

// Base test directory.
std::filesystem::path FileHelper::basedir_;
std::filesystem::path FileHelper::testdata_;

FileHelper::FileHelper() {
  const auto* const test_info = testing::UnitTest::GetInstance()->current_test_info();
  const auto dir = fmt::format("{}_{}", test_info->test_suite_name(), test_info->name());
  tmp_ = File::canonical(CreateTempDir(dir));
}

std::string FileHelper::DirName(const std::string& oname) const {
  const auto name = File::FixPathSeparators(oname);
  const auto tmpname = FilePath(tmp_, name).string();
  return File::EnsureTrailingSlash(tmpname);
}

std::filesystem::path FileHelper::Dir(const std::string& oname) const {
  const auto name = File::FixPathSeparators(oname);
  return FilePath(tmp_, name);
}

bool FileHelper::Mkdir(const std::string& oname) const {
  return File::mkdir(Dir(File::FixPathSeparators(oname)));
}

// static
void FileHelper::set_wwiv_test_tempdir(const std::string& d) noexcept {
  // Use absolute vs. canonical here so that if you have symlinks
  // or other things (like subst drives), you can use that to shorten
  // the path -- making it less likely wwiv 4.x data structures will
  // blow up on path length.
  basedir_ = File::absolute(d);
}

void FileHelper::set_wwiv_test_tempdir_from_commandline(int argc, char* argv[]) noexcept {
  CommandLine cmdline(argc, argv, "net");
  cmdline.AddStandardArgs();
  cmdline.add_argument(
      {"wwiv_test_tempdir", "Use instead of WWIV_TEST_TEMPDIR environment variable.", ""});
  cmdline.add_argument(
      {"wwiv_testdata", "Sets the location of the testdata directory for this test.", ""});
  cmdline.set_no_args_allowed(true);
  if (!cmdline.Parse()) {
    LOG(ERROR) << "Failed to parse cmdline.";
  }

  auto tmpdir = cmdline.arg("wwiv_test_tempdir").as_string();
  if (tmpdir.empty()) {
    tmpdir = wwiv::os::environment_variable("WWIV_TEST_TEMPDIR");
  }
  if (!tmpdir.empty()) {
    if (!File::Exists(tmpdir)) {
      File::mkdirs(tmpdir);
    }
    File::set_current_directory(tmpdir);
    set_wwiv_test_tempdir(tmpdir);
  }

  auto testdata = cmdline.arg("wwiv_testdata").as_string();
  if (testdata.empty()) {
    testdata = wwiv::os::environment_variable("WWIV_TESTDATA");
  }
  FileHelper::testdata_ = testdata;
}

// static
std::filesystem::path FileHelper::TestData() {
  CHECK(!testdata_.empty()) << "WWIV_TESTDATA must be set for using testdata.";
  CHECK(File::Exists(testdata_))
      << "WWIV_TESTDATA must be set to a directory that exists for using testdata.";
  LOG(INFO) << "TestData: " << testdata_;
  return testdata_;
}

// static
bool FileHelper::HasTestData() { return !testdata_.empty(); }

  // static
std::filesystem::path FileHelper::GetTestTempDir() {
  if (!basedir_.empty()) {
    return basedir_;
  }
  const auto raw_temp_path = File::canonical(std::filesystem::temp_directory_path());
  VLOG(2) << "raw_temp_path: " << raw_temp_path;
  const auto temp_path = File::absolute(raw_temp_path);
  VLOG(1) << "temp_path: " << temp_path;
  auto path = temp_path / "wwiv_test_out";
  if (!exists(path)) {
    create_directories(path);
  }
  return path;
}

// static
std::filesystem::path FileHelper::CreateTempDir(const std::string& base) {
  const auto temp_path = GetTestTempDir();
  // TODO(rushfan): This may be good enough for linux too.
#if defined(_WIN32) || defined(__OS2__)
  auto dir = FilePath(temp_path, StrCat(base, ".", time_t_now()));
  if (std::error_code ec; create_directories(dir, ec)) {
    return dir;
  }
#else
  char local_dir_template[MAX_PATH];
  const auto template_ = StrCat(temp_path.string(), "/", base, "XXXXXX");
  to_char_array(local_dir_template, template_);
  char* result = mkdtemp(local_dir_template);
  if (result) {
    return std::filesystem::path{result};
  }
#endif
  return {};
}

std::filesystem::path FileHelper::CreateTempFilePath(const std::string& orig_name) const {
  const auto name{File::FixPathSeparators(orig_name)};
  return File::canonical(FilePath(TempDir(), name));
}

std::tuple<FILE*, std::filesystem::path>
FileHelper::OpenTempFile(const std::string& orig_name) const {
  const auto path = CreateTempFilePath(orig_name);
  const auto fn = path.string();
  auto* fp = fopen(fn.c_str(), "wt");
  assert(fp);
  return std::make_tuple(fp, path);
}

// ReSharper disable once CppMemberFunctionMayBeConst
std::filesystem::path FileHelper::CreateTempFile(const std::string& orig_name,
                                                 const std::string& contents) {
  auto [file, path] = OpenTempFile(orig_name);
  assert(file);
  fputs(contents.c_str(), file);
  fclose(file);
  return path;
}

// N.B.: We don't use TextFile::ReadFileIntoString since we are
// testing TextFile with this helper.
std::string FileHelper::ReadFile(const std::filesystem::path& name) const {
  const auto name_string = name.string();
  // ReSharper disable once CppLocalVariableMayBeConst
  auto* fp = fopen(name_string.c_str(), "rt");
  if (!fp) {
    const auto msg = StrCat("Unable to open file: ", name_string, "; errno: ", errno);
    throw std::runtime_error(msg);
  }
  std::string contents;
  fseek(fp, 0, SEEK_END);
  contents.resize(ftell(fp));
  rewind(fp);
  const auto num_read = fread(&contents[0], 1, contents.size(), fp);
  contents.resize(num_read);
  fclose(fp);
  return contents;
}

} // namespace wwiv::core::test
