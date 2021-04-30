/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services             */
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

#ifndef INCLUDED_CORE_WFNDFILE_H
#define INCLUDED_CORE_WFNDFILE_H

#include <any>
#include <filesystem>
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
  std::filesystem::path dir_;
  std::string filename_;
  std::string filespec_;
  long file_size_{0};
  WFindFileTypeMask type_mask_{WFindFileTypeMask::WFINDFILE_ANY};
  unsigned char file_type_{0};
  bool open_{false};
  bool use_long_filenames_{false};

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

#if defined(_WIN32)
  typedef void* HANDLE;
  std::any ffdata_;
  HANDLE hFind{nullptr};
#elif defined ( __OS2__ )
  struct dirent **entries = nullptr;
  int nMatches = 0;
  int nCurrentEntry = 0;
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
  /** Should WFindFile use long filenames. */
  void set_use_long_filenames(bool u) { use_long_filenames_ = u; }

  [[nodiscard]] std::string GetFileName() const { return filename_; }
  [[nodiscard]] long GetFileSize() const { return file_size_; }
  [[nodiscard]] bool IsDirectory() const;
  [[nodiscard]] bool IsFile() const;
};


#endif
