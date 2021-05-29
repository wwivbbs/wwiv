/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                Copyright (C)2017-2021, WWIV Software Services          */
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
#ifndef INCLUDED_SDK_WWIVD_CONFIG_H
#define INCLUDED_SDK_WWIVD_CONFIG_H

#include "sdk/config.h"
#include <string>
#include <vector>

namespace wwiv::sdk {

struct wwivd_blocking_t {
  bool mailer_mode = false;

  bool use_badip_txt = true;
  bool use_goodip_txt = true;
  int max_concurrent_sessions = 1;
  bool auto_blocklist = true;
  int auto_bl_sessions = 3;
  int auto_bl_seconds = 30;

  bool use_dns_rbl = true;
  // xbl.spamhaus.org
  std::string dns_rbl_server{"xbl.spamhaus.org"};

  bool use_dns_cc = true;
  // zz.countries.nerd.dk
  std::string dns_cc_server{"zz.countries.nerd.dk"};
  std::vector<int> block_cc_countries;
  std::vector<std::string> block_duration;
};

enum class wwivd_data_mode_t { socket, pipe };

struct wwivd_matrix_entry_t {
  /** Key to use when displaying this BBS in the matrix logon. */
  char key;
  /**
   * Used for semaphore names and also short-hand way to call the bbs.
   * It should be not more than 8-12 characters in length.
   */
  std::string name;
  /** Description to use to display this BBS in the matrix logon. */
  std::string description;
  /** Command to launch this BBS over Telnet */
  std::string telnet_cmd;
  /** Command to launch this BBS over SSH */
  std::string ssh_cmd;
  /** Working directory to use when launching the BBS */
  std::string working_directory;
  /** Does using this BBS require ANSI? */
  bool require_ansi{false};
  /** Start node for this bbs */
  int start_node;
  /** End node for this bbs */
  int end_node;
  /** Local node for this bbs */
  int local_node;
  /** Mode for passing data to the BBS: Socket or Named Pipe */
  wwivd_data_mode_t data_mode{wwivd_data_mode_t::socket};
};

class wwivd_config_t {
public:
  /** Configuration for non-bbs invocations. */

  int binkp_port{-1};
  std::string binkp_cmd;
  bool do_network_callouts{false};
  std::string network_callout_cmd;
  bool do_beginday_event{true};
  std::string beginday_cmd;

  int http_port{-1};
  std::string http_address;

  /** Blocking configuration */
  wwivd_blocking_t blocking;

  /**
   * Matrix Logon Settings
   */

  int telnet_port = 
#ifdef _WIN32
    23;
#else
    2323;
#endif
  int ssh_port{-1};
  /** Filename (under WWIV/GFILES) to display before showing the shuttle logon menu */
  std::string matrix_filename;
  /** Vector of BBS'es to display in the matrix logon. */
  std::vector<wwivd_matrix_entry_t> bbses;
  /** Should the network and bbs connections be launched minimized */
  bool launch_minimized{false};

  bool Load(const Config& config);
  bool Save(const Config& config);
};

} // namespace

#endif
