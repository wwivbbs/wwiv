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
#include "bbs/ssh.h"
#include "cryptlib.h"

#include <memory>
#include <string>

using std::string;
using std::unique_ptr;

namespace wwiv {
namespace bbs {

static constexpr char WWIV_SSH_KEY_NAME[] = "wwiv_ssh_server";
#define RETURN_IF_ERROR(s) { if (!cryptStatusOK(s)) return false; }
#define OK(s) cryptStatusOK(s)

bool Key::Open() {
  CRYPT_KEYSET keyset;
  int status = cryptKeysetOpen(&keyset, CRYPT_UNUSED, CRYPT_KEYSET_FILE, filename_.c_str(), CRYPT_KEYOPT_NONE);
  RETURN_IF_ERROR(status);

  status = cryptGetPrivateKey(keyset, &context_, CRYPT_KEYID_NAME, WWIV_SSH_KEY_NAME, password_.c_str());
  RETURN_IF_ERROR(status);

  status = cryptKeysetClose(keyset);
  RETURN_IF_ERROR(status);

  return true;
}

bool Key::Create() {
  CRYPT_KEYSET keyset;
  int status = CRYPT_ERROR_INVALID;

  status = cryptCreateContext(&context_, CRYPT_UNUSED, CRYPT_ALGO_RSA);
  RETURN_IF_ERROR(status);

  status = cryptSetAttributeString(context_, CRYPT_CTXINFO_LABEL, WWIV_SSH_KEY_NAME, strlen(WWIV_SSH_KEY_NAME));
  RETURN_IF_ERROR(status);

  status = cryptGenerateKey(context_);
  RETURN_IF_ERROR(status);

  status = cryptKeysetOpen(&keyset, CRYPT_UNUSED, CRYPT_KEYSET_FILE, filename_.c_str(), CRYPT_KEYOPT_CREATE);
  RETURN_IF_ERROR(status);

  status = cryptAddPrivateKey(keyset, context_, password_.c_str());
  RETURN_IF_ERROR(status);

  status = cryptKeysetClose(keyset);
  RETURN_IF_ERROR(status);

  return true;
}

Session::Session() {
  int status = CRYPT_ERROR_INVALID;

  status = cryptCreateSession(&session_, CRYPT_UNUSED, CRYPT_SESSION_SSH_SERVER);
  if (OK(status)) {

  }
}

bool Session::AddKey(const Key& key) {
  cryptSetAttribute(session_, CRYPT_SESSINFO_PRIVATEKEY, key.context());

  return true;
}

}
}
