/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2022, WWIV Software Services            */
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
/*                                                                        */
/**************************************************************************/
#include "local_io/local_io.h"

#include "core/strings.h"

namespace wwiv::local::io {

using namespace wwiv::strings;

class DefaultCurAttrProvider final : public wwiv::local::io::curatr_provider {
public:
  DefaultCurAttrProvider() = default;
  [[nodiscard]] uint8_t curatr() const noexcept override { return a_; }
  void curatr(int n) override { a_ = static_cast<uint8_t>(n); }

private:
  uint8_t a_{7};
};

static std::unique_ptr<DefaultCurAttrProvider> default_curatr_provider_ =
    std::make_unique<DefaultCurAttrProvider>();

LocalIO::LocalIO() : curatr_(default_curatr_provider_.get()) {}
LocalIO::~LocalIO() = default;

void LocalIO::set_curatr_provider(wwiv::local::io::curatr_provider* p) { curatr_ = p; }
wwiv::local::io::curatr_provider* LocalIO::curatr_provider() { return curatr_; }

uint8_t LocalIO::curatr() const { return curatr_->curatr(); }
void LocalIO::curatr(int c) { curatr_->curatr(c); }

EditlineResult LocalIO::EditLine(std::string& s, int len, AllowedKeys allowed_keys,
                    const std::string& allowed_set_chars) {
  char p[1024];
  to_char_array(p, s);
  auto rc{EditlineResult::PREV};
  EditLine(p, len, allowed_keys, &rc, allowed_set_chars.c_str());
  s = p;
  return rc;
}

EditlineResult LocalIO::EditLine(std::string& s, int len, AllowedKeys allowed_keys) {
  return EditLine(s, len, allowed_keys, {});
}

void LocalIO::increment_topdata() {
  switch (topdata_) {
  case topdata_t::none:
    topdata_ = topdata_t::system;
    break;
  case topdata_t::system:
    topdata_ = topdata_t::user;
    break;
  case topdata_t::user:
    topdata_ = topdata_t::none;
    break;
  }
}

}
