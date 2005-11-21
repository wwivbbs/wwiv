/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2005, WWIV Software Services             */
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

#if defined( _MSC_VER )
#define _CRT_SECURE_NO_DEPRECATE
#endif	// _MSC_VER 

#include "testos.h"

#include "wwivassert.h"


//
// Include platform set of standard header files
//

#include "incl1.h"


//
// Normal ANSI C type includes
//

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

#if defined( _MSC_VER )
#pragma warning( push, 3 )
#endif // _MSC_VER

#include <algorithm>
#include <locale>

#if defined( _MSC_VER )
#pragma warning( pop )
#endif // _MSC_VER

//
// Include platform set after standard header files
//

#include "incl2.h"


//
// WWIV includes
//
#include "wtypes.h"
#include "vardec.h"
#include "WUser.h"
#include "WSession.h"
#include "bbs.h"
#include "WComm.h"
#include "WFile.h"
#include "wutil.h"
#include "instmsg.h"
#include "WConstants.h"
#include "net.h"
#include "common.h"
#include "menu.h"
#include "fcns.h"
#include "platformfcns.h"
#include "vars.h"
#include "subxtr.h"
#include "wfndfile.h"
#include "WStringUtils.h"
#include "WOutStreamBuffer.h"

#include "incl3.h"

#include "filenames.h"

#endif	// __INCLUDED_WWIV_H__
