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
#include "core/wfile.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#ifdef _WIN32
#include <io.h>
#include <share.h>
#endif  // _WIN32
#include <sstream>
#include <string>
#include <sys/stat.h>
#ifndef _WIN32
#include <sys/file.h>
#include <unistd.h>
#endif  // _WIN32

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef CopyFile
#undef GetFileTime
#undef GetFullPathName
#undef MoveFile
#endif  // _WIN32

#include "core/wfndfile.h"
#include "core/wwivassert.h"

#if defined( __APPLE__ )
#if !defined( O_BINARY )
#define O_BINARY 0
#endif
#endif

/////////////////////////////////////////////////////////////////////////////
// Constants

const int WFile::modeUnknown        = -1;
const int WFile::shareUnknown       = -1;
const int WFile::permUnknown        = -1;

const int WFile::seekBegin          = SEEK_SET;
const int WFile::seekCurrent        = SEEK_CUR;
const int WFile::seekEnd            = SEEK_END;

const int WFile::invalid_handle     = -1;

WLogger*  WFile::m_pLogger;
int       WFile::m_nDebugLevel;

// WAIT_TIME is 10 seconds
#define WAIT_TIME 10
#define TRIES 100

/////////////////////////////////////////////////////////////////////////////
// Constructors/Destructors

WFile::WFile() {
  init();
}

WFile::WFile(const std::string fileName) {
  init();
  this->SetName(fileName);
}

WFile::WFile(const std::string dirName, const std::string fileName) {
  init();
  this->SetName(dirName, fileName);
}

void WFile::init() {
  m_bOpen                 = false;
  m_hFile                 = WFile::invalid_handle;
  memset(m_szFileName, 0, MAX_PATH + 1);
}

WFile::~WFile() {
  if (this->IsOpen()) {
    this->Close();
  }
}

/////////////////////////////////////////////////////////////////////////////
// Member functions

bool WFile::SetName(const std::string fileName) {
  strncpy(m_szFileName, fileName.c_str(), MAX_PATH);
  return true;
}

bool WFile::SetName(const std::string dirName, const std::string fileName) {
  std::stringstream fullPathName;
  fullPathName << dirName;
  if (!dirName.empty() && dirName[ dirName.length() - 1 ] == pathSeparatorChar) {
    fullPathName << fileName;
  } else {
    fullPathName << pathSeparatorChar << fileName;
  }
  return SetName(fullPathName.str());
}

int WFile::Read(void * pBuffer, size_t nCount) {
  int ret = read(m_hFile, pBuffer, nCount);
  if (ret == -1) {
    std::cout << "[DEBUG: errno: " << errno << " -- Please screen capture this and email to Rushfan]\r\n";
  }
  // TODO: Make this an WWIV_ASSERT once we get rid of these issues
  return ret;
}

int WFile::Write(const void * pBuffer, size_t nCount) {
  int nRet = _write(m_hFile, pBuffer, nCount);
  if (nRet == -1) {
    std::cout << "[DEBUG: errno: " << errno << " -- Please screen capture this and email to Rushfan]\r\n";
  }
  // TODO: Make this an WWIV_ASSERT once we get rid of these issues
  return nRet;
}

long WFile::Seek(long lOffset, int nFrom) {
  WWIV_ASSERT(nFrom == WFile::seekBegin || nFrom == WFile::seekCurrent || nFrom == WFile::seekEnd);
  WWIV_ASSERT(WFile::IsFileHandleValid(m_hFile));

  return lseek(m_hFile, lOffset, nFrom);
}

bool WFile::Exists() const {
  return WFile::Exists(m_szFileName);
}

bool WFile::Delete() {
  if (this->IsOpen()) {
    this->Close();
  }
  return (unlink(m_szFileName) == 0) ? true : false;
}

bool WFile::IsFile() {
  return !this->IsDirectory();
}

bool WFile::SetFilePermissions(int nPermissions) {
  return (chmod(m_szFileName, nPermissions) == 0) ? true : false;
}

/////////////////////////////////////////////////////////////////////////////
// Static functions

bool WFile::Remove(const std::string fileName) {
  return (unlink(fileName.c_str()) ? false : true);
}

bool WFile::Remove(const std::string directoryName, const std::string fileName) {
  std::string strFullFileName = directoryName;
  strFullFileName += fileName;
  return WFile::Remove(strFullFileName);
}

bool WFile::Exists(const std::string directoryName, const std::string fileName) {
  std::stringstream fullPathName;
  if (!directoryName.empty() && directoryName[directoryName.length() - 1] == pathSeparatorChar) {
    fullPathName << directoryName << fileName;
  } else {
    fullPathName << directoryName << pathSeparatorChar << fileName;
  }
  return Exists(fullPathName.str());
}

bool WFile::ExistsWildcard(const std::string wildCard) {
  WFindFile fnd;
  return (fnd.open(wildCard.c_str(), 0));
}

bool WFile::SetFilePermissions(const std::string fileName, int nPermissions) {
  WWIV_ASSERT(!fileName.empty());
  return (chmod(fileName.c_str(), nPermissions) == 0) ? true : false;
}

bool WFile::IsFileHandleValid(int hFile) {
  return (hFile != WFile::invalid_handle) ? true : false;
}
