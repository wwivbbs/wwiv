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

__EXTRN__ bool  
          forcescansub,
          guest_user,
          smwcheck,
          chatcall,
          chat_file,
          emchg,
          hangup,
          incom,
          outcom,
          okskey,
          okmacro,
          ok_modem_stuff;

__EXTRN__ int curatr;
__EXTRN__ unsigned int
          dirconfnum,
          subconfnum;

__EXTRN__ daten_t nscandate;

__EXTRN__ int32_t g_flags;

// qsc is the qscan pointer. The 1st 4 bytes are the sysop sub number.
__EXTRN__ uint32_t *qsc;
// A bitfield controlling if the directory should be included in the new scan.
__EXTRN__ uint32_t *qsc_n;
// A bitfield controlling if the sub should be included in the new scan.
__EXTRN__ uint32_t *qsc_q;
// Array of 32-bit unsigned integers for the qscan pointer value
// aka high message read pointer) for each sub.
__EXTRN__ uint32_t *qsc_p;

__EXTRN__ long extratimecall;

#endif // __INCLUDED_VARS_H__

