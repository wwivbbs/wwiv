/**************************************************************************/
/*                                                                        */
/*                 WWIV Initialization Utility Version 5.0                */
/*             Copyright (C)1998-2014, WWIV Software Services             */
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
#ifndef __INCLUDED_INIT_H__
#define __INCLUDED_INIT_H__

#ifndef NOT_BBS
#error "NOT_BBS MUST BE DEFINED"
#endif  // NOT_BBS
#ifndef INIT
#error "INIT MUST BE DEFINED"
#endif  // INIT

/*!
 * @class WInitApp  Main Application object for WWIV 5.0
 */
class WInitApp {
 public:
  WInitApp();
  virtual ~WInitApp();

  int main(int argc, char* argv[]);
};


#endif