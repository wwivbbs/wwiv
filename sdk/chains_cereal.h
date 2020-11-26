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
/*                                                                        */
/**************************************************************************/

#ifndef INCLUDED_SDK_CHAINS_CEREAL_H
#define INCLUDED_SDK_CHAINS_CEREAL_H

#include "core/cereal_utils.h"
#include <cereal/specialize.hpp>
#include "sdk/chains.h"

using namespace wwiv::sdk;

// We want to override how we store some enums as a string, not int.
// This has to be in the global namespace.
CEREAL_SPECIALIZE_FOR_ALL_ARCHIVES(chain_exec_mode_t, specialization::non_member_load_save_minimal);
CEREAL_SPECIALIZE_FOR_ALL_ARCHIVES(chain_exec_dir_t, specialization::non_member_load_save_minimal);

namespace cereal {

template <class Archive>
std::string save_minimal(Archive const&, const chain_exec_mode_t& t) {
  return to_enum_string<chain_exec_mode_t>(t, {"none", "DOS", "FOSSIL", "STDIO", "NETFOSS"});
}
template <class Archive>
void load_minimal(Archive const&, chain_exec_mode_t& t, const std::string& s) {
  t = from_enum_string<chain_exec_mode_t>(s, {"none", "DOS", "FOSSIL", "STDIO", "NETFOSS"});
}

template <class Archive>
std::string save_minimal(Archive const&, const chain_exec_dir_t& t) {
  return to_enum_string<const chain_exec_dir_t>(t, {"bbs", "temp"});
}
template <class Archive>
void load_minimal(Archive const&, chain_exec_dir_t& t, const std::string& v) {
  t = from_enum_string<const chain_exec_dir_t>(v, {"bbs", "temp"});
}

template <class Archive> void serialize(Archive& ar, chain_55_t& n) {
  SERIALIZE(n, filename);
  SERIALIZE(n, description);
  SERIALIZE(n, exec_mode);
  SERIALIZE(n, dir);
  SERIALIZE(n, ansi);
  SERIALIZE(n, local_only);
  SERIALIZE(n, multi_user);
  SERIALIZE(n, sl);
  SERIALIZE(n, ar);
  SERIALIZE(n, regby);
  SERIALIZE(n, usage);
  SERIALIZE(n, minage);
  SERIALIZE(n, maxage);
}

template <class Archive> void serialize(Archive& ar, chain_t& n) {
  SERIALIZE(n, filename);
  SERIALIZE(n, description);
  SERIALIZE(n, exec_mode);
  SERIALIZE(n, dir);
  SERIALIZE(n, ansi);
  SERIALIZE(n, local_only);
  SERIALIZE(n, multi_user);
  SERIALIZE(n, acs);
  SERIALIZE(n, regby);
  SERIALIZE(n, usage);
  SERIALIZE(n, local_console_cp437);
}

} // namespace cereal


#endif
