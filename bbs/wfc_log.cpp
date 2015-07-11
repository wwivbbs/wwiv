/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2015, WWIV Software Services             */
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
#include "bbs/wfc_log.h"

#include <algorithm>
#include <iterator>
#include <mutex>
#include <string>
#include <vector>

using std::string;
using std::vector;

WfcLog::WfcLog(std::size_t size) : size_(size), dirty_(false) {}
WfcLog::~WfcLog() {}

bool WfcLog::Put(const std::string& log_line) {
  dirty_ = true;
  std::lock_guard<std::mutex> lock(mu_);
  queue_.emplace_back(log_line);
  while (queue_.size() > size_) {
    queue_.pop_front();
  }
  return true;
}

bool WfcLog::Get(vector<string>& lines) {
  lines.clear();
  std::lock_guard<std::mutex> lock(mu_);
  if (queue_.empty()) {
    return false;
  }

  std::copy(std::begin(queue_), std::end(queue_), std::inserter(lines, std::end(lines)));
  dirty_ = false;
  return true;
}
