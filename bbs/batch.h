/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2020, WWIV Software Services             */
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
#ifndef __INCLUDED_BBS_BATCH_H__
#define __INCLUDED_BBS_BATCH_H__

#include "core/stl.h"
#include "core/wwivport.h"
#include "sdk/files/file_record.h"
#include <chrono>
#include <string>
#include <vector>

/**
 * One Batch Upload/Download entry.
 */
class BatchEntry {
public:
  BatchEntry();
  BatchEntry(std::string fn, int d, int l, bool s);
  BatchEntry(const wwiv::sdk::files::FileName& fn, int d, int l, bool s);

  [[nodiscard]] int32_t time(int modem_speed) const;

  /** Aligned filename */
  [[nodiscard]] std::string aligned_filename() const { return filename_; }

  /** The read directory number (as in directoryrec) */
  [[nodiscard]] int16_t dir() const { return dir_; } 

  /** Size of the file*/
  [[nodiscard]] int32_t len() const { return len_; }

  [[nodiscard]] bool sending() const { return sending_; }

private:
  std::string filename_;
  int16_t dir_{-1};
  int32_t len_{0};
  bool sending_{false};
};

class Batch {
public:
  bool clear() {
    entry.clear();
    return true;
  }

  bool AddBatch(const BatchEntry& b) {
    entry.push_back(b);
    return true;
  }

  bool AddBatch(BatchEntry&& b) {
    entry.emplace_back(b);
    return true;
  }

  [[nodiscard]] int FindBatch(const std::string& file_name);
  bool RemoveBatch(const std::string& file_name);
  // deletes an entry by position;
  bool delbatch(size_t pos);
  std::vector<BatchEntry>::iterator delbatch(std::vector<BatchEntry>::iterator& it);

  [[nodiscard]] long dl_time_in_secs() const;
  [[nodiscard]] bool contains_file(const std::string& file_name) const;
  [[nodiscard]] bool contains_file(const wwiv::sdk::files::FileName& fn) const;

  [[nodiscard]] int numbatchdl() const {
    auto r = 0;
    for (const auto& e : entry) {
      if (e.sending())
        r++;
    }
    return r;
  }

  [[nodiscard]] size_t numbatchul() const noexcept {
    size_t r = 0;
    for (const auto& e : entry) {
      if (!e.sending())
        r++;
    }
    return r;
  }

  [[nodiscard]] int size() const noexcept {
    return entry.size();
  }

  [[nodiscard]] ssize_t ssize() const noexcept {
    return wwiv::stl::ssize(entry);
  }

  [[nodiscard]] bool empty() const noexcept {
    return entry.empty();
  }

  std::vector<BatchEntry> entry;
};

void upload(int dn);
void dszbatchdl(bool bHangupAfterDl, const std::string& command_line, const std::string& description);
int batchdl(int mode);
void didnt_upload(const BatchEntry& b);
void ymbatchdl(bool bHangupAfterDl);
void zmbatchdl(bool bHangupAfterDl);

std::chrono::seconds time_to_transfer(int32_t file_size, int32_t modem_speed);

#endif  // __INCLUDED_BBS_BATCH_H__
