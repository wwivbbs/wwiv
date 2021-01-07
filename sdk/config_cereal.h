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

#ifndef INCLUDED_SDK_CONFIG_CEREAL_H
#define INCLUDED_SDK_CONFIG_CEREAL_H

#include "core/cereal_utils.h"
#include "sdk/config.h"

// ReSharper disable once CppUnusedIncludeDirective
#include <cereal/specialize.hpp>

using namespace wwiv::sdk;

namespace cereal {

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


template <class Archive> void serialize(Archive& ar, config_t& n) {
  SERIALIZE(n, header);
  SERIALIZE(n, systempw);
  SERIALIZE(n, msgsdir);
  SERIALIZE(n, gfilesdir);
  SERIALIZE(n, datadir);
  SERIALIZE(n, dloadsdir);
  SERIALIZE(n, scriptdir);
  SERIALIZE(n, logdir);
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
