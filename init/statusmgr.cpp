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
#include "wwivinit.h"


// Stipped down version of what the BBS uses, this one will not create
// the status.dat since we know INIT already did it for us.


static int statusfile = -1;

void InitStatusMgr::Get(bool bFailOnFailure, bool bLockFile)
{
	char s[81];
	
	if (statusfile < 0) 
	{
		sprintf(s, "%sSTATUS.DAT", syscfg.datadir);
		int nLockMode = (bLockFile) ? (O_RDWR | O_BINARY) : (O_RDONLY | O_BINARY);
		statusfile = sh_open1(s, nLockMode);
	} 
	else 
	{
		_lseek(statusfile, 0L, SEEK_SET);
	}
	if ((statusfile < 0) && (!bFailOnFailure))
	{
		Printf("%s NOT FOUND\r\n", s);
		exit( 1 );
	} 
}

void InitStatusMgr::Lock()
{
	this->Get(true, true);
}

void InitStatusMgr::Read()
{
	this->Get(true, false);
}

void InitStatusMgr::Write()
{
	char s[81];
	
	if (statusfile < 0) 
	{
		sprintf(s, "%sSTATUS.DAT", syscfg.datadir);
		statusfile = sh_open1(s, O_RDWR | O_BINARY);
	} 
	else 
	{
		_lseek(statusfile, 0L, SEEK_SET);
	}
	
	if (statusfile >= 0) 
	{
		sh_write(statusfile, (void *) (&status), sizeof(statusrec));
		statusfile = sh_close(statusfile);
	}
}


