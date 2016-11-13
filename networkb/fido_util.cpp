/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*               Copyright (C)2016 WWIV Software Services                 */
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
#include "networkb/fido_util.h"

#include <chrono>
#include <string>

#include "core/command_line.h"
#include "core/file.h"
#include "core/log.h"
#include "core/stl.h"
#include "core/strings.h"
#include "sdk/datetime.h"
#include "sdk/filenames.h"

using std::string;
using namespace wwiv::core;
using namespace wwiv::stl;
using namespace wwiv::strings;

namespace wwiv {
namespace net {
namespace fido {

// We use DDHHMMSS like SBBSECHO does.
std::string packet_name(time_t now) {
  auto tm = localtime(&now);

  string fmt = "%d%H%M%S";

  char buf[1024];
  auto res = strftime(buf, sizeof(buf), fmt.c_str(), tm);
  if (res <= 0) {
    LOG(ERROR) << "Unable to create packet name from strftime";
    to_char_array(buf, "DDHHMMSS");
  }
  return buf;
}

std::string bundle_name(const wwiv::sdk::fido::FidoAddress& source, const wwiv::sdk::fido::FidoAddress& dest, const std::string& extension) {
  int16_t net = source.net() - dest.net();
  uint16_t node = source.node() - dest.node();

  return StringPrintf("%04.4x%04.4x.%s", net, node, extension.c_str());
}

std::string bundle_name(const wwiv::sdk::fido::FidoAddress& source, const wwiv::sdk::fido::FidoAddress& dest, int dow, int bundle_number) {
  return bundle_name(source, dest, dow_extension(dow, bundle_number));
}

std::string dow_extension(int dow_num, int bundle_number) {
  // TODO(rushfan): Should we assert of bundle_number > 25 (0-9=10, + a-z=26 = 0-35)

  static const std::vector<string> dow = {"su", "mo", "tu", "we", "th", "fr", "sa", "su"};
  std::string ext = dow.at(dow_num);
  char c = static_cast<char>('0' + bundle_number);
  if (bundle_number > 9) {
    c = static_cast<char>('a' + bundle_number - 10);
  }
  ext.push_back(c);
  return ext;
}

}  // namespace fido
}  // namespace net
}  // namespace wwiv

