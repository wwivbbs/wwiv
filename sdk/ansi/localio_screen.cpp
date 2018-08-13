/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5                              */
/*               Copyright (C)2018, WWIV Software Services                */
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
#include "sdk/ansi/localio_screen.h"

#include "core/stl.h"
#include "local_io/curatr_provider.h"
#include <algorithm>

using namespace wwiv::stl;

namespace wwiv {
namespace sdk {
namespace ansi {

class LocalIOScreenCurAttrProvider : public wwiv::local_io::curatr_provider {
public:
  LocalIOScreenCurAttrProvider(LocalIOScreen* lio) : lio_(lio) {}
  virtual int curatr() const noexcept override { return lio_->curatr(); }
  virtual void curatr(int n) override { lio_->curatr(n); }

private:
  LocalIOScreen* lio_;
};


LocalIOScreen::LocalIOScreen(LocalIO* io, int cols)
    : VScreen(cols), io_(io), cols_(cols),
      curatr_provider_(std::make_unique<LocalIOScreenCurAttrProvider>(this)) {
  io_->set_curatr_provider(curatr_provider_.get());
}

} // namespace ansi
} // namespace sdk
} // namespace wwiv
