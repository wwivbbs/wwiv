/**************************************************************************/
/*                                                                        */
/*                          WWIV BBS Software                             */
/*             Copyright (C)2017-2022, WWIV Software Services             */
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

#include <string>
#include <vector>

#include <cereal/archives/json.hpp>
#include <cereal/cereal.hpp>
#include <cereal/types/memory.hpp>
// ReSharper disable once CppUnusedIncludeDirective
#include <cereal/types/string.hpp>
// ReSharper disable once CppUnusedIncludeDirective
#include <cereal/types/vector.hpp>

#include "core/http_server.h"
#include "core/jsonfile.h"
#include "core/log.h"
#include "core/net.h"
#include "core/os.h"
#include "core/socket_connection.h"
#include "core/stl.h"
#include "core/strings.h"
#include "sdk/config.h"
#include "wwivd/connection_data.h"
#include "wwivd/node_manager.h"

#include <string>

namespace wwiv::wwivd {

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;
using namespace wwiv::os;

struct status_reponse_t {
  int num_instances;
  int used_instances;
  std::vector<std::string> lines;

  template <class Archive> void serialize(Archive& ar) {
    ar(cereal::make_nvp("num_instances", num_instances),
      cereal::make_nvp("used_instances", used_instances), cereal::make_nvp("lines", lines));
  }
};

std::string ToJson(status_reponse_t r) {
  std::ostringstream ss;
  try {
    cereal::JSONOutputArchive save(ss);
    save(cereal::make_nvp("status", r));
  }
  catch (const cereal::RapidJSONException& e) {
    LOG(ERROR) << e.what();
  }
  return ss.str();
}

class StatusHandler : public HttpHandler {
public:
  StatusHandler(std::map<const std::string, std::shared_ptr<NodeManager>>* nodes) : nodes_(nodes) {}

  HttpResponse Handle(HttpMethod, const std::string&, std::vector<std::string> headers) override {
    // We only handle status
    HttpResponse response(200);
    response.headers.emplace("Content-Type: ", "text/json");

    status_reponse_t r{};
    for (const auto& n : *nodes_) {
      const auto v = n.second->status_lines();
      r.num_instances += n.second->total_nodes();
      r.used_instances += n.second->nodes_used();
      for (const auto& l : v) {
        r.lines.push_back(l);
      }
    }
    response.text = ToJson(r);
    return response;
  }

private:
  std::map<const std::string, std::shared_ptr<NodeManager>>* nodes_;
};

void HandleHttpConnection(ConnectionData data, accepted_socket_t r) {
  const auto sock = r.client_socket;
  const auto& b = data.c->blocking;

  try {
    std::string remote_peer;
    if (GetRemotePeerAddress(sock, remote_peer)) {
      const auto cc = get_dns_cc(remote_peer, b.dns_cc_server);
      LOG(INFO) << "Accepted HTTP connection on port: " << r.port << "; from: " << remote_peer
        << "; country code: " << cc;
    }

    // HTTP Request
    HttpServer h(std::make_unique<SocketConnection>(r.client_socket));
    StatusHandler status(data.nodes);
    h.add(HttpMethod::GET, "/status", &status);
    h.Run();

  }
  catch (const std::exception& e) {
    LOG(ERROR) << "HandleHttpConnection: Handled Uncaught Exception: " << e.what();
  }
  VLOG(1) << "Exiting HandleHttpConnection (exception)";
}


}
