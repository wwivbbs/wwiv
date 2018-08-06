/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                  Copyright (C)2018, WWIV Software Services             */
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
#ifndef __INCLUDED_BBS_CONTEXT_H__
#define __INCLUDED_BBS_CONTEXT_H__

class Application;

namespace wwiv {
namespace bbs {

class SessionContext {
public:
  SessionContext(Application* a);
  virtual ~SessionContext() = default;

  /**
   * Resets the application level context.
   */
  void reset();

private:
  Application* a_;
};


}
}


#endif  // __INCLUDED_BBS_CONTEXT_H__