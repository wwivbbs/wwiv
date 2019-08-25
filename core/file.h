/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2019, WWIV Software Services            */
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

#ifndef __INCLUDED_CORE_FILE_H__
#define __INCLUDED_CORE_FILE_H__

#include <cstring>
#include <ctime>
#include <iostream>
#include <memory>
#include <string>
#include <sys/types.h>

#include "core/file_lock.h"
#include "core/filesystem.h"
#include "core/wwivport.h"

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#ifndef _WIN32
#if !defined(O_BINARY)
#define O_BINARY 0
#endif
#if !defined(O_TEXT)
#define O_TEXT 0
#endif
#endif // _WIN32

namespace wwiv {
namespace core {

/**
 * Creates a full pathname of directory_name + file_name ensuring that any
 * path separators are added as needed.
 */
std::string FilePath(const std::string& directory_name, const std::string& file_name);

/**
 * File: Provides a high level, cross-platform common wrapper for file handling using C++.
 *
 * Example:
 *   File f("/home/wwiv/bbs/config.dat");
 *   if (!f) { LOG(FATAL) << "config.dat does not exist!" << endl; }
 *   if (!f.Read(config, sizeof(configrec)) { LOG(FATAL) << "unable to load config.dat"; }
 *   // No need to close f since when f goes out of scope it'll close automatically.
 */
class File final {
public:
  // Constants
  static const int modeDefault;
  static const int modeUnknown;
  static const int modeAppend;
  static const int modeBinary;
  static const int modeCreateFile;
  static const int modeReadOnly;
  static const int modeReadWrite;
  static const int modeText;
  static const int modeWriteOnly;
  static const int modeTruncate;
  static const int modeExclusive;

  static const int shareUnknown;
  static const int shareDenyReadWrite;
  static const int shareDenyWrite;
  static const int shareDenyRead;
  static const int shareDenyNone;

  static const int permReadWrite;

  enum class Whence : int { begin = SEEK_SET, current = SEEK_CUR, end = SEEK_END };

  static const int invalid_handle;

  static const char pathSeparatorChar;
  static const char pathSeparatorString[];
  static const char separatorChar;

  // Constructor/Destructor

  /** Constructs a file from a full pathname. */
  explicit File(const std::string& full_file_name);
  /** Destructs File. Closes any open file handles. */
  virtual ~File();

  // Public Member functions
  bool Open(int nFileMode = File::modeDefault, int nShareMode = File::shareUnknown);
  void Close();
  bool IsOpen() const { return File::IsFileHandleValid(handle_); }

  ssize_t Read(void* buf, size_t count);
  ssize_t Write(const void* buf, size_t count);

  ssize_t Write(const std::string& s) { return this->Write(s.data(), s.length()); }

  ssize_t Writeln(const void* buffer, size_t nCount) {
    auto ret = this->Write(buffer, nCount);
    ret += this->Write("\r\n", 2);
    return ret;
  }

  ssize_t Writeln(const std::string& s) { return this->Writeln(s.c_str(), s.length()); }

  off_t length();
  off_t Seek(off_t lOffset, Whence whence);
  void set_length(off_t lNewLength);
  off_t current_position() const;

  bool Exists() const;
  bool Delete();

  bool IsDirectory() const;
  bool IsFile() const;

  bool SetFilePermissions(int nPermissions);
  time_t creation_time();
  time_t last_write_time();
  bool set_last_write_time(time_t last_write_time);

  std::string parent() const {
    // TODO(rushfan): 
    const auto s = full_path_name_.string();
    auto found = s.find_last_of(File::pathSeparatorChar);
    if (found == std::string::npos) {
      return {};
    }
    return s.substr(0, found);
  }

  std::string GetName() const {
    return full_path_name_.filename().string();
  }

  std::unique_ptr<wwiv::core::FileLock> lock(wwiv::core::FileLockType lock_type);

  std::string full_pathname() const noexcept { return full_path_name_.string(); }
  const std::string native() const noexcept { return full_path_name_.string(); }
  const char* c_str() const noexcept { return full_path_name_.string().c_str(); }
  std::string last_error() const { return error_text_; }

  // operators
  explicit operator bool() const { return IsOpen(); }
  friend std::ostream& operator<<(std::ostream& os, const File& f);

  // static functions
  static bool Remove(const std::string& fileName);
  static bool Remove(const std::string& directoryName, const std::string& fileName);
  static bool Rename(const std::string& origFileName, const std::string& newFileName);
  static bool Exists(const std::string& fileName);
  static bool Exists(const std::string& directoryName, const std::string& fileName);
  static bool ExistsWildcard(const std::string& wildCard);
  static bool Copy(const std::string& sourceFileName, const std::string& destFileName);
  static bool Move(const std::string& sourceFileName, const std::string& destFileName);

  static bool SetFilePermissions(const std::string& fileName, int nPermissions);
  static bool IsFileHandleValid(int hFile);

  static std::string EnsureTrailingSlash(const std::string& path);
  static std::string current_directory();
  static bool set_current_directory(const std::string& dir);
  static std::string FixPathSeparators(const std::string& path);
  static std::string absolute(const std::string& base, const std::string& relative);
  static bool is_absolute(const std::string& path);
  static bool is_relative(const std::string& path) { return !is_absolute(path); }

  static bool canonical(const std::string& path, std::string* resolved);

  /**
   * Creates the directory {path} by creating the leaf most directory.
   *
   * Returns true if the new directory is created.
   * Also returns true if there is nothing to do. This is unlike
   * filesystem::mkdir which returns false if {path} already exists.
   */
  static bool mkdir(const std::string& path);

  /**
   * Creates the directory {path} and all parent directories needed
   * along the way.
   * 
   * Returns true if the new directory is created.
   * Also returns true if there is nothing to do. This is unlike
   * filesystem::mkdir which returns false if {path} already exists.
   */
  static bool mkdirs(const std::string& path);

  /**
   * Creates the directory {path} by calling File::mkdir on the
   * full pathname of this file object.
   */
  static bool mkdir(const File& dir) { return File::mkdir(dir.full_pathname()); }

  /**
   * Creates the directory {path} by calling File::mkdirs on the
   * full pathname of this file object.
   */
  static bool mkdirs(const File& dir) { return File::mkdirs(dir.full_pathname()); }

  static long freespace_for_path(const std::string& path);
  static bool is_directory(const std::string& path);

 private:
   // Helper functions

 private: 

  int handle_{-1};
  std::filesystem::path full_path_name_;
  std::string error_text_;
};

bool backup_file(const File& file);
bool backup_file(const std::string& path);

} // namespace core
} // namespace wwiv

#endif // __INCLUDED_CORE_FILE_H__
