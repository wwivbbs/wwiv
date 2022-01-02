/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services             */
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
#ifndef INCLUDED_BBS_HELPER_H
#define INCLUDED_BBS_HELPER_H

#include "bbs/bbs.h"
#include "common/common_helper.h"
#include <memory>

/*
 * 
 * Test helper for using code with heavy BBS dependencies.
 * 
 * To use, add an instance as a field in the test class.  Then invoke.
 * BbsHelper::SetUp before use. typically from Test::SetUp.
 */
class BbsHelper : public CommonHelper {
public:
  BbsHelper();
  virtual ~BbsHelper() = default;
  void SetUp() override;
  [[nodiscard]] wwiv::sdk::Config& config() const override;
  [[nodiscard]] wwiv::common::SessionContext& sess() override;
  [[nodiscard]] wwiv::sdk::User* user() override;
  [[nodiscard]] const wwiv::sdk::User* user() const;
  [[nodiscard]] wwiv::common::Context& context() override;

  std::unique_ptr<Application> app_;

};

#endif
