/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2022, WWIV Software Services            */
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
#ifndef INCLUDED_COMMON_REMOTE_PIPE_IO_H
#define INCLUDED_COMMON_REMOTE_PIPE_IO_H

#include "common/remote_io.h"
#include "core/pipe.h"
#include <atomic>
#include <cstdint>
#include <mutex>
#include <deque>
#include <thread>

namespace wwiv::common {

class RemotePipeIO final : public RemoteIO {
 public:
  RemotePipeIO(unsigned int node_number, bool telnet);
  ~RemotePipeIO() override;

  bool open() override;
  void close(bool bIsTemporary) override;
  unsigned char getW() override;
  bool disconnect() override;
  void purgeIn() override;
  unsigned int put(unsigned char ch) override;
  unsigned int read(char *buffer, unsigned int count) override;
  unsigned int write(const char *buffer, unsigned int count, bool no_translation = false) override;
  bool connected() override;
  bool incoming() override;
  void StopThreads();
  void StartThreads();
  unsigned int GetHandle() const override;

  void set_binary_mode(bool b) override;
  std::optional<ScreenPos> screen_position() override;

  // These aren't part of RemoteIO

  // VisibleForTesting
  void AddStringToInputBuffer(int start, int end, const char* buffer);
  std::deque<char>& queue() { return queue_; }


private:
  void HandleTelnetIAC(unsigned char nCmd, unsigned char nParam);
  void PipeLoop();

  std::deque<char> queue_;
  mutable std::mutex mu_;
  mutable std::mutex threads_started_mu_;
  core::Pipe data_pipe_;
  core::Pipe control_pipe_;
  int node_number_{0};
  std::thread read_thread_;
  std::atomic<bool> stop_;
  bool threads_started_{false};
  bool telnet_{true};
  bool skip_next_{false};
};


} // namespace wwiv::common

#endif
