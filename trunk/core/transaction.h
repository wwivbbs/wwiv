/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*                 Copyright (C)2014, WWIV Software Services              */
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
#ifndef __INCLUDED_TRANSACTION_H__
#define __INCLUDED_TRANSACTION_H__

#include <functional>

namespace wwiv {
namespace core {

class Transaction {
public:
  Transaction(std::function<void()> commit_fn, std::function<void()> rollback_fn) 
    : commit_fn_(commit_fn), rollback_fn_(rollback_fn), rollback_(false) {}

  ~Transaction() { 
    if (rollback_) { 
      if (rollback_fn_) { rollback_fn_(); }
    } else {
      if (commit_fn_) { commit_fn_(); }
    }
  }
  void Rollback() { rollback_ = true; }

private:
  std::function<void()> commit_fn_;
  std::function<void()> rollback_fn_;
  bool rollback_;
};

}  // namespace core
}  // namespace wwiv

#endif // __INCLUDED_TRANSACTION_H__
