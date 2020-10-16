/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2020, WWIV Software Services             */
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

#include "bbs/bbs.h"
#include "core_test/file_helper.h"
#include "local_io/local_io.h"
#include "sdk/user.h"
#include <memory>
#include <string>

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
    const std::string& data() const { return dir_data_; }
    const std::string& gfiles() const { return dir_gfiles_; }

public:
    FileHelper files_;
    std::string dir_data_;
    std::string dir_gfiles_;
    std::string dir_en_gfiles_;
    std::string dir_menus_;
    std::string dir_dloads_;
    std::string dir_msgs_;
    std::unique_ptr<Application> app_;
    std::unique_ptr<TestIO> io_;
    wwiv::sdk::User* user_;
};

class TestIO {
public:
  TestIO();
  void Clear() { captured_.clear(); rcaptured_.clear(); }
  std::string captured();
  std::string rcaptured();
  LocalIO* local_io() const { return local_io_; }
  wwiv::common::RemoteIO* remote_io() const { return remote_io_; }
private:
  LocalIO* local_io_;
  wwiv::common::RemoteIO* remote_io_;
  std::string captured_;
  std::string rcaptured_;
};

class TestLocalIO : public LocalIO {
public:
  TestLocalIO(std::string* captured);
  void Putch(unsigned char ch) override;
  void GotoXY(int, int) override {}
  [[nodiscard]] int WhereX() const noexcept override { return 0; }
  [[nodiscard]] int WhereY() const noexcept override { return 0; }
  void Lf() override {}
  void Cr() override {}
  void Cls() override {}
  void Backspace() override {}
  void PutchRaw(unsigned char) override {}
  void Puts(const std::string& ) override {}
  void PutsXY(int, int, const std::string&) override {}
  void PutsXYA(int, int, int, const std::string&) override {}
  void FastPuts(const std::string&) override {}
  void set_protect(int) override {}
  void savescreen() override {}
  void restorescreen() override {}
  bool KeyPressed() override { return false; }
  [[nodiscard]] unsigned char GetChar() override { return static_cast<unsigned char>(getchar()); }
  void MakeLocalWindow(int, int, int, int) override {}
  void SetCursor(int) override {}
  void ClrEol() override {}
  void WriteScreenBuffer(const char *) override {}
  [[nodiscard]] int GetDefaultScreenBottom() const noexcept override { return 25; }
  void EditLine(char *, int, AllowedKeys, EditlineResult *, const char *) override {}
  void UpdateNativeTitleBar(const std::string&, int) override {}

  std::string* captured_;
};

class TestRemoteIO : public wwiv::common::RemoteIO {
public:
  TestRemoteIO(std::string* captured);
  virtual ~TestRemoteIO() = default;

  bool open() override { return true; }
  void close(bool) override {}
  unsigned char getW() override { return 0; }
  bool disconnect() override { return true; }
  void purgeIn() override {}
  unsigned int put(unsigned char ch) override;
  unsigned int write(const char *buffer, unsigned int count, bool bNoTranslation = false) override;
  unsigned int read(char *, unsigned int) override { return 0; }
  bool connected() override { return true; }
  bool incoming() override { return false; }
  [[nodiscard]] unsigned int GetHandle() const override { return 0; }

private:
  std::string* captured_;
};

#endif // __INCLUDED_BBS_HELPER_H__
