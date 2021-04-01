/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*           Copyright (C)2016-2021, WWIV Software Services               */
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
#ifndef __INCLUDED_SDK_FIDO_FLO_FILE_H__
#define __INCLUDED_SDK_FIDO_FLO_FILE_H__

#include "sdk/fido/fido_callout.h"
#include <filesystem>
#include <string>

namespace wwiv::sdk::fido {
  
enum class flo_directive : char {
  truncate_file = '#',
  delete_file = '^',
  skip_file = '~',
  unknown = ' '
};

/**
 * Represents a FLO file on disk.  The file is locked open the entire time that
 * this class exists.
 */
class FloFile {
public:
  FloFile(const net::Network& net, std::filesystem::path p);
  virtual ~FloFile();

  [[nodiscard]] FidoAddress destination_address() const;
  [[nodiscard]] bool poll() const { return poll_; }
  void set_poll(bool p);
  [[nodiscard]] net::fido_bundle_status_t status() const { return status_; }
  [[nodiscard]] const std::vector<std::pair<std::string, flo_directive>>& flo_entries() const;

  [[nodiscard]] bool exists() const noexcept { return exists_; }
  [[nodiscard]] bool empty() const noexcept { return entries_.empty(); }
  [[nodiscard]] bool insert(const std::string& file, flo_directive directive);
  bool clear() noexcept;
  bool erase(const std::string& file);
  bool Load();
  bool Save();

private:
  const net::Network& net_;
  const std::filesystem::path path_;
  net::fido_bundle_status_t status_{net::fido_bundle_status_t::unknown};
  std::unique_ptr<wwiv::sdk::fido::FidoAddress> dest_;
  bool exists_{false};
  bool poll_{false};

  std::vector<std::pair<std::string, flo_directive>> entries_;
};

}

#endif