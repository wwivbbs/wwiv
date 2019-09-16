/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2019, WWIV Software Services             */
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
#ifndef __INCLUDED_FILE_HELPER_H__
#define __INCLUDED_FILE_HELPER_H__

#include <cstdio>
#include <string>
#include <tuple>
#include <filesystem>

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
  // in wwiv datastructures.
  [[nodiscard]] std::string DirName(const std::string& name) const;
  // Returns a fully qualified path to "name" under the temporary directory.
  // This is suitable for use in constructing paths
  [[nodiscard]] std::filesystem::path Dir(const std::string& name) const;
  // Creates a directory under TempDir.
  bool Mkdir(const std::string& name) const;
  std::filesystem::path CreateTempFilePath(const std::string& name) const;
  std::tuple<FILE*, std::filesystem::path> OpenTempFile(const std::string& name) const;
  std::filesystem::path CreateTempFile(const std::string& name, const std::string& contents);
  [[nodiscard]] const std::filesystem::path& TempDir() const { return tmp_; }
  [[nodiscard]] std::string ReadFile(const std::filesystem::path& name) const;
  static void set_wwiv_test_tempdir(const std::string& d) noexcept;

private:
  static std::filesystem::path GetTestTempDir();
  static std::filesystem::path CreateTempDir(const std::string& base);
  std::filesystem::path tmp_;
  static std::filesystem::path basedir_;
};

#endif // __INCLUDED_FILE_HELPER_H__