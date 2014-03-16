/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2014, WWIV Software Services             */
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
#ifndef __INCLUDED_WWIV_H__
#define __INCLUDED_WWIV_H__

//
// Make sure right number of OS flags are defined.  Some platform #defines are
// defined in this file, so anything that relies on thse values MUST
// be included after testos.h has been included.
//

// make sure we use 32bit time_t even on x64 platforms.
// this can't be in platform/incl1.h due to other includes of
// windows headers so putting it here.
#define _USE_32BIT_TIME_T

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif  // _CRT_SECURE_NO_WARNINGS
#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#endif  // _CRT_SECURE_NO_DEPRECATE

#include "platform/testos.h"

#include "wwivassert.h"


//
// Include platform set of standard header files
//

#include "platform/incl1.h"


//
// Normal ANSI C type includes
//
#include <memory>
#include <cstdlib>
#include <cstdio>
#include <cerrno>

#include <cmath>
#include <csignal>
#include <cstdarg>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <ctime>
#include <fcntl.h>

#include <algorithm>
#include <locale>
#include <sstream>
#include <iostream>

//
// WWIV includes
//
#include "wtypes.h"
#include "vardec.h"
#include "WUser.h"
#include "WSession.h"
#include "bbs.h"
#include "WComm.h"
#include "platform/WFile.h"
#include "WTextFile.h"
#include "instmsg.h"
#include "WConstants.h"
#include "net.h"
#include "common.h"
#include "fcns.h"
#include "platform/platformfcns.h"
#include "vars.h"
#include "subxtr.h"
#include "platform/wfndfile.h"
#include "WStringUtils.h"
#include "WOutStreamBuffer.h"
#include "WStatus.h"
#include "ini.h"
#include "platform/WLocalIO.h"

#include "filenames.h"

#endif	// __INCLUDED_WWIV_H__
