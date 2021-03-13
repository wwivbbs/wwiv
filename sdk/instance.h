/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*                  Copyright (C)2021, WWIV Software Services             */
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
#ifndef INCLUDED_SDK_INSTANCE_H
#define INCLUDED_SDK_INSTANCE_H

#include "core/datetime.h"

#include <filesystem>
#include <string>
#include <vector>
#include "sdk/config.h"
#include "sdk/vardec.h"

namespace wwiv::sdk {

class Instance final {
public:
  explicit Instance(instancerec ir);
  explicit Instance(int instance_num);
  Instance() : Instance(0) {}
  Instance(const Instance&) = delete;
  Instance(Instance&&) noexcept;
  Instance& operator=(const Instance&);
  Instance& operator=(Instance&&) noexcept;
  ~Instance() = default;

  /**
   * Mutable member to manipulate the instance record.
   */
  [[nodiscard]] const instancerec& ir() const;
  [[nodiscard]] instancerec& ir();

  /**
   * The node number for this instance metadata.
   */
  [[nodiscard]] int node_number() const noexcept;

  /**
   * The user number for this current or last user on this node.
   */
  [[nodiscard]] int user_number() const noexcept;

  /**
   * Is this instance available with a user online who can receive messages.
   */
  [[nodiscard]] bool available() const noexcept;

  /**
   * Is this instance available with a user online who can receive messages.
   */
  [[nodiscard]] bool online() const noexcept;
  
  /**
   * Is this node active with a user.
   */
  [[nodiscard]] bool invisible() const noexcept;

  /**
   * Description of the caller's location within the BBS.
   */
  [[nodiscard]] std::string location_description() const;

  /**
   * The integer code for the location.
   */
  [[nodiscard]] int loc_code() const noexcept;

  /**
   * Is the caller in a numbered channel 
   */
  [[nodiscard]] bool in_channel() const noexcept;

  /**
   * The connection speed of the current or last session on this node.
   */
  [[nodiscard]] int modem_speed() const noexcept;

  /**
   * The sub-location code. This is typically a pointer into the list of
   * items referred to by location.  For example, in subs, it's the index
   * number of the sub the user is browsing.
   */
  [[nodiscard]] int subloc_code() const noexcept;

  /**
   * When was this instance started.
   */
  [[nodiscard]] core::DateTime started() const;
  
  /**
   * When was this instance's metadata last updated.
   */
  [[nodiscard]] core::DateTime updated() const;
  
private:
  instancerec ir_;
};

class Instances final {
public:
  typedef std::vector<Instance>::iterator iterator;
  typedef std::vector<Instance>::const_iterator const_iterator;
  typedef std::size_t size_type;

  [[nodiscard]] iterator begin() { return instances_.begin(); }
  [[nodiscard]] const_iterator begin() const { return instances_.begin(); }
  [[nodiscard]] const_iterator cbegin() const { return instances_.cbegin(); }
  [[nodiscard]] iterator end() { return instances_.end(); }
  [[nodiscard]] const_iterator end() const { return instances_.end(); }
  [[nodiscard]] const_iterator cend() const { return instances_.cend(); }

  Instances() = delete;
  Instances(const Instances&) = delete;
  Instances(Instances&&) = delete;
  explicit Instances(const Config& config);
  Instances& operator=(const Instances&) = delete;
  Instances& operator=(Instances&&) = delete;
  ~Instances() = default;

  [[nodiscard]] bool IsInitialized() const { return initialized_; }

  size_type size() const;
  Instance at(size_type pos);
  std::vector<Instance> all();

  bool upsert(size_type pos, const instancerec& ir);
  bool upsert(size_type pos, const Instance& ir);

  explicit operator bool() const noexcept { return IsInitialized(); }


private:
  [[nodiscard]] const std::filesystem::path& fn_path() const;

  bool initialized_;
  std::string datadir_;
  const std::filesystem::path path_;
  std::vector<Instance> instances_;
};

std::string instance_location(const instancerec& ir);

}


#endif 
