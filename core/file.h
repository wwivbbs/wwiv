/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2020, WWIV Software Services            */
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

#ifndef INCLUDED_CORE_FILE_H
#define INCLUDED_CORE_FILE_H

#include "core/file_lock.h"
#include "core/wwivport.h"
#include <ctime>
#include <filesystem>
#include <memory>
#include <string>

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

namespace wwiv::core {


/**
 * Creates a full std::filesystem::path of directory_name + file_name ensuring that any
 * path separators are added as needed.
 */
std::filesystem::path FilePath(const std::filesystem::path& directory_name,
                                   const std::filesystem::path& file_name);

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

  // Types.  This should eventually switch to a type supporting
  // Large files.   long is what off_t was.
  using size_type = ssize_t;

  // Constructor/Destructor

  /** Constructs a file from a path. */
  explicit File(std::filesystem::path full_path_name);
  /** Destructs File. Closes any open file handles. */
  File(File&& other) noexcept;
  File& operator=(File&& other) noexcept;

  ~File();

  // Public Member functions
  bool Open(int nFileMode = modeDefault, int nShareMode = shareUnknown);
  void Close() noexcept;
  [[nodiscard]] bool IsOpen() const noexcept;

  size_type Read(void* buf, size_type size);
  size_type Write(const void* buffer, size_type count);

  size_type Write(const std::string& s) { return this->Write(s.data(), s.length()); }

  size_type Writeln(const void* buffer, size_type count) {
    auto ret = this->Write(buffer, count);
    ret += this->Write("\r\n", 2);
    return ret;
  }

  size_type Writeln(const std::string& s) { return this->Writeln(s.c_str(), s.length()); }

  [[nodiscard]] size_type length() const noexcept;
  size_type Seek(size_type offset, Whence whence);
  void set_length(size_type l);
  [[nodiscard]] size_type current_position() const;

  [[nodiscard]] bool Exists() const noexcept;

  [[nodiscard]] time_t last_write_time() const;
  bool set_last_write_time(time_t last_write_time) noexcept;

  std::unique_ptr<FileLock> lock(FileLockType lock_type);

  /** Returns the file path as a std::string path */
  [[nodiscard]] std::string full_pathname() const noexcept { return full_path_name_.string(); }

  /** Returns the file path as a std::filesystem path */
  [[nodiscard]] const std::filesystem::path& path() const noexcept { return full_path_name_; }

  [[nodiscard]] std::string last_error() const noexcept { return error_text_; }

  // operators
  /** Returns true if the file is open */
  explicit operator bool() const noexcept { return IsOpen(); }
  friend std::ostream& operator<<(std::ostream& os, const File& f);

  // static functions
  /**
   * Removes a file or empty directory referred to by path.
   * If force is true, then also reset the permissions to Read/Write before
   * calling delete in case the permissions were read-only.
   */
  static bool Remove(const std::filesystem::path& path, bool force = false);
  static bool Rename(const std::filesystem::path& origFileName,
                     const std::filesystem::path& newFileName);
  [[nodiscard]] static bool Exists(const std::filesystem::path& p);
  [[nodiscard]] static bool ExistsWildcard(const std::filesystem::path& wildCard);
  static bool Copy(const std::filesystem::path& from,
                   const std::filesystem::path& to);
  static bool Move(const std::filesystem::path& from,
                   const std::filesystem::path& to);

  static bool SetFilePermissions(const std::filesystem::path& path, int perm);

  [[nodiscard]] static std::string EnsureTrailingSlash(const std::filesystem::path& path);
  [[nodiscard]] static std::filesystem::path current_directory();
  static bool set_current_directory(const std::filesystem::path& dir);
  [[nodiscard]] static std::string FixPathSeparators(const std::string& path);
  [[nodiscard]] static std::filesystem::path absolute(const std::filesystem::path& base,
                                                      const std::filesystem::path& relative);

  [[nodiscard]] static time_t last_write_time(const std::filesystem::path& path);

  /**
   * Returns an canonical absolute path.
   *
   * That means there are no dot or dot-dots or double-slashes in a non-UNC
   * portion of the path.  On POSIX systems, this is congruent with how
   * realpath behaves.
   */
  [[nodiscard]] static std::string canonical(const std::string& path);

  /**
   * Creates the directory {path} by creating the leaf most directory.
   *
   * Returns true if the new directory is created.
   * Also returns true if there is nothing to do. This is unlike
   * filesystem::mkdir which returns false if {path} already exists.
   */
  static bool mkdir(const std::filesystem::path& path);

  /**
   * Creates the directory {path} and all parent directories needed
   * along the way.
   *
   * Returns true if the new directory is created.
   * Also returns true if there is nothing to do. This is unlike
   * filesystem::mkdir which returns false if {path} already exists.
   */
  static bool mkdirs(const std::filesystem::path& path);

  /**
   * Creates the directory {path} by calling File::mkdir on the
   * full pathname of this file object.
   */
  static bool mkdir(const File& dir) { return mkdir(dir.full_pathname()); }

  /**
   * Creates the directory {path} by calling File::mkdirs on the
   * full pathname of this file object.
   */
  static bool mkdirs(const File& dir) { return mkdirs(dir.full_pathname()); }

  /** Returns the number of free space in kilobytes. i.e. 1 = 1024 free bytes. */
  [[nodiscard]] static long freespace_for_path(const std::filesystem::path& p);
  [[nodiscard]] static bool is_directory(const std::string& path) noexcept;

  /** For debugging and testing only */
  int handle() const noexcept { return handle_; }

private:
  // Helper functions
  [[nodiscard]] static bool IsFileHandleValid(int handle) noexcept;

private:
  int handle_{-1};
  std::filesystem::path full_path_name_;
  std::string error_text_;
};

/** Makes a backup of path using a custom suffix with the time and date */
bool backup_file(const std::filesystem::path& from, int max_backups = 0);

} // namespace

#endif
