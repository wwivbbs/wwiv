/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*               Copyright (C)2014, WWIV Software Services                */
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
#include "qscan.h"

#include <cstdint>
#include <memory>
#include "bbs.h"
#include "vars.h"
#include "wsession.h"

namespace wwiv {
namespace bbs {

SaveQScanPointers::SaveQScanPointers() : restore_(false) {
  save_qsc_p_.reset(new uint32_t[session()->GetMaxNumberMessageAreas()]);
  for (int i = 0; i < session()->GetMaxNumberMessageAreas(); i++) {
    save_qsc_p_[i] = qsc_p[i];
  }
}

SaveQScanPointers::~SaveQScanPointers() {
  if (restore_) {
    for (int i = 0; i < session()->GetMaxNumberMessageAreas(); i++) {
      qsc_p[i] = save_qsc_p_[i];
    }
  }
}
  
}  // namespace bbs
}  // namespace wwiv

