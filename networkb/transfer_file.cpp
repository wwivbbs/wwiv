/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2019, WWIV Software Services             */
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
#include "networkb/transfer_file.h"

#include "core/crc32.h"
#include "core/log.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <string>

using std::chrono::seconds;
using std::chrono::system_clock;
using std::endl;
using std::string;
using namespace wwiv::strings;

namespace wwiv::net {

TransferFile::TransferFile(const string& filename, time_t timestamp, uint32_t crc)
  : filename_(filename), timestamp_(timestamp), crc_(crc) {}

TransferFile::~TransferFile() = default;

string TransferFile::as_packet_data(int size, int offset) const {
  string dataline = fmt::format("{} {} {} {}", filename_, size, timestamp_, offset);
  if (crc_ != 0) {
    dataline += fmt::sprintf(" %08X", crc_);
  }
  return dataline;
}

InMemoryTransferFile::InMemoryTransferFile(const std::string& filename, const std::string& contents, time_t timestamp)
  : TransferFile(filename, timestamp, wwiv::core::crc32string(contents)), 
    contents_(contents) {}

InMemoryTransferFile::InMemoryTransferFile(const std::string& filename, const std::string& contents)
  : InMemoryTransferFile(filename, contents, system_clock::to_time_t(std::chrono::system_clock::now())) {}

InMemoryTransferFile::~InMemoryTransferFile() = default;

bool InMemoryTransferFile::GetChunk(char* chunk, size_t start, size_t size) {
  if ((start + size) > contents_.size()) {
    LOG(ERROR) << "ERROR InMemoryTransferFile::GetChunk (start + size) > file_size():"
        << "values[ start: " << start << "; size: " << size
	      << "; file_size(): " << file_size() << " ]";
    return false;
  }
  memcpy(chunk, &contents_.data()[start], size);
  return true;
}

bool InMemoryTransferFile::WriteChunk(const char* chunk, size_t size) {
  contents_.append(string(chunk, size));
  return true;
}

bool InMemoryTransferFile::Close() {
  return true;
}


} // namespace wwiv
