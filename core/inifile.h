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
#ifndef __INCLUDED_INIFILE_H__
#define __INCLUDED_INIFILE_H__

#include <string>
#include <map>

namespace wwiv {
namespace core {

std::string FilePath(const std::string directoryName, const std::string fileName);

class IniFile {
 public:
  IniFile(const std::string filename, const std::string primarySection, const std::string secondarySection = "");
  // Constructor/Destructor
  virtual ~IniFile(); 

  // Member functions
  void Close();
  bool IsOpen() const { return open_; }

  const char* GetValue(const char *key, const char *default_value = NULL) const;
  const long GetNumericValue(const char *key, int default_value = 0) const;
  const bool GetBooleanValue(const char *key, bool default_value = false) const;

 private:
  // This class should not be assigneable via '=' so remove the implicit operator=
  // and Copy constructor.
  IniFile(const IniFile& other) = delete;
  IniFile& operator=(const IniFile& other) = delete;

  bool Open();

  /**
   * Reads a specified value from INI file data (contained in *inidata). The
   * name of the value to read is contained in *value_name. If such a name
   * doesn't exist in this INI file subsection, then *val is NULL, else *val
   * will be set to the string value of that value name. If *val has been set
   * to something, then this function returns 1, else it returns 0.
   */
  static bool StringToBoolean(const char *p);

  const std::string file_name_;
  bool open_;
  const std::string primary_;
  const std::string secondary_;
  std::map<std::string, std::string> data_;
};

}  // namespace core
}  // namespace wwiv

#endif  // __INCLUDED_INIFILE_H__
