/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*           Copyright (C)2014-2017, WWIV Software Services               */
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

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <iostream>
#include <string>

#ifdef _WIN32
#include <io.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif  // WIN32_LEAN_AND_MEAN

#include <Windows.h>
#else
#include <sys/stat.h>
#endif

#include "core/file.h"
#include "core/os.h"
#include "core/strings.h"
#include "core/wwivport.h"

using std::string;
using namespace wwiv::strings;

FileHelper::FileHelper() {
  const ::testing::TestInfo* const test_info =
      ::testing::UnitTest::GetInstance()->current_test_info();
  const string dir = StrCat(test_info->test_case_name(), "_", test_info->name());
  tmp_ = FileHelper::CreateTempDir(dir);
}

const string FileHelper::DirName(const string& oname) const {
  string name = File::FixPathSeparators(oname);
  return StrCat(tmp_, File::pathSeparatorString, name, File::pathSeparatorString);
}

bool FileHelper::Mkdir(const string& oname) const {
  return File::mkdir(DirName(File::FixPathSeparators(oname)));
}

static const string GetTestTempDir() {
  string test_tempdir = wwiv::os::environment_variable("WWIV_TEST_TEMPDIR");
  if (test_tempdir.empty()) {
#ifdef _WIN32
    char temp_path[_MAX_PATH];
    GetTempPath(_MAX_PATH, temp_path);
    test_tempdir = temp_path;
#else  // _WIN32
    test_tempdir = "/tmp";
#endif  // _WIN32
  }
  return test_tempdir;
}

// static
string FileHelper::CreateTempDir(const string base) {
    const string temp_path = GetTestTempDir();
    string template_ = StrCat(temp_path, "/fileXXXXXX");
#ifdef _WIN32
    time_t now = time(nullptr);
    const auto fn_template = StringPrintf("%s.%lx", base.c_str(), now);
    const auto local_dir_template = wwiv::core::FilePath(temp_path, fn_template);
    if (CreateDirectory(local_dir_template.c_str(), nullptr)) {
      return local_dir_template;
    }
#else
    char local_dir_template[MAX_PATH];
    strcpy(local_dir_template, template_.c_str());
    char *result = mkdtemp(local_dir_template);
    if (result) {
      return string(result);
    }
#endif
    return "";
}

string FileHelper::CreateTempFilePath(const string& orig_name) {
  string name(File::FixPathSeparators(orig_name));
  return StrCat(TempDir(), File::pathSeparatorString, name);
}

FILE* FileHelper::OpenTempFile(const string& orig_name, string* path) {
  *path = CreateTempFilePath(orig_name);
  FILE* file = fopen(path->c_str(), "wt");
  assert(file);
  return file;
}

string FileHelper::CreateTempFile(const string& orig_name, const string& contents) {
  string path;
  FILE* file = OpenTempFile(orig_name, &path);
  assert(file);
  fputs(contents.c_str(), file);
  fclose(file);
  return path;
}

// N.B.: We don't use TextFile::ReadFileIntoString since we are
// testing TextFile with this helper.
const string FileHelper::ReadFile(const string name) const {
  std::FILE *fp = fopen(name.c_str(), "rt");
  if (!fp) {
    throw (errno);
  }
  string contents;
  fseek(fp, 0, SEEK_END);
  contents.resize(ftell(fp));
  rewind(fp);
  int num_read = fread(&contents[0], 1, contents.size(), fp);
  contents.resize(num_read);
  fclose(fp);
  return contents;
}
