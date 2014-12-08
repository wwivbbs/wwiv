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
#ifndef __INCLUDED_WTEXTFILE_H__
#define __INCLUDED_WTEXTFILE_H__

#include <cstdio>
#include <cstring>
#include <string>


class TextFile {
public:
  TextFile(const std::string& file_name, const std::string& file_mode);
  TextFile(const std::string& directory_name, const std::string& file_name, const std::string& file_mode);
  bool Open(const std::string& file_name, const std::string& file_mode);
  bool Close();

  bool IsOpen() const { return file_ != nullptr; }
  bool IsEndOfFile() { return feof(file_) ? true : false; }

  // Writes a line of text without \r\n
  int Write(const std::string& text) { return (fputs(text.c_str(), file_) >= 0) ? text.size() : 0; }
  
  // Writes a line of text including \r\n
  int WriteLine(const std::string& text);
  
  // Writes a single character to a text file.
  int WriteChar(char ch) { return fputc(ch, file_); }
  
  // Writes a line of pszFormatText like printf
  int WriteFormatted(const char *pszFormatText, ...);
  
  // Writes a binary blob as binary data
  int WriteBinary(const void *pBuffer, size_t nSize) {
    return (int)fwrite(pBuffer, nSize, 1, file_);
  }
  
  // Reads one line of text, removing the \r\n in the end of the line.
  bool ReadLine(char *pszBuffer, int nBufferSize) {
    return (fgets(pszBuffer, nBufferSize, file_) != nullptr) ? true : false;
  }

  // Reads one line of text, removing the \r\n in the end of the line.
  bool ReadLine(std::string *buffer);
  long GetPosition() { return ftell(file_); }
  const std::string full_pathname() const { return file_name_; }
  FILE* GetFILE() { return file_; } 

  // Reads the entire contents of the file into the returned string.
  // The file position will be at the end of the file after returning.
  std::string ReadFileIntoString();

 public:
  ~TextFile();

 private:
  std::string file_name_;
  std::string file_mode_;
  FILE* file_;
  FILE* OpenImpl();
  static const int TRIES;
  static const int WAIT_TIME;
};

#endif // __INCLUDED_WTEXTFILE_H__

