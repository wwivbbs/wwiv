/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2022, WWIV Software Services             */
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
#ifndef INCLUDED_SDK_CONF_H
#define INCLUDED_SDK_CONF_H

#include "sdk/key.h"
#include "sdk/subxtr.h"
#include "sdk/files/dirs.h"
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace wwiv::sdk {
class Config;

typedef uint16_t subconf_t;

static constexpr int CONF_UPDATE_INSERT = 1;
static constexpr int CONF_UPDATE_DELETE = 2;
static constexpr int CONF_UPDATE_SWAP = 3;

enum class ConferenceType { CONF_SUBS, CONF_DIRS };

/** 
 * Note: This isn't used on disk 
 */
struct confrec_430_t {
  // A to Z?
  uint8_t key;
  // Name of conference                                          
  std::string conf_name;
  // Minimum SL needed for access
  uint8_t minsl;
  // Maximum SL allowed for access
  uint8_t maxsl;
  // Minimum DSL needed for access
  uint8_t mindsl;
  // Maximum DSL allowed for access
  uint8_t maxdsl;
  // Minimum age needed for access
  uint8_t minage;
  // Maximum age allowed for access
  uint8_t maxage;
  // Gender: 0=male, 1=female 2=all
  uint8_t sex;
  // Bit-mapped stuff
  uint16_t status;
  // Minimum bps rate for access
  uint16_t minbps;
  // ARs necessary for access
  uint16_t ar;
  // DARs necessary for access
  uint16_t dar;
  // Num "subs" in this conference
  uint16_t num;
  // max num subs allocated in 'subs'
  uint16_t maxnum;
  // "Sub" numbers in the conference
  std::set<subconf_t> subs;
};


struct conference_t {
  // A to Z?
  key_t key;
  // Name of conference                                          
  std::string conf_name;
  // ACS needed for access
  std::string acs;
};
inline bool operator< (const conference_t &c, const conference_t & c2) { return c.key < c2.key; }
inline bool operator< (const conference_t &c, const char& key) { return c.key < key; }
inline bool operator< (const char& key, const conference_t &c) { return key < c.key; }


struct conference_file_t {
  std::set<conference_t> subs;
  std::set<conference_t> dirs;
};

class Conference final {
public:
  Conference(ConferenceType type, const std::set<conference_t>& confs);
  ~Conference() = default;

  [[nodiscard]] const conference_t& conf(char key) const;
  [[nodiscard]] std::optional<conference_t> try_conf(char key) const;
  conference_t& conf(char key);
  conference_t& front();
  [[nodiscard]] bool exists(char key) const;

  [[nodiscard]] std::set<conference_t> confs() const;
  [[nodiscard]] std::string keys_string() const;
  bool add(conference_t r);
  bool erase(char key);
  [[nodiscard]] int size() const { return stl::size_int(confs_); }
  [[nodiscard]] bool empty() const { return stl::size_int(confs_) == 0; }

  [[nodiscard]] ConferenceType type() const noexcept { return type_; }

private:
  const ConferenceType type_;
  std::map<char, conference_t> confs_;
};

class Conferences final {
public:
  Conferences(const std::filesystem::path& datadir, Subs& subs, files::Dirs& dirs, int max_backups = 0);
  ~Conferences() = default;

  [[nodiscard]] std::optional<conference_file_t> Load() const;
  bool LoadFromFile(const conference_file_t&);
  bool Save();

  [[nodiscard]] Conference& subs_conf() const { return *subs_conf_; }
  [[nodiscard]] Conference& dirs_conf() const { return *dirs_conf_; }

private:
  const std::filesystem::path datadir_;
  Subs& subs_;
  files::Dirs& dirs_;
  const int max_backups_;
  std::unique_ptr<Conference> subs_conf_;
  std::unique_ptr<Conference> dirs_conf_;
};

conference_file_t UpgradeConferences(const Config& config, Subs& subs, files::Dirs& dirs);

std::vector<confrec_430_t> read_conferences_430(const std::filesystem::path& path);

}

#endif
