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
#ifndef INCLUDED_SDK_CHAINS_H
#define INCLUDED_SDK_CHAINS_H

#include "core/wwivport.h"
#include "sdk/config.h"
#include <initializer_list>
#include <set>
#include <string>
#include <vector>

namespace wwiv::sdk {

enum class chain_exec_mode_t : uint8_t { none = 0, dos, fossil, stdio, netfoss };
enum class chain_exec_dir_t : uint8_t { bbs = 0, temp };

chain_exec_mode_t& operator++(chain_exec_mode_t&);
chain_exec_mode_t operator++(chain_exec_mode_t&, int);
chain_exec_dir_t& operator++(chain_exec_dir_t&);
chain_exec_dir_t operator++(chain_exec_dir_t&, int);

// DATA FOR OTHER PROGRAMS AVAILABLE (5.5 format)
struct chain_55_t {
  std::string filename;
  std::string description;
  chain_exec_mode_t exec_mode{chain_exec_mode_t::none};
  chain_exec_dir_t dir{chain_exec_dir_t::bbs};
  bool ansi{false};
  bool local_only{false};
  bool multi_user{false};

  uint8_t sl{0};
  // AR restriction
  uint16_t ar{0};
  // who registered
  std::set<int16_t> regby;
  // number of runs
  uint16_t usage{0};
  // minimum age necessary
  uint8_t minage{0};
  // maximum age allowed
  uint8_t maxage{0};
};

// 5.6 format chain record.
struct chain_t {
  // command to execute
  std::string filename;
  // description of the chain
  std::string description;
  // ACS expression for accessing chain.
  std::string acs;
  chain_exec_mode_t exec_mode{chain_exec_mode_t::none};
  chain_exec_dir_t dir{chain_exec_dir_t::bbs};
  bool ansi{false};
  bool local_only{false};
  bool multi_user{false};
  bool local_console_cp437{false};

  // who registered
  std::set<int16_t> regby;
  // number of runs
  uint16_t usage{0};
};

class Chains final {
public:
  typedef ssize_t size_type;
  typedef chain_t& reference;
  typedef const chain_t& const_reference;
  static const size_type npos = -1;
  explicit Chains(const Config& config);
  // [[ VisibleForTesting ]]
  explicit Chains(std::initializer_list<chain_t> l) : chains_(l) {}
  Chains() = default;
  ~Chains();

  [[nodiscard]] bool IsInitialized() const { return initialized_; }
  [[nodiscard]] const std::vector<chain_t>& chains() const { return chains_; }
  [[nodiscard]] const chain_t& at(size_type num) const { return chains_.at(num); }
  [[nodiscard]] chain_t& at(size_type num) { return chains_.at(num); }

  bool insert(size_type n, chain_t r);
  bool erase(size_type n);
  bool Load();
  bool Save();

  [[nodiscard]] static uint16_t to_ansir(const chain_t& c);

  /**
   * Is at least one chain registered or sponsored by someone?  If so return
   * true, false otherwise.
   */
  [[nodiscard]] bool HasRegisteredChains() const;

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

} // namespace

#endif
