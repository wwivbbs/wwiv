/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2014-2021, WWIV Software Services             */
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
#ifndef INCLUDED_SDK_NETWORKS_H
#define INCLUDED_SDK_NETWORKS_H

#include <filesystem>
#include <initializer_list>
#include <string>
#include <vector>
#include "sdk/config.h"
#include "sdk/net/net.h"

namespace wwiv::sdk {

class Networks final {
public:
  typedef int size_type;
  typedef net::Network& reference;
  typedef const net::Network& const_reference;
  static const size_type npos = -1;
  explicit Networks(const Config& config);
  // [[ VisibleForTesting ]]
  explicit Networks(std::initializer_list<net::Network> l) : networks_(l) {}
  ~Networks();

  [[nodiscard]] bool IsInitialized() const { return initialized_; }
  [[nodiscard]] const std::vector<net::Network>& networks() const { return networks_; }
  [[nodiscard]] const net::Network& at(size_type num) const;
  [[nodiscard]] const net::Network& at(const std::string& name) const;
  net::Network& at(size_type num);
  net::Network& at(const std::string& name);
  std::optional<const net::Network> by_uuid(const wwiv::core::uuid_t& uuid);
  std::optional<const net::Network> by_uuid(const std::string& uuid_text);

  net::Network& operator[](size_type num) { return at(num); }
  net::Network& operator[](const std::string& name) { return at(name); }
  const net::Network& operator[](int num) const { return at(num); }
  const net::Network& operator[](const std::string& name) const { return at(name); }

  [[nodiscard]] size_type network_number(const std::string& network_name) const;
  [[nodiscard]] bool contains(const std::string& network_name) const;
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] bool empty() const noexcept;

  bool insert(int n, net::Network r);
  bool erase(int n);
  bool Load();
  bool Save();

private:
  void EnsureNetworksHaveUUID();
  void EnsureNetDirAbsolute();
  void SetNetworkNumbers();
  bool LoadFromJSON();
  bool LoadFromDat();
  bool SaveToJSON();
  bool SaveToDat();

  bool initialized_{false};
  std::filesystem::path root_directory_;
  std::filesystem::path datadir_;
  std::vector<net::Network> networks_;
};


}

#endif
