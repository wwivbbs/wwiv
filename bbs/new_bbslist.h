/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                Copyright (C)2014-2016 WWIV Software Services           */
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
#ifndef __INCLUDED_NEW_BBSLIST_H__
#define __INCLUDED_NEW_BBSLIST_H__

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <cereal/cereal.hpp>


namespace wwiv {
namespace bbslist {

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

bool LoadFromJSON(const std::string& dir, const std::string& filename,
                  std::vector<BbsListEntry>& entries);

bool SaveToJSON(const std::string& dir, const std::string& filename, 
                const std::vector<BbsListEntry>& entries);

void NewBBSList();

}  // bbslist
}  // wwiv


   // TODO - move this into it's own header somewhere.
namespace cereal {
//! Saving for std::map<std::string, std::string> for text based archives
// Note that this shows off some internal cereal traits such as EnableIf,
// which will only allow this template to be instantiated if its predicates
// are true
template <class Archive, class C, class A,
  traits::EnableIf<traits::is_text_archive<Archive>::value> = traits::sfinae> inline
  void save(Archive & ar, std::map<std::string, std::string, C, A> const & map) {
  for (const auto & i : map)
    ar(cereal::make_nvp(i.first, i.second));
}

//! Loading for std::map<std::string, std::string> for text based archives
template <class Archive, class C, class A,
  traits::EnableIf<traits::is_text_archive<Archive>::value> = traits::sfinae> inline
  void load(Archive & ar, std::map<std::string, std::string, C, A> & map) {
  map.clear();

  auto hint = map.begin();
  while (true) {
    const auto namePtr = ar.getNodeName();

    if (!namePtr)
      break;

    std::string key = namePtr;
    std::string value; ar(value);
    hint = map.emplace_hint(hint, std::move(key), std::move(value));
  }
}
} // namespace cereal



#endif  // __INCLUDED_NEW_BBSLIST_H__
