/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services             */
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

#include "core/log.h"
#include <functional>

using namespace wwiv::core;

namespace wwiv::common {

// Context stuff

sdk::User& IOBase::user() const { 
  DCHECK(context_provider_);
  return context_provider_().u();
}

SessionContext& IOBase::sess() { 
  DCHECK(context_provider_);
  return context_provider_().session_context();
}

SessionContext& IOBase::sess() const { 
  DCHECK(context_provider_);
  return context_provider_().session_context();
}

Context& IOBase::context() { 
  DCHECK(context_provider_);
  return context_provider_(); 
}

}