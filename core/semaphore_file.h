/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2017, WWIV Software Services            */
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
#include <string>
#include <sys/types.h>


namespace wwiv {
namespace core {

struct semaphore_not_acquired : public std::runtime_error {
  semaphore_not_acquired(const std::string& filename) : std::runtime_error(filename) {}
};

class SemaphoreFile {
public:

  /** 
   * Tries to create the semaphore file, waiting up to timeout and then
   * failing by throwing an exception.
   */
  static SemaphoreFile try_acquire(const std::string& filepath, std::chrono::duration<double> timeout);
  static SemaphoreFile acquire(const std::string& filepath);
  ~SemaphoreFile();

  const std::string& filename() const { return filename_; }

private:
  SemaphoreFile(const std::string& filepath, int fd);

  const std::string filename_;
  int fd_ = -1;
};

}  // namespace core
}  // namespace wwiv



#endif // __INCLUDED_CORE_SEMAPHORE_FILE_H__
