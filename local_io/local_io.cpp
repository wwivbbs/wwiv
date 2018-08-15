/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2017, WWIV Software Services            */
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

class DefaultCurAttrProvider : public wwiv::local_io::curatr_provider {
public:
  DefaultCurAttrProvider() = default;
  virtual int curatr() const noexcept override { return a_; }
  virtual void curatr(int n) override { a_ = n; }

private:
  uint8_t a_{7};
};

static std::unique_ptr<DefaultCurAttrProvider> default_curatr_provider_ =
    std::make_unique<DefaultCurAttrProvider>();

LocalIO::LocalIO() : curatr_(default_curatr_provider_.get()) {}
LocalIO::~LocalIO() = default;

void LocalIO::set_curatr_provider(wwiv::local_io::curatr_provider* p) { curatr_ = p; }
wwiv::local_io::curatr_provider* LocalIO::curatr_provider() { return curatr_; }

int LocalIO::curatr() const { return curatr_->curatr(); }
void LocalIO::curatr(int c) { curatr_->curatr(c); }
