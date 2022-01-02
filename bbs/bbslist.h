/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                Copyright (C)2014-2022, WWIV Software Services          */
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
#ifndef INCLUDED_BBS_BBSLIST_H
#define INCLUDED_BBS_BBSLIST_H

#include <filesystem>
#include <string>
#include <vector>

namespace wwiv::bbslist {

struct BbsListAddress {
  std::string type;
  std::string address;
};

struct BbsListEntry {
  int id;
  std::string name;
  std::string software;
  std::string sysop_name;
  std::string location;
  std::vector<BbsListAddress> addresses;
};

bool LoadFromJSON(const std::filesystem::path& dir, const std::string& filename,
                  std::vector<BbsListEntry>& entries);

bool SaveToJSON(const std::filesystem::path& dir, const std::string& filename, 
                const std::vector<BbsListEntry>& entries);

void read_bbslist();
void delete_bbslist();
void add_bbslist();
void BBSList();

}  // namespace


#endif
