/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                 Copyright (C)2016, WWIV Software Services              */
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
#ifndef __INCLUDED_JSONFILE_H__
#define __INCLUDED_JSONFILE_H__

#include <cstdio>
#include <cstring>
#include <string>

//#include <cereal/types/vector.hpp>
//#include <cereal/types/memory.hpp>
//#include <cereal/archives/json.hpp>

#include "core/textfile.h"

namespace wwiv {
namespace core {

template <typename T>
class JsonFile {
public:
  JsonFile(const std::string& file_name, const std::string& key, T& t): file_name_(file_name), key_(key), t_(t) {}
  JsonFile(const std::string& directory_name, const std::string& file_name, const std::string& key, T& t)
    : JsonFile(FilePath(directory_name, file_name), key, t) {};
  virtual ~JsonFile() {}

  bool Load() {
    TextFile file(file_name_, "r");
    if (!file.IsOpen()) {
      return false;
    }
    std::string text = file.ReadFileIntoString();
    std::stringstream ss;
    ss << text;
    cereal::JSONInputArchive load(ss);
    load(cereal::make_nvp(key_, t_));
    return true;
  }
  bool Save() { 
    std::ostringstream ss;
    {
      cereal::JSONOutputArchive save(ss);
      save(cereal::make_nvp(key_, t_));
    }

    TextFile file(file_name_, "w");
    if (!file.IsOpen()) {
      // rapidjson will assert if the file does not exist, so we need to 
      // verify that the file exists first.
      return false;
    }

    file.Write(ss.str());
    return true;
  }


 private:
   const std::string file_name_;
   const std::string key_;
   T& t_;
};

}
}

#endif // __INCLUDED_JSONFILE_H__

