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
#ifndef __INCLUDED_BBS_SSH_H__
#define __INCLUDED_BBS_SSH_H__

#include <memory>
#include <mutex>
#include <string>

#include "bbs/wcomm.h"
#include "bbs/wiot.h"

namespace wwiv {
namespace bbs {

class Key {
public:
  explicit Key(const std::string& filename, const std::string& password) 
    : filename_(filename), password_(password) {}
  virtual ~Key() {};
  bool Open();
  bool Create();
  int context() const { return context_; }

private:
  const std::string password_;
  const std::string filename_;
  int context_ = 0;
};

class SSHSession {
public:
  SSHSession(int socket_handle, const Key& key);
  virtual ~SSHSession() {}
  int PushData(const char* data, size_t size);
  int PopData(char* data, size_t buffer_size);
  int socket_handle() const { return socket_handle_; }
  bool initialized() const { return initialized_; }

private:
  mutable std::mutex mu_;
  int session_ = 0;
  int socket_handle_ = -1;
  bool initialized_ = false;
};

class IOSSH: public WComm {
public:
  IOSSH(SOCKET socket, Key& key);
  virtual ~IOSSH();

  bool ssh_initalize();

  virtual bool open() override;
  virtual void close(bool temporary) override;
  virtual unsigned char getW() override;
  virtual bool dtr(bool raise) override;
  virtual void purgeIn() override;
  virtual unsigned int put(unsigned char ch) override;
  virtual unsigned int read(char *buffer, unsigned int count) override;
  virtual unsigned int write(const char *buffer, unsigned int count, bool bNoTranslation) override;
  virtual bool carrier() override;
  virtual bool incoming() override;
  virtual unsigned int GetHandle() const override;
  virtual unsigned int GetDoorHandle() const override;

private:
  std::thread ssh_receive_thread_;
  std::thread ssh_send_thread_;
  std::unique_ptr<WIOTelnet> io_;
  SOCKET ssh_socket_;
  SOCKET plain_socket_;
  SSHSession session_;
};


}
}

#endif  // __INCLUDED_BBS_SSH_H__