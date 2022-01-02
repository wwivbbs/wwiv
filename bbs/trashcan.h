/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2022, WWIV Software Services             */
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
#ifndef __INCLUDED_BBS_TRASHCAN_H__
#define __INCLUDED_BBS_TRASHCAN_H__

#include <string>
#include "core/file.h"
#include "sdk/config.h"

class Trashcan {
public:
  Trashcan(wwiv::sdk::Config& config);
  virtual ~Trashcan();
  bool IsTrashName(const std::string& name);

private:
  wwiv::core::File file_;
};

#endif  // __INCLUDED_BBS_TRASHCAN_H__
