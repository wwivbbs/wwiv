/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2016, WWIV Software Services             */
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
#pragma once
#ifndef __INCLUDED_SDK_BINKP_H__
#define __INCLUDED_SDK_BINKP_H__

#include <cstdint>
#include <exception>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>

#include "core/inifile.h"
#include "networkb/config_exceptions.h"
#include "sdk/callout.h"
#include "sdk/networks.h"

namespace wwiv {
namespace sdk {

struct BinkNodeConfig {
  std::string host;
  int port;
};

class Binkp {
 public:
  Binkp(const std::string& network_dir);
  virtual ~Binkp();
  const BinkNodeConfig* node_config_for(int node) const;

 private:
  std::map<uint16_t, BinkNodeConfig> node_config_;
  std::string network_dir_;
};

bool ParseBinkConfigLine(const std::string& line,
			 uint16_t* node,
			 BinkNodeConfig* config);

}  // namespace sdk
}  // namespace wwiv


#endif  // __INCLUDED_SDK_BINKP_H__
