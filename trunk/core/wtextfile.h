#ifndef __INCLUDED_WTEXTFILE_H__
#define __INCLUDED_WTEXTFILE_H__
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

#include <cstdio>
#include <cstring>
#include <string>


class WTextFile {
public:
  WTextFile(const std::string file_name, const std::string file_mode);
  WTextFile(const std::string directory_name, const std::string file_name, const std::string file_mode);
  bool Open(const std::string file_name, const std::string file_mode);
  bool Close();
  bool IsOpen() const { return file_ != nullptr; }
  bool IsEndOfFile() { return feof(file_) ? true : false; }
  int Write(const std::string text) { return (fputs(text.c_str(), file_) >= 0) ? text.size() : 0; }
  int WriteChar(char ch) { return fputc(ch, file_); }
  int WriteFormatted(const char *pszFormatText, ...);
  int WriteBinary(const void *pBuffer, size_t nSize) {
    return (int)fwrite(pBuffer, nSize, 1, file_);
  }
  // Reads one line of text, leaving the \r\n in the end of the file.
  bool ReadLine(char *pszBuffer, int nBufferSize) {
    return (fgets(pszBuffer, nBufferSize, file_) != NULL) ? true : false;
  }
  bool ReadLine(std::string *buffer);
  long GetPosition() { return ftell(file_); }
  const std::string GetFullPathName() const { return file_name_; }

 public:
  ~WTextFile();

 private:
  std::string file_name_;
  std::string file_mode_;
  FILE* file_;
  FILE* OpenImpl();
  static const int TRIES;
  static const int WAIT_TIME;
};

#endif // __INCLUDED_WTEXTFILE_H__

