/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*                Copyright (C)2019, WWIV Software Services               */
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
#ifndef __INCLUDED_SDK_CHAINS_H__
#define __INCLUDED_SDK_CHAINS_H__

#include "sdk/config.h"
#include "sdk/vardec.h"
#include <initializer_list>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace wwiv {
namespace sdk {

enum class chain_exec_mode_t : uint8_t { none = 0, dos, fossil, stdio };
enum class chain_exec_dir_t : uint8_t { bbs = 0, temp };

chain_exec_mode_t& operator++(chain_exec_mode_t&);
chain_exec_mode_t operator++(chain_exec_mode_t&, int);
chain_exec_dir_t& operator++(chain_exec_dir_t&);
chain_exec_dir_t operator++(chain_exec_dir_t&, int);

// DATA FOR OTHER PROGRAMS AVAILABLE
struct chain_t {
  std::string filename;
  std::string description;
  chain_exec_mode_t exec_mode;
  chain_exec_dir_t dir;
  bool ansi;
  bool local_only;
  bool multi_user;

  uint8_t sl;
  // AR restriction
  uint16_t ar;
  // who registered
  std::set<int16_t> regby;
  // number of runs
  uint16_t usage;
  // minimum age necessary
  uint8_t minage;
  // maximum age allowed
  uint8_t maxage;
};

class Chains {
public:
  typedef int size_type;
  static const size_type npos = -1;
  explicit Chains(const Config& config);
  // [[ VisibleForTesting ]]
  explicit Chains(std::initializer_list<chain_t> l) : chains_(l) {}
  Chains() {}
  virtual ~Chains();

  bool IsInitialized() const { return initialized_; }
  const std::vector<chain_t>& chains() const { return chains_; }
  const chain_t& at(size_type num) const { return chains_.at(num); }
  chain_t& at(size_type num) { return chains_.at(num); }

  chain_t& operator[](size_type num) { return at(num); }
  const chain_t& operator[](int num) const { return at(num); }

  bool insert(std::size_t n, chain_t r);
  bool erase(std::size_t n);
  bool Load();
  bool Save();

  static uint8_t to_ansir(chain_t c);
  static std::string exec_mode_to_string(const wwiv::sdk::chain_exec_mode_t& t);
  static wwiv::sdk::chain_exec_mode_t exec_mode_from_string(const std::string& s);

  /**
   * Is at least one chain registered or sponsored by someone?  If so return
   * true, false otherwise.
   */
  bool HasRegisteredChains() const;

  /** 
   * Increments usage of chain number "num" by one.
   */
  void increment_chain_usage(int num);

private:
  bool LoadFromJSON();
  bool LoadFromDat();
  bool SaveToJSON();
  bool SaveToDat();

  bool initialized_{false};
  std::string datadir_;
  std::vector<chain_t> chains_;
};

} // namespace sdk
} // namespace wwiv

#endif // __INCLUDED_SDK_CHAINS_H__
