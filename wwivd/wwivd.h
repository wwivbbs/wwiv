/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                Copyright (C)2017-2020, WWIV Software Services          */
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
#ifndef INCLUDED_WWIV_WWIV_WWIVD_H
#define INCLUDED_WWIV_WWIV_WWIVD_H

#include <string>
#include <core/net.h>
#include "sdk/config.h"

namespace wwiv::sdk {
class wwivd_config_t;
}

void BeforeStartServer();
void signal_handler(int mysignal);
void SwitchToNonRootUser(const std::string& wwiv_user);

/**
 * Executes a command and waits. 
 * If sock is > -1 then we'll close the socket after executing the command 
 * since this is the child socket.
 * pid and node_number is just used for logging.
 */
bool ExecCommandAndWait(const wwiv::sdk::wwivd_config_t& wc, const std::string& cmd,
                        const std::string& pid, int node_number, SOCKET sock);

#endif
