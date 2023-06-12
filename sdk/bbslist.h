/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2014-2022, WWIV Software Services             */
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
#ifndef INCLUDED_SDK_BBSLIST_H
#define INCLUDED_SDK_BBSLIST_H

#include <filesystem>
#include <initializer_list>
#include <map>
#include <optional>
#include <string>

struct net_system_list_rec;

namespace wwiv::sdk {
  
class BbsListNet {
 public:
   static BbsListNet ParseBbsListNet(uint16_t net_node_number, const std::filesystem::path& network_dir);
   static BbsListNet ReadBbsDataNet(const std::filesystem::path& network_dir);
  // VisibleForTesting
  BbsListNet(std::initializer_list<net_system_list_rec> l);
  BbsListNet(const wwiv::sdk::BbsListNet&) = default;
  virtual ~BbsListNet();
  [[nodiscard]] std::optional<net_system_list_rec> node_config_for(int node) const;
  BbsListNet& operator=(const BbsListNet& rhs) { node_config_ = rhs.node_config_; return *this; }
  [[nodiscard]] std::string ToString() const;

  [[nodiscard]] bool empty() const { return node_config_.empty(); }
  [[nodiscard]] const std::map<uint16_t, net_system_list_rec>& node_config() const { return node_config_; }
  [[nodiscard]] const std::map<uint16_t, int32_t>& reg_number() const { return reg_number_; }

 private:
   BbsListNet();
   std::map<uint16_t, net_system_list_rec> node_config_;
   std::map<uint16_t, int32_t> reg_number_;
};

bool ParseBbsListNetLine(const std::string& line, net_system_list_rec* config, int32_t* reg_number);

}  // namespace

#endif
