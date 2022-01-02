/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2022, WWIV Software Services             */
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
#include "bbs/qwk/qwk_util.h"

namespace wwiv::bbs::qwk {

int _fieeetomsbin(float *src4, float *dest4) {
  auto* ieee = reinterpret_cast<unsigned char*>(src4);
  auto* msbin = reinterpret_cast<unsigned char*>(dest4);
  unsigned char msbin_exp = 0x00;

  /* See _fmsbintoieee() for details of formats   */
  const unsigned char sign = ieee[3] & 0x80;
  msbin_exp |= ieee[3] << 1;
  msbin_exp |= ieee[2] >> 7;

  /* An ieee exponent of 0xfe overflows in MBF    */
  if (msbin_exp == 0xfe) {
    return 1;
  }

  msbin_exp += 2;     /* actually, -127 + 128 + 1 */

  for (auto i = 0; i < 4; i++) {
    msbin[i] = 0;
  }

  msbin[3] = msbin_exp;
  msbin[2] |= sign;
  msbin[2] |= ieee[2] & 0x7f;
  msbin[1] = ieee[1];
  msbin[0] = ieee[0];

  return 0;
}

}