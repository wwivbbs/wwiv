/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2016, WWIV Software Services             */
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
#ifndef __INCLUDED_SDK_DATETIME_H__
#define __INCLUDED_SDK_DATETIME_H__

#include <ctime>
#include <string>

namespace wwiv {
namespace sdk {

uint32_t date_to_daten(std::string datet);
std::string daten_to_mmddyy(time_t date);
std::string daten_to_wwivnet_time(time_t t);
uint32_t time_t_to_daten(time_t t);
std::string date();
std::string fulldate();
std::string times();

}
}

#endif // __INCLUDED_SDK_DATETIME_H__
