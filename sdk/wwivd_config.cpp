/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5                            */
/*             Copyright (C)1998-2021, WWIV Software Services             */
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

#include <iostream>
#include <map>
#include <string>
#include <cereal/cereal.hpp>
#include <cereal/access.hpp>
#include "core/file.h"
#include "core/jsonfile.h"
#include "core/cereal_utils.h"
#include "sdk/config.h"

using std::map;
using std::string;
using namespace wwiv::core;

#ifdef DELETE
#undef DELETE
#endif  // DELETE

namespace wwiv::sdk {

template <class Archive>
void serialize(Archive& ar, wwivd_blocking_t &b) {
  SERIALIZE(b, mailer_mode);
  SERIALIZE(b,use_badip_txt);
  SERIALIZE(b, use_goodip_txt);
  SERIALIZE(b, max_concurrent_sessions);

  SERIALIZE(b, auto_blocklist);
  try {
    // TODO(rushfan): Get rid of this later in 5.5.  This will read in the old name
    // and next time wwivconfig saves this out, it'll write out both the new
    // and old names. In 5.6 we'll get rid of this and only use the new name.
    // This code will no longer be needed after 5.5 final release.
    ar(cereal::make_nvp("auto_blacklist", b.auto_blocklist));
  } catch (const cereal::Exception&) {
    ar.setNextName(nullptr);
  }

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
  ar(cereal::make_nvp("description", a.description));
  ar(cereal::make_nvp("end_node", a.end_node));
  ar(cereal::make_nvp("key", a.key));
  ar(cereal::make_nvp("local_node", a.local_node));
  ar(cereal::make_nvp("name", a.name));
  ar(cereal::make_nvp("require_ansi", a.require_ansi));
  ar(cereal::make_nvp("ssh_cmd", a.ssh_cmd));
  ar(cereal::make_nvp("start_node", a.start_node));
  ar(cereal::make_nvp("telnet_cmd", a.telnet_cmd));
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
