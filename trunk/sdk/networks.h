/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.0x                             */
/*               Copyright (C)2014, WWIV Software Services                */
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

#include <memory>
#include <string>
#include <vector>
#include "sdk/config.h"
#include "sdk/net.h"
#include "sdk/vardec.h"

namespace wwiv {
namespace sdk {

class Networks {
public:
  explicit Networks(const Config& config);
  virtual ~Networks();

  bool IsInitialized() const { return initialized_; }
  const std::vector<net_networks_rec>& networks() const { return networks_; }
  const net_networks_rec& at(int num) const { return networks_.at(num); }
  const net_networks_rec& at(const std::string& name) const;
  net_networks_rec& at(int num) { return networks_.at(num); }
  net_networks_rec& at(const std::string& name);

  net_networks_rec& operator[](int num) { return at(num); }
  net_networks_rec& operator[](const std::string& name) { return at(name); }
  const net_networks_rec& operator[](int num) const { return at(num); }
  const net_networks_rec& operator[](const std::string& name) const { return at(name); }

private:
  bool initialized_;
  std::vector<net_networks_rec> networks_;
};

}
}

#endif  // __INCLUDED_SDK_NETWORKS_H__
