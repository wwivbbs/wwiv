/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2016-2020, WWIV Software Services             */
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
#ifndef __INCLUDED_NETWORKB_CRAM_H__
#define __INCLUDED_NETWORKB_CRAM_H__

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace wwiv {
namespace net {
  
class Cram {
public:
  Cram() noexcept {}
  virtual ~Cram() {}

  bool ValidatePassword(const std::string& challenge, 
                        const std::string& secret, 
                        const std::string& given_hashed_secret);
  std::string CreateHashedSecret(const std::string& challenge, const std::string& secret);

  bool GenerateChallengeData();
  std::string challenge_data() const { return challenge_data_; }
  void set_challenge_data(const std::string& challenge_data) { challenge_data_ = challenge_data; }

private:
  std::string challenge_data_;
  bool initialized_ = false;
};


}  // namespace net
}  // namespace wwiv

#endif  // __INCLUDED_NETWORKB_CRAM_H__
