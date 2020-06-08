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
/*    "AS IS"  BASIS, WITHOUT  WARRANTIES  OR  CONDITIONS OF ANY  KIND,   */
/*    either  express  or implied.  See  the  License for  the specific   */
/*    language governing permissions and limitations under the License.   */
/*                                                                        */
/**************************************************************************/
#include "file_helper.h"
#include "gtest/gtest.h"

#include <cerrno>
#include <cstdio>
#include <string>

#include "core/datetime.h"
#include "core/file.h"
#include <filesystem>
#include "core/strings.h"

using std::string;
using namespace wwiv::core;
using namespace wwiv::strings;

// Base test directory.
std::filesystem::path FileHelper::basedir_;

FileHelper::FileHelper() {
  const auto* const test_info = ::testing::UnitTest::GetInstance()->current_test_info();
  const auto dir = StrCat(test_info->test_case_name(), "_", test_info->name());
  tmp_ = CreateTempDir(dir);
}

string FileHelper::DirName(const string& oname) const {
  const auto name = File::FixPathSeparators(oname);
  const auto tmpname = FilePath(tmp_, name).string();
  return File::EnsureTrailingSlash(tmpname);
}

std::filesystem::path FileHelper::Dir(const string& oname) const {
  const auto name = File::FixPathSeparators(oname);
  return FilePath(tmp_, name);
}

bool FileHelper::Mkdir(const string& oname) const {
  return File::mkdir(Dir(File::FixPathSeparators(oname)));
}

// static
void FileHelper::set_wwiv_test_tempdir(const std::string& d) noexcept {
  basedir_ = std::filesystem::canonical(d);
}

// static
std::filesystem::path FileHelper::GetTestTempDir() {
  if (!basedir_.empty()) {
    return basedir_;
  }
  const auto temp_path = canonical(std::filesystem::temp_directory_path());
  auto path = temp_path / "wwiv_test_out";
  if (!exists(path)) {
    create_directories(path);
  }
  return path;
}

// static
std::filesystem::path FileHelper::CreateTempDir(const string& base) {
  const auto temp_path = GetTestTempDir();
  // TODO(rushfan): This may be good enough for linux too.
#ifdef _WIN32
  auto dir = wwiv::core::FilePath(temp_path, StrCat(base, ".", time_t_now()));
  std::error_code ec;
  if (std::filesystem::create_directories(dir, ec)) {
    return dir;
  }
#else
  char local_dir_template[MAX_PATH];
  const auto template_ = StrCat(temp_path.string(), "/fileXXXXXX");
  to_char_array(local_dir_template, template_);
  char* result = mkdtemp(local_dir_template);
  if (result) {
    return std::filesystem::path{result};
  }
#endif
  return {};
}

std::filesystem::path FileHelper::CreateTempFilePath(const string& orig_name) const {
  const auto name{File::FixPathSeparators(orig_name)};
  return FilePath(TempDir(), name);
}

std::tuple<FILE*, std::filesystem::path> FileHelper::OpenTempFile(const string& orig_name) const {
  const auto path = CreateTempFilePath(orig_name);
  const auto fn = path.string();
  auto* fp = fopen(fn.c_str(), "wt");
  assert(fp);
  return std::make_tuple(fp, path);
}

std::filesystem::path FileHelper::CreateTempFile(const string& orig_name, const string& contents) {
  auto [file, path] = OpenTempFile(orig_name);
  assert(file);
  fputs(contents.c_str(), file);
  fclose(file);
  return path;
}

// N.B.: We don't use TextFile::ReadFileIntoString since we are
// testing TextFile with this helper.
string FileHelper::ReadFile(const std::filesystem::path& name) const {
  const auto name_string = name.string();
  // ReSharper disable once CppLocalVariableMayBeConst
  auto* fp = fopen(name_string.c_str(), "rt");
  if (!fp) {
    const auto msg = StrCat("Unable to open file: ", name_string, "; errno: ", errno);
    throw std::runtime_error(msg);
  }
  string contents;
  fseek(fp, 0, SEEK_END);
  contents.resize(ftell(fp));
  rewind(fp);
  const auto num_read = fread(&contents[0], 1, contents.size(), fp);
  contents.resize(num_read);
  fclose(fp);
  return contents;
}
