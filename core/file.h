/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2016,WWIV Software Services             */
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

#ifndef __INCLUDED_PLATFORM_WFILLE_H__
#define __INCLUDED_PLATFORM_WFILLE_H__

#include <cstring>
#include <ctime>
#include <iostream>
#include <string>
#include <sys/types.h>

#include "core/wwivport.h"

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#ifdef __unix__
#define O_BINARY  0
#define O_TEXT    0
#endif  // __unix__

namespace wwiv {
namespace core {
/**
* Creates a full pathname of directory_name + file_name ensuring that any
* path separators are added as needed.
*/
std::string FilePath(const std::string& directory_name, const std::string& file_name);

}  // namespace core
}  // namespace wwiv

/**
 * File - File I/O Class.
 */
class File {
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

  enum class Whence : int {
    begin = SEEK_SET, 
    current = SEEK_CUR, 
    end = SEEK_END
  };

  static const int invalid_handle;

  static const char pathSeparatorChar;
  static const char pathSeparatorString[];
  static const char separatorChar;

  // Constructor/Destructor
  File();
  File(const std::string& dir, const std::string& filename);
  explicit File(const std::string& full_file_name);
  virtual ~File();

  // Public Member functions
  virtual bool SetName(const std::string& fileName);
  virtual bool SetName(const std::string& dirName, const std::string& fileName);
  virtual bool Open(int nFileMode = File::modeDefault,
                    int nShareMode = File::shareUnknown);
  virtual void Close();
  virtual bool IsOpen() const { return File::IsFileHandleValid(handle_); }

  virtual ssize_t Read(void * pBuffer, size_t nCount);
  virtual ssize_t Write(const void * pBuffer, size_t nCount);

  virtual ssize_t Write(const std::string& s) {
    return this->Write(s.data(), s.length());
  }
  
  virtual ssize_t Writeln(const void *pBuffer, size_t nCount) {
    auto ret = this->Write(pBuffer, nCount);
    ret += this->Write("\r\n", 2);
    return ret;
  }

  virtual ssize_t Writeln(const std::string& s) {
    return this->Writeln(s.c_str(), s.length());
  }

  virtual off_t GetLength();
  virtual off_t Seek(off_t lOffset, Whence whence);
  virtual void SetLength(off_t lNewLength);
  virtual off_t current_position() const;

  virtual bool Exists() const;
  virtual bool Delete();

  virtual bool IsDirectory() const;
  virtual bool IsFile() const ;

  virtual bool SetFilePermissions(int nPermissions);
  virtual time_t creation_time();
  virtual time_t last_write_time();
  virtual bool set_last_write_time(time_t last_write_time);

  virtual const std::string GetParent() const {
    size_t found = full_path_name_.find_last_of(File::pathSeparatorChar);
    if (found == std::string::npos) {
      return std::string("");
    }
    return full_path_name_.substr(0, found);
  }

  virtual std::string GetName() const {
    size_t found = full_path_name_.find_last_of(File::pathSeparatorChar);
    if (found == std::string::npos) {
      return std::string("");
    }
    return full_path_name_.substr(found + 1);
  }

  virtual const std::string full_pathname() const { return full_path_name_; }
  virtual const std::string GetLastError() const { return error_text_; }

  // operators
  explicit operator bool() const { return IsOpen(); }
  friend std::ostream& operator<< (std::ostream &os, const File &f);

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

  static void EnsureTrailingSlash(std::string* path);
  static std::string current_directory();
  static bool set_current_directory(const std::string& dir);
  static void FixPathSeparators(std::string* path);
  static std::string FixPathSeparators(const std::string& path);
  static void MakeAbsolutePath(const std::string& base, std::string* relative);
  static std::string MakeAbsolutePath(const std::string& base, const std::string& relative);
  static bool IsAbsolutePath(const std::string& path);
  static bool IsRelativePath(const std::string& path) { return !IsAbsolutePath(path); }

  static bool RealPath(const std::string& path, std::string* resolved);
  static bool mkdir(const std::string& path);
  static bool mkdirs(const std::string& path);

  static bool mkdir(const File& dir) { return File::mkdir(dir.full_pathname()); }
  static bool mkdirs(const File& dir) { return File::mkdirs(dir.full_pathname()); }

  static long GetFreeSpaceForPath(const std::string& path);

 private:
   int handle_;
   std::string full_path_name_;
   std::string error_text_;
};


#endif // __INCLUDED_PLATFORM_WFILLE_H__
