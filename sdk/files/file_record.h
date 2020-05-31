/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5                              */
/*                Copyright (C)2020, WWIV Software Services               */
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
#ifndef __INCLUDED_SDK_FILES_FILE_RECORD_H__
#define __INCLUDED_SDK_FILES_FILE_RECORD_H__

#include "sdk/config.h"
#include <string>

namespace wwiv::sdk::files {

class FileRecord final {
public:
  explicit FileRecord(const uploadsrec& u) : u_(u) {}
  FileRecord();
  ~FileRecord() = default;

  [[nodiscard]] uploadsrec& u() { return u_; }
  [[nodiscard]] const uploadsrec& u() const { return u_; }
  [[nodiscard]] uint32_t numbytes() const noexcept { return u_.numbytes; }
  [[nodiscard]] bool has_extended_description() const noexcept;
  void set_extended_description(bool b);

  [[nodiscard]] bool pd_or_shareware() const noexcept { return mask(mask_PD); }
  void set_pd_or_shareware(bool b) { set_mask(mask_PD, b); }

  [[nodiscard]] bool mask(int mask) const;
  void set_mask(int mask, bool on);

  bool set_filename(const std::string& unaligned_filename);
  bool set_description(const std::string& desc);
  [[nodiscard]] std::string aligned_filename() const ;
  [[nodiscard]] std::string unaligned_filename() const ;

private:
  uploadsrec u_;
};

} // namespace wwiv::sdk::files

#endif  // __INCLUDED_SDK_FILES_FILE_RECORD_H__
