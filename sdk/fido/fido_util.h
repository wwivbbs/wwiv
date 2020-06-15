/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*           Copyright (C)2016-2020, WWIV Software Services               */
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
#ifndef __INCLUDED_SDK_FIDO_FIDO_UTIL_H__
#define __INCLUDED_SDK_FIDO_FIDO_UTIL_H__

#include "core/datetime.h"
#include "core/file.h"
#include "sdk/config.h"
#include "sdk/fido/fido_address.h"
#include "sdk/fido/fido_callout.h"
#include <ctime>
#include <filesystem>
#include <string>
#include <vector>

namespace wwiv {
namespace sdk {
namespace fido {

std::string packet_name(wwiv::core::DateTime& dt);
std::string bundle_name(const FidoAddress& source, const FidoAddress& dest, int dow, int bundle_number);
std::string bundle_name(const FidoAddress& source, const FidoAddress& dest, const std::string& extension);
std::string net_node_name(const FidoAddress& dest, const std::string& extension);
std::string flo_name(const FidoAddress& dest, fido_bundle_status_t status);
std::vector<std::string> dow_prefixes();
std::string dow_extension(int dow, int bundle_number);
bool is_bundle_file(const std::string& name);
bool is_packet_file(const std::string& name);
std::string control_file_name(const FidoAddress& dest, fido_bundle_status_t status);
std::string daten_to_fido(time_t t);
daten_t fido_to_daten(std::string d);
std::string to_net_node(const FidoAddress& a);
std::string to_zone_net_node(const FidoAddress& a);
std::string to_zone_net_node_point(const FidoAddress& a);

/** Splits a message to find a specific line. This will strip blank lines. */
std::vector<std::string> split_message(const std::string& string);

std::string FidoToWWIVText(const std::string& ft, bool convert_control_codes = true);
std::string WWIVToFidoText(const std::string& wt);
std::string WWIVToFidoText(const std::string& wt, int8_t max_optional_val_to_include);

FidoAddress get_address_from_single_line(const std::string& line);
FidoAddress get_address_from_origin(const std::string& text);

bool RoutesThroughAddress(const FidoAddress& a, const std::string& routes);
FidoAddress FindRouteToAddress(const FidoAddress& a, const FidoCallout& callout);

bool exists_bundle(const wwiv::sdk::Config& config, const net_networks_rec& net);
bool exists_bundle(const std::string& dir);
std::string tz_offset_from_utc();

enum class flo_directive : char {
  truncate_file = '#',
  delete_file = '^',
  skip_file = '~',
  unknown = ' '
};

/**
 * Representes a FLO file on disk.  The file is locked open the entire time that
 * this class exists.
 */
class FloFile {
public:
  FloFile(const net_networks_rec& net, std::filesystem::path p);
  virtual ~FloFile();

  wwiv::sdk::fido::FidoAddress destination_address() const;
  bool poll() const { return poll_; }
  void set_poll(bool p) { poll_ = p; }
  fido_bundle_status_t status() const { return status_; }
  const std::vector<std::pair<std::string, flo_directive>>& flo_entries() const { return entries_; }

  bool exists() const noexcept { return exists_; }
  bool empty() const noexcept { return entries_.empty(); }
  bool insert(const std::string& file, flo_directive directive);
  bool clear() noexcept {
    entries_.clear();
    poll_ = false;
    return true;
  }
  bool erase(const std::string& file);
  bool Load();
  bool Save();

private:
  const net_networks_rec& net_;
  const std::filesystem::path path_;
  fido_bundle_status_t status_{fido_bundle_status_t::unknown};
  std::unique_ptr<wwiv::sdk::fido::FidoAddress> dest_;
  bool exists_{false};
  bool poll_{false};

  std::vector<std::pair<std::string, flo_directive>> entries_;
};

class FtnDirectories {
public:
  FtnDirectories(const std::string& bbsdir, const net_networks_rec& net, std::string receive_dir);
  FtnDirectories(const std::string& bbsdir, const net_networks_rec& net);
  virtual ~FtnDirectories();

  [[nodiscard]] const std::string& net_dir() const;
  [[nodiscard]] const std::string& inbound_dir() const;
  [[nodiscard]] const std::string& temp_inbound_dir() const;
  [[nodiscard]] const std::string& temp_outbound_dir() const;
  [[nodiscard]] const std::string& outbound_dir() const;
  [[nodiscard]] const std::string& netmail_dir() const;
  [[nodiscard]] const std::string& bad_packets_dir() const;
  [[nodiscard]] const std::string& receive_dir() const;
  [[nodiscard]] const std::string& tic_dir() const;
  [[nodiscard]] const std::string& unknown_dir() const;

private:
  const std::string bbsdir_;
  const net_networks_rec net_;
  const std::string net_dir_;
  const std::string inbound_dir_;
  const std::string temp_inbound_dir_;
  const std::string temp_outbound_dir_;
  const std::string outbound_dir_;
  const std::string netmail_dir_;
  const std::string bad_packets_dir_;
  const std::string receive_dir_;
  const std::string tic_dir_;
  const std::string unknown_dir_;
};
}  // namespace fido
}  // namespace net
}  // namespace wwiv

#endif  // __INCLUDED_SDK_FIDO_FIDO_UTIL_H__
