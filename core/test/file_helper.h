/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2022, WWIV Software Services             */
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
#ifndef INCLUDED_CORE_TEST_FILE_HELPER_H
#define INCLUDED_CORE_TEST_FILE_HELPER_H

#include <cstdio>
#include <string>
#include <tuple>
#include <filesystem>

namespace wwiv::core::test {

/**
 * Helper class for tests requiring local filesystem access.
 *
 * Note: This class can not use File since it is used by the tests for File.
 */
class FileHelper {
public:
  FileHelper();
  // Returns a fully qualified path name to "name" under the temporary directory.
  // This will end with a pathSeparator and is suitable for use
  // in wwiv data structures.
  [[nodiscard]] std::string DirName(const std::string& name) const;
  // Returns a fully qualified path to "name" under the temporary directory.
  // This is suitable for use in constructing paths
  [[nodiscard]] std::filesystem::path Dir(const std::string& name) const;
  // Creates a directory under TempDir.
  bool Mkdir(const std::string& name) const; // NOLINT(modernize-use-nodiscard)
  // Creates a path to a filename under TempDir named 'name'.  Does not create nor
  // write any contents to this file.
  [[nodiscard]] std::filesystem::path CreateTempFilePath(const std::string& name) const;
  // Opens a FILE* handle to a file named 'name' under TempDir
  [[nodiscard]] std::tuple<FILE*, std::filesystem::path>
  OpenTempFile(const std::string& name) const;
  // Creates a a file under TempDir named 'name' containing the text 'contents'
  std::filesystem::path CreateTempFile(const std::string& name, const std::string& contents);
  // Returns the path of the temp directory for this testcase.
  [[nodiscard]] const std::filesystem::path& TempDir() const { return tmp_; }
  // Reads and returns all of the text in a file under TempDir named 'name'
  [[nodiscard]] std::string ReadFile(const std::filesystem::path& name) const;
  static void set_wwiv_test_tempdir(const std::string& d) noexcept;
  static void set_wwiv_test_tempdir_from_commandline(int argc, char* argv[]) noexcept;

  // Gets the root testdata directory specified as "WWIV_TESTDATA" on the environment
  [[nodiscard]] static std::filesystem::path TestData();
  [[nodiscard]] static bool HasTestData();

private:
  static std::filesystem::path GetTestTempDir();
  static std::filesystem::path CreateTempDir(const std::string& base);
  std::filesystem::path tmp_;
  static std::filesystem::path basedir_;
  static std::filesystem::path testdata_;
};

} // namespace wwiv::core::test

#endif
