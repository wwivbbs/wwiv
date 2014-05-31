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
#include "wuser.h"
#include "wsession.h"
#include "bbs.h"
#include "wcomm.h"
#include "platform/wfile.h"
#include "wtextfile.h"
#include "instmsg.h"
#include "wconstants.h"
#include "net.h"
#include "common.h"
#include "fcns.h"
#include "platform/platformfcns.h"
#include "vars.h"
#include "subxtr.h"
#include "platform/wfndfile.h"
#include "wstringutils.h"
#include "woutstreambuffer.h"
#include "wstatus.h"
#include "ini.h"
#include "platform/wlocal_io.h"

#include "filenames.h"

#endif	// __INCLUDED_WWIV_H__
