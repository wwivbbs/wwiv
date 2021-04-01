/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2014-2021, WWIV Software Services             */
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
#ifndef __INCLUDED_SDK_NETWORKS_CEREAL_H__
#define __INCLUDED_SDK_NETWORKS_CEREAL_H__

// We want to override how we store network_type_t to store it as a string, not int.
// This has to be in the global namespace. 
CEREAL_SPECIALIZE_FOR_ALL_ARCHIVES(std::filesystem::path, cereal::specialization::non_member_load_save_minimal);
CEREAL_SPECIALIZE_FOR_ALL_ARCHIVES(network_type_t, cereal::specialization::non_member_load_save_minimal);
CEREAL_SPECIALIZE_FOR_ALL_ARCHIVES(fido_packet_t, cereal::specialization::non_member_load_save_minimal);
CEREAL_SPECIALIZE_FOR_ALL_ARCHIVES(fido_bundle_status_t, cereal::specialization::non_member_load_save_minimal);

#include "core/cereal_utils.h"
#include "sdk/net/net.h"
#include "sdk/uuid_cereal.h"
#include <filesystem>
#include <string>

// ICK. See https://github.com/USCiLab/cereal/issues/562
namespace std::filesystem {

template<class Archive>
void CEREAL_LOAD_MINIMAL_FUNCTION_NAME(
    const Archive&, path& out, const std::string& in)
{
  out = in;
}

template<class Archive>
std::string CEREAL_SAVE_MINIMAL_FUNCTION_NAME(const Archive&, const path& p)
{
  return p.string();
}


}
namespace cereal {

template <class Archive>
std::string save_minimal(Archive const &, const network_type_t& t) {
  return to_enum_string<network_type_t>(t, {"wwivnet", "fido", "internet"});
}
template <class Archive>
void load_minimal(Archive const &, network_type_t& t, const std::string& v) {
  t = from_enum_string<network_type_t>(v, {"wwivnet", "fido", "internet"});
}

template <class Archive> inline
std::string save_minimal(Archive const &, const fido_bundle_status_t& t) {
  const auto c = static_cast<char>(t);
  return std::string(1, c);
}
template <class Archive>
void load_minimal(Archive const &, fido_bundle_status_t& t, const std::string& v) {
  auto c = 'f';
  if (!v.empty()) {
    c = v.front();
  }
  t = static_cast<fido_bundle_status_t>(c);
}

template <class Archive>
std::string save_minimal(Archive const &, const fido_packet_t& t) {
  return to_enum_string<fido_packet_t>(t, {"unset", "type2+"});
}
template <class Archive> inline
void load_minimal(Archive const &, fido_packet_t& t, const std::string& v) {
  t = from_enum_string<fido_packet_t>(v, {"unset", "type2+"});
}

template <class Archive> inline
std::string save_minimal(Archive const &, const fido_transport_t& t) {
  return to_enum_string<fido_transport_t>(t, {"unset", "directory", "binkp"});
}
template <class Archive> inline
void load_minimal(Archive const &, fido_transport_t& t, const std::string& v) {
  t = from_enum_string<fido_transport_t>(v, {"unset", "directory", "binkp"});
}

template <class Archive> inline
std::string save_minimal(Archive const &, const fido_mailer_t& t) {
  return to_enum_string<fido_mailer_t>(t, {"unset", "flo", "attach"});
}
template <class Archive> inline
void load_minimal(Archive const &, fido_mailer_t& t, const std::string& v) {
  t = from_enum_string<fido_mailer_t>(v, {"unset", "flo", "attach"});
}

template <class Archive>
void serialize(Archive & ar, fido_packet_config_t& n) {
  SERIALIZE(n, packet_type);
  SERIALIZE(n, compression_type);
  SERIALIZE(n, packet_password);
  SERIALIZE(n, tic_password);
  SERIALIZE(n, areafix_password);
  SERIALIZE(n, max_archive_size);
  SERIALIZE(n, max_packet_size);
  SERIALIZE(n, netmail_status);
}

// fido_node_config_t
template <class Archive>
void serialize(Archive & ar, fido_node_config_t& n) {
  SERIALIZE(n, routes);
  SERIALIZE(n, packet_config);
  SERIALIZE(n, binkp_config);
  SERIALIZE(n, callout_config);
}

// binkp_session_config_t
template <class Archive>
void serialize(Archive & ar, binkp_session_config_t& n) {
  SERIALIZE(n, host);
  SERIALIZE(n, port);
  SERIALIZE(n, password);
}

// network_callout_config_t
template <class Archive> void serialize(Archive& ar, network_callout_config_t& n) {
  SERIALIZE(n, auto_callouts);
  SERIALIZE(n, call_every_x_minutes);
  SERIALIZE(n, min_k);
}

template <class Archive>
void load(Archive & ar, fido_network_config_t& n) {
  SERIALIZE(n, fido_address);
  SERIALIZE(n, nodelist_base);
  SERIALIZE(n, mailer_type);
  SERIALIZE(n, transport);
  SERIALIZE(n, inbound_dir);
  SERIALIZE(n, temp_inbound_dir);
  SERIALIZE(n, temp_outbound_dir);
  SERIALIZE(n, outbound_dir);
  SERIALIZE(n, netmail_dir);
  SERIALIZE(n, bad_packets_dir);
  SERIALIZE(n, tic_dir);
  SERIALIZE(n, unknown_dir);
  SERIALIZE(n, origin_line);
  SERIALIZE(n, packet_config);
  SERIALIZE(n, process_tic);
  SERIALIZE(n, wwiv_heart_color_codes);
  SERIALIZE(n, wwiv_pipe_color_codes);
  SERIALIZE(n, allow_any_pipe_codes);
}

template <class Archive>
void save(Archive & ar, const fido_network_config_t& n) {
  SERIALIZE(n, fido_address);
  SERIALIZE(n, nodelist_base);
  SERIALIZE(n, mailer_type);
  SERIALIZE(n, transport);
  SERIALIZE(n, inbound_dir);
  SERIALIZE(n, temp_inbound_dir);
  SERIALIZE(n, temp_outbound_dir);
  SERIALIZE(n, outbound_dir);
  SERIALIZE(n, netmail_dir);
  SERIALIZE(n, bad_packets_dir);
  SERIALIZE(n, tic_dir);
  SERIALIZE(n, unknown_dir);
  SERIALIZE(n, origin_line);
  //SERIALIZE(n, packet_config);
  SERIALIZE(n, process_tic);
  SERIALIZE(n, wwiv_heart_color_codes);
  SERIALIZE(n, wwiv_pipe_color_codes);
  SERIALIZE(n, allow_any_pipe_codes);
}

// This has to be in the cereal or default to match Network which
// is in the default namespace.
template <class Archive>
void serialize(Archive & ar, Network& n) {
  SERIALIZE(n, type);
  SERIALIZE(n, name);
  SERIALIZE(n, dir);
  SERIALIZE(n, sysnum);
  // Serialize the Fido config
  if (n.type == network_type_t::ftn) {
    SERIALIZE(n, fido);
  }
  SERIALIZE(n, uuid);
}

}  // namespace cereal



#endif  // __INCLUDED_SDK_NETWORKS_CEREAL_H__
