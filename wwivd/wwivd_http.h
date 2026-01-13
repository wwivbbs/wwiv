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
#ifndef INCLUDED_WWIVD_WWIVD_HTTP_H
#define INCLUDED_WWIVD_WWIVD_HTTP_H

#include "wwivd/connection_data.h"

namespace httplib {
  struct Response;
  struct Request;
  class Server;
} // namespace httplib


namespace wwiv::wwivd {

void StatusHandler(std::map<const std::string, std::shared_ptr<NodeManager>>* nodes,
                   const httplib::Request&, httplib::Response& res);

void BlockingHandler(ConnectionData* data,
                     const httplib::Request&, httplib::Response& res);

void SysopHandler(ConnectionData* data,
                  const httplib::Request&, httplib::Response& res);

} // namespace wwiv::wwivd

#endif
