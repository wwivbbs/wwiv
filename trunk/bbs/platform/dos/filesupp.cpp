/****************************************************************************/
/*                                                                          */
/*                             WWIV Version 5.0x                            */
/*            Copyright (C) 1998-2005 by WWIV Software Services             */
/*                                                                          */
/*      Distribution or publication of this source code, it's individual    */
/*       components, or a compiled version thereof, whether modified or     */
/*        unmodified, without PRIOR, WRITTEN APPROVAL of WWIV Software      */
/*        Services is expressly prohibited.  Distribution of compiled       */
/*            versions of WWIV is restricted to copies compiled by          */
/*           WWIV Software Services.  Violators will be procecuted!         */
/*                                                                          */
/****************************************************************************/
#include "../../wwiv.h"


double WWIV_WIN32_FreeSpaceForDriveLetter(int nDrive);


bool WWIV_CopyFile(const char * szSourceFileName, const char * szDestFileName)
{
    return CopyFile(szSourceFileName, szDestFileName, false);
}

/**
 * Returns the free disk space for a drive letter, where nDrive is the drive number
 * 0 = 'A', 1 = 'B', etc.
 *
 * @param nDrive The drive number to get the free disk space for.
 */
double WWIV_WIN32_FreeSpaceForDriveLetter(int nDrive)
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


double WWIV_GetFreeSpaceForPath(const char * szPath)
{
	char szWWIVHome[MAX_PATH];
	strcpy(szWWIVHome, GetApplication()->GetHomeDir());
    int nDrive = szWWIVHome[0];;
    
	if (szPath[1] == ':')
	{
        nDrive = szPath[0];
	}
    
	nDrive = toupper(nDrive) - 'A' + 1;

    return (WWIV_WIN32_FreeSpaceForDriveLetter(nDrive));

}


void WWIV_ChangeDirTo(const char *s)
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


void WWIV_GetDir(char *s, int be)
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


void WWIV_GetFileNameFromPath(const char *pszPath, char *pszFileName)
{
	_splitpath(pszPath, NULL, NULL, pszFileName, NULL);
}
