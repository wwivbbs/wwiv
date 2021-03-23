/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5                              */
/*           Copyright (C)2020-2021, WWIV Software Services               */
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
#ifndef INCLUDED_SDK_VALUEPROVIDER_USERVALUEPROVIDER_H
#define INCLUDED_SDK_VALUEPROVIDER_USERVALUEPROVIDER_H

#include "common/context.h"
#include "sdk/chains.h"
#include "sdk/config.h"
#include "sdk/user.h"
#include "sdk/value/value.h"
#include "sdk/value/valueprovider.h"
#include <optional>
#include <string>

namespace wwiv::common::value {

/**
 * ValueProvider for "user" record attributes.
 */
class UserValueProvider final : public sdk::value::ValueProvider {
public:
  /** 
   * Constructs a new ValueProvider.  'user' must remain valid for 
   * the duration of this instance lifetime.
   */
  UserValueProvider(const sdk::Config& config, const sdk::User& user, int effective_sl, slrec sl);
  explicit UserValueProvider(Context& c);
  UserValueProvider(Context& context, int effective_sl);
  [[nodiscard]] std::optional<sdk::value::Value> value(const std::string& name) const override;

private:
  typedef std::function<std::optional<sdk::value::Value>()> makeval_fn;
  std::map<const std::string, makeval_fn> fns_;
  const sdk::Config& config_;
  const sdk::User& user_;
  int effective_sl_;
  slrec sl_;
  std::vector<editorrec> editors_;
  sdk::Chains chains_;
};

}

#endif
