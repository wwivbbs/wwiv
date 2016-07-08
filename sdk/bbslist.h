/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2014-2016 WWIV Software Services              */
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
#ifndef __INCLUDED_SDK_BBSLIST_H__
#define __INCLUDED_SDK_BBSLIST_H__

#include <initializer_list>
#include <map>
#include <string>

#include "sdk/net.h"

namespace wwiv {
namespace sdk {

  
class BbsListNet {
 public:
   static BbsListNet ParseBbsListNet(uint16_t net_node_number, const std::string& network_dir);
   static BbsListNet ReadBbsDataNet(const std::string& network_dir);
  // VisibleForTesting
  BbsListNet(std::initializer_list<net_system_list_rec> l);
  virtual ~BbsListNet();
  const net_system_list_rec* node_config_for(int node) const;
  BbsListNet& operator=(const BbsListNet& rhs) { node_config_ = rhs.node_config_; return *this; }
  std::string ToString() const;

  bool empty() const { return node_config_.empty(); }
  const std::map<uint16_t, net_system_list_rec>& node_config() const { return node_config_; }

 private:
   BbsListNet();
   std::map<uint16_t, net_system_list_rec> node_config_;
};

bool ParseBbsListNetLine(const std::string& line, net_system_list_rec* config, int32_t* reg_number);

}  // namespace net
}  // namespace wwiv

#endif  // __INCLUDED_SDK_BBSLIST_H__
