/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2019, WWIV Software Services             */
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

#ifndef __INCLUDED_FINDFILES_H__
#define __INCLUDED_FINDFILES_H__

#include <cstring>
#include <string>
#include <vector>
#include "core/filesystem.h"

namespace wwiv {
namespace core {

enum class FindFilesType { directories, files, any };

struct FileEntry {
  const std::string name;
  long size;
};

class FindFiles {
public:
  typedef std::vector<FileEntry>::iterator iterator;
  typedef std::vector<FileEntry>::const_iterator const_iterator;

  FindFiles(const std::filesystem::path& mask, const FindFilesType type);

  iterator begin() { return entries_.begin(); }
  const_iterator begin() const { return entries_.begin(); }
  const_iterator cbegin() const { return entries_.cbegin(); }
  iterator end() { return entries_.end(); }
  const_iterator end() const { return entries_.end(); }
  const_iterator cend() const { return entries_.cend(); }
  bool empty() const { return entries_.empty(); }

private:
  std::vector<FileEntry> entries_; //we wish to iterate over this
                            //container implementation code

};

}
}


#endif  // __INCLUDED_FINDFILES_H__
