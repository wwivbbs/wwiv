/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2007, WWIV Software Services             */
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
#include <string>

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

class WLogger {
 public:
  /////////////////////////////////////////////////////////////////////////
  //
  // Member functions
  //

  WLogger() {}
  virtual ~WLogger() {}
  virtual bool LogMessage(const char* pszFormat, ...) = 0;
};

/**
 * WFile - File I/O Class.
 */
class WFile {

 public:
  /////////////////////////////////////////////////////////////////////////
  //
  // Constants
  //

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

  static const int shareUnknown;
  static const int shareDenyReadWrite;
  static const int shareDenyWrite;
  static const int shareDenyRead;
  static const int shareDenyNone;

  static const int permUnknown;
  static const int permWrite;
  static const int permRead;
  static const int permReadWrite;

  static const int seekBegin;
  static const int seekCurrent;
  static const int seekEnd;

  static const int invalid_handle;

  static const char pathSeparatorChar;
  static const char separatorChar;

 private:

  int handle_;
  bool open_;
  std::string full_path_name_;
  std::string error_text_;
  static  WLogger* logger_;
  static int debug_level_;

  void init();

 public:

  /////////////////////////////////////////////////////////////////////////
  //
  // Constructor/Destructor
  //

  WFile();
  WFile(const std::string dirName, const std::string fileName);
  explicit WFile(const std::string strFileName);
  virtual ~WFile();

  /////////////////////////////////////////////////////////////////////////
  //
  // Public Member functions
  //

  virtual bool SetName(const std::string fileName);
  virtual bool SetName(const std::string dirName, const std::string fileName);

  virtual bool Open(int nFileMode = WFile::modeDefault,
                    int nShareMode = WFile::shareUnknown,
                    int nPermissions = WFile::permUnknown);

  virtual void Close();
  virtual bool IsOpen() const {
    return open_;
  }

  virtual int Read(void * pBuffer, size_t nCount);
  virtual int Write(const void * pBuffer, size_t nCount);

  virtual int Write(const std::string& s) {
    return this->Write(s.c_str(), s.length());
  }
  
  virtual int Writeln(const void *pBuffer, size_t nCount) {
    int ret = this->Write(pBuffer, nCount);
    ret += this->Write("\r\n", 2);
    return ret;
  }

  virtual int Writeln(const std::string& s) {
    return this->Writeln(s.c_str(), s.length());
  }

  virtual long GetLength();
  virtual long Seek(long lOffset, int nFrom);
  virtual void SetLength(long lNewLength);

  virtual bool Exists() const;
  virtual bool Delete();

  virtual bool IsDirectory();
  virtual bool IsFile();

  virtual bool SetFilePermissions(int nPermissions);
  virtual time_t GetFileTime();

  virtual const std::string GetParent() {
    size_t found = full_path_name_.find_last_of(WFile::pathSeparatorChar);
    if (found == std::string::npos) {
      return std::string("");
    }
    return full_path_name_.substr(0, found);
  }

  virtual std::string GetName() {
    size_t found = full_path_name_.find_last_of(WFile::pathSeparatorChar);
    if (found == std::string::npos) {
      return std::string("");
    }
    return full_path_name_.substr(found + 1);
  }

  virtual const std::string GetFullPathName() { return full_path_name_; }
  virtual const std::string GetLastError() const { return error_text_; }

 public:

  /////////////////////////////////////////////////////////////////////////
  //
  // static functions
  //

  static bool Remove(const std::string fileName);
  static bool Remove(const std::string directoryName, const std::string fileName);
  static bool Rename(const std::string origFileName, const std::string newFileName);
  static bool Exists(const std::string fileName);
  static bool Exists(const std::string directoryName, const std::string fileName);
  static bool ExistsWildcard(const std::string wildCard);
  static bool CopyFile(const std::string sourceFileName, const std::string destFileName);
  static bool MoveFile(const std::string sourceFileName, const std::string destFileName);

  static bool SetFilePermissions(const std::string fileName, int nPermissions);
  static bool IsFileHandleValid(int hFile);

  static void SetLogger(WLogger* logger) { logger_ = logger; }
  static void SetDebugLevel(int nDebugLevel) { debug_level_ = nDebugLevel; }
  static int GetDebugLevel() { return debug_level_; }
  static void EnsureTrailingSlash(std::string* path);
  static void CurrentDirectory(std::string* current_dir);
  static void MakeAbsolutePath(const std::string base, std::string* relative);
  static bool IsAbsolutePath(const std::string path);
  static bool IsRelativePath(const std::string path) { return !IsAbsolutePath(path); }
};

#endif // __INCLUDED_PLATFORM_WFILLE_H__
