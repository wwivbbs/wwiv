/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2022, WWIV Software Services            */
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

#ifndef INCLUDED_CORE_SEMAPHORE_FILE_H
#define INCLUDED_CORE_SEMAPHORE_FILE_H

#include <chrono>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace wwiv::core {

struct semaphore_not_acquired final : std::runtime_error {
  explicit semaphore_not_acquired(const std::filesystem::path& filename)
    : std::runtime_error(filename.string()) {
  }
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

  [[nodiscard]] const std::filesystem::path& path() const { return path_; }
  [[nodiscard]] int fd() const { return fd_; }

  SemaphoreFile(SemaphoreFile&&) = default;
  SemaphoreFile(const SemaphoreFile&) = delete;
  SemaphoreFile& operator=(const SemaphoreFile&) = delete;

private:
  SemaphoreFile(std::filesystem::path, int fd);

  const std::filesystem::path path_;
  int fd_{-1};
};

} // namespace


#endif
