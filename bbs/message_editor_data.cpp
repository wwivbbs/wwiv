/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                 Copyright (C)2018, WWIV Software Services              */
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
#include "bbs/message_editor_data.h"

#include "core/strings.h"
#include <string>

using std::string;
using namespace wwiv::bbs;
using namespace wwiv::strings;

bool MessageEditorData::is_email() const {
  return iequals(aux, "email");
}
