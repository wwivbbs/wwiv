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

private:
  bool initialized_;
  std::vector<net_networks_rec> networks_;
};

}
}

#endif  // __INCLUDED_SDK_NETWORKS_H__
