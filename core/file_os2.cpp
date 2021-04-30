/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                  Copyright (C)2021, WWIV Software Services             */
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
#include "core/file.h"

#include <share.h>
#include <sys/stat.h>

namespace wwiv::core {

const int File::shareDenyReadWrite = SH_DENYRW;
const int File::shareDenyWrite = SH_DENYWR;
const int File::shareDenyRead = SH_DENYRD;
const int File::shareDenyNone = SH_DENYNO;

const int File::permReadWrite = (S_IREAD | S_IWRITE);

const char File::pathSeparatorChar = '\\';

} // namespace wwiv
