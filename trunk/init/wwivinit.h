/**************************************************************************/
/*                                                                        */
/*                 WWIV Initialization Utility Version 5.0                */
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
#ifndef __INCLUDED_WWIVINIT_H__
#define __INCLUDED_WWIVINIT_H__

#ifndef NOT_BBS
#define NOT_BBS 0
#endif  // NOT_BBS

#ifndef INIT
#define INIT
#endif  // INIT

#include "ivardec.h"
#include "vars.h"

// defined in VARS.H net_networks_rec *net_networks;
// defined in VARS.H int initinfo.net_num_max;
#define MAX_NETWORKS 100
#define MAX_LANGUAGES 100
#define MAX_ALLOWED_PORT 8

// Misc Junk for now
#define textattr(x) curatr = (x)
class WInitApp;

__EXTRN__ WInitApp *app;
//__EXTRN__ userrec thisuser;
__EXTRN__ initinfo_rec initinfo;

#if defined(__unix__)

#define stricmp strcasecmp
#define strnicmp strncasecmp
#define _strupr strupr
#define _open open
#define _close close
#define _lseek lseek
#define _read read
#define _write write
#define chsize ftruncate

#endif  // __unix__

#endif // __INCLUDED_WWIVINIT_H__
