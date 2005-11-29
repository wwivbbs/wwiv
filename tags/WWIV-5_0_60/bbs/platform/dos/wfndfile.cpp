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


bool WFindFile::open(const char * pszFileSpec, UINT32 nTypeMask)
{
	__open(pszFileSpec, nTypeMask);

	if (findfirst(pszFileSpec, &hFind, 0) == -1)
	{
		return false;
	}
	strcpy(szFileName,hFind.ff_name); 
	lFileSize = hFind.ff_fsize;

	return true;
}



bool WFindFile::next()
{
	if (findnext(&hFind) == -1)
	{
		return false;
	}
	strcpy(szFileName,hFind.ff_name); 
	lFileSize = hFind.ff_fsize;

	return true;
}


bool WFindFile::close()
{
	__close();
	return true;
}


bool WFindFile::IsDirectory()
{
	if (IsFile())
	{
		return false;
	}
	return true;
}

bool WFindFile::IsFile()
{
	if (hFind.ff_attrib & FA_DIREC)
	{
		return false;
	}
	return true;
}

