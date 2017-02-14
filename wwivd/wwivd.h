/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                  Copyright (C)2017, WWIV Software Services             */
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
#ifndef __INCLUDED_WWIV_WWIV_WWIVD_H__
#define __INCLUDED_WWIV_WWIV_WWIVD_H__

#include <string>
#include "core/net.h"
#include "sdk/config.h"

struct wwivd_config_t {
  std::string bbsdir;

  int telnet_port = 2323;
  std::string telnet_cmd;

  int ssh_port = -1;
  std::string ssh_cmd;

  int binkp_port = -1;
  std::string binkp_cmd;

  int http_port = -1;
  std::string http_address;

  int start_node;
  int end_node;
  int local_node;
};

enum class ConnectionType { UNKNOWN, SSH, TELNET, BINKP, HTTP };

void BeforeStartServer();

void SwitchToNonRootUser(const std::string& wwiv_user);
bool ExecCommandAndWait(const std::string& cmd, const std::string& pid, int node_number, SOCKET sock);


#endif  // __INCLUDED_WWIV_WWIV_WWIVD_H__
