/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2005-2016, WWIV Software Services             */
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
#include <type_traits>

/**
 \class TextFile textfile.h "core/textfile.h"
 \brief A class that represents a text file.

 Used to read and write text files.

 When creating a `TextFile` You should specify the following
 subset of the POSIX file modes. 

 - - -
 ### File Modes ###
 mode | Description
 -----|-----------------------------
    r | Read
    w | Write, Truncate if exists
    a | Append/Write.
    t | Text Mode (default)
    b | Binary Mode
    + | Read and Write

 ### Example: ###
 \code{.cpp}
   TextFile f("/tmp/foo.txt", "wt");
   f.WriteLine("Hello World");
 \endcode
 */
class TextFile {
public:
  /** 
   * Constructs a TextFile.
   * Constructs a TextFile using the full path `file_name` and mode of `file_mode`.
   */
  TextFile(const std::string& file_name, const std::string& file_mode);
  /**
   * Constructs a TextFile.
   * Constructs a TextFile using the directory `directory_name`, 
   * file name`file_name` and mode of `file_mode`.
   */
  TextFile(const std::string& directory_name, const std::string& file_name, const std::string& file_mode);

  ~TextFile();

  /**
   * Explicitly closes an existing TextFile.
   *
   * Normally this isn't needed since it will be invoked by the destructor.
   */
  bool Close();

  /** 
   Used to check if the file has been sucessfully open. 
   
   Usually code should use `operator bool()` vs. this method.
   */
  bool IsOpen() const { return file_ != nullptr; }
  bool IsEndOfFile() { return feof(file_) ? true : false; }

  /** Writes a line of text without `\r\n` */
  int Write(const std::string& text) { return (fputs(text.c_str(), file_) >= 0) ? text.size() : 0; }
  
  /** Writes a line of text including `\r\n`. */
  int WriteLine(const char* text) {
    return WriteLine(std::string(text));
  }

  /** Writes a line of text including `\r\n`. */
  int WriteLine(char* text) {
    return WriteLine(std::string(text));
  }

  template<size_t N>
  int WriteLine(const char t[N]) {
    return WriteLine(std::string(t));
  }

  template<typename T>
  int WriteLine(T t) {
    return WriteLine(std::to_string(t));
  }

  int WriteLine(const std::string& text);

  /** Writes a single character to a text file. */
  int WriteChar(char ch) { return fputc(ch, file_); }
  
  /** Writes a line of formatText like printf. */
  int WriteFormatted(const char *formatText, ...);
  
  /** Writes a binary blob as binary data. */
  int WriteBinary(const void *pBuffer, size_t nSize) {
    return (int)fwrite(pBuffer, nSize, 1, file_);
  }
  
  /** Reads one line of text, removing the `\r\n` in the end of the line. */
  bool ReadLine(char *buffer, int nBufferSize) {
    return (fgets(buffer, nBufferSize, file_) != nullptr) ? true : false;
  }

  /** Reads one line of text, removing the `\r\n` in the end of the line. */
  bool ReadLine(std::string *buffer);
  long position() { return ftell(file_); }
  const std::string full_pathname() const { return file_name_; }
  FILE* GetFILE() { return file_; } 

  /**
    Reads the entire contents of the file into the returned string.

    Note: The file position will be at the end of the file after returning.
   */
  std::string ReadFileIntoString();

  // operators

  /**
  Used to check if the file has been sucessfully open.

  Usually code should use `operator bool()` vs. this method.
  */
  explicit operator bool() const { return IsOpen(); }
  friend std::ostream& operator<< (std::ostream &os, const TextFile &f);

private:
  /**
  * Opens an existing TextFile instance with a new `file_mode`.
  *
  * Normally this isn't needed since the constructor opens the file.
  */
  bool Open(const std::string& file_name, const std::string& file_mode);

 private:
  std::string file_name_;
  std::string file_mode_;
  FILE* file_;
  FILE* OpenImpl();
  static const int TRIES;
  static const int WAIT_TIME;
};

#endif // __INCLUDED_WTEXTFILE_H__

