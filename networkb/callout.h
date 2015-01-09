/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.0x                             */
/*             Copyright (C)2014-2015 WWIV Software Services              */
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
#ifndef __INCLUDED_NETORKB_CALLOUT_H__
#define __INCLUDED_NETORKB_CALLOUT_H__

#include <initializer_list>
#include <map>
#include <string>

#include "sdk/net.h"

namespace wwiv {
namespace net {

  
class Callout {
 public:
  explicit Callout(const std::string& network_dir);
  // VisibleForTesting
  Callout(std::initializer_list<net_call_out_rec> l);
  virtual ~Callout();
  const net_call_out_rec* node_config_for(int node) const;
  Callout& operator=(const Callout& rhs) { node_config_ = rhs.node_config_; return *this; }

 private:
  std::map<uint16_t, net_call_out_rec> node_config_;
};

bool ParseCalloutNetLine(const std::string& line, net_call_out_rec* config);

}  // namespace net
}  // namespace wwiv

#endif  // __INCLUDED_NETORKB_CALLOUT_H__
