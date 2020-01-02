/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5                              */
/*             Copyright (C)2015-2020, WWIV Software Services             */
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
#ifndef __INCLUDED_SDK_SUBSCRIBERS_H__
#define __INCLUDED_SDK_SUBSCRIBERS_H__

#include <set>
#include <string>

#include <filesystem>
#include "sdk/fido/fido_address.h"

namespace wwiv {
namespace sdk {

std::set<wwiv::sdk::fido::FidoAddress> ReadFidoSubcriberFile(const std::filesystem::path& filename);
bool ReadSubcriberFile(const std::filesystem::path& filename,
                       std::set<uint16_t>& subscribers);
bool WriteSubcriberFile(const std::filesystem::path& path,
                        const std::set<uint16_t>& subscribers);

}  // namespace sdk
}  // namespace wwiv

#endif  // __INCLUDED_SDK_SUBSCRIBERS_H__
