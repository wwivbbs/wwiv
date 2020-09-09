/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2020, WWIV Software Services             */
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
#include "common/iobase.h"

#include "core/eventbus.h"
#include "core/stl.h"
#include "core/strings.h"
#include "local_io/keycodes.h"
#include <string>

using std::string;
using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::stl;
using namespace wwiv::strings;


namespace wwiv::common {

// Context stuff

wwiv::sdk::User& IOBase::user() { return context_provider_().u(); }

wwiv::common::SessionContext& IOBase::sess() { return context_provider_().session_context(); }

wwiv::common::SessionContext& IOBase::sess() const { return context_provider_().session_context(); }

wwiv::common::Context& IOBase::context() { return context_provider_(); }

}