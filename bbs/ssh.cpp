/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2016-2021, WWIV Software Services             */
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
#include "bbs/ssh.h"

#ifdef _WIN32
#define NOCRYPT                /* Disable include of wincrypt.h */
#include <winsock2.h>
#pragma comment(lib, "Ws2_32.lib")
#include "WS2tcpip.h"
#else

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#endif  // _WIN32

#include "core/log.h"
#include "core/net.h"
#include "core/os.h"
#include "cryptlib.h"
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

using wwiv::common::RemoteInfo;
using wwiv::common::RemoteSocketIO;
using wwiv::common::RemoteIO;
using std::string;
using std::thread;
using std::unique_ptr;

namespace wwiv::bbs{

struct socket_error: public std::runtime_error {
  socket_error(const std::string& message): std::runtime_error(message) {}
};

static constexpr char WWIV_SSH_KEY_NAME[] = "wwiv_ssh_server";
#define RETURN_IF_ERROR(s) { if (!cryptStatusOK(s)) return false; }
#define OK(s) cryptStatusOK(s)

static bool ssh_once_init() {
  // This only should be called once.
  int status = cryptInit();
  return OK(status);
}

bool Key::Open() {
  static bool once = ssh_once_init();
  CRYPT_KEYSET keyset;
  int status = cryptKeysetOpen(&keyset, CRYPT_UNUSED, CRYPT_KEYSET_FILE, filename_.c_str(), CRYPT_KEYOPT_NONE);
  RETURN_IF_ERROR(status);

  status = cryptGetPrivateKey(keyset, &context_, CRYPT_KEYID_NAME, WWIV_SSH_KEY_NAME, password_.c_str());
  RETURN_IF_ERROR(status);

  status = cryptKeysetClose(keyset);
  RETURN_IF_ERROR(status);

  return true;
}

bool Key::Create() {
  static bool once = ssh_once_init();
  CRYPT_KEYSET keyset;
  int status = CRYPT_ERROR_INVALID;

  status = cryptCreateContext(&context_, CRYPT_UNUSED, CRYPT_ALGO_RSA);
  RETURN_IF_ERROR(status);

  status = cryptSetAttributeString(context_, CRYPT_CTXINFO_LABEL, WWIV_SSH_KEY_NAME, strlen(WWIV_SSH_KEY_NAME));
  RETURN_IF_ERROR(status);

  // We want 8K keys.
  //status = cryptSetAttribute(context_, CRYPT_CTXINFO_KEYSIZE, 2048 / 8);
  //RETURN_IF_ERROR(status);

  status = cryptGenerateKey(context_);
  RETURN_IF_ERROR(status);

  status = cryptKeysetOpen(&keyset, CRYPT_UNUSED, CRYPT_KEYSET_FILE, filename_.c_str(), CRYPT_KEYOPT_CREATE);
  RETURN_IF_ERROR(status);

  status = cryptAddPrivateKey(keyset, context_, password_.c_str());
  RETURN_IF_ERROR(status);

  status = cryptKeysetClose(keyset);
  RETURN_IF_ERROR(status);

  return true;
}

static bool GetSSHUserNameAndPassword(CRYPT_HANDLE session, std::string& username, std::string& password) {
  char username_str[CRYPT_MAX_TEXTSIZE + 1];
  char password_str[CRYPT_MAX_TEXTSIZE + 1];
  int usernameLength = 0;
  int passwordLength = 0;

  username.clear();
  password.clear();

  // Get the user name and password
  if (!OK(cryptGetAttributeString(session, CRYPT_SESSINFO_USERNAME, username_str, &usernameLength))) {
    return false;
  }
  username_str[usernameLength] = '\0';

  if (!OK(cryptGetAttributeString(session, CRYPT_SESSINFO_PASSWORD, password_str, &passwordLength))) {
    return false;
  }
  password_str[passwordLength] = '\0';
  username.assign(username_str);
  password.assign(password_str);

  return true;
}

SSHSession::SSHSession(int socket_handle, const Key& key) : socket_handle_(socket_handle) {
  static bool once = ssh_once_init();
  // GCC 4.9 has issues with assigning this to false in the class.
  closed_.store(false);
  int status = CRYPT_ERROR_INVALID;

  status = cryptCreateSession(&session_, CRYPT_UNUSED, CRYPT_SESSION_SSH_SERVER);
  if (!OK(status)) {
    VLOG(1) << "ERROR: Unable to create SSH session.";
    return;
  }
  
  VLOG(1) << "setting private key.";
  status = cryptSetAttribute(session_, CRYPT_SESSINFO_PRIVATEKEY, key.context());
  if (!OK(status)) {
    VLOG(1) << "ERROR: Failed to set private key";
    return;
  }

  VLOG(1) << "setting socket handle.";
  status = cryptSetAttribute(session_, CRYPT_SESSINFO_NETWORKSOCKET, socket_handle_);
  if (!OK(status)) {
    VLOG(1) << "ERROR adding socket handle!";
    return;
  }

  bool success = false;
  for (int i = 0; i < 10; i++) {
    VLOG(1) << "Looping to activate SSH Session" << status;
    status = cryptSetAttribute(session_, CRYPT_SESSINFO_AUTHRESPONSE, 1);
    if (!OK(status)) {
      VLOG(1) << "Error setting CRYPT_SESSINFO_AUTHRESPONSE " << status;
      continue;
    }
    status = cryptSetAttribute(session_, CRYPT_SESSINFO_ACTIVE, 1);
    if (OK(status)) {
      VLOG(1) << "Success setting CRYPT_SESSINFO_ACTIVE " << status;
      success = true;
      break;
    } else {
      VLOG(1) << "Error setting CRYPT_SESSINFO_ACTIVE " << status;
    }
  }

#ifdef PAUSE_ON_SSH_CONNECT_FOR_DEBUGGER
  LOG(ERROR) << "Waiting for debugger..." <<;
  getchar();
#endif  // PAUSE_ON_SSH_CONNECT_FOR_DEBUGGER

  if (!success) {
    VLOG(1) << "We don't have a valid SSH connection here!";
  } else {
    // Clear out any remaining control messages.
    int bytes_received = 0;
    char buffer[4096];
    status = cryptPopData(session_, buffer, 4096, &bytes_received);
    if (!OK(status)) {
    	VLOG(1) << "ERROR clearing buffer (but that is ok). status: "
    	     << status;
    }

    GetSSHUserNameAndPassword(session_, remote_username_, remote_password_);
    VLOG(1) << "Got Username and Password!";
  }
  initialized_ = success;
}

int SSHSession::PushData(const char* data, size_t size) {
  int bytes_copied = 0;
  VLOG(2) << "SSHSession::PushData: " << string(data, size);
  std::lock_guard<std::mutex> lock(mu_);
  int status = cryptPushData(session_, data, size, &bytes_copied);
  if (!OK(status)) return 0;
  status = cryptFlushData(session_);
  if (!OK(status)) return 0;
  return bytes_copied;
}

int SSHSession::PopData(char* data, size_t buffer_size) {
  int bytes_copied = 0;
  std::lock_guard<std::mutex> lock(mu_);
  int status = cryptPopData(session_, data, buffer_size, &bytes_copied);
  if (!OK(status)) return -1;
  return bytes_copied;
}

bool SSHSession::close() {
  std::lock_guard<std::mutex> lock(mu_);
  if (closed_.load()) {
    return false;
  }
  cryptDestroySession(session_);
  closed_.store(true);
  return true;
}

std::string SSHSession::GetAndClearRemoteUserName() {
  string temp = remote_username_;
  remote_username_.clear();
  return temp;
}

std::string SSHSession::GetAndClearRemotePassword() {
  string temp = remote_password_;
  remote_password_.clear();
  return temp;
}

static bool socket_avail(SOCKET sock, int seconds) {
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(sock, &fds);

  timeval tv;
  tv.tv_sec = seconds;
  tv.tv_usec = 0;

  int result = select(sock + 1, &fds, 0, 0, &tv);
  if (result == SOCKET_ERROR) {
    throw socket_error("Error on select for socket.");
  }
  return result == 1;
}

// Reads from remote socket using session, writes to socket
static void reader_thread(SSHSession& session, SOCKET socket) {
  constexpr size_t size = 16 * 1024;
  std::unique_ptr<char[]> data = std::make_unique<char[]>(size);
  try {
    while (true) {
      if (session.closed()) {
        break;
      }
      if (!socket_avail(session.socket_handle(), 1)) {
        continue;
      }
      memset(data.get(), 0, size);
      int num_read = session.PopData(data.get(), size);
      if (num_read == -1) {
        // error.
        break;
      }
      int num_sent = send(socket, data.get(), num_read, 0);
      VLOG(1) << "reader_thread: sent " << num_sent;
    }
  } catch (const socket_error& e) {
    VLOG(1) << e.what();
  }
  closesocket(socket);
}

// Reads from local socket socket, writes to remote socket using session.
static void writer_thread(SSHSession& session, SOCKET socket) {
  constexpr size_t size = 16 * 1024;
  std::unique_ptr<char[]> data = std::make_unique<char[]>(size);
  try {
    while (true) {
        if (session.closed()) {
          break;
        }
      if (!socket_avail(socket, 1)) {
        continue;
      }
      memset(data.get(), 0, size);
      int num_read = recv(socket, data.get(), size, 0);
      if (num_read > 0) {
        int num_sent = session.PushData(data.get(), num_read);
        VLOG(1) << "writer_thread: pushed_data: " << num_sent;
      }
    }
  } catch (const socket_error& e) {
    VLOG(1) << e.what();
  }
  closesocket(socket);
}

IOSSH::IOSSH(SOCKET ssh_socket, Key& key) 
  : ssh_socket_(ssh_socket), session_(ssh_socket, key) {
  static bool initialized = RemoteSocketIO::Initialize();
  if (!ssh_initalize()) {
    VLOG(1) << "ERROR INITIALIZING SSH (ssh_initalize)";
    closesocket(ssh_socket_);
    ssh_socket_ = INVALID_SOCKET;
    return;
  }
  if (!session_.initialized()) {
    //LOG(ERROR) << "ERROR INITIALIZING SSH (SSHSession::initialized)";
    closesocket(ssh_socket_);
    ssh_socket_ = INVALID_SOCKET;
    return;
  }
  io_.reset(new RemoteSocketIO(plain_socket_, false));
  RemoteInfo& info = remote_info();
  info.username = session_.GetAndClearRemoteUserName();
  info.password = session_.GetAndClearRemotePassword();

  initialized_ = true;
}

IOSSH::~IOSSH() {
  if (ssh_receive_thread_.joinable()) {
    ssh_receive_thread_.join();
  }
  if (ssh_send_thread_.joinable()) {
    ssh_send_thread_.join();
  }

  // Release the remote before it happens automatically.
  io_.reset(nullptr);

  std::cerr << "~IOSSH";
}

bool IOSSH::ssh_initalize() {
  sockaddr_in a{};

  SOCKET listener = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listener == INVALID_SOCKET) {
#ifdef _WIN32    
    int last_error = WSAGetLastError();
#else
    int last_error = errno;
#endif
    VLOG(1) << "WSAGetLastError: " << last_error;
    return false;
  }

  // IP, localhost, any port.
  a.sin_family = AF_INET;
  a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  a.sin_port = 0;

  int result = bind(listener, reinterpret_cast<struct sockaddr*>(&a), sizeof(a));
  if (result == SOCKET_ERROR) {
    closesocket(listener);
    return false;
  }

  result = listen(listener, 1);
  if (result == SOCKET_ERROR) {
    closesocket(listener);
    return false;
  }

  socklen_t addr_len = sizeof(a);
  result = getsockname(listener, reinterpret_cast<struct sockaddr*>(&a), &addr_len);
  if (result == SOCKET_ERROR) {
    closesocket(listener);
    return false;
  }

  SOCKET pipe_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  result = connect(pipe_socket, reinterpret_cast<struct sockaddr*>(&a), addr_len);
  if (result == SOCKET_ERROR) {
    closesocket(listener);
    return false;
  }

  plain_socket_ = accept(listener, reinterpret_cast<struct sockaddr*>(&a), &addr_len);
  if (plain_socket_ == INVALID_SOCKET) {
    closesocket(listener);
    closesocket(pipe_socket);
    return false;
  }

  // Since we'll only ever accept one connection, we can close
  // the listener socket.
  closesocket(listener);

  // assign and start the threads.
  ssh_receive_thread_ = thread(reader_thread, std::ref(session_), pipe_socket);
  ssh_send_thread_ = thread(writer_thread, std::ref(session_), pipe_socket);
  return true;
}

bool IOSSH::open() { 
  if (!initialized_) return false;  
  
  wwiv::core::GetRemotePeerAddress(ssh_socket_, remote_info().address);
  return io_->open(); 
}

void IOSSH::close(bool temporary) { 
  if (!initialized_) return;
  if (!temporary) {
    session_.close();
  }
  io_->close(temporary);
}

unsigned char IOSSH::getW() { 
  if (!initialized_) return 0;
  return io_->getW();
}
bool IOSSH::disconnect() {
  if (!initialized_) return false;
  return io_->disconnect();
}
void IOSSH::purgeIn() { 
  if (!initialized_) return;
  io_->purgeIn();
}
unsigned int IOSSH::put(unsigned char ch) { 
  if (!initialized_) return 0;
  return io_->put(ch);
}
unsigned int IOSSH::read(char *buffer, unsigned int count) {
  if (!initialized_) return 0;
  return io_->read(buffer, count);
}
unsigned int IOSSH::write(const char *buffer, unsigned int count, bool bNoTranslation) {
  if (!initialized_) return 0;
  return io_->write(buffer, count, bNoTranslation);
}
bool IOSSH::connected() { 
  if (!initialized_) return false;
  return io_->connected(); 
}

bool IOSSH::incoming() {
  if (!initialized_) return false;
  return io_->incoming();
}

unsigned int IOSSH::GetHandle() const { 
  if (!initialized_) return false;
  return io_->GetHandle();
}

unsigned int IOSSH::GetDoorHandle() const {
  if (!initialized_) return false;
  return io_->GetDoorHandle();
}

} // namespace wwiv::bbs
