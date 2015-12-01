/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.0x                             */
/*               Copyright (C)2015, WWIV Software Services                */
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
#ifndef __INCLUDED_NETORKB_RECEIVE_FILE_H__
#define __INCLUDED_NETORKB_RECEIVE_FILE_H__

#include <ctime>
#include <memory>
#include <string>

#include "networkb/transfer_file.h"

namespace wwiv {
namespace net {

class  ReceiveFile {
 public:
 	ReceiveFile(TransferFile* file, const std::string& filename, long expected_length, time_t timestamp)
      : file_(file), filename_(filename), expected_length_(expected_length), timestamp_(timestamp), length_(0) {}
 	~ReceiveFile() {}

  bool WriteChunk (const char* chunk, size_t size) { 
    bool ok = file_->WriteChunk(chunk, size); 
    if (ok) {
      length_ += size;
    }
    return ok;
  }

  const std::string filename() const { return filename_; }
  long expected_length() const { return expected_length_; }
  long length() const { return length_; }
  time_t timestamp() const { return timestamp_; }
  bool Close() { return file_->Close(); }

  std::unique_ptr<TransferFile> file_;
  std::string filename_;
  long expected_length_;
  time_t timestamp_;
  long length_;
};

}  // namespace net
} // namespace wwiv

#endif  // __INCLUDED_NETORKB_RECEIVE_FILE_H__
