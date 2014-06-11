/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)2005-2014, WWIV Software Services             */
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
#include <iostream>
#include <string>
#include <cstring>
#include <stdarg.h>

#include "wwiv.h"
#include "wtextfile.h"

WTextFile::WTextFile(const std::string fileName, const std::string fileMode) {
  Open(fileName, fileMode);
}


WTextFile::WTextFile(const std::string directoryName, const std::string fileName, const std::string fileMode) {
  std::string fullPathName(directoryName);
  fullPathName.append(fileName);
  Open(fullPathName, fileMode);
}


bool WTextFile::Open(const std::string fileName, const std::string fileMode) {
  m_fileName = fileName;
  m_fileMode = fileMode;
  m_hFile = WTextFile::OpenImpl(m_fileName.c_str(), m_fileMode.c_str());
  return (m_hFile != NULL) ? true : false;
}



bool WTextFile::Close() {
  if (m_hFile != NULL) {
    fclose(m_hFile);
    m_hFile = NULL;
    return true;
  }
  return false;
}


int WTextFile::WriteFormatted(const char *pszFormatText, ...) {
  va_list ap;
  char szBuffer[ 4096 ];

  va_start(ap, pszFormatText);
  WWIV_VSNPRINTF(szBuffer, sizeof(szBuffer), pszFormatText, ap);
  va_end(ap);
  return Write(szBuffer);
}


bool WTextFile::ReadLine(std::string &buffer) {
  char szBuffer[4096];
  char *pszBuffer = fgets(szBuffer, sizeof(szBuffer), m_hFile);
  buffer = szBuffer;
  return (pszBuffer != NULL);
}


WTextFile::~WTextFile() {
  Close();
}
