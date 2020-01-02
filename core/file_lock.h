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

#ifndef __INCLUDED_CORE_FILE_LOCK_H__
#define __INCLUDED_CORE_FILE_LOCK_H__

#include <cstring>
#include <ctime>
#include <iostream>
#include <string>
#include <sys/types.h>


namespace wwiv {
namespace core {

enum class FileLockType {
  read_lock,
  write_lock
};

class FileLock {
public:
  FileLock(int fd, const std::string& filename, FileLockType lock_type);
  virtual ~FileLock();
private:
  int fd_;
  const std::string filename_;
  FileLockType lock_type_;
};

} // namespace core
} // namespace wwiv


#endif // __INCLUDED_CORE_FILE_LOCK_H__
