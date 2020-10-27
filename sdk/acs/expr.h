/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5                              */
/*                Copyright (C)2020, WWIV Software Services               */
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
#ifndef INCLUDED_SDK_ACS_EXPR_H
#define INCLUDED_SDK_ACS_EXPR_H

#include <string>

namespace wwiv::sdk::acs {

/** Provides a value for an identifier of the form "object.attribute" */
class AcsExpr final {
public:
  AcsExpr() = default;
  ~AcsExpr() = default;

  AcsExpr& min_age(int min_age) {
    min_age_ = min_age;
    return *this;
  }
  AcsExpr& max_age(int max_age) {
    max_age_ = max_age;
    return *this;
  }
  AcsExpr& min_sl(int min_sl) {
    min_sl_ = min_sl;
    return *this;
  }
  AcsExpr& max_sl(int max_sl) {
    max_sl_ = max_sl;
    return *this;
  }
  AcsExpr& ar(char ar) {
    ar_ = ar;
    return *this;
  }

  AcsExpr& ar_int(int ar) {
    if (ar != 0) {
      for (auto i = 0; i < 16; i++) {
        if ((1 << i) & ar) {
          ar_ = static_cast<char>('A' + i);
        }
      }
    }
    return *this;
  }

  AcsExpr& min_dsl(int min_dsl) {
    min_dsl_ = min_dsl;
    return *this;
  }
  AcsExpr& max_dsl(int max_dsl) {
    max_dsl_ = max_dsl;
    return *this;
  }
  AcsExpr& dar(char dar) {
    dar_ = dar;
    return *this;
  }
  AcsExpr& dar_int(int ar) {
    if (ar != 0) {
      for (auto i = 0; i < 16; i++) {
        if ((1 << i) & ar) {
         dar_ = static_cast<char>('A' + i);
        }
      }
    }
    return *this;
  }
  AcsExpr& regnum() {
    regnum_ = true;
    return *this;
  }

  /** 
   * Gets the ACS expression for the given constraints.
   */
  [[nodiscard]] std::string get();

private:
  int min_age_{0};
  int max_age_{255};
  char ar_{0};
  int min_sl_{0};
  int max_sl_{255};
  char dar_{0};
  int min_dsl_{0};
  int max_dsl_{255};
  bool regnum_{false};
};

}

#endif
