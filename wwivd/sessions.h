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
#ifndef INCLUDED_WWIVD_SESSIONS_H
#define INCLUDED_WWIVD_SESSIONS_H

#include "core/log.h"
#include "core/net.h"
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <exception>
#include <list>
#include <mutex>
#include <set>
#include <stop_token>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace wwiv::wwivd {

#ifdef _WIN32
/** A child process: its process HANDLE. */
using child_process_t = void*;
#else
/** A child process: its pid, which is also its process group id. */
using child_process_t = int;
#endif

// Per platform, in wwivd_{unix,win,os2}.cpp.
/**
 * Asks a child to hang up on its caller and exit. On Unix this is SIGHUP to
 * its process group; the BBS takes that as the caller hanging up and logs
 * them off. Does nothing on Windows and OS/2.
 */
void HangupChild(child_process_t child);
/** Ends a child (and on Unix, its process group) now. */
void KillChild(child_process_t child);

class Sessions;

/**
 * Owns a socket that wwivd itself reads and writes, while Sessions can shut
 * it down. It stops being watched before it is closed or released, so a
 * shutdown can never reach a reused socket number.
 */
class WatchedSocket final {
public:
  WatchedSocket() = default;
  /** Watched by sessions, or just owned when sessions is null. */
  WatchedSocket(Sessions* sessions, core::SocketHandle sock);
  ~WatchedSocket();
  WatchedSocket(const WatchedSocket&) = delete;
  WatchedSocket& operator=(const WatchedSocket&) = delete;
  WatchedSocket(WatchedSocket&& o) noexcept;
  WatchedSocket& operator=(WatchedSocket&& o) noexcept;

  [[nodiscard]] SOCKET get() const noexcept { return sock_.get(); }
  explicit operator bool() const noexcept { return static_cast<bool>(sock_); }
  /** Stops watching the socket and hands it over. */
  [[nodiscard]] core::SocketHandle release() noexcept;

private:
  void unwatch() noexcept;
  Sessions* sessions_{nullptr};
  core::SocketHandle sock_;
};

/**
 * The threads wwivd runs (one per caller, plus the callout thread), the
 * sockets they read, and the children they wait on, so that shutdown can
 * hang up every caller and wait for every thread before the data they use
 * goes away.
 */
class Sessions final {
public:
  /** grace is how long stop() waits after hanging up before killing children. */
  explicit Sessions(std::chrono::milliseconds grace = std::chrono::seconds(10));
  /** Calls stop(). */
  ~Sessions();
  Sessions(const Sessions&) = delete;
  Sessions& operator=(const Sessions&) = delete;

  /**
   * Runs fn on a new thread, passing it a std::stop_token if it takes one.
   * Returns false, without running fn, once stop() has begun.
   */
  template <typename Fn> bool start(Fn&& fn) {
    std::lock_guard lock(mu_);
    if (phase_ != phase_t::running) {
      return false;
    }
    prune_locked();
    auto& t = threads_.emplace_back();
    t.thread = std::jthread([this, &t, fn = std::forward<Fn>(fn)](std::stop_token st) mutable {
      {
        // Destroyed before finished(): what it owns (a caller's socket) may
        // take mu_ as it goes, and prune_locked joins done threads while
        // holding mu_.
        auto f = std::move(fn);
        try {
          if constexpr (std::is_invocable_v<decltype(f)&, std::stop_token>) {
            f(st);
          } else {
            f();
          }
        } catch (const std::exception& e) {
          LOG(ERROR) << "Session thread ended with an exception: " << e.what();
        } catch (...) {
          LOG(ERROR) << "Session thread ended with an unknown exception.";
        }
      }
      finished(t);
    });
    return true;
  }

  /** Watches sock. After stop() has begun, it is shut down at once. */
  [[nodiscard]] WatchedSocket watch(core::SocketHandle sock);

  /**
   * Registers a child that a session thread is waiting on. Returns false
   * when stop() has begun, in which case the child has already been hung up
   * (or killed). Call remove_child before reaping it.
   */
  bool add_child(child_process_t child);
  void remove_child(child_process_t child);

  /** True once stop() has begun. */
  [[nodiscard]] bool stopping() const;

  /**
   * Shuts down every watched socket, hangs up every child, and asks every
   * thread to stop. Waits up to the grace period for the threads to finish,
   * kills any child still running, then waits for every thread. New
   * threads, sockets, and children are refused (or hung up) from the start.
   * Safe to call more than once.
   */
  void stop();

  /** Number of threads that haven't finished. */
  [[nodiscard]] std::size_t running() const;
  /** Number of registered children. */
  [[nodiscard]] std::size_t children() const;

private:
  friend class WatchedSocket;
  enum class phase_t { running, hanging_up, killing };
  struct thread_t {
    std::jthread thread;
    bool done{false};
  };

  void unwatch(SOCKET sock) noexcept;
  void finished(thread_t& t);
  /** Joins and drops threads that have finished. Called with mu_ held. */
  void prune_locked();

  const std::chrono::milliseconds grace_;
  mutable std::mutex mu_;
  std::condition_variable finished_cv_;
  phase_t phase_{phase_t::running};
  std::set<SOCKET> sockets_;
  std::vector<child_process_t> children_;
  std::list<thread_t> threads_;
};

/** Sleeps for d, or until st is stopped. Returns false if it was stopped. */
bool sleep_unless_stopped(const std::stop_token& st, std::chrono::milliseconds d);

} // namespace wwiv::wwivd

#endif
