/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5                              */
/*             Copyright (C)2018-2022, WWIV Software Services             */
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
#ifndef __INCLUDED_SDK_QSCAN_H__
#define __INCLUDED_SDK_QSCAN_H__

#include <memory>
#include "core/file.h"

namespace wwiv {
namespace sdk {

size_t calculate_qscan_length(size_t max_subs, size_t max_dirs);

// ReSharper disable CppMemberFunctionMayBeConst

/**
 * Bitfield style class for handling qscan on/off for subs and dirs.
 */
class qscan_bitset {
public:
  qscan_bitset(uint32_t* q, size_t max_size);
  qscan_bitset() = delete;
  ~qscan_bitset() = default;

  void set(size_t n);

  void reset(size_t n);

  [[nodiscard]] bool test(size_t n) const;

  void flip(size_t n);

  [[nodiscard]] size_t size() const noexcept;

private:
  uint32_t* q_;
  size_t max_size_;
};
// ReSharper restore CppMemberFunctionMayBeConst


class RawUserQScan {
public:
  RawUserQScan(uint32_t* q, int qscan_length, int max_subs, int max_dirs);
  RawUserQScan(int qscan_length, int max_subs, int max_dirs);
  RawUserQScan() = delete;
  ~RawUserQScan() = default;

  qscan_bitset& dirs() { return dirs_; };
  qscan_bitset& subs() { return subs_; };

  [[nodiscard]] uint32_t lastread_pointer(int n) const { return lastread_pointer_[n]; }
  // ReSharper disable once CppMemberFunctionMayBeConst
  void lastread_pointer(int n, uint32_t val) { lastread_pointer_[n] = val; }
  [[nodiscard]] uint32_t* qsc() const { return qscan_.get(); }

private:
  std::unique_ptr<uint32_t[]> qscan_;
  uint32_t* newscan_dir_{};
  uint32_t* newscan_sub_{};
  uint32_t* lastread_pointer_{};

  qscan_bitset dirs_;
  qscan_bitset subs_;
};

// Experimental SDK class for working with QScan data.
class UserQScan : public RawUserQScan {
public:
  UserQScan(const std::string& filename, int user_number, int qscan_length, int max_subs,
            int max_dirs);
  UserQScan() = delete;
  ~UserQScan();

  bool save();
  bool clear();

private:
  wwiv::core::File file_;
  bool open_{false};

};

// TODO(rushfan): Implement this
class AllUserQScan {
public:
  AllUserQScan(const std::string& /*filename*/, int /*max_users*/) {}
  void swap_dir(int num);
  void swap_sub(int num);
  bool insert(int pos);
  bool remove(int pos);

};

} // namespace sdk
} // namespace wwiv

#endif  // __INCLUDED_SDK_QSCAN_H__
