/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*               Copyright (C)2015, WWIV Software Services                */
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
#ifndef __INCLUDED_SDK_MSGAPI_MESSAGE_API_WWIV_H__
#define __INCLUDED_SDK_MSGAPI_MESSAGE_API_WWIV_H__

#include "sdk/msgapi/message_api.h"
#include "sdk/msgapi/message_area_wwiv.h"

#include <memory>
#include <string>

namespace wwiv {
namespace sdk {
namespace msgapi {

class WWIVMessageArea;

class WWIVMessageApi: public MessageApi {
public:
  WWIVMessageApi(const std::string& subs_directory,
    const std::string& messages_directory,
    const std::vector<net_networks_rec>& net_networks);
  virtual ~WWIVMessageApi();
  virtual bool Exist(const std::string& name) const override;
  virtual WWIVMessageArea* Create(const std::string& name) override;
  virtual bool Remove(const std::string& name) override;
  virtual WWIVMessageArea* Open(const std::string& name) override;
};

}  // namespace msgapi
}  // namespace sdk
}  // namespace wwiv

#endif  // __INCLUDED_SDK_MSGAPI_MESSAGE_API_WWIV_H__
