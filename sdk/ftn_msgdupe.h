/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2016-2017, WWIV Software Services             */
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
#ifndef __INCLUDED_SDK_FTN_MSGDUPE_H__
#define __INCLUDED_SDK_MSGID_H__

#include <string>
#include <set>
#include <vector>
#include "sdk/config.h"
#include "sdk/vardec.h"
#include "sdk/fido/fido_address.h"
#include "sdk/fido/fido_packets.h"

namespace wwiv {
namespace sdk {

 uint64_t as64(uint32_t header, uint32_t msgid);

 struct msgids {
   uint32_t msgid{};
   uint32_t header{};
 };

 static_assert(sizeof(msgids) == sizeof(uint64_t), "sizeof(msgids) must be the same as an int64.");

class FtnMessageDupe {
public:
  explicit FtnMessageDupe(const Config& config);
  explicit FtnMessageDupe(const std::string& datadir);
  virtual ~FtnMessageDupe() = default;

  bool IsInitialized() const { return initialized_; }
  const std::string CreateMessageID(const wwiv::sdk::fido::FidoAddress& a);
  bool add(const wwiv::sdk::fido::FidoPackedMessage& msg);
  bool add(uint32_t header_crc32, uint32_t msgid_crc32);
  bool remove(uint32_t header_crc32, uint32_t msgid_crc32);
  /** returns true if either the header or msgid crc is duplicated */
  bool is_dupe(uint32_t header_crc32, uint32_t msgid_crc32) const;
  bool is_dupe(const wwiv::sdk::fido::FidoPackedMessage& msg) const;

  /** Returns the MSGID from this message or an empty string. */
  static std::string GetMessageIDFromText(const std::string& text);
  /**
   * Returns the MSGID from this message in WWIV format 
   * (^D0MSGID vs. ^AMSGID) or an empty string.
   */
  static std::string GetMessageIDFromWWIVText(const std::string& text);

private:
  bool Load();
  bool Save();

  bool initialized_;
  std::string datadir_;
  std::vector<msgids> dupes_;
  std::set<uint32_t> msgid_dupes_;
  std::set<uint32_t> header_dupes_;
};


}
}


#endif  // __INCLUDED_SDK_MSGID_H__
