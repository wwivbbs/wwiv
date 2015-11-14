/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.0x                             */
/*                Copyright (C)2015 WWIV Software Services                */
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
#ifndef __INCLUDED_NETORKB_NET_LOG_H__
#define __INCLUDED_NETORKB_NET_LOG_H__

#include <chrono>
#include <cstdint>
#include <ctime>
#include <initializer_list>
#include <map>
#include <string>

#include "sdk/config.h"
#include "sdk/net.h"

namespace wwiv {
namespace net {

/**
 * Handles writing new log entries into NET.LOG.

 * Format of gfiles/NET.LOG:
 * 
 * 01/03/15 20:26:23 To 32767,                             0.1 min  wwivnet
 * 01/03/15 20:26:23 To     1, S : 4k, R : 3k,             0.1 min  wwivnet
 */

enum NetworkSide { FROM, TO };

class NetworkLog {
 public:
  explicit NetworkLog(const std::string& gfiles_directory);
  virtual ~NetworkLog();

  bool Log(
    time_t time, NetworkSide side, int16_t node,
    unsigned int bytes_sent, unsigned int bytes_received,
    std::chrono::seconds seconds_elapsed, const std::string& network_name);
  std::string GetContents() const;

  std::string ToString() const;

 private:
   std::string CreateLogLine(
     time_t time, NetworkSide side, int16_t node,
     unsigned int bytes_sent, unsigned int bytes_received,
     std::chrono::seconds seconds_elapsed, const std::string& network_name);
   
   std::string gfiles_directory_;
};

}  // namespace net
}  // namespace wwiv

#endif  // __INCLUDED_NETORKB_NET_LOG_H__
