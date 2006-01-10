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

#ifndef __INCLUDED_WFNDFILE_H__
#define __INCLUDED_WFNDFILE_H__

class WFindFile
{
protected:
	char szFileName[MAX_PATH];
	char szFileSpec[MAX_PATH];
	long lFileSize;
	long lTypeMask;
	bool bIsOpen;

	void __open(const char * pszFileSpec, UINT32 nTypeMask)
	{
		strcpy(szFileSpec, pszFileSpec);
		lTypeMask = nTypeMask;
	}

	void __close() { szFileName[0] = '\0'; szFileSpec[0] = '\0'; lFileSize=0; lTypeMask = 0; bIsOpen=false;}

#if defined (_WIN32)
	WIN32_FIND_DATA ffdata;
	HANDLE	hFind;
#elif defined (_UNIX)
	struct dirent **entries;
	int nMatches;
	int nCurrentEntry;
#elif defined (__OS2__)
#error "Hey OS/2 Dude... WRITE ME!"
#elif defined (__MSDOS__)
	ffblk hFind;
#else
#error "No platform specified, cannot build this __FILE__"
#endif


public:
	WFindFile() { this->__close(); }
	bool open(const char * pszFileSpec, UINT32 nTypeMask);
	bool next();
	bool close();
	virtual ~WFindFile() { close(); }

	const char * GetFileName() { return (const char*) &szFileName; }
	long GetFileSize() { return lFileSize; }
	bool IsDirectory();
	bool IsFile();
};


/**
 * Bit-mapped values for what WFindFile is searching
 */
enum WFindFileTypeMask
{
	WFINDFILE_FILES = 0x01,
	WFINDFILE_DIRS	= 0x02
};


#endif	// __INCLUDED_WFNDFILE_H__
