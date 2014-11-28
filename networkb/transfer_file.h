#pragma once
#ifndef __INCLUDED_NETWORKB_TRANSFER_FILE_H__
#define __INCLUDED_NETWORKB_TRANSFER_FILE_H__

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace wwiv {
namespace net {
  
class TransferFile {
public:
  TransferFile(const std::string& filename, time_t timestamp);
  virtual ~TransferFile();

  const std::string filename() const { return filename_; }
  virtual const std::string as_packet_data(int offset) const {
    return as_packet_data(file_size(), offset);
  }

  virtual int file_size() const = 0;
  virtual bool GetChunk(char* chunk, std::size_t start, std::size_t size) = 0;

 protected:
  virtual const std::string as_packet_data(int size, int offset) const final;

  const std::string filename_;
  const time_t timestamp_;
};

class InMemoryTransferFile : public TransferFile {
public:
  InMemoryTransferFile(const std::string& filename, const std::string& contents,
                       std::chrono::time_point<std::chrono::system_clock> timestamp);

  InMemoryTransferFile(const std::string& filename, const std::string& contents);
  virtual ~InMemoryTransferFile();

  virtual int file_size() const override final { return contents_.length(); }
  virtual bool GetChunk(char* chunk, std::size_t start, std::size_t size) override final;

private:
  const std::string contents_;
};


}  // namespace net
}  // namespace wwiv

#endif  // __INCLUDED_NETWORKB_TRANSFER_FILE_H__
