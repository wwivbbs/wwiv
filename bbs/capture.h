/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2015, WWIV Software Services             */
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
#ifndef __INCLUDED_BBS_CAPTURE_H__
#define __INCLUDED_BBS_CAPTURE_H__

#include "core/file.h"

namespace wwiv {
namespace bbs {

 class Capture {
 public:
   Capture();
   ~Capture();

  void set_global_handle(bool bOpenFile, bool bOnlyUpdateVariable = false);
  void global_char(char ch);
  void set_x_only(bool tf, const char *pszFileName, bool ovwr);
  bool is_open() const { return fileGlobalCap.IsOpen(); }
  int wx() const { return wx_; }
  void set_wx(int wx) { wx_ = wx; }

 private:
  File fileGlobalCap;
  std::string global_buf;
  int wx_;
 };

}
}


#endif  // __INCLUDED_BBS_CAPTURE_H__