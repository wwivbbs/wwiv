/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                Copyright (C)2016, WWIV Software Services               */
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
// work around error using inet_ntoa on build machine.
#define NOCRYPT                /* Disable include of wincrypt.h */
#include <winsock2.h>
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#pragma comment(lib, "Ws2_32.lib")
#include "WS2tcpip.h"
#else

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

typedef int HANDLE;
typedef int SOCKET;
constexpr int SOCKET_ERROR = -1;
#define SOCKADDR_IN sockaddr_in
#define closesocket(s) close(s)
#endif  // _WIN32

#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "cryptlib.h"

#include "core/os.h"

using std::clog;
using std::endl;
using std::string;
using std::thread;
using std::unique_ptr;

namespace wwiv {
namespace bbs {

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
    clog << "ERROR: Unable to create SSH session." << endl;
    return;
  }
  
  clog << "setting private key." << endl;
  status = cryptSetAttribute(session_, CRYPT_SESSINFO_PRIVATEKEY, key.context());
  if (!OK(status)) {
    clog << "ERROR: Failed to set private key" << endl;
    return;
  }

  clog << "setting socket handle." << endl;
  status = cryptSetAttribute(session_, CRYPT_SESSINFO_NETWORKSOCKET, socket_handle_);
  if (!OK(status)) {
    clog << "ERROR adding socket handle!" << endl;
    return;
  }

  bool success = false;
  for (int i = 0; i < 10; i++) {
    status = cryptSetAttribute(session_, CRYPT_SESSINFO_AUTHRESPONSE, 1);
    if (!OK(status)) {
      continue;
    }
    status = cryptSetAttribute(session_, CRYPT_SESSINFO_ACTIVE, 1);
    if (status != CRYPT_ENVELOPE_RESOURCE) {
      clog << "Hmm...";
      //getchar();
    }
    if (OK(status)) {
      success = true;
      break;
    }
  }
  if (!success) {
    clog << "We don't have a valid SSH connection here!" << endl;
  } else {
    // Clear out any remaining control messages.
    int bytes_received = 0;
    char buffer[4096];
    for (int count = 0; bytes_received <= 0 && count < 10; count++) {
      cryptPopData(session_, buffer, 4096, &bytes_received);
      wwiv::os::sleep_for(std::chrono::milliseconds(100));
    }
  }
  initialized_ = success;
  GetSSHUserNameAndPassword(session_, remote_username_, remote_password_);
}

int SSHSession::PushData(const char* data, size_t size) {
  int bytes_copied = 0;
  // clog << "SSHSession::PushData: " << string(data, size) << endl;
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
  return std::move(temp);
}

std::string SSHSession::GetAndClearRemotePassword() {
  string temp = remote_password_;
  remote_password_.clear();
  return std::move(temp);
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
        return;
      }
      if (!socket_avail(session.socket_handle(), 1)) {
        continue;
      }
      memset(data.get(), 0, size);
      int num_read = session.PopData(data.get(), size);
      if (num_read == -1) {
        // error.
        return;
      }
      int num_sent = send(socket, data.get(), num_read, 0);
      // clog << "reader_thread: sent " << num_sent << endl;
    }
  } catch (const socket_error& e) {
    clog << e.what() << endl;
  }
}

// Reads from local socket socket, writes to remote socket using session.
static void writer_thread(SSHSession& session, SOCKET socket) {
  constexpr size_t size = 16 * 1024;
  std::unique_ptr<char[]> data = std::make_unique<char[]>(size);
  try {
    while (true) {
        if (session.closed()) {
          return;
        }
      if (!socket_avail(socket, 1)) {
        continue;
      }
      memset(data.get(), 0, size);
      int num_read = recv(socket, data.get(), size, 0);
      if (num_read > 0) {
        int num_sent = session.PushData(data.get(), num_read);
        // clog << "writer_thread: pushed_data: " << num_sent << endl;
      }
    }
  } catch (const socket_error& e) {
    clog << e.what() << endl;
  }
}

IOSSH::IOSSH(SOCKET ssh_socket, Key& key) 
  : ssh_socket_(ssh_socket), session_(ssh_socket, key) {
  static bool initialized = RemoteSocketIO::Initialize();
  if (!ssh_initalize()) {
    clog << "ERROR INITIALIZING SSH (ssh_initalize)" << endl;
  }
  if (!session_.initialized()) {
    clog << "ERROR INITIALIZING SSH (SSHSession::initialized)" << endl;
  }
  io_.reset(new RemoteSocketIO(plain_socket_, false));
  RemoteInfo& info = remote_info();
  info.username = session_.GetAndClearRemoteUserName();
  info.password = session_.GetAndClearRemotePassword();
}

IOSSH::~IOSSH() {
  ssh_receive_thread_.join();
  ssh_send_thread_.join();
}

bool IOSSH::ssh_initalize() {
  SOCKADDR_IN a{};

  SOCKET listener = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listener == INVALID_SOCKET) {
#ifdef _WIN32    
    int last_error = WSAGetLastError();
#else
    int last_error = errno;
#endif
    clog << "WSAGetLastError: " << last_error << endl;
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

bool IOSSH::open() { return io_->open(); }

void IOSSH::close(bool temporary) { 
  if (!temporary) {
    session_.close();
  }
  io_->close(temporary);
}

unsigned char IOSSH::getW() { return io_->getW();  }
bool IOSSH::dtr(bool raise) { return io_->dtr(raise);  }
void IOSSH::purgeIn() { io_->purgeIn();  }
unsigned int IOSSH::put(unsigned char ch) { return io_->put(ch);  }
unsigned int IOSSH::read(char *buffer, unsigned int count) { return io_->read(buffer, count);  }
unsigned int IOSSH::write(const char *buffer, unsigned int count, bool bNoTranslation) {
  return io_->write(buffer, count, bNoTranslation); 
}
bool IOSSH::carrier() { return io_->carrier(); }
bool IOSSH::incoming() { return io_->incoming(); }
unsigned int IOSSH::GetHandle() const { return io_->GetHandle(); }
unsigned int IOSSH::GetDoorHandle() const { return io_->GetDoorHandle(); }

}
}
