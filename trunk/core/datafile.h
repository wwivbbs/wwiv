/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.0x                             */
/*               Copyright (C)2014, WWIV Software Services                */
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
#ifndef __INCLUDED_CORE_DATAFILE_H__
#define __INCLUDED_CORE_DATAFILE_H__

#include <cstddef>
#include <string>
#include "core/file.h"
#include "core/inifile.h" // for FilePath

namespace wwiv {
namespace core {

template <typename RECORD, std::size_t SIZE = sizeof(RECORD)>
class DataFile {
public:
  DataFile(const std::string& dir, const std::string& filename,
           int nFileMode = File::modeDefault,
           int nShareMode = File::shareUnknown) 
      : DataFile(FilePath(dir, filename), nFileMode, nShareMode) {}
  explicit DataFile(const std::string& full_file_name,
           int nFileMode = File::modeDefault,
           int nShareMode = File::shareUnknown) 
      : file_(full_file_name) {
    file_.Open(nFileMode, nShareMode);
  }
  virtual ~DataFile() {}

  File& file() const { return file_; }
  bool ok() const { return file_.IsOpen(); }
  bool Read(RECORD* record) { return file_.Read(record, SIZE) == SIZE; }
  bool Read(int record_number, RECORD* record) {
    if (!Seek(record_number)) {
      return false;
    }
    return Read(record);
  }

  bool Write(const RECORD* record) { return file_.Write(record, SIZE) == SIZE; }
  bool Write(int record_number, const RECORD* record) {
    if (!Seek(record_number)) {
      return false;
    }
    return Write(record);
  }
  bool Seek(int record_number) { return file_.Seek(record_number * SIZE, File::seekBegin) == (record_number * SIZE); }
  int number_of_records() { return file_.GetLength() / SIZE; }

  explicit operator bool() const { return file_.IsOpen(); }

private:
  File file_;
};

}
}

#endif  // __INCLUDED_CORE_DATAFILE_H__
