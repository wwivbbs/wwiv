/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*              Copyright (C)2014-2021, WWIV Software Services            */
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
#ifndef INCLUDED_COMMON_PRINTFILE_H
#define INCLUDED_COMMON_PRINTFILE_H

#include "common/output.h"
#include <string>
#include <filesystem>
#include <vector>

namespace wwiv::common {

/**
 * Creates the best fully qualified filename to display including support for embedding
 * column number into the path as basename.COLUMNS.extension.
 */
std::filesystem::path CreateFullPathToPrintWithCols(const std::filesystem::path& filename,
                                                    int screen_length);

 /**
  * Creates the fully qualified filename to display adding extensions and directories as needed.
  */
std::filesystem::path CreateFullPathToPrint(const std::vector<std::filesystem::path>& dirs,
                                            const sdk::User& user,
                                            const std::string& basename);

} // namespace wwiv::common

#endif