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
#include "common/value/bbsvalueprovider.h"

#include "core/strings.h"
#include "common/context.h"
#include "core/version.h"
#include "fmt/printf.h"
#include "sdk/user.h"
#include "sdk/acs/eval_error.h"

#include <optional>
#include <string>

using namespace wwiv::common::value;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::acs;
using namespace wwiv::sdk::value;
using namespace parser;
using namespace wwiv::strings;

namespace wwiv::common::value {


/** Shorthand to create an optional Value */
template <typename T> static std::optional<Value> val(T&& v) {
  return std::make_optional<Value>(std::forward<T>(v));
}

BbsValueProvider::BbsValueProvider(const Config& config,  const common::SessionContext& sess)
  : ValueProvider("bbs"), config_(config), sess_(sess) {
}

BbsValueProvider::BbsValueProvider(common::Context& context)
  : BbsValueProvider(context.config(), context.session_context()) {
}

std::optional<Value> BbsValueProvider::value(const std::string& name) const {
  if (iequals(name, "name")) {
    return val(config_.system_name());
  }
  if (iequals(name, "sysopname")) {
    return val(config_.sysop_name());
  }
  if (iequals(name, "phone")) {
    return val(config_.system_phone());
  }
  if (iequals(name, "node")) {
    return val(sess_.instance_number());
  }
  if (iequals(name, "reg")) {
    return val(static_cast<int>(config_.wwiv_reg_number()));
  }
  if (iequals(name, "os")) {
    return val(os::os_version_string());
  }
  if (iequals(name, "version")) {
    return val(full_version());
  }
  if (iequals(name, "compiletime")) {
    return val(wwiv_compile_datetime());
  }
  throw eval_error(fmt::format("No attribute named 'bbs.{}' exists.", name));
}

}