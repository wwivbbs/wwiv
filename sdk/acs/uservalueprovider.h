/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5                              */
/*                Copyright (C)2020, WWIV Software Services               */
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
#ifndef __INCLUDED_SDK_ACS_USERVALUEPROVIDER_H__
#define __INCLUDED_SDK_ACS_USERVALUEPROVIDER_H__

#include "sdk/user.h"
#include "sdk/acs/value.h"
#include "sdk/acs/valueprovider.h"
#include <any>
#include <memory>
#include <optional>
#include <string>

namespace wwiv::sdk::acs {


class UserValueProvider : public ValueProvider {
public:
  UserValueProvider(wwiv::sdk::User* user) : ValueProvider("user"), user_(user) {}
  std::optional<Value> value(const std::string& name) override;

private:
  wwiv::sdk::User* user_;
};

}

#endif // __INCLUDED_SDK_ACS_EVAL_H__