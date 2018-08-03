/**************************************************************************/
/*                                                                        */
/*                                WWIV Version 5                          */
/*             Copyright (C)1998-2016, WWIV Software Services             */
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
#include "bbs/make_abs_cmd.h"

#include <sys/stat.h>
#include <unistd.h>
#include <string>

#include "bbs/bbs.h"
#include "bbs/application.h"

#include "core/strings.h"
#include "core/file.h"
#include "core/wwivport.h"

using std::string;
using wwiv::strings::StrCat;

void make_abs_cmd(const string root, string* out) {
  if (out->find("/") == string::npos) {
	// Use current path of we don't have an abs path.
    string s(*out);
    File f(a()->GetHomeDir(), s);
    *out = f.full_pathname();
  }
}

