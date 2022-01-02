/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5                            */
/*             Copyright (C)1998-2022, WWIV Software Services             */
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
/*                                                                        */
/**************************************************************************/
#include "sdk/wwivd_config.h"

#include "core/cereal_utils.h"
#include "core/file.h"
#include "core/jsonfile.h"
#include "sdk/config.h"

#include <cereal/access.hpp>
#include <cereal/cereal.hpp>

using namespace wwiv::core;

#ifdef DELETE
#undef DELETE
#endif  // DELETE

CEREAL_SPECIALIZE_FOR_ALL_ARCHIVES(wwiv::sdk::wwivd_data_mode_t,
                                   cereal::specialization::non_member_load_save_minimal);

namespace cereal {

template <class Archive> std::string save_minimal(Archive const&, const wwiv::sdk::wwivd_data_mode_t& t) {
  return to_enum_string<wwiv::sdk::wwivd_data_mode_t>(
      t, {"socket", "pipe"});
}

template <class Archive>
void load_minimal(Archive const&, wwiv::sdk::wwivd_data_mode_t& t, const std::string& v) {
  t = from_enum_string<wwiv::sdk::wwivd_data_mode_t>(
      v, {"socket", "pipe"});
}

} // namespace cereal

namespace wwiv::sdk {

template <class Archive>
void serialize(Archive& ar, wwivd_blocking_t &b) {
  SERIALIZE(b, mailer_mode);
  SERIALIZE(b,use_badip_txt);
  SERIALIZE(b, use_goodip_txt);
  SERIALIZE(b, max_concurrent_sessions);

  SERIALIZE(b, auto_blocklist);
  SERIALIZE(b, auto_bl_sessions);
  SERIALIZE(b, auto_bl_seconds);
  SERIALIZE(b, use_dns_rbl);
  SERIALIZE(b, dns_rbl_server);
  SERIALIZE(b, use_dns_cc);
  SERIALIZE(b, dns_cc_server);
  SERIALIZE(b, block_cc_countries);
  SERIALIZE(b, block_duration);
}

template <class Archive>
void serialize(Archive& ar, wwivd_matrix_entry_t &a) {
  SERIALIZE(a, description);
  SERIALIZE(a, end_node);
  SERIALIZE(a, key);
  SERIALIZE(a, local_node);
  SERIALIZE(a, name);
  SERIALIZE(a, require_ansi);
  SERIALIZE(a, ssh_cmd);
  SERIALIZE(a, start_node);
  SERIALIZE(a, telnet_cmd);
  SERIALIZE(a, data_mode);
  SERIALIZE(a, working_directory);
}

template <class Archive>
void serialize(Archive & ar, wwivd_config_t &a) {
  SERIALIZE(a, telnet_port);
  SERIALIZE(a, ssh_port);
  SERIALIZE(a, binkp_port);
  SERIALIZE(a, binkp_cmd);
  SERIALIZE(a, do_network_callouts);
  SERIALIZE(a, network_callout_cmd);
  SERIALIZE(a, do_beginday_event);
  SERIALIZE(a, beginday_cmd);
  SERIALIZE(a, http_address);
  SERIALIZE(a, http_port);
  SERIALIZE(a, bbses);
  SERIALIZE(a, launch_minimized);
  SERIALIZE(a, blocking);
}

bool wwivd_config_t::Load(const Config & config) {
  JsonFile<wwivd_config_t> file(FilePath(config.datadir(), "wwivd.json"), "wwivd", *this);
  return file.Load();
}

bool wwivd_config_t::Save(const Config & config) {
  JsonFile<wwivd_config_t> file(FilePath(config.datadir(), "wwivd.json"), "wwivd", *this);
  return file.Save();
}

}
