/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2021, WWIV Software Services             */
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
#ifndef INCLUDED_NETWORKB_TRANSFER_FILE_H
#define INCLUDED_NETWORKB_TRANSFER_FILE_H

#include "core/stl.h"
#include <chrono>
#include <cstdint>
#include <string>

namespace wwiv::net {
  
class TransferFile {
public:
  TransferFile(std::string filename, time_t timestamp, uint32_t crc);
  virtual ~TransferFile();

  [[nodiscard]] std::string filename() const { return filename_; }
  [[nodiscard]] std::string as_packet_data(int offset) const {
    return as_packet_data(file_size(), offset);
  }

  [[nodiscard]] virtual int file_size() const = 0;
  virtual bool Delete() = 0;
  virtual bool GetChunk(char* chunk, int start, int size) = 0;
  virtual bool WriteChunk(const char* chunk, int size) = 0;
  virtual bool Close() = 0;

 protected:
  [[nodiscard]] std::string as_packet_data(int size, int offset) const;

  const std::string filename_;
  const time_t timestamp_ = 0;
  const uint32_t crc_ = 0;
};

class InMemoryTransferFile : public TransferFile {
public:
  InMemoryTransferFile(const std::string& filename, const std::string& contents, time_t timestamp);

  InMemoryTransferFile(const std::string& filename, const std::string& contents);
  virtual ~InMemoryTransferFile();

  // for testing.
  [[nodiscard]] virtual const std::string& contents() const final { return contents_; }

  [[nodiscard]] int file_size() const override final { return stl::size_int(contents_); }
  bool Delete() override { contents_.clear(); return true; }
  bool GetChunk(char* chunk, int start, int size) override final;
  bool WriteChunk(const char* chunk, int size) override final;
  bool Close() override final;

private:
  std::string contents_;
};


}  // namespace

#endif
