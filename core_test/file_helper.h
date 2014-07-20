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
#ifndef __INCLUDED_FILE_HELPER_H__
#define __INCLUDED_FILE_HELPER_H__

#include <string>

/**
 * Helper class for tests requing local filesystem access.  
 *
 * Note: This class can not use WFile since it is used by the tests for WFile.
 */
class FileHelper {
public:
    FileHelper();
    // Returns a fully qualified path name to "name" under the temporary directory.
    const std::string DirName(const std::string& name) const;
    // Creates a directory.
    bool Mkdir(const std::string& name) const;
    std::string CreateTempFile(const std::string& name, const std::string& contents);
    const std::string& TempDir() const { return tmp_; }
    const std::string ReadFile(const std::string name) const;
private:
    static std::string CreateTempDir();
    std::string tmp_;
};

#endif // __INCLUDED_FILE_HELPER_H__