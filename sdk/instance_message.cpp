/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2021-2022, WWIV Software Services             */ 
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
/**************************************************************************/
#include "sdk/instance_message.h"

#include "bbs/instmsg.h"
#include "cereal/archives/json.hpp"
#include "core/cereal_utils.h"
#include "core/file.h"
#include "core/findfiles.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "fmt/format.h"

#include <filesystem>
#include <cereal/specialize.hpp>

using namespace wwiv::core;
using namespace wwiv::strings;

// We want to override how we store some enums as a string, not int.
// This has to be in the global namespace.
CEREAL_SPECIALIZE_FOR_ALL_ARCHIVES(wwiv::sdk::instance_message_type_t, specialization::non_member_load_save_minimal);

namespace cereal {

template <class Archive>
std::string save_minimal(Archive const&, const wwiv::sdk::instance_message_type_t& t) {
  return to_enum_string<wwiv::sdk::instance_message_type_t>(t, {"user", "system"});
}
template <class Archive>
void load_minimal(Archive const&, wwiv::sdk::instance_message_type_t& t, const std::string& s) {
  t = from_enum_string<wwiv::sdk::instance_message_type_t>(s, {"user", "system"});
}

template <class Archive> void serialize(Archive& ar, wwiv::sdk::instance_message_t& n) {
  SERIALIZE(n, message_type);
  SERIALIZE(n, from_instance);
  SERIALIZE(n, from_user);
  SERIALIZE(n, dest_inst);
  SERIALIZE(n, daten);
  SERIALIZE(n, message);
}
}

namespace wwiv::sdk {

static std::optional<File> create_file(const std::filesystem::path& path, const std::string& fn_pattern) {
  for (auto i = 0; i < 1000; i++) {
    File file(FilePath(path, fmt::format(fn_pattern, i)));
    if (file.Open(File::modeText | File::modeReadWrite | File::modeCreateFile | File::modeExclusive, File::shareDenyReadWrite)) {
      return file;
    }
  }
  return std::nullopt;
}

bool send_instance_message(const Config& config, instance_message_t& msg) {
  const auto scratch = config.scratch_dir(msg.dest_inst);
  if (auto o = create_file(scratch, "msg{}.json")) {
    std::ostringstream ss;
    try {
      {
        cereal::JSONOutputArchive ar(ss);
        serialize(ar, msg);
      }
      o.value().Write(ss.str());
    } catch (const cereal::RapidJSONException& e) {
      LOG(ERROR) << "Caught cereal::RapidJSONException: " << e.what();
      return false;
    }
  }
  return false;
}

std::filesystem::path instance_message_filespec(const Config& config, int instance_num) {
  const auto scratch = config.scratch_dir(instance_num);
  return FilePath(scratch, "msg*.json");
}

bool send_instance_string(const Config& config, instance_message_type_t t, int dest_instance,
    int from_user, int from_instance, const std::string& text) {
  instance_message_t m{};
  m.message_type = t;
  m.dest_inst = dest_instance;
  m.daten = DateTime::now().to_daten_t();
  m.from_instance = from_instance;
  m.from_user = from_user;
  m.message = text;

  return send_instance_message(config, m);
}

std::vector<instance_message_t> read_all_instance_messages(const Config& config, int instance_num, int limit) {
  const auto fndspec = instance_message_filespec(config, instance_num);
  FindFiles ff(fndspec, FindFiles::FindFilesType::files);
  auto current = 0;
  std::vector<instance_message_t> out;
  for (const auto& f : ff) {
    TextFile tf(FilePath(config.scratch_dir(instance_num), f.name), "rt");
    if (!tf) {
      continue;
    }
    auto s = tf.ReadFileIntoString();
    std::stringstream ss(s);
    tf.Close();
    if (!File::Remove(tf.full_pathname())) {
      VLOG(1) << "Failed to delete instance message: " << tf.full_pathname();
    }
    instance_message_t msg;
    try {
      cereal::JSONInputArchive ar(ss);
      serialize(ar, msg);
    } catch (const cereal::RapidJSONException& e) {
      LOG(ERROR) << "Exception parsing: " << e.what();
      LOG(ERROR) << "FileName: " << tf.full_pathname();
      LOG(ERROR) << "Text: " << s;
    }
    out.emplace_back(msg);
    if (++current > limit) {
      VLOG(1) << "Hit limit, ending early";
      break;
    }
  }

  return out;
}

}
