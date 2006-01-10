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

#include <sys/wait.h>
//
// Local function prototypes
//

int UnixSpawn (char *pszCommand, char* environ[]);

//
// Implementation
//

int ExecExternalProgram( const char *pszCommandLine, int flags )
{
    (void)flags;

	if (ok_modem_stuff)
    {
		app->comm->close( true );
	}

    char s[256];
    strcpy(s, pszCommandLine);
    int i = UnixSpawn(s, NULL);

	// reengage comm stuff
	if (ok_modem_stuff)
    {
		app->comm->open();
		app->comm->dtr( true );
    }

    return i;
}


int UnixSpawn (char *pszCommand, char* environ[])
{
	if (pszCommand == 0)
	{
		return 1;
	}
	int pid = fork();
	if (pid == -1)
	{
		return -1;
	}
	if (pid == 0)
	{
		char *argv[4];
		argv[0] = "/bin/sh";
		argv[1] = "-c";
		argv[2] = pszCommand;
		argv[3] = 0;
		execv( "/bin/sh", argv );
		exit(127);
	}

	for( ;; )
	{
		int status = 1;
		if (waitpid(pid, &status, 0) == -1)
		{
			if (errno != EINTR)
			{
				return -1;
			}
		}
		else
		{
			return status;
		}
	}
}


