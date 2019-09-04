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

#ifndef __INCLUDED_CORE_SEMAPHORE_FILE_H__
#define __INCLUDED_CORE_SEMAPHORE_FILE_H__

#include <chrono>
#include <cstring>
#include <ctime>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/types.h>

#include "core/filesystem.h"

namespace wwiv {
namespace core {

struct semaphore_not_acquired : public std::runtime_error {
  semaphore_not_acquired(const std::filesystem::path& filename)
      : std::runtime_error(filename.string()) {}
};

class SemaphoreFile final {
public:
  SemaphoreFile() = delete;

  /** 
   * Tries to create the semaphore file, waiting up to timeout and then
   * failing by throwing a semaphore_not_acquired exception.
   * Will write 'text' into the semaphore file.
   */
  static SemaphoreFile try_acquire(const std::filesystem::path& filepath, 
                                   const std::string& text,
                                   std::chrono::duration<double> timeout);

  /**
   * Tries to create the semaphore file, waiting up to timeout and then
   * failing by throwing a semaphore_not_acquired exception.
   */
  static SemaphoreFile try_acquire(const std::filesystem::path& filepath,
    std::chrono::duration<double> timeout) {
    return try_acquire(filepath, "", timeout);
  }

  /**
   * Tries to create the semaphore file, waiting forever.
   * Will write 'text' into the semaphore file.
   */
  static SemaphoreFile acquire(const std::filesystem::path& filepath, const std::string& text);

  /**
   * Tries to create the semaphore file, waiting forever.
   */
  static SemaphoreFile acquire(const std::filesystem::path& filepath) {
    return acquire(filepath, "");
  }

  ~SemaphoreFile();

  const std::filesystem::path& path() const { return path_; }
  int fd() const { return fd_; }

  SemaphoreFile(SemaphoreFile&&) = default;
  SemaphoreFile(const SemaphoreFile&) = delete;
  SemaphoreFile& operator= (const SemaphoreFile&) = delete;

private:
  SemaphoreFile(const std::filesystem::path&, int fd);

  const std::filesystem::path path_;
  int fd_{-1};
};

}  // namespace core
}  // namespace wwiv



#endif // __INCLUDED_CORE_SEMAPHORE_FILE_H__
