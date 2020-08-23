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
#ifndef __INCLUDED_SDK_NETWORKS_H__
#define __INCLUDED_SDK_NETWORKS_H__

#include <initializer_list>
#include <string>
#include <vector>
#include "sdk/config.h"
#include "sdk/net/net.h"

namespace wwiv {
namespace sdk {

class Networks final {
public:
  typedef int size_type;
  static const size_type npos = -1;
  explicit Networks(const Config& config);
  // [[ VisibleForTesting ]]
  explicit Networks(std::initializer_list<net_networks_rec> l) : networks_(l) {}
  ~Networks();

  [[nodiscard]] bool IsInitialized() const { return initialized_; }
  [[nodiscard]] const std::vector<net_networks_rec>& networks() const { return networks_; }
  [[nodiscard]] const net_networks_rec& at(size_type num) const { return networks_.at(num); }
  [[nodiscard]] const net_networks_rec& at(const std::string& name) const;
  net_networks_rec& at(size_type num);
  net_networks_rec& at(const std::string& name);
  std::optional<const net_networks_rec> by_uuid(const wwiv::core::uuid_t& uuid);
  std::optional<const net_networks_rec> by_uuid(const std::string& uuid_text);

  net_networks_rec& operator[](size_type num) { return at(num); }
  net_networks_rec& operator[](const std::string& name) { return at(name); }
  const net_networks_rec& operator[](int num) const { return at(num); }
  const net_networks_rec& operator[](const std::string& name) const { return at(name); }

  [[nodiscard]] size_type network_number(const std::string& network_name) const;
  [[nodiscard]] bool contains(const std::string& network_name) const;
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] bool empty() const noexcept;

  bool insert(int n, net_networks_rec r);
  bool erase(int n);
  bool Load();
  bool Save();

private:
  void EnsureNetworksHaveUUID();
  bool LoadFromJSON();
  bool LoadFromDat();
  bool SaveToJSON();
  bool SaveToDat();

  bool initialized_{false};
  std::string datadir_;
  std::vector<net_networks_rec> networks_;
};

}
}

#endif  // __INCLUDED_SDK_NETWORKS_H__
