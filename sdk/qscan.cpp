/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*               Copyright (C)2018, WWIV Software Services                */
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
#include "sdk/qscan.h"

#include <stdexcept>
#include "core/log.h"

namespace wwiv {
namespace sdk {

using namespace wwiv::core;

/** Calculates the qscan_len field based on max_subs and max_dirs */
size_t calculate_qscan_length(size_t max_subs, size_t max_dirs) {
  const auto bits = 8 * sizeof(uint32_t);
  return sizeof(uint32_t) * (1 + max_subs + ((max_subs + bits - 1) / bits) + ((max_dirs + bits - 1) / bits));
}

qscan_bitset::qscan_bitset(uint32_t* q, size_t max_size)
  : q_(q), max_size_(max_size) {}

  // ReSharper disable CppMemberFunctionMayBeConst

void qscan_bitset::set(size_t n) {
  if (n > max_size_) {
    throw std::out_of_range("qscan_bitset::set out of range");
  }
  q_[n / 32] |= (1L << (n % 32));
}

void qscan_bitset::reset(size_t n) {
  if (n > max_size_) {
    throw std::out_of_range("qscan_bitset::reset out of range");
  }
  q_[n / 32] &= ~(1L << (n % 32));
}

bool qscan_bitset::test(size_t n) const {
  if (n > max_size_) {
    throw std::out_of_range("qscan_bitset::test out of range");
  }
  return q_[n / 32] & (1L << (n % 32));
}

void qscan_bitset::flip(size_t n) {
  if (n > max_size_) {
    throw std::out_of_range("qscan_bitset::flip out of range");
  }
  q_[n / 32] ^= (1L << (n % 32));
}

size_t qscan_bitset::max_size() const { return max_size_; }

// ReSharper restore CppMemberFunctionMayBeConst


RawUserQScan::RawUserQScan(uint32_t* q, int qscan_length, int max_subs, int max_dirs)
  : qscan_(std::make_unique<uint32_t[]>(qscan_length)),
    newscan_dir_(qscan_.get() + 1),
    newscan_sub_(newscan_dir_ + (max_dirs + 31) / 32),
    lastread_pointer_(newscan_sub_ + (max_subs + 31) / 32),
    dirs_(newscan_dir_, max_dirs),
    subs_(newscan_sub_, max_subs) {
  if (q != nullptr) {
    memcpy(qscan_.get(), q, qscan_length);
  } else {
    VLOG(1) << "Creating empty QScan Record";
    // Create new empty qscan record.
    memset(qscan_.get(), 0, qscan_length);
    *qscan_.get() = 999;
    memset(qscan_.get() + 1, 0xff, ((max_dirs + 31) / 32) * 4);
    memset(qscan_.get() + 1 + (max_subs + 31) / 32, 0xff, ((max_subs + 31) / 32) * 4);
  }
}

RawUserQScan::RawUserQScan(int qscan_length, int max_subs, int max_dirs)
  : RawUserQScan(nullptr, qscan_length, max_subs, max_dirs) {
}

UserQScan::UserQScan(
  const std::string& filename, int user_number, int qscan_length, int max_subs, int max_dirs)
  : RawUserQScan(qscan_length, max_subs, max_dirs), file_(filename) {

  const auto pos = user_number * qscan_length;
  open_ = file_.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile);
  if (open_ && (pos + qscan_length <= file_.length())) {
    // qsc() is already initialized by the RawUserQScan constructor.
    file_.Seek(pos, File::Whence::begin);
    file_.Read(qsc(), qscan_length);
  }

}

UserQScan::~UserQScan() = default;

bool UserQScan::save() {
  return false;
}

bool UserQScan::clear() {
  return false;
}

}
}
