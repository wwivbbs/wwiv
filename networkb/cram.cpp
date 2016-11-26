/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                Copyright (C)2016, WWIV Software Services               */
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
#include "networkb/cram.h"

#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "core/md5.h"
#include "core/log.h"

using std::string;

namespace wwiv {
namespace net {

bool Cram::GenerateChallengeData() {
  if (challenge_data_.empty()) {
    challenge_data_ = "cafebabecafebabe";
  }
  initialized_ = true;
  return true;
}

bool Cram::ValidatePassword(const std::string& challenge,
                            const std::string& secret, 
                            const std::string& given_hashed_secret) {
  auto expected = CreateHashedSecret(challenge, secret);
  VLOG(2) << "expected pw: " << expected << "; given: " << given_hashed_secret;
  return expected == given_hashed_secret;
}

static std::string SecretOrHash(const std::string& secret) {
  if (secret.size() <= 64) {
    return secret;
  }

  VLOG(1) << "secret is >64 bytes";

  MD5_CTX ctx;
  MD5_Init(&ctx);

  unsigned char hash[16];
  MD5_Update(&ctx, &secret[0], secret.size());
  MD5_Final(hash, &ctx);

  return string(reinterpret_cast<const char*>(hash), 16);
}

std::string Cram::CreateHashedSecret(
  const std::string& original_challenge, const std::string& secret) {
  if (!initialized_) {
    if (!GenerateChallengeData()) {
      // TODO: Go Boom
    }
  }

  string challenge = SecretOrHash(original_challenge);
  unsigned char ipad[65];
  unsigned char opad[65];
  memset(ipad, 0, sizeof(ipad));
  memset(opad, 0, sizeof(opad));
  memcpy(ipad, challenge.data(), challenge.size());
  memcpy(opad, challenge.data(), challenge.size());

  for (size_t i = 0; i < 65; i++) {
    ipad[i] ^= 0x36;
    opad[i] ^= 0x5c;
  }

  // Inner
  MD5_CTX ctx;
  unsigned char digest[17];
  MD5_Init(&ctx);
  MD5_Update(&ctx, ipad, 64);
  MD5_Update(&ctx, secret.data(), secret.size());
  MD5_Final(digest, &ctx);

  // Outer
  MD5_Init(&ctx);
  MD5_Update(&ctx, opad, 64);
  MD5_Update(&ctx, digest, 16);
  MD5_Final(digest, &ctx);

  std::ostringstream ss;
  for (int i = 0; i < 16; i++) {
    ss << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(digest[i]);
  }
  return ss.str();
}

}
}
