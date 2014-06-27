/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
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
#include "ini.h"
#include "wtextfile.h"

#include "wwiv.h"

using namespace wwiv::strings;


WIniFile::WIniFile(const std::string fileName) {
  m_strFileName = fileName;
  m_bOpen = false;
  memset(&m_primarySection, 0, sizeof(m_primarySection));
  memset(&m_secondarySection, 0, sizeof(m_secondarySection));
}

WIniFile::~WIniFile() {
  if (m_bOpen) {
    Close();
  }
}


bool WIniFile::Open(const std::string primarySection, const std::string secondarySection) {
  std::stringstream iniFileStream;
  iniFileStream << GetApplication()->GetHomeDir() << m_strFileName.c_str();

  // first, zap anything there currently
  if (m_bOpen) {
    //TODO ASSERT here, this should not happen
    Close();
  }

  // read in primary section
  char* pBuffer = ReadFile(iniFileStream.str(), primarySection);

  if (pBuffer) {
    // parse the data
    Parse(pBuffer, &m_primarySection);

    // read in secondary file
    pBuffer = ReadFile(iniFileStream.str(), secondarySection);
    if (pBuffer) {
      Parse(pBuffer, &m_secondarySection);
    }

  } else {
    // read in the secondary section, as the primary one
    pBuffer = ReadFile(iniFileStream.str(), secondarySection);
    if (pBuffer) {
      Parse(pBuffer, &m_primarySection);
    }
  }

  m_bOpen = (m_primarySection.pIniSectionBuffer) ? true : false;
  return m_bOpen;
}


bool WIniFile::Close() {
  m_bOpen = false;
  if (m_primarySection.pIniSectionBuffer) {
    free(m_primarySection.pIniSectionBuffer);
    if (m_primarySection.pKeyArray) {
      free(m_primarySection.pKeyArray);
    }
    if (m_primarySection.pValueArray) {
      free(m_primarySection.pValueArray);
    }
  }
  memset(&m_primarySection, 0, sizeof(m_primarySection));
  if (m_secondarySection.pIniSectionBuffer) {
    free(m_secondarySection.pIniSectionBuffer);
    if (m_secondarySection.pKeyArray) {
      free(m_secondarySection.pKeyArray);
    }
    if (m_secondarySection.pValueArray) {
      free(m_secondarySection.pValueArray);
    }
  }
  memset(&m_secondarySection, 0, sizeof(m_secondarySection));
  return true;
}


const char* WIniFile::GetValue(const char *pszKey, const char *pszDefaultValue) {
  if (!m_primarySection.pIniSectionBuffer || !pszKey || !(*pszKey)) {
    return pszDefaultValue;
  }

  // loop through both sets of data and search them, in order
  for (int i = 0; i <= 1; i++) {
    ini_info_type *pIniSection;
    // get pointer to data area to use
    if (i == 0) {
      pIniSection = &m_primarySection;
    } else if (m_secondarySection.pIniSectionBuffer) {
      pIniSection = &m_secondarySection;
    } else {
      break;
    }

    // search for it
    for (int i1 = 0; i1 < pIniSection->nIndex; i1++) {
      if (IsEqualsIgnoreCase(pIniSection->pKeyArray[ i1 ], pszKey)) {
        return pIniSection->pValueArray[ i1 ];
      }
    }
  }

  // nothing found
  return pszDefaultValue;
}


const bool WIniFile::GetBooleanValue(const char *pszKey, bool defaultValue) {
  const char *pszValue = GetValue(pszKey);
  return (pszValue != NULL) ? WIniFile::StringToBoolean(pszValue) : defaultValue;
}


bool WIniFile::StringToBoolean(const char *p) {
  if (!p) {
    return false;
  }
  char ch = wwiv::UpperCase<char>(*p);
  return (ch == 'Y' || ch == 'T' || ch == '1') ? true : false;
}


const long WIniFile::GetNumericValue(const char *pszKey, int defaultValue) {
  const char *pszValue = GetValue(pszKey);
  return (pszValue != NULL) ? atoi(pszValue) : defaultValue;
}


char *WIniFile::ReadSectionIntoMemory(const std::string &fileName, long begin, long end) {
  WFile file(fileName);
  if (file.Open(WFile::modeReadOnly | WFile::modeBinary)) {
    char *ss = static_cast<char *>(malloc(end - begin + 2));
    if (ss) {
      file.Seek(begin, WFile::seekBegin);
      file.Read(ss, (end - begin + 1));
      ss[(end - begin + 1)] = '\0';
      return ss;
    }
  }
  return NULL;
}


void WIniFile::FindSubsectionArea(const std::string& fileName, const std::string& section, long *begin, long *end) {
  char s[255];

  *begin = *end = -1L;
  std::stringstream ss;
  ss << "[" << section << "]";
  std::string header = ss.str();

  WTextFile file(fileName, "rt");
  if (!file.IsOpen()) {
    return;
  }

  long pos = 0L;

  while (file.ReadLine(s, sizeof(s) - 1)) {
    // Get rid of CR/LF at end
    char *ss = strchr(s, '\n');
    if (ss) {
      *ss = '\0';
    }

    // Get rid of trailing/leading spaces
    StringTrim(s);

    // A comment or blank line?
    if (s[0] && s[0] != ';') {
      // Is it a subsection header?
      if ((strlen(s) > 2) && (s[0] == '[') && (s[strlen(s) - 1] == ']')) {
        // Does it match requested subsection name (section)?
        if (header == s) {   //if ( WWIV_STRNICMP(&s[0], &szTempHeader[0], strlen( szTempHeader ) ) == 0 )
          if (*begin == -1L) {
            *begin = file.GetPosition();
          }
        } else {
          if (*begin != -1L) {
            if (*end == -1L) {
              *end = pos - 1;
              break;
            }
          }
        }
      }
    }
    // Update file position pointer
    pos = file.GetPosition();
  }

  // Mark end as end of the file if not already found
  if (*begin != -1L && *end == -1L) {
    *end = file.GetPosition() - 1;
  }

  file.Close();
}


char *WIniFile::ReadFile(const std::string fileName, const std::string header) {
  // Header must be "valid", and file must exist
  if (header.empty() || !WFile::Exists(fileName)) {
    return NULL;
  }

  // Get area to read in
  long beginloc = -1L, endloc = -1L;
  FindSubsectionArea(fileName, header, &beginloc, &endloc);

  // Validate
  if (beginloc >= endloc) {
    return NULL;
  }

  // Allocate pointer to hold data
  char* ss = ReadSectionIntoMemory(fileName, beginloc, endloc);
  return ss;
}


void WIniFile::Parse(char *pBuffer, ini_info_type * pIniSection) {
  char *ss1, *ss, *ss2;
  unsigned int count = 0;

  memset(pIniSection, 0, sizeof(ini_info_type));
  pIniSection->pIniSectionBuffer = pBuffer;

  // first, count # pszKey-pValueArray pairs
  unsigned int i1 = strlen(pBuffer);
  char* tempb = static_cast<char *>(malloc(i1 + 20));
  if (!tempb) {
    return;
  }

  memmove(tempb, pBuffer, i1);
  tempb[i1] = 0;

  for (ss = strtok(tempb, "\r\n"); ss; ss = strtok(NULL, "\r\n")) {
    StringTrim(ss);
    if (ss[0] == 0 || ss[0] == ';') {
      continue;
    }

    ss1 = strchr(ss, '=');
    if (ss1) {
      *ss1 = 0;
      StringTrim(ss);
      if (*ss) {
        count++;
      }
    }
  }

  free(tempb);

  if (!count) {
    return;
  }

  // now, allocate space for pKeyArray-pValueArray pairs
  pIniSection->pKeyArray = static_cast<char **>(malloc(count * sizeof(char *)));
  if (!pIniSection->pKeyArray) {
    return;
  }
  pIniSection->pValueArray = static_cast<char **>(malloc(count * sizeof(char *)));
  if (!pIniSection->pValueArray) {
    free(pIniSection->pKeyArray);
    pIniSection->pKeyArray = NULL;
    return;
  }
  // go through and add in pszKey-pValueArray pairs
  for (ss = strtok(pBuffer, "\r\n"); ss; ss = strtok(NULL, "\r\n")) {
    StringTrim(ss);
    if (ss[0] == 0 || ss[0] == ';') {
      continue;
    }

    ss1 = strchr(ss, '=');
    if (ss1) {
      *ss1 = 0;
      StringTrim(ss);
      if (*ss) {
        ss1++;
        ss2 = ss1;
        while ((ss2[0]) && (ss2[1]) && ((ss2 = strchr(ss2 + 1, ';')) != NULL)) {
          if (isspace(*(ss2 - 1))) {
            *ss2 = 0;
            break;
          }
        }
        StringTrim(ss1);
        pIniSection->pKeyArray[pIniSection->nIndex] = ss;
        pIniSection->pValueArray[pIniSection->nIndex] = ss1;
        pIniSection->nIndex++;
      }
    }
  }
}


