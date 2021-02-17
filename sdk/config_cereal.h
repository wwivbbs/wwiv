/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
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

#ifndef INCLUDED_SDK_CONFIG_CEREAL_H
#define INCLUDED_SDK_CONFIG_CEREAL_H

#include "core/cereal_utils.h"
#include "sdk/config.h"

// ReSharper disable once CppUnusedIncludeDirective
#include <cereal/specialize.hpp>

// We want to override how we store network_type_t to store it as a string, not int.
// This has to be in the global namespace. 
CEREAL_SPECIALIZE_FOR_ALL_ARCHIVES(wwiv::sdk::newuser_item_type_t, cereal::specialization::non_member_load_save_minimal);

namespace cereal {

using namespace wwiv::sdk;

template <class Archive> void serialize(Archive& ar, config_header_t& n) {
  SERIALIZE(n, config_revision_number);
  SERIALIZE(n, last_written_date);
  SERIALIZE(n, written_by_wwiv_num_version);
  SERIALIZE(n, written_by_wwiv_version);
}
template <class Archive> void serialize(Archive& ar, slrec& n) {
  SERIALIZE(n, time_per_day);
  SERIALIZE(n, time_per_logon);
  SERIALIZE(n, messages_read);
  SERIALIZE(n, emails);
  SERIALIZE(n, posts);
  SERIALIZE(n, ability); // todo: change to booleans?
}

template <class Archive> void serialize(Archive& ar, valrec& n) {
  SERIALIZE(n, sl);
  SERIALIZE(n, dsl);
  SERIALIZE(n, ar);
  SERIALIZE(n, dar);
  SERIALIZE(n, restrict);
}

template <class Archive>
std::string save_minimal(Archive const &, const newuser_item_type_t& t) {
  return to_enum_string<newuser_item_type_t>(t, {"unused", "optional", "required"});
}
template <class Archive> inline
void load_minimal(Archive const &, newuser_item_type_t& t, const std::string& v) {
  t = from_enum_string<newuser_item_type_t>(v, {"unused", "optional", "required"});
}

template <class Archive> void serialize(Archive& ar, newuser_config_t& n) {
  SERIALIZE(n, use_real_name);
  SERIALIZE(n, use_voice_phone);
  SERIALIZE(n, use_data_phone);
  SERIALIZE(n, use_address_street);
  SERIALIZE(n, use_address_city_state);
  SERIALIZE(n, use_address_zipcode);
  SERIALIZE(n, use_address_country);
  SERIALIZE(n, use_callsign);
  SERIALIZE(n, use_gender);
  SERIALIZE(n, use_birthday);
  SERIALIZE(n, use_computer_type);
  SERIALIZE(n, use_email_address);
}

template <class Archive> void serialize(Archive& ar, config_t& n) {
  SERIALIZE(n, header);
  SERIALIZE(n, systempw);
  SERIALIZE(n, msgsdir);
  SERIALIZE(n, gfilesdir);
  SERIALIZE(n, datadir);
  SERIALIZE(n, dloadsdir);
  SERIALIZE(n, scriptdir);
  SERIALIZE(n, logdir);
  SERIALIZE(n, num_instances);
  SERIALIZE(n, tempdir_format);
  SERIALIZE(n, batchdir_format);
  SERIALIZE(n, scratchdir_format);
  SERIALIZE(n, systemname);
  SERIALIZE(n, systemphone);
  SERIALIZE(n, sysopname);
  SERIALIZE(n, menudir);

  SERIALIZE(n, closedsystem);

  SERIALIZE(n, newuserpw);
  SERIALIZE(n, newusersl);
  SERIALIZE(n, newuserdsl);
  SERIALIZE(n, newusergold);
  SERIALIZE(n, newuser_restrict);
  SERIALIZE(n, newuser_config);

  SERIALIZE(n, newuploads);
  SERIALIZE(n, sysconfig);
  SERIALIZE(n, sysoplowtime);
  SERIALIZE(n, sysophightime);
  SERIALIZE(n, wwiv_reg_number);
  SERIALIZE(n, post_call_ratio);
  SERIALIZE(n, req_ratio);

  // We don't want to serialize this here, this goes into data/sl.json
  // SERIALIZE(n, sl);
  // We don't want to serialize this here, this goes into data/autoval.json
  // SERIALIZE(n, autoval);

  SERIALIZE(n, userreclen);
  SERIALIZE(n, waitingoffset);
  SERIALIZE(n, inactoffset);
  SERIALIZE(n, sysstatusoffset);
  SERIALIZE(n, fsoffset);
  SERIALIZE(n, fuoffset);
  SERIALIZE(n, fnoffset);
  SERIALIZE(n, qscn_len);

  SERIALIZE(n, maxwaiting);
  SERIALIZE(n, maxusers);
  SERIALIZE(n, max_subs);
  SERIALIZE(n, max_dirs);
  SERIALIZE(n, max_backups);
  SERIALIZE(n, script_flags);
}


} // namespace cereal


#endif
