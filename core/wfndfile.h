/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2016,WWIV Software Services              */
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

#include <cstring>
#include <string>
#include "bbs/wwiv_windows.h"
#include "core/wwivport.h"


class WFindFile {
 protected:
  std::string filename_;
  std::string filespec_;
  long file_size_;
  unsigned int type_mask_;
  unsigned char nFileType;
  bool open_;

  void __open(const std::string& file_spec, unsigned int type_mask) {
    filespec_ = file_spec;
    type_mask_ = type_mask;
  }

  void __close() {
    filespec_.clear();
    filename_.clear();
    file_size_ = 0;
    type_mask_ = 0;
    open_ = false;
  }

#if defined (_WIN32)
  WIN32_FIND_DATA ffdata;
  HANDLE  hFind;
#elif defined ( __unix__ )
  struct dirent **entries;
  int nMatches;
  int nCurrentEntry;
#endif

 public:
  WFindFile() { this->__close(); }
  bool open(const std::string& filespec, unsigned int nTypeMask);
  bool next();
  bool close();
  virtual ~WFindFile() { close(); }

  const char* GetFileName() { return filename_.c_str(); }
  long GetFileSize() { return file_size_; }
  bool IsDirectory();
  bool IsFile();
};


/**
 * Bit-mapped values for what WFindFile is searching
 */
enum WFindFileTypeMask {
  WFNDFILE_ANY = 0x00,
  WFINDFILE_FILES = 0x01,
  WFINDFILE_DIRS  = 0x02
};


#endif  // __INCLUDED_WFNDFILE_H__
