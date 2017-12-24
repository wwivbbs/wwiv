/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2017, WWIV Software Services             */
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

#include <chrono>
#include <ctime>
#include <string>

#include "core/wwivport.h"

namespace wwiv {
namespace sdk {

time_t time_t_now();
daten_t daten_t_now();
daten_t date_to_daten(std::string datet);
std::string daten_to_mmddyy(daten_t date);
std::string time_t_to_mmddyy(time_t date);

std::string daten_to_mmddyyyy(daten_t date);
std::string time_t_to_mmddyyyy(time_t date);

std::string daten_to_wwivnet_time(daten_t t);
std::string time_t_to_wwivnet_time(time_t t);
daten_t time_t_to_daten(time_t t);
std::string date();
std::string fulldate();
std::string times();

/** Displays dd as a human readable time */
std::string to_string(std::chrono::duration<double> dd);

}
}

#endif // __INCLUDED_SDK_DATETIME_H__
