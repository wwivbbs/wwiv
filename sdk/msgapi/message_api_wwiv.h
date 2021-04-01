/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2021, WWIV Software Services             */
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
#ifndef INCLUDED_SDK_MSGAPI_MESSAGE_API_WWIV_H
#define INCLUDED_SDK_MSGAPI_MESSAGE_API_WWIV_H

#include "sdk/config.h"
#include "sdk/msgapi/email_wwiv.h"
#include "sdk/msgapi/message_api.h"
#include "sdk/net/net.h"
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace wwiv::sdk::msgapi {

class WWIVMessageArea;

// Can't merge with MessageAreaLastRead since that knows the area
// Yet this one knows the user since it's used in the context
// of a user session, otherwise we ignore the last_read values
// when used from tools.
class WWIVLastReadImpl {
public:
  virtual ~WWIVLastReadImpl() = default;
  [[nodiscard]] virtual uint32_t last_read(int area) const = 0;
  virtual void set_last_read(int area, uint32_t last_read) = 0;
  virtual void Load() = 0;
  virtual void Save() = 0;
};

class NullLastReadImpl final : public WWIVLastReadImpl {
  [[nodiscard]] uint32_t last_read(int) const override { return 0; }
  void set_last_read(int, uint32_t) override {}
  void Load() override {}
  void Save() override {}
};

class WWIVMessageApi final : public MessageApi {
public:
  WWIVMessageApi(const MessageApiOptions& options, const Config& config,
                 const std::vector<net::Network>& net_networks, WWIVLastReadImpl* last_read);

  [[nodiscard]] bool Exist(const subboard_t& sub) const override;
  [[nodiscard]] bool Create(const std::string& name, const std::string& sub_ext,
                      const std::string& text_ext, int subnum);
  [[nodiscard]] bool Create(const subboard_t& sub, int subnum) override;
  [[nodiscard]] bool Remove(const std::string& name) override;
  [[nodiscard]] MessageArea* Open(const subboard_t& sub, int subnum) override;
  [[nodiscard]] std::unique_ptr<WWIVEmail> OpenEmail();
  [[nodiscard]] uint32_t last_read(int area) const;
  void set_last_read(int area, uint32_t last_read);
  [[nodiscard]] const Config& config() const noexcept { return config_; }

private:
  std::unique_ptr<WWIVLastReadImpl> last_read_;
  const Config config_;
};

} // namespace

#endif
