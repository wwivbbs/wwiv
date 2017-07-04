/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2017, WWIV Software Services             */
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
#ifndef __INCLUDED_BBS_SSH_H__
#define __INCLUDED_BBS_SSH_H__

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

#include "bbs/remote_io.h"
#include "bbs/remote_socket_io.h"

#include <libssh/libssh.h>
#include <libssh/server.h>

namespace wwiv {
namespace bbs {

class Keys {
public:
  Keys();
  virtual ~Keys() {};
  bool Validate();
  bool Generate();
  bool Exist();
};

class SSHSession {
public:
  SSHSession(int socket_handle);
  virtual ~SSHSession() { close();  }
  int PushData(const char* data, size_t size);
  int PopData(char* data, size_t buffer_size);
  int socket_handle() const { return socket_handle_; }
  bool initialized() const { return initialized_; }
  bool closed() const { return closed_.load(); }
  bool close();
  std::string GetAndClearRemoteUserName();
  std::string GetAndClearRemotePassword();

private:
  mutable std::mutex mu_;
  bool SSHAuthenticate(ssh_session session);
  int session_ = 0;
  int socket_handle_ = -1;
  bool initialized_ = false;
  ssh_session session;
  ssh_channel chan = 0;
  std::atomic<bool> closed_;
  std::string remote_username_;
  std::string remote_password_;
};

class IOSSH: public RemoteIO {
public:
  IOSSH(SOCKET socket);
  virtual ~IOSSH();

  bool ssh_initialize();

  bool open() override;
  void close(bool temporary) override;
  unsigned char getW() override;
  bool disconnect() override;
  void purgeIn() override;
  unsigned int put(unsigned char ch) override;
  unsigned int read(char *buffer, unsigned int count) override;
  unsigned int write(const char *buffer, unsigned int count, bool bNoTranslation) override;
  bool connected() override;
  bool incoming() override;
  unsigned int GetHandle() const override;
  unsigned int GetDoorHandle() const override;

private:
  std::thread ssh_receive_thread_;
  std::thread ssh_send_thread_;
  bool initialized_ = false;
  std::unique_ptr<RemoteSocketIO> io_;
  SOCKET ssh_socket_;
  SOCKET plain_socket_;
  SSHSession session_;
};


}
}

#endif  // __INCLUDED_BBS_SSH_H__
