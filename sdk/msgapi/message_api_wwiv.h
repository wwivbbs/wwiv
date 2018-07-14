/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2017, WWIV Software Services             */
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

#include "sdk/config.h"
#include "sdk/msgapi/email_wwiv.h"
#include "sdk/msgapi/message_api.h"
#include "sdk/msgapi/message_area_wwiv.h"
#include "sdk/net.h"

#include <memory>
#include <string>
#include <vector>

namespace wwiv {
namespace sdk {
namespace msgapi {

class WWIVMessageArea;

// Can't merge with MessageAreaLastRead since that knows the area
// Yet this one knows the user since it's used in the context
// of a user session, otherwise we ignore the last_read values
// when used from tools.
class WWIVLastReadImpl {
public:
  virtual uint32_t last_read(int area) const = 0;
  virtual void set_last_read(int area, uint32_t last_read) = 0;
  virtual void Load() = 0;
  virtual void Save() = 0;
};

class NullLastReadImpl : public WWIVLastReadImpl {
  uint32_t last_read(int) const override { return 0; }
  void set_last_read(int, uint32_t) override {}
  void Load() override {}
  void Save() override {}
};

class WWIVMessageApi : public MessageApi {
public:
  WWIVMessageApi(const wwiv::sdk::msgapi::MessageApiOptions& options,
                 const wwiv::sdk::Config& config, const std::vector<net_networks_rec>& net_networks,
                 WWIVLastReadImpl* last_read);

  virtual bool Exist(const wwiv::sdk::subboard_t& sub) const override;
  virtual bool Create(const std::string& name, const std::string& sub_ext,
                      const std::string& text_ext, int subnum);
  virtual bool Create(const wwiv::sdk::subboard_t& sub, int subnum) override;
  virtual bool Remove(const std::string& name) override;
  virtual MessageArea* Open(const wwiv::sdk::subboard_t& sub, int subnum) override;
  virtual WWIVEmail* OpenEmail();
  uint32_t last_read(int area) const;
  void set_last_read(int area, uint32_t last_read);
  const Config& config() const noexcept { return config_; }

private:
  std::unique_ptr<WWIVLastReadImpl> last_read_;
  const Config config_;
};

} // namespace msgapi
} // namespace sdk
} // namespace wwiv

#endif // __INCLUDED_SDK_MSGAPI_MESSAGE_API_WWIV_H__
