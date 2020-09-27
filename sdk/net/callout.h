/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2014-2020, WWIV Software Services             */
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
#ifndef INCLUDED_SDK_CALLOUT_H
#define INCLUDED_SDK_CALLOUT_H

#include "sdk/net/net.h"
#include <initializer_list>
#include <map>
#include <string>

namespace wwiv::sdk {
  
class Callout {
 public:
  Callout(const net_networks_rec& net, int max_backups);
  // VisibleForTesting
  Callout(std::initializer_list<net_call_out_rec> l);
  virtual ~Callout();
  [[nodiscard]] virtual const net_call_out_rec* net_call_out_for(int node) const;
  [[nodiscard]] virtual const net_call_out_rec* net_call_out_for(const std::string& node) const;
  Callout& operator=(const Callout& rhs) { node_config_ = rhs.node_config_; return *this; }

  bool insert(uint16_t node, const net_call_out_rec& c);
  bool erase(uint16_t node);
  virtual bool Save();

  [[nodiscard]] const std::map<uint16_t, net_call_out_rec>& callout_config() const { return node_config_; }
  [[nodiscard]] std::string ToString() const;

 private:
  net_networks_rec net_;
  const int max_backups_;
  std::map<uint16_t, net_call_out_rec> node_config_;
};

[[nodiscard]] bool ParseCalloutNetLine(const std::string& line, net_call_out_rec* config);
std::string WriteCalloutNetLine(const net_call_out_rec& con);
std::string CalloutOptionsToString(uint16_t options);

}  // namespace wwiv::net

#endif  // INCLUDED_SDK_CALLOUT_H
