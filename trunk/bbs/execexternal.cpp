/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2004, WWIV Software Services             */
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

#include "wwiv.h"

#if defined (_UNIX)
#include <sys/types.h>
#include <sys/wait.h>

//
// Local UNIX functions
//
int UnixSpawn( char *pszCommand, char* environ[] );

#endif // _UNIX



//
// Implementation
//


int ExecuteExternalProgram( const char *pszCommandLine, int nFlags )
{
    // forget it if the user has hung up
    if (!(nFlags & EFLAG_NOHUP))
	{
        if ( CheckForHangup() )
		{
            return -1;
		}
    }
    create_chain_file();

    // get ready to run it
    if ( GetSession()->IsUserOnline() )
	{
        GetSession()->WriteCurrentUser( GetSession()->usernum );
        write_qscn(GetSession()->usernum, qsc, false);
    }
    close_strfiles();
    GetApplication()->GetLocalIO()->set_global_handle( false );

    // extra processing for net programs
    if (nFlags & EFLAG_NETPROG)
    {
        write_inst(INST_LOC_NET, GetSession()->GetNetworkNumber() + 1, INST_FLAGS_NONE);
    }

    // Execute the program and make sure the workingdir is reset
    int nExecRetCode = ExecExternalProgram( pszCommandLine, nFlags );
    GetApplication()->CdHome();

    // Reread the user record.
    if ( GetSession()->IsUserOnline() )
	{
        GetApplication()->GetUserManager()->ReadUser( &GetSession()->thisuser, GetSession()->usernum, true );
        read_qscn( GetSession()->usernum, qsc, false, true );
        GetApplication()->GetLocalIO()->UpdateTopScreen();
    }

    // return to caller
    return nExecRetCode;
}

