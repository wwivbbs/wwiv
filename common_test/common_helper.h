/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services             */
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
#ifndef INCLUDED_COMMON_TEST_COMMON_HELPER_H
#define INCLUDED_COMMON_TEST_COMMON_HELPER_H

#include "common/context.h"
#include "common/remote_io.h"
#include "core_test/file_helper.h"
#include "local_io/local_io.h"
#include "sdk/chains.h"
#include "sdk/config.h"
#include "sdk/user.h"
#include <memory>
#include <string>

class TestIO;
class TestContext;


/*
 * 
 * Test helper for using code with heavy BBS dependencies.
 * 
 * To use, add an instance as a field in the test class.  Then invoke.
 * BbsHelper::SetUp before use. typically from Test::SetUp.
 */
class CommonHelper {
public:
  explicit CommonHelper(int instance_num);
  CommonHelper();
  virtual ~CommonHelper() = default;
  virtual void SetUp();
  [[nodiscard]] virtual wwiv::sdk::Config& config() const;
  [[nodiscard]] virtual wwiv::sdk::User* user() { return &common_user; }
  [[nodiscard]] TestIO* io() const { return io_.get(); }
  [[nodiscard]] virtual wwiv::common::SessionContext& sess();
  [[nodiscard]] virtual wwiv::common::Context& context();

  // Accessors for various directories
  FileHelper& files() { return files_; }
  [[nodiscard]] const std::string& data() const { return dir_data_; }
  [[nodiscard]] const std::string& gfiles() const { return dir_gfiles_; }
  [[nodiscard]] const std::string& menus() const { return dir_menus_; }

  FileHelper files_;
  std::string dir_data_;
  std::string dir_gfiles_;
  std::string dir_menus_;
  std::string current_menu_dir_;
  std::string dir_dloads_;
  std::string dir_msgs_;
  std::unique_ptr<TestIO> io_;

protected:
  std::unique_ptr<wwiv::sdk::Config> config_;

private:
  std::unique_ptr<wwiv::common::SessionContext> common_session_context_;
  wwiv::sdk::User common_user{};
  std::unique_ptr<TestContext> context_;
};


class TestIO {
public:
  TestIO();
  void Clear() { captured_.clear(); rcaptured_.clear(); }
  std::string captured();
  std::string rcaptured();
  [[nodiscard]] wwiv::local::io::LocalIO* local_io() const { return local_io_; }
  [[nodiscard]] wwiv::common::RemoteIO* remote_io() const { return remote_io_; }

private:
  wwiv::local::io::LocalIO* local_io_;
  wwiv::common::RemoteIO* remote_io_;
  std::string captured_;
  std::string rcaptured_;
};

class TestLocalIO : public wwiv::local::io::LocalIO {
public:
  explicit TestLocalIO(std::string* captured);
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
  void EditLine(char*, int, wwiv::local::io::AllowedKeys, wwiv::local::io::EditlineResult*,
                const char*) override {}
  void UpdateNativeTitleBar(const std::string&, int) override {}

  std::string* captured_;
};

class TestRemoteIO final : public wwiv::common::RemoteIO {
public:
  explicit TestRemoteIO(std::string* captured);
  ~TestRemoteIO() override = default;

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

class TestContext final : public wwiv::common::Context {
public:
  explicit TestContext(CommonHelper& helper);
  [[nodiscard]] wwiv::sdk::Config& config() override { return helper_.config(); }
  [[nodiscard]] wwiv::sdk::User& u() override { return *helper_.user(); }
  wwiv::common::SessionContext& session_context() override { return sess_ctx_; }
  [[nodiscard]] bool mci_enabled() const override { return true; }
  [[nodiscard]] const std::vector<editorrec>& editors() const { return editors_; }
  [[nodiscard]] const wwiv::sdk::Chains& chains() const { return chains_; }

  CommonHelper& helper_;
  wwiv::common::SessionContext sess_ctx_;
  std::vector<editorrec> editors_;
  wwiv::sdk::Chains chains_;
};

#endif
