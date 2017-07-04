/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2016-2017, WWIV Software Services             */
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

#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "cryptlib.h"

#include "core/log.h"
#include "core/net.h"
#include "core/os.h"
#include "core/strings.h"
#include "bbs/bbs.h"

#include <libssh/libssh.h>
#include <libssh/server.h>

using std::string;
using std::thread;
using std::unique_ptr;

using namespace wwiv::core;
using namespace wwiv::strings;

namespace wwiv {
namespace bbs {

struct socket_error: public std::runtime_error {
  socket_error(const std::string& message): std::runtime_error(message) {}
};

Keys::Keys() {

}

// Check that at least one private key exists
bool Keys::Exist() {
  File rsafile (a()->config()->datadir(), "ssh_host_rsa_key");
  File dsafile (a()->config()->datadir(), "ssh_host_dsa_key");
  File ecdsafile (a()->config()->datadir(), "ssh_host_ecdsa_key");

  if (rsafile.Exists() || dsafile.Exists() || ecdsafile.Exists()) {
    return true;
  } else {
    return false;
  }
}

// Validate that we have at least one valid private key
bool Keys::Validate() {
  int rsastatus = 0;
  int dsastatus = 0;
  int ecdsastatus = 0;
  int importstatus = 0;

  ssh_key rsasshkey;
  ssh_key dsasshkey;
  ssh_key ecdsasshkey;

  string *rsakeyfilestr = new string (a()->config()->datadir() + "ssh_host_rsa_key");
  string *dsakeyfilestr = new string (a()->config()->datadir() + "ssh_host_dsa_key");
  string *ecdsakeyfilestr = new string (a()->config()->datadir() + "ssh_host_ecdsa_key");

  // Validate RSA Private Key
  //importstatus = ssh_pki_import_privkey_file(rsakeyfilestr_->c_str(), a()->config()->config()->systempw, NULL, NULL, &rsasshkey);
  importstatus = ssh_pki_import_privkey_file(rsakeyfilestr->c_str(), NULL, NULL, NULL, &rsasshkey);

  if (importstatus==SSH_OK) {
    if (ssh_key_is_private(rsasshkey) && ssh_key_type(rsasshkey)==SSH_KEYTYPE_RSA) {
      // RSA Key Private Key Validated
      rsastatus = 1;
    } else {
      // RSA Private Key Failure
      LOG(INFO) << "SSH RSA Keyfile Invalid Format or Type (is it a public key?): " << *rsakeyfilestr;
    }
  } else {
    if (importstatus==SSH_EOF) { 
      // RSA Private Key File Not Found or Permission Denied
      LOG(INFO) << "SSH RSA Keyfile Not Found or Permission Denied: " << *rsakeyfilestr;
    } else
    // RSA Private Key File Unknown Import Error
    LOG(INFO) << "SSH RSA Import Failed (Unknown Error): " << *rsakeyfilestr;
  }
  ssh_key_free(rsasshkey);

  // Validate DSA Private Key
  importstatus = ssh_pki_import_privkey_file(dsakeyfilestr->c_str(), NULL, NULL, NULL, &dsasshkey);

  if (importstatus==SSH_OK) {
    if (ssh_key_is_private(dsasshkey) && ssh_key_type(dsasshkey)==SSH_KEYTYPE_DSS) {
      // DSA Key Private Key Validated
      dsastatus = 1;
    } else {
      // DSA Private Key Failure
      LOG(INFO) << "SSH DSA Keyfile Invalid Format or Type (is it a public key?): " << *dsakeyfilestr;
    }
  } else {
    if (importstatus==SSH_EOF) { 
      // DSA Private Key File Not Found or Permission Denied
      LOG(INFO) << "SSH DSA Keyfile Not Found or Permission Denied: " << *dsakeyfilestr;
    } else
    // DSA Private Key File Unknown Import Error
    LOG(INFO) << "SSH DSA Import Failed (Unknown Error): " << *dsakeyfilestr;
  }
  ssh_key_free(dsasshkey);

  // Validate ECDSA Private Key
  ecdsastatus = ssh_pki_import_privkey_file(ecdsakeyfilestr->c_str(), NULL, NULL, NULL, &ecdsasshkey);

   if (importstatus==SSH_OK) {
    if (ssh_key_is_private(ecdsasshkey) && ssh_key_type(ecdsasshkey)==SSH_KEYTYPE_ECDSA) {
      // ECDSA Key Private Key Validated
      ecdsastatus = 1;
    } else {
      // ECDSA Private Key Failure
      LOG(INFO) << "SSH ECDSA Keyfile Invalid Format or Type (is it a public key?): " << *ecdsakeyfilestr;
    }
  } else {
    if (importstatus==SSH_EOF) { 
      // ECDSA Private Key File Not Found or Permission Denied
      LOG(INFO) << "SSH ECDSA Keyfile Not Found or Permission Denied: " << *ecdsakeyfilestr;
    } else
    // ECDSA Private Key File Unknown Import Error
    LOG(INFO) << "SSH ECDSA Import Failed (Unknown Error): " << *ecdsakeyfilestr;
  }
  ssh_key_free(ecdsasshkey);

  // Check if we have a valid private key
  if (rsastatus || dsastatus || ecdsastatus) {
    // At least one key is valid
    return true;
  } else {
    // No valid keys found
    LOG(ERROR) << "No valid SSH keys found";
    return false;
  }

}

// Generate SSH Private Keys
bool Keys::Generate() {
  int rsastatus = 0;
  int dsastatus = 0;
  int ecdsastatus = 0;

  ssh_key rsasshkey;
  ssh_key dsasshkey;
  ssh_key ecdsasshkey;

  string *rsakeyfilestr = new string (a()->config()->datadir() + "ssh_host_rsa_key");
  string *dsakeyfilestr = new string (a()->config()->datadir() + "ssh_host_dsa_key");
  string *ecdsakeyfilestr = new string (a()->config()->datadir() + "ssh_host_ecdsa_key");

  #define RSA_KEYLEN 4096
  #define DSA_KEYLEN 1024
  #define ECDSA_KEYLEN 521

// ssh_pki_export_privkey_file does not set UMASK/file-mode making keys world readable
#ifdef __unix__
  umask(0177);
#endif

// TODO: Handle umask for Win32
#ifdef _WIN32
#endif

  // Generate RSA Private Key
  rsastatus = ssh_pki_generate(SSH_KEYTYPE_RSA, RSA_KEYLEN, &rsasshkey);
  if (rsastatus==SSH_OK) {
    LOG(INFO) << "SSH Exporting RSA Keyfile: " << *rsakeyfilestr;
    //ssh_pki_export_privkey_file(rsasshkey, a()->config()->config()->systempw, NULL, NULL, rsakeyfilestr_->c_str());
    ssh_pki_export_privkey_file(rsasshkey, NULL, NULL, NULL, rsakeyfilestr->c_str());
  } else {
    LOG(ERROR) << "SSH RSA Private Key Generation Failed";
  }
  ssh_key_free(rsasshkey);

  // Generate DSA Private Key
  dsastatus = ssh_pki_generate(SSH_KEYTYPE_DSS, DSA_KEYLEN, &dsasshkey);
  if (dsastatus==SSH_OK) {
    LOG(INFO) << "SSH Exporting DSA Keyfile: " << *dsakeyfilestr;
    ssh_pki_export_privkey_file(dsasshkey, NULL, NULL, NULL, dsakeyfilestr->c_str());

  } else {
    LOG(ERROR) << "SSH ECDSA Private Key Generation Failed";
  }
  ssh_key_free(dsasshkey);

  // Generate ECDSA Private Key
  ecdsastatus = ssh_pki_generate(SSH_KEYTYPE_ECDSA, ECDSA_KEYLEN, &ecdsasshkey);
  if (ecdsastatus==SSH_OK) {
    LOG(INFO) << "SSH Exporting ECDSA Keyfile: " << *ecdsakeyfilestr;
    ssh_pki_export_privkey_file(ecdsasshkey, NULL, NULL, NULL, ecdsakeyfilestr->c_str());
  } else {
    LOG(ERROR) << "SSH ECDSA Private Key Generation Failed";
  }
  ssh_key_free(ecdsasshkey);

  // Check if we succesfully created our keys
  if ((rsastatus==SSH_OK) && (dsastatus==SSH_OK) && (ecdsastatus==SSH_OK)) {
    // Keys should be created successfully, let's make sure they exist and are valid
    if (Exist() && Validate()) {
      // Private Keys are good to go!
      return true;
    } else {
      // One of the key's doesn't exist or doesn't validate
      LOG(ERROR) << "SSH Key Generation Failed during Validation";
      return false;
    }

  } else {
    // At least one key failed to create
    LOG(ERROR) << "SSH Key Generation Failed";
    return false;
  }

}

bool SSHSession::SSHAuthenticate(ssh_session session) {

  ssh_message message;

  do {
      message=ssh_message_get(session);
      if(!message)
	  break;
      switch(ssh_message_type(message)){
	  case SSH_REQUEST_AUTH:
	      switch(ssh_message_subtype(message)) {
		  case SSH_AUTH_METHOD_NONE:
		      ssh_message_auth_reply_success(message,0);
		      ssh_message_free(message);
		      return 1;
		  default:
		      ssh_message_auth_set_methods(message, SSH_AUTH_METHOD_NONE);
		      ssh_message_reply_default(message);
		      break;
	      }
	      break;
	  default:
	      ssh_message_auth_set_methods(message, SSH_AUTH_METHOD_NONE);
	      ssh_message_reply_default(message);
      }
      ssh_message_free(message);
  } while (1);
  return 0;

}

SSHSession::SSHSession(int socket_handle) : socket_handle_(socket_handle) {
  // GCC 4.9 has issues with assigning this to false in the class.
  closed_.store(false);

  ssh_bind sshbind;
  ssh_message message;
  int auth=0;
  int shell=0;

  Keys keys;
  string *rsakeyfilestr_ = new string (a()->config()->datadir() + "ssh_host_rsa_key");
  string *dsakeyfilestr_ = new string (a()->config()->datadir() + "ssh_host_dsa_key");
  string *ecdsakeyfilestr_ = new string (a()->config()->datadir() + "ssh_host_ecdsa_key");

  sshbind = ssh_bind_new();
  session = ssh_new();

  // For Debug set to 3 for protocol, 4 for packets
  ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_LOG_VERBOSITY_STR,"0");
  ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_RSAKEY, rsakeyfilestr_->c_str());
  ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_DSAKEY, dsakeyfilestr_->c_str());
  ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_ECDSAKEY, ecdsakeyfilestr_->c_str());
  ssh_bind_options_set(sshbind, SSH_BIND_OPTIONS_BANNER, "WWIV");
  ssh_init();
  ssh_bind_set_fd(sshbind, socket_handle);

  if (ssh_bind_accept_fd(sshbind, session, socket_handle) == SSH_OK) {
    LOG(INFO) << "Accepted SSH connection ";
  } else {
    LOG(ERROR) << "SSH Connection Bind Failed";
    ssh_finalize();
    return;
  }

#ifdef PAUSE_ON_SSH_CONNECT_FOR_DEBUGGER
  LOG(ERROR) << "Waiting for debugger..." <<;
  getchar();
#endif  // PAUSE_ON_SSH_CONNECT_FOR_DEBUGGER

  if (ssh_handle_key_exchange(session)) {
    LOG(ERROR) << "SSH Key Exchange Failed ";
    ssh_finalize();
    return;
  }

  // Lets Authenticate!
  auth = SSHAuthenticate(session);

  if (!auth) {
    LOG(ERROR) << "SSH Authenticate Failed!";
    ssh_disconnect(session);
    ssh_finalize();
    return;
  } else {
  }

  // Setup Channel
  do {
    message = ssh_message_get(session);
    if(message) {
	if(ssh_message_type(message) == SSH_REQUEST_CHANNEL_OPEN &&
		ssh_message_subtype(message) == SSH_CHANNEL_SESSION) {
	    chan = ssh_message_channel_request_open_reply_accept(message);
	    ssh_message_free(message);
	    break;
	} else {
	    ssh_message_reply_default(message);
	    ssh_message_free(message);
	}
    } else {
	break;
    }
  } while(!chan);

  if(!chan) {
    LOG(ERROR) << "SSH client did not ask for a channel ("<< ssh_get_error(session) << ")";
    ssh_disconnect(session);
    ssh_finalize();
    return;
  }

  // Setup Interactive Shell/PTY
  do {
      message = ssh_message_get(session);
      if(message != NULL) {
	  if(ssh_message_type(message) == SSH_REQUEST_CHANNEL) {
	      if(ssh_message_subtype(message) == SSH_CHANNEL_REQUEST_SHELL) {
		  shell = 1;
		  ssh_message_channel_request_reply_success(message);
		  ssh_message_free(message);
		  break;
	      } else if(ssh_message_subtype(message) == SSH_CHANNEL_REQUEST_PTY) {
		  ssh_message_channel_request_reply_success(message);
		  ssh_message_free(message);
		  continue;
	      }
	  }
	  ssh_message_reply_default(message);
	  ssh_message_free(message);
      } else {
	  break;
      }
  } while(!shell);

  if(!shell) {
    LOG(ERROR) << "SSH client did not ask for a shell ("<< ssh_get_error(session) << ")";
    ssh_disconnect(session);
    ssh_finalize();
    return;
  }

  initialized_ = true;

}

int SSHSession::PushData(const char* data, size_t size) {
  int bytes_copied = 0;
  VLOG(2) << "SSHSession::PushData: " << string(data, size);
  std::lock_guard<std::mutex> lock(mu_);
  bytes_copied = ssh_channel_write(chan, data, size);
  if (bytes_copied < 0) {
    return SSH_ERROR;
  } else {
    return bytes_copied;
  }
}

int SSHSession::PopData(char* data, size_t buffer_size) {
  int bytes_copied = 0;
  VLOG(2) << "SSHSession::PopData";
  std::lock_guard<std::mutex> lock(mu_);
  bytes_copied = ssh_channel_read(chan, data, buffer_size, 0);
  if (bytes_copied < 0) {
    return SSH_ERROR;
  } else {
    return bytes_copied;
  }
}

bool SSHSession::close() {
  std::lock_guard<std::mutex> lock(mu_);
  if (closed_.load()) {
    return false;
  }
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
      if (num_read == SSH_ERROR) {
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
	if (num_sent == SSH_ERROR) {
	  break;
	}
        VLOG(1) << "writer_thread: pushed_data: " << num_sent;
      }
    }
  } catch (const socket_error& e) {
    VLOG(1) << e.what();
  }
  closesocket(socket);
}

IOSSH::IOSSH(SOCKET ssh_socket) 
  : ssh_socket_(ssh_socket), session_(ssh_socket) {
  static bool initialized = RemoteSocketIO::Initialize();
  if (!ssh_initialize()) {
    VLOG(1) << "ERROR INITIALIZING SSH (ssh_initialize)";
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

bool IOSSH::ssh_initialize() {
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
    LOG(ERROR) << "IOSSH bind failed";
    closesocket(listener);
    return false;
  }

  result = listen(listener, 1);
  if (result == SOCKET_ERROR) {
    LOG(ERROR) << "IOSSH listen failed";
    closesocket(listener);
    return false;
  }

  socklen_t addr_len = sizeof(a);
  result = getsockname(listener, reinterpret_cast<struct sockaddr*>(&a), &addr_len);
  if (result == SOCKET_ERROR) {
    LOG(ERROR) << "IOSSH getsockname failed";
    closesocket(listener);
    return false;
  }

  SOCKET pipe_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  result = connect(pipe_socket, reinterpret_cast<struct sockaddr*>(&a), addr_len);
  if (result == SOCKET_ERROR) {
    LOG(ERROR) << "IOSSH pipe_socket failed";
    closesocket(listener);
    return false;
  }

  plain_socket_ = accept(listener, reinterpret_cast<struct sockaddr*>(&a), &addr_len);
  if (plain_socket_ == INVALID_SOCKET) {
    LOG(ERROR) << "IOSSH accept failed";
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

}
}
