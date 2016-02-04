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
/*                                                                        */
/**************************************************************************/
#ifndef __INCLUDED_BBS_HELPER_H__
#define __INCLUDED_BBS_HELPER_H__

#include <memory>
#include <string>
#include "bbs/bbs.h"
#include "core_test/file_helper.h"
#include "bbs/local_io.h"
#include "sdk/user.h"

class TestIO;

/*
 * 
 * Test helper for using code with heavy BBS dependencies.
 * 
 * To use, add an instance as a field in the test class.  Then invoke.
 * BbsHelper::SetUp before use. typically from Test::SetUp.
 */
class BbsHelper {
public:
    virtual void SetUp();
    virtual void TearDown();
    wwiv::sdk::User* user() const { return user_; }
    TestIO* io() const { return io_.get(); }

    // Accessors for various directories
    FileHelper& files() { return files_; }
    const std::string& data() { return dir_data_; }
    const std::string& gfiles() { return dir_gfiles_; }

public:
    FileHelper files_;
    std::string dir_data_;
    std::string dir_gfiles_;
    std::string dir_en_gfiles_;
    std::string dir_menus_;
    std::unique_ptr<WApplication> app_;
    std::unique_ptr<WSession> sess_;
    std::unique_ptr<TestIO> io_;
    wwiv::sdk::User* user_;
};

class TestIO {
public:
  TestIO();
  void Clear() { captured_.clear(); } 
  std::string captured();
  LocalIO* local_io() const { return local_io_; }
private:
  LocalIO* local_io_;
  std::string captured_;
};

class TestLocalIO : public LocalIO {
public:
  TestLocalIO(std::string* captured);
  void LocalPutch(unsigned char ch) override;
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
  void tleft(WSession* session, bool temp_sysop, bool sysop, bool user_online) override {}
  bool LocalKeyPressed() override { return false; }
  void SaveCurrentLine(char *cl, char *atr, char *xl, char *cc) override {}
  unsigned char LocalGetChar() override { return getchar(); }
  void MakeLocalWindow(int x, int y, int xlen, int ylen) override {}
  void SetCursor(int cursorStyle) override {}
  void LocalClrEol() override {}
  void LocalWriteScreenBuffer(const char *buffer) override {}
  size_t GetDefaultScreenBottom() override { return 25; }
  void LocalEditLine(char *s, int len, int status, int *returncode, char *ss) override {}
  void UpdateNativeTitleBar(WSession* session) override {}
  void UpdateTopScreen(WStatus* pStatus, WSession *pSession, int nInstanceNumber) override {}

  std::string* captured_;
};

#endif // __INCLUDED_BBS_HELPER_H__
