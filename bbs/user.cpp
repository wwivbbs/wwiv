/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2007, WWIV Software Services             */
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


#include "platform/incl1.h"
#include "WConstants.h"
#include "filenames.h"
#include "platform/WFile.h"
#include "WUser.h"
#include "WSession.h"
#include "WStringUtils.h"
#include "vars.h"
#include "bbs.h"

// TODO - Remove this and finduser, finduser1, ISR, DSR, and add_add
#include "fcns.h"

/*
* Checks status of given userrec to see if conferencing is turned on.
*/
bool okconf( WUser *pUser )
{
    if ( g_flags & g_flag_disable_conf )
    {
        return false;
    }

    return pUser->HasStatusFlag( WUser::conference );
}



void add_ass( int nNumPoints, const char *pszReason )
{
    sysoplog(  "***" );
    sysoplogf( "*** ASS-PTS: %d, Reason: [%s]", nNumPoints, pszReason );
    GetSession()->GetCurrentUser()->IncrementAssPoints( nNumPoints );
}



