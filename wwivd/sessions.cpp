/**************************************************************************/
/*                                                                        */
/*                          WWIV BBS Software                             */
/*             Copyright (C)2026, WWIV Software Services                  */
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
#include "wwivd/sessions.h"

#include "core/log.h"
#include <algorithm>
#include <condition_variable>
#include <mutex>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

namespace wwiv::wwivd {

using namespace wwiv::core;

static void shutdown_socket(SOCKET sock) {
#ifdef _WIN32
  shutdown(sock, SD_BOTH);
#else
  shutdown(sock, SHUT_RDWR);
#endif
}

WatchedSocket::WatchedSocket(Sessions* sessions, SocketHandle sock)
    : sessions_(sessions), sock_(std::move(sock)) {}

WatchedSocket::~WatchedSocket() {
  // Before sock_ closes the socket.
  unwatch();
}

WatchedSocket::WatchedSocket(WatchedSocket&& o) noexcept
    : sessions_(std::exchange(o.sessions_, nullptr)), sock_(std::move(o.sock_)) {}

WatchedSocket& WatchedSocket::operator=(WatchedSocket&& o) noexcept {
  if (this != &o) {
    unwatch();
    sock_ = std::move(o.sock_);
    sessions_ = std::exchange(o.sessions_, nullptr);
  }
  return *this;
}

SocketHandle WatchedSocket::release() noexcept {
  unwatch();
  return std::move(sock_);
}

void WatchedSocket::unwatch() noexcept {
  if (sessions_ && sock_) {
    sessions_->unwatch(sock_.get());
  }
  sessions_ = nullptr;
}

Sessions::Sessions(std::chrono::milliseconds grace) : grace_(grace) {}

Sessions::~Sessions() { stop(); }

WatchedSocket Sessions::watch(SocketHandle sock) {
  if (!sock) {
    return {};
  }
  std::lock_guard lock(mu_);
  sockets_.insert(sock.get());
  if (phase_ != phase_t::running) {
    shutdown_socket(sock.get());
  }
  return {this, std::move(sock)};
}

void Sessions::unwatch(SOCKET sock) noexcept {
  std::lock_guard lock(mu_);
  sockets_.erase(sock);
}

bool Sessions::add_child(child_process_t child) {
  std::lock_guard lock(mu_);
  children_.push_back(child);
  switch (phase_) {
  case phase_t::running:
    return true;
  case phase_t::hanging_up:
    HangupChild(child);
    return false;
  case phase_t::killing:
    KillChild(child);
    return false;
  }
  return false;
}

void Sessions::remove_child(child_process_t child) {
  std::lock_guard lock(mu_);
  if (const auto it = std::ranges::find(children_, child); it != children_.end()) {
    children_.erase(it);
  }
}

bool Sessions::stopping() const {
  std::lock_guard lock(mu_);
  return phase_ != phase_t::running;
}

void Sessions::finished(thread_t& t) {
  {
    std::lock_guard lock(mu_);
    t.done = true;
  }
  finished_cv_.notify_all();
}

void Sessions::prune_locked() {
  // A thread marks itself done as the last thing it does, so these joins
  // don't wait.
  std::erase_if(threads_, [](const thread_t& t) { return t.done; });
}

void Sessions::stop() {
  std::unique_lock lock(mu_);
  if (phase_ == phase_t::running) {
    phase_ = phase_t::hanging_up;
    LOG(INFO) << "Hanging up " << sockets_.size() << " connection(s) and " << children_.size()
              << " child process(es).";
    for (const auto s : sockets_) {
      shutdown_socket(s);
    }
    for (const auto c : children_) {
      HangupChild(c);
    }
    for (auto& t : threads_) {
      t.thread.request_stop();
    }

    const auto all_done = [this] {
      return std::ranges::all_of(threads_, [](const thread_t& t) { return t.done; });
    };
    if (!finished_cv_.wait_for(lock, grace_, all_done)) {
      phase_ = phase_t::killing;
      LOG(INFO) << "Killing " << children_.size() << " child process(es) still running.";
      for (const auto c : children_) {
        KillChild(c);
      }
    }
  }
  phase_ = phase_t::killing;
  // Join outside the lock, since a finishing thread takes it. The list's
  // nodes move with it, so a thread still finishing updates the right one.
  auto threads = std::move(threads_);
  threads_.clear();
  lock.unlock();
  threads.clear();
}

std::size_t Sessions::running() const {
  std::lock_guard lock(mu_);
  return static_cast<std::size_t>(
      std::ranges::count_if(threads_, [](const thread_t& t) { return !t.done; }));
}

std::size_t Sessions::children() const {
  std::lock_guard lock(mu_);
  return children_.size();
}

bool sleep_unless_stopped(const std::stop_token& st, std::chrono::milliseconds d) {
  std::mutex mu;
  std::condition_variable_any cv;
  std::unique_lock lock(mu);
  cv.wait_for(lock, st, d, [] { return false; });
  return !st.stop_requested();
}

} // namespace wwiv::wwivd
