/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2017, WWIV Software Services             */
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
#ifndef __INCLUDED_VARS_H__
#define __INCLUDED_VARS_H__

#include <ctime>

#include "bbs/conf.h"
#include "sdk/vardec.h"
#include "sdk/net.h"

///////////////////////////////////////////////////////////////////////////////

#ifdef _DEFINE_GLOBALS_
#define __EXTRN__
#else
#define __EXTRN__ extern
#endif

///////////////////////////////////////////////////////////////////////////////

__EXTRN__ char charbuffer[255];
__EXTRN__ char irt[81];
__EXTRN__ char* quotes_ind;

__EXTRN__ unsigned int
          dirconfnum,
          subconfnum;

__EXTRN__ daten_t nscandate;

__EXTRN__ int32_t g_flags;

#endif // __INCLUDED_VARS_H__

