/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                Copyright (C)2017-2022, WWIV Software Services          */
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

#include <filesystem>
#include <string>
#include <tuple>
#include "core/net.h"
#include "sdk/config.h"

namespace wwiv::sdk {
class wwivd_config_t;
}

namespace wwiv::wwivd {
class NodeManager;
class Sessions;
}

void BeforeStartServer();
void signal_handler(int mysignal);
void SwitchToNonRootUser(const std::string& wwiv_user);

/**
 * Executes a command and waits for it to exit.
 *
 * Takes ownership of sock (which may be empty). When it is valid, it is the
 * only socket the child inherits, and this process's copy is closed as soon
 * as the child has been started, so the connection drops when the child
 * closes it. pid and node_number are only used for logging.
 *
 * The child starts in working_dir, or in wwivd's own directory (the BBS
 * root) when it is empty. wwivd's current directory is never changed, since
 * other threads are launching children at the same time.
 *
 * When sessions is given, the child is registered with it while it runs,
 * so that shutdown can hang it up. On Unix the child gets its own process
 * group, so that reaches whatever it starts too.
 */
bool ExecCommandAndWait(const wwiv::sdk::wwivd_config_t& wc, wwiv::wwivd::NodeManager& node_manager,
                        const std::string& cmd, const std::string& pid, int node_number,
                        wwiv::core::SocketHandle sock,
                        const std::filesystem::path& working_dir = {},
                        wwiv::wwivd::Sessions* sessions = nullptr);

#endif
