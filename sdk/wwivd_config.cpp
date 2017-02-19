/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5                            */
/*             Copyright (C)1998-2016, WWIV Software Services             */
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

#include <cctype>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <cereal/cereal.hpp>
#include <cereal/access.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/archives/json.hpp>

#include "core/file.h"
#include "core/jsonfile.h"
#include "sdk/config.h"

using std::map;
using std::string;
using namespace wwiv::core;

#ifdef DELETE
#undef DELETE
#endif  // DELETE

#define SERIALIZE(field) { try { ar(cereal::make_nvp(#field, field)); } catch(const cereal::Exception&) { ar.setNextName(nullptr); } }

namespace wwiv {
namespace sdk {

template <class Archive>
void serialize(Archive& ar, wwivd_blocking_t &a) {
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
}

template <class Archive>
void serialize(Archive & ar, wwivd_config_t &a) {
  ar(cereal::make_nvp("telnet_port", a.telnet_port));
  ar(cereal::make_nvp("ssh_port", a.ssh_port));
  ar(cereal::make_nvp("binkp_port", a.binkp_port));

  ar(cereal::make_nvp("binkp_cmd", a.binkp_cmd));


  ar(cereal::make_nvp("http_address", a.http_address));
  ar(cereal::make_nvp("http_port", a.http_port));

  ar(cereal::make_nvp("bbses", a.bbses));
}

bool wwivd_config_t::Load(const Config & config) {
  JsonFile<wwivd_config_t> file(config.datadir(), "wwivd.json", "wwivd", *this);
  if (!file.Load()) {
    binkp_port = -1;
    telnet_port = 2323;
    http_port = 8080;
    http_address = "127.0.0.1";
    binkp_cmd = "./networkb --receive --handle=@H";

    wwivd_matrix_entry_t e{};
    e.key = 'W';
    e.description = "WWIV";
    e.name = "WWIV";
    e.local_node = 1;
    e.require_ansi = false;
    e.start_node = 2;
    e.end_node = 4;
    e.telnet_cmd = "./bbs -XT -H@H -N@N";
    e.ssh_cmd = "./bbs -XS -H@H -N@N";

    bbses.push_back(e);
    return file.Save();
  }
  return true;
}

bool wwivd_config_t::Save(const Config & config) {
  JsonFile<wwivd_config_t> file(config.datadir(), "wwivd.json", "wwivd", *this);
  return file.Save();
}

}
}
