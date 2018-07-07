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
#ifndef __INCLUDED_CORE_DATETIME_H__
#define __INCLUDED_CORE_DATETIME_H__

#include <chrono>
#include <ctime>
#include <string>

#include "core/wwivport.h"

namespace wwiv {
namespace core {

time_t time_t_now();
daten_t daten_t_now();
daten_t date_to_daten(const std::string& datet);
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

class DateTime {
public:

  static DateTime from_time_t(time_t t) {
    return DateTime(t);
  }

  static DateTime from_daten(daten_t t) {
    time_t tt = t;
    return from_time_t(tt);
  }

  static DateTime now();

  int hour() const { return tm_.tm_hour; }
  int minute() const { return tm_.tm_min; }
  int second() const { return tm_.tm_sec; }

  int month() const { return tm_.tm_mon + 1; }
  int day() const { return tm_.tm_mday; }
  int year() const { return tm_.tm_year + 1900; }

  int dow() const { return tm_.tm_wday; }

  /** Prints a date using the strftime format specified.  */
  std::string to_string(const std::string& format) const;

  /** Prints a Date using asctime */
  std::string to_string() const;

  time_t to_time_t() const { return t_; }
  daten_t to_daten_t() const { return time_t_to_daten(t_); }
  struct tm to_tm() const;

private:
  DateTime(std::chrono::system_clock::time_point t);
  DateTime(time_t t);

  time_t t_;
  std::chrono::system_clock::time_point tp_;
  tm tm_ {};
  int millis_;
};

}
}

#endif // __INCLUDED_CORE_DATETIME_H__
