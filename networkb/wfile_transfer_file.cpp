#include "networkb/wfile_transfer_file.h"

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
using std::clog;
using std::endl;
using std::string;
using wwiv::strings::StringPrintf;

namespace wwiv {
namespace net {

WFileTransferFile::WFileTransferFile(const string& filename,
				     std::unique_ptr<File>&& file)
  : TransferFile(filename, file->Exists() ? file->last_write_time() : time(nullptr)), file_(std::move(file)) {
}

WFileTransferFile::~WFileTransferFile() {}

int WFileTransferFile::file_size() const {
  return file_->GetLength();
}

bool WFileTransferFile::Delete() {
  return file_->Delete();
}

bool WFileTransferFile::GetChunk(char* chunk, size_t start, size_t size) {
  if (!file_->IsOpen()) {
    if (!file_->Open(File::modeBinary | File::modeReadOnly)) {
      return false;
    }
  }
  
  if ((start + size) > file_size()) {
    clog << "ERROR WFileTransferFile::GetChunk (start + size) > file_size():"
         << "values[ start: " << start << "; size: " << size
	 << "; file_size(): " << file_size() << " ]" << endl;
    return false;
  }

  // TODO(rushfan): Cache the current file pointer and only re-seek
  // if needed (realistically we should ever have to seek after the
  // first time.
  file_->Seek(start, File::seekBegin);
  return file_->Read(chunk, size) == size;
}

bool WFileTransferFile::WriteChunk(const char* chunk, size_t size) {
  if (!file_->IsOpen()) {
    if (!file_->Open(File::modeBinary | File::modeReadWrite | File::modeCreateFile)) {
      return false;
    }
  }
  int num_written = file_->Write(chunk, size);
  return num_written == size;
}

}  // namespace net
} // namespace wwiv
