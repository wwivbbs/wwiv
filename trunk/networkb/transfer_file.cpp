#include "networkb/transfer_file.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <string>

#include "core/strings.h"

using std::chrono::seconds;
using std::chrono::system_clock;
using std::endl;
using std::string;
using wwiv::strings::StringPrintf;

namespace wwiv {
namespace net {

TransferFile::TransferFile(const string& filename, time_t timestamp)
  : filename_(filename), timestamp_(timestamp) {}

TransferFile::~TransferFile() {}

const string TransferFile::as_packet_data(int size, int offset) const {
  return StringPrintf("%s %u %u %d", filename_.c_str(), size, timestamp_, offset);
}

InMemoryTransferFile::InMemoryTransferFile(const std::string& filename, const std::string& contents, time_t timestamp)
  : TransferFile(filename, timestamp), 
    contents_(contents) {}

InMemoryTransferFile::InMemoryTransferFile(const std::string& filename, const std::string& contents)
  : InMemoryTransferFile(filename, contents, system_clock::to_time_t(std::chrono::system_clock::now())) {}

InMemoryTransferFile::~InMemoryTransferFile() {}

bool InMemoryTransferFile::GetChunk(char* chunk, size_t start, size_t size) {
  if ((start + size) > contents_.size()) {
    std::clog << "ERROR InMemoryTransferFile::GetChunk (start + size) > file_size():"
              << "values[ start: " << start << "; size: " << size
	            << "; file_size(): " << file_size() << " ]" << endl;
    return false;
  }
  memcpy(chunk, &contents_.data()[start], size);
  return true;
}

bool InMemoryTransferFile::WriteChunk(const char* chunk, size_t size) {
  contents_.append(string(chunk, size));
  return true;
}

}  // namespace net
} // namespace wwiv
