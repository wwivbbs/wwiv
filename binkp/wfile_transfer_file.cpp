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
#include "binkp/wfile_transfer_file.h"

#include "core/crc32.h"
#include "core/datetime.h"
#include "core/log.h"
#include "core/strings.h"
#include "sdk/fido/fido_util.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>

using std::clog;
using std::endl;
using std::string;
using std::chrono::seconds;
using std::chrono::system_clock;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::fido;
using namespace wwiv::strings;

namespace wwiv::net {

WFileTransferFile::WFileTransferFile(const string& filename, std::unique_ptr<File>&& file)
    : TransferFile(filename, file->Exists() ? file->last_write_time() : time_t_now(),
                   crc32file(file->path())),
      file_(std::move(file)) {
  LOG(INFO) << "WFileTransferFile: " << filename;
  if (filename.find(File::pathSeparatorChar) != string::npos) {
    // Don't allow filenames with slashes in it.
    throw std::invalid_argument("filename can not be relative pathed");
  }
}

WFileTransferFile::~WFileTransferFile() = default;

// TODO(rushfan): This needs to be fixed to handle >2GB files.
int WFileTransferFile::file_size() const { return static_cast<int>(file_->length()); }

bool WFileTransferFile::Delete() {
  // Since this file may still be open, need to ensure
  // that it is closed so File::Remove will work.
  if (file_->IsOpen()) {
    file_->Close();
  }
  if (!File::Remove(file_->full_pathname())) {
    return false;
  }
  if (!flo_file_) {
    return true;
  }
  flo_file_->Load();
  if (flo_file_->flo_entries().size() <= 1) {
    flo_file_->clear();
  } else {
    flo_file_->erase(file_->full_pathname());
  }
  return flo_file_->Save();
}

bool WFileTransferFile::GetChunk(char* chunk, int start, int size) {
  if (!file_->IsOpen()) {
    if (!file_->Open(File::modeBinary | File::modeReadOnly)) {
      return false;
    }
  }

  if (static_cast<int>(start + size) > file_size()) {
    LOG(ERROR) << "ERROR WFileTransferFile::GetChunk (start + size) > file_size():"
               << "values[ start: " << start << "; size: " << size
               << "; file_size(): " << file_size() << " ]";
    return false;
  }

  // TODO(rushfan): Cache the current file pointer and only re-seek
  // if needed (realistically we should ever have to seek after the
  // first time.
  file_->Seek(start, File::Whence::begin);
  return file_->Read(chunk, size) == size;
}

bool WFileTransferFile::WriteChunk(const char* chunk, int size) {
  VLOG(3) << "WFileTransferFile::WriteChunk";
  if (!file_->IsOpen()) {
    if (file_->Exists()) {
      // Don't overwrite an existing file.  Rename it away to: FILENAME.timestamp
      auto newpath = file_->path();
      newpath += StrCat(".", system_clock::to_time_t(system_clock::now()));
      File::Rename(file_->path(), newpath);
    }
    if (!file_->Open(File::modeBinary | File::modeReadWrite | File::modeCreateFile)) {
      return false;
    }
  }
  return file_->Write(chunk, size) == size;
}

bool WFileTransferFile::Close() {
  VLOG(1) << "WFileTransferFile::Close " << file_->path().string();
  file_->Close();
  return true;
}

} // namespace wwiv
