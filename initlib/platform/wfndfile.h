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

#ifndef __INCLUDED_WFNDFILE_H__
#define __INCLUDED_WFNDFILE_H__

#include "platform/wfile.h"

#if defined( _WIN32 )
#define WIN32_LEAN_AND_MEAN
#undef MOUSE_MOVED
#include <windows.h>
#endif // _WIN32

#if defined ( __APPLE__ ) && !defined ( __unix__ )
#define __unix__ 1
#endif

class WFindFile {
 protected:
  char szFileName[MAX_PATH];
  char szFileSpec[MAX_PATH];
  long lFileSize;
  long lTypeMask;
  unsigned char nFileType;
  bool bIsOpen;

  void __open(const char * pszFileSpec, unsigned int nTypeMask) {
    strcpy(szFileSpec, pszFileSpec);
    lTypeMask = nTypeMask;
  }

  void __close() {
    szFileName[0] = '\0';
    szFileSpec[0] = '\0';
    lFileSize = 0;
    lTypeMask = 0;
    bIsOpen = false;
  }

#if defined (_WIN32)
  WIN32_FIND_DATA ffdata;
  HANDLE  hFind;
#elif defined (__unix__)
  bool dos_flag;
  struct dirent **entries;
  int nMatches;
  int nCurrentEntry;
#else
#error "No platform specified, cannot build this __FILE__"
#endif


 public:
  WFindFile() {
    this->__close();
  }
  bool open(const char * pszFileSpec, unsigned int nTypeMask);
  bool next();
  bool close();
  virtual ~WFindFile() {
    close();
  }

  const char * GetFileName() {
    return (const char*) &szFileName;
  }
  long GetFileSize() {
    return lFileSize;
  }
  bool IsDirectory();
  bool IsFile();
};


/**
 * Bit-mapped values for what WFindFile is searching
 */
enum {
  WFINDFILE_FILES = 0x01,
  WFINDFILE_DIRS  = 0x02
};


#endif  // __INCLUDED_WFNDFILE_H__
