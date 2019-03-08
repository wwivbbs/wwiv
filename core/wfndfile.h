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

#ifndef __INCLUDED_WFNDFILE_H__
#define __INCLUDED_WFNDFILE_H__

#include "core/wwiv_windows.h"
#include <cstring>
#include <string>

/**
 * Values for what WFindFile is searching
 */
enum class WFindFileTypeMask {
  WFINDFILE_ANY = 0x00,
  WFINDFILE_FILES = 0x01,
  WFINDFILE_DIRS = 0x02
};

class WFindFile {
 protected:
  std::string dir_;
  std::string filename_;
  std::string filespec_;
  long file_size_ = 0;
  WFindFileTypeMask type_mask_ = WFindFileTypeMask::WFINDFILE_ANY;
  unsigned char file_type_ = 0;
  bool open_ = false;

  void __open(const std::string& file_spec, WFindFileTypeMask type_mask) {
    filespec_ = file_spec;
    type_mask_ = type_mask;
  }

  void __close() {
    filespec_.clear();
    filename_.clear();
    file_size_ = 0;
    type_mask_ = WFindFileTypeMask::WFINDFILE_ANY;
    open_ = false;
  }

#if defined (_WIN32)
  WIN32_FIND_DATA ffdata{};
  HANDLE  hFind = nullptr;
#elif defined ( __unix__ )
  struct dirent **entries = nullptr;
  int nMatches = 0;
  int nCurrentEntry = 0;
#endif

 public:
  WFindFile() noexcept { this->__close(); }
  bool open(const std::string& filespec, WFindFileTypeMask nTypeMask);
  bool next();
  bool close();
  virtual ~WFindFile() { close(); }

  std::string GetFileName() const { return filename_; }
  long GetFileSize() const { return file_size_; }
  bool IsDirectory();
  bool IsFile();
};


#endif  // __INCLUDED_WFNDFILE_H__
