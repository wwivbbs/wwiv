/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5                              */
/*             Copyright (C)2018-2022, WWIV Software Services             */
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
#ifndef INCLUDED_SDK_MAKEANSI_H
#define INCLUDED_SDK_MAKEANSI_H

#include <string>

namespace wwiv::sdk::ansi {

/** 
 * Passed to this function is a one-byte attribute as defined for IBM type
 * screens.  Returned is a string which, when printed, will change the
 * display to the color desired, from the current function.
 */
std::string makeansi(int attr, int current_attr);

} // namespace wwiv::sdk::ansi

#endif // INCLUDED_SDK_MAKEANSI_H
