/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2005-2020, WWIV Software Services             */
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

#include "core/file.h"
#include "core/wwivport.h"
#include <cstdio>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

typedef std::basic_ostream<char>&(ENDL_TYPE2)(std::basic_ostream<char>&);

/**
 \class TextFile
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
    d | DOS Text mode (always use \r\n, even on *nix)
    t | Text Mode (default)
    b | Binary Mode
    + | Read and Write

 ### Example: ###
 \code{.cpp}
   TextFile f("/tmp/foo.txt", "wt");
   f.WriteLine("Hello World");
 \endcode
 */
class TextFile final {
public:
  /**
   * Constructs a TextFile.
   * Constructs a TextFile using the full path `file_name` and mode of `file_mode`.
   */
  TextFile(const std::filesystem::path& file_name, const std::string& file_mode) noexcept;

  ~TextFile() noexcept;

  /**
   * Explicitly closes an existing TextFile.
   *
   * Normally this isn't needed since it will be invoked by the destructor.
   */
  bool Close() noexcept;

  /**
   * Used to check if the file has been successfully open.
   * Usually code should use `operator bool()` vs. this method.
   */
  [[nodiscard]] bool IsOpen() const noexcept { return file_ != nullptr; }
  [[nodiscard]] bool IsEndOfFile() const noexcept { return feof(file_) != 0; }

  /** Writes a line of text without `\r\n` */
  ssize_t Write(const std::string& text) noexcept;

  /** Writes a line of text including `\r\n`. */
  ssize_t WriteLine(const char* text) noexcept { return WriteLine(std::string(text)); }

  /** Writes a line of text including `\r\n`. */
  // Add this since WriteLine(T) below will match that and this
  // explicit match keeps that from happening
  ssize_t WriteLine(char* text) noexcept { return WriteLine(std::string(text)); }

  ssize_t WriteLine() noexcept { return WriteLine(""); }

  /** Writes a char[N] representing a line of text including `\r\n`. */
  template <size_t N> ssize_t WriteLine(const char t[N]) noexcept {
    return WriteLine(std::string(t));
  }

  /**
   * Writes a T that is transformable to a string using std::to_string(T)
   * representing a line of text including `\r\n`.
   */
  template <typename T> ssize_t WriteLine(T t) noexcept { return WriteLine(std::to_string(t)); }

  /** Writes a std::string representing a line of text including `\r\n`. */
  ssize_t WriteLine(const std::string& text) noexcept;

  /** Writes a single character to a text file. */
  ssize_t WriteChar(char ch) noexcept;

  /** Writes a binary blob as binary data. */
  // ReSharper disable once CppMemberFunctionMayBeConst
  ssize_t WriteBinary(const void* buffer, ssize_t num) noexcept {
    return static_cast<int>(fwrite(buffer, num, 1, file_));
  }

  /** Reads one line of text, removing the `\r\n` in the end of the line. */
  // ReSharper disable once CppMemberFunctionMayBeConst
  [[nodiscard]] bool ReadLine(char* buffer, int nBufferSize) noexcept {
    return fgets(buffer, nBufferSize, file_) != nullptr;
  }

  /** Reads one line of text, removing the `\r\n` in the end of the line. */
  [[nodiscard]] bool ReadLine(std::string* out) noexcept;
  [[nodiscard]] wwiv::core::File::size_type position() const noexcept { return ftell(file_); }
  [[nodiscard]] const std::filesystem::path& path() const noexcept;
  [[nodiscard]] std::string full_pathname() const;
  [[nodiscard]] FILE* GetFILE() const noexcept { return file_; }

  /**
    Reads the entire contents of the file into the returned string.

    Note: The file position will be at the end of the file after returning.
   */
  [[nodiscard]] std::string ReadFileIntoString();

  /**
   * Reads up to max_lines, the contents of the file into a vector of strings.
   * 
   * Note: The file position will be at the end of the file after returning.
   * Note: max_lines defaults to max size for an int64_t.
   */
  [[nodiscard]] std::vector<std::string> ReadFileIntoVector(int64_t max_lines = std::numeric_limits<int64_t>::max());

  // operators

  /**
   * Used to check if the file has been successfully open.
   * This operator is preferred over the function IsOpen.
  */
  explicit operator bool() const { return IsOpen(); }
  friend std::ostream& operator<<(std::ostream& os, const TextFile& f);

  template <typename T> TextFile& operator<<(T const& value) {
    std::ostringstream ss;
    ss << value;
    Write(ss.str());
    return *this;
  }

  TextFile& operator<<(ENDL_TYPE2* value) {
    std::ostringstream ss;
    ss << value;
    Write(ss.str());
    return *this;
  }

private:
  const std::filesystem::path file_name_;
  mutable FILE* file_;
  bool open_{false};
  const bool dos_mode_{false};
};

#endif // __INCLUDED_WTEXTFILE_H__
