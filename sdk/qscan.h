/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.0x                             */
/*               Copyright (C)2018, WWIV Software Services                */
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
/**************************************************************************/
#ifndef __INCLUDED_SDK_QSCAN_H__
#define __INCLUDED_SDK_QSCAN_H__

#include <string>
#include "core/file.h"

namespace wwiv {
namespace sdk {

// Experimental SDK class for working with QScan data.
class UserQScan {
public:
  UserQScan(const std::string& filename, int user_number, int qscan_length, int max_subs, int max_dirs);
  ~UserQScan();

  bool Save();

private:
  File file_;
  bool open_ = false;
  std::unique_ptr<uint32_t[]> qscan_;
  
};

class AllUserQScan {
public:
  AllUserQScan(const std::string& filename, int max_users) {}

};

}  // namespace sdk
}  // namespace wwiv

#endif  // __INCLUDED_SDK_QSCAN_H__
