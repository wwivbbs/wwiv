/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2020, WWIV Software Services             */
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
#ifndef INCLUDED_NETORKB_RECEIVE_FILE_H
#define INCLUDED_NETORKB_RECEIVE_FILE_H

#include "core/log.h"
#include "core/strings.h"
#include "binkp/transfer_file.h"
#include <cstdint>
#include <ctime>
#include <memory>
#include <string>
#include <utility>

namespace wwiv::net {

class ReceiveFile {
public:
  ReceiveFile(TransferFile* file, std::string filename, long expected_length, time_t timestamp,
              uint32_t crc)
      : file_(file), filename_(std::move(filename)), expected_length_(expected_length),
        timestamp_(timestamp), length_(0), crc_(crc) {
    VLOG(1) << "ReceiveFile: " << filename;
  }
  ~ReceiveFile() = default;

  bool WriteChunk(const char* chunk, int size) {
    const auto ok = file_->WriteChunk(chunk, size);
    if (ok) {
      length_ += size;
    }
    return ok;
  }

  bool WriteChunk(const std::string& chunk) {
    const auto cs = wwiv::strings::ssize(chunk);
    if (!file_->WriteChunk(chunk.data(), cs)) {
      return false;
    }
    length_ += cs;
    return true;
  }

  [[nodiscard]] std::string filename() const { return filename_; }
  [[nodiscard]] long expected_length() const { return expected_length_; }
  [[nodiscard]] long length() const { return length_; }
  [[nodiscard]] time_t timestamp() const { return timestamp_; }
  [[nodiscard]] bool Close() { return file_->Close(); }
  [[nodiscard]] uint32_t crc() const { return crc_; }

  std::unique_ptr<TransferFile> file_;
  std::string filename_;
  long expected_length_{0};
  time_t timestamp_{0};
  long length_{0};
  uint32_t crc_{0};
};

} // namespace

#endif
