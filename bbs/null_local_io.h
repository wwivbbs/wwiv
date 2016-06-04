/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2016, WWIV Software Services             */
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
#ifndef __INCLUDED_BBS_NULL_LOCAL_IO_H__
#define __INCLUDED_BBS_NULL_LOCAL_IO_H__
#include "bbs/local_io.h"

#include <string>

#if defined( _MSC_VER )
#pragma warning( push )
#pragma warning( disable : 4125 4100 )
#endif

class NullLocalIO : public LocalIO {
public:
  NullLocalIO();
  virtual ~NullLocalIO();
  void LocalPutch(unsigned char ch) override {};
  void LocalGotoXY(int x, int y) override {}
  size_t WhereX() override { return 0; }
  size_t WhereY() override { return 0; }
  void LocalLf() override {}
  void LocalCr() override {}
  void LocalCls() override {}
  void LocalBackspace() override {}
  void LocalPutchRaw(unsigned char ch) override {}
  void LocalPuts(const std::string& s) override {}
  void LocalXYPuts(int x, int y, const std::string& text) override {}
  void LocalFastPuts(const std::string& text) override {}
  int LocalPrintf(const char *formatted_text, ...) override { return 0; }
  int LocalXYPrintf(int x, int y, const char *formatted_text, ...) override { return 0; }
  int LocalXYAPrintf(int x, int y, int nAttribute, const char *formatted_text, ...) override { return 0; }
  void set_protect(WSession* session, int l) override {}
  void savescreen() override {}
  void restorescreen() override {}
  bool LocalKeyPressed() override { return false; }
  void SaveCurrentLine(char *cl, char *atr, char *xl, char *cc) override {}
  unsigned char LocalGetChar() override { return static_cast<unsigned char>(getchar()); }
  void MakeLocalWindow(int x, int y, int xlen, int ylen) override {}
  void SetCursor(int cursorStyle) override {}
  void LocalClrEol() override {}
  void LocalWriteScreenBuffer(const char *buffer) override {}
  size_t GetDefaultScreenBottom() override { return 24; }
  void LocalEditLine(char *s, int len, int statusx, int *returncode, char *ss) override {}
  void UpdateNativeTitleBar(WSession* session) override {}
};

#if defined( _MSC_VER )
#pragma warning( pop )
#endif // _MSC_VER


#endif  // __INCLUDED_BBS_NULL_LOCAL_IO_H__
