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

#if !defined (__INCLUDED_BBS_REMOTE_IO_H__)
#define __INCLUDED_BBS_REMOTE_IO_H__

#include <string>

enum class CommunicationType {
  NONE,
  TELNET,
  SSH,
  STDIO
};

/** Information about the remote session. */
class RemoteInfo {
public:
  void clear() {
    address.clear();
    address_name.clear();
    username.clear();
    password.clear();
  }

  std::string address;
  std::string address_name;
  std::string username;
  std::string password;
};
/**
 * Base Communication Class.
 */
class RemoteIO {
 public:
  RemoteIO() = default;
  virtual ~RemoteIO() = default;

  virtual bool open() = 0;
  virtual void close(bool temporary) = 0;
  virtual unsigned char getW() = 0;
  virtual bool disconnect() = 0;
  virtual void purgeIn() = 0;
  virtual unsigned int put(unsigned char ch) = 0;
  virtual unsigned int read(char *buffer, unsigned int count) = 0;
  virtual unsigned int write(const char *buffer, unsigned int count, bool bNoTranslation = false) = 0;
  virtual bool connected() = 0;
  virtual bool incoming() = 0;

  [[nodiscard]] virtual unsigned int GetHandle() const = 0;
  [[nodiscard]] virtual unsigned int GetDoorHandle() const { return GetHandle(); }

  void set_binary_mode(bool b) { binary_mode_ = b; }
  [[nodiscard]] bool binary_mode() const { return binary_mode_; }

  virtual RemoteInfo& remote_info() { return remote_info_; }

protected:
  bool binary_mode_{false};

  static std::string GetLastErrorText();

private:
  // used by the GetLastErrorText() method
  static std::string error_text_;
  RemoteInfo remote_info_;
};

#endif  // #if !defined (__INCLUDED_BBS_REMOTE_IO_H__)

