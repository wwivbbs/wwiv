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

#if defined( _WIN32 )
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif // _WIN32

double WOSD_WIN32_FreeSpaceForDriveLetter(int nDrive);


bool WOSD_CopyFile(const char * szSourceFileName, const char * szDestFileName)
{
    return CopyFileA(szSourceFileName, szDestFileName, 0) ? true : false;
}

/**
 * Returns the free disk space for a drive letter, where nDrive is the drive number
 * 0 = 'A', 1 = 'B', etc.
 *
 * @param nDrive The drive number to get the free disk space for.
 */
double WOSD_WIN32_FreeSpaceForDriveLetter(int nDrive)
{
    DWORD spc, bps, nfc, ntc;
    // win95 osr2+ allows the GetDiskFreeSpaceEx call
    // this one will artificially cap free space at 2 gigs
    if (nDrive)
    {
        char s[] = "X:\\";
        s[0] = 'A' + nDrive - 1;
        if (!GetDiskFreeSpace(s,&spc,&bps,&nfc,&ntc))
        {
            return -1.0;
        }
    }
    else
    {
        if (!GetDiskFreeSpace(NULL,&spc,&bps,&nfc,&ntc))
        {
            return -1.0;
        }
    }
    
    return ((double)(nfc * spc * bps))/ 1024.0;
}


double WOSD_GetFreeSpaceForPath(const char * szPath)
{
	char szWWIVHome[MAX_PATH];
	strcpy(szWWIVHome, "C:\\");
    int nDrive = szWWIVHome[0];
    
	if (szPath[1] == ':')
	{
        nDrive = szPath[0];
	}
    
	nDrive = toupper(nDrive) - 'A' + 1;

    return WOSD_WIN32_FreeSpaceForDriveLetter( nDrive );

}


void WOSD_ChangeDirTo(const char *s)
{
    char s1[81];
    int i, db;
    
    strcpy(s1, s);
    i = strlen(s1) - 1;
    db = (s1[i] == '\\');
    if (i == 0)
    {
        db = 0;
    }
    if ((i == 2) && (s1[1] == ':'))
    {
        db = 0;
    }
    if (db)
    {
        s1[i] = 0;
    }
    chdir(s1);
    if (s[1] == ':') 
    {
        _chdrive(s[0] - 'A' + 1);	// FIX, On Win32, _chdrive is 'A' = 1, etc..
        if (s[2] == 0)
        {
            chdir("\\");
        }
    }
}


void WOSD_GetDir(char *s, int be)
{
    strcpy(s, "X:\\");
    s[0] = 'A' + (_getdrive() - 1);
    _getdcwd(0, &s[0], 161);
    if (be) 
    {
        if (s[strlen(s) - 1] != '\\')
        {
            strcat(s, "\\");
        }
    }
}


void WOSD_GetFileNameFromPath(const char *pszPath, char *pszFileName)
{
	_splitpath(pszPath, NULL, NULL, pszFileName, NULL);
}
