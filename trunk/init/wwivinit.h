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
#error "NOT_BBS MUST BE DEFINED"
#endif  // NOT_BBS
#ifndef INIT
#error "INIT MUST BE DEFINED"
#endif  // INIT

#ifndef EDITLINE_FILENAME_CASE
#if __unix__
#define EDITLINE_FILENAME_CASE ALL
#else
#define EDITLINE_FILENAME_CASE UPPER_ONLY
#endif  // __unix__
#endif  // EDITLINE_FILENAME_CASE

#include "bbs/vardec.h"
#include "bbs/net.h"
#include "ivardec.h"

#if !defined (MAX_PATH)
#define MAX_PATH 260
#endif  // MAX_PATH

#define MAX_NETWORKS 100
#define MAX_LANGUAGES 100
#define MAX_ALLOWED_PORT 8

extern configrec syscfg;
extern configoverrec syscfgovr;
extern statusrec status;
extern subboardrec *subboards;
extern chainfilerec *chains;
extern newexternalrec *externs, *over_intern;
extern editorrec *editors;
extern arcrec *arcs;
extern net_networks_rec *net_networks;
extern languagerec *languages;
extern initinfo_rec initinfo;

#if defined(__unix__)

#define _open open
#define _close close
#define _lseek lseek
#define _read read
#define _write write

#endif  // __unix__

#endif // __INCLUDED_WWIVINIT_H__
