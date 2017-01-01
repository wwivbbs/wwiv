/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.0x                             */
/*             Copyright (C)2015-2017, WWIV Software Services             */
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
#ifndef __INCLUDED_SDK_TYPE2_TEXT_H__
#define __INCLUDED_SDK_TYPE2_TEXT_H__

#include <cstdint>
#include <string>
#include <vector>

#include "core/file.h"
#include "sdk/msgapi/message_wwiv.h"

namespace wwiv {
namespace sdk {
namespace msgapi {

class Type2Text {
public:
  Type2Text(const std::string& text_filename);
  virtual ~Type2Text();

  std::vector<uint16_t> load_gat(File& file, size_t section);
  void save_gat(File& f, size_t section, const std::vector<uint16_t>& gat);
  bool readfile(const messagerec* msg, std::string* out);
  bool savefile(const std::string& text, messagerec* pMessageRecord);
  bool remove_link(messagerec& msg);

private:
  std::unique_ptr<File> OpenMessageFile();
  const std::string filename_;
};

}  // namespace msgapi
}  // namespace sdk
}  // namespace wwiv


#endif  // __INCLUDED_SDK_TYPE2_TEXT_H__
