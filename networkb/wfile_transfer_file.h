#pragma once
#ifndef __INCLUDED_NETWORKB_WFILE_TRANSFER_FILE_H__
#define __INCLUDED_NETWORKB_WFILE_TRANSFER_FILE_H__

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <core/file.h>

#include "networkb/transfer_file.h"

namespace wwiv {
namespace net {
  
class WFileTransferFile : public TransferFile {
public:
  WFileTransferFile(const std::string& filename, std::unique_ptr<File>&& file);
  virtual ~WFileTransferFile();

  virtual int file_size() const override final;
  virtual bool Delete() override final;
  virtual bool GetChunk(char* chunk, std::size_t start, std::size_t size) override final;
  virtual bool WriteChunk(const char* chunk, std::size_t size) override final;
  virtual bool Close() override final;

 private:
  std::unique_ptr<File> file_; 
};


}  // namespace net
}  // namespace wwiv

#endif  // __INCLUDED_NETWORKB_WFILE_TRANSFER_FILE_H__
