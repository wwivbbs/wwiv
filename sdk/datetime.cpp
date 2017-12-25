/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2017, WWIV Software Services            */
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
#include "sdk/datetime.h"

#include <chrono>
#include <cstring>
#include <iomanip>
#include <ctime>
#include <string>
#include <sstream>

#include "core/strings.h"

using std::string;

using namespace std::chrono;
using namespace wwiv::strings;

namespace wwiv {
namespace sdk {

time_t time_t_now() {
  return time(nullptr);
}

daten_t daten_t_now() {
  return time_t_to_daten(time(nullptr));
}

//
// This kludge will get us through 2019 and should not interfere anywhere
// else.
//

daten_t date_to_daten(const std::string& datet) {
  if (datet.size() != 8) {
    return 0;
  }

  time_t t = time_t_now();
  struct tm* pTm = localtime(&t);
  pTm->tm_mon   = atoi(datet.c_str()) - 1;
  pTm->tm_mday  = atoi(datet.c_str() + 3);
  // N.B. tm_year is years since 1900
  pTm->tm_year  = atoi(datet.c_str() + 6);         // fixed for 1920-2019
  if (datet[6] < '2') {
    pTm->tm_year += 100;
  }
  pTm->tm_hour = 0;
  pTm->tm_min = 0;
  pTm->tm_sec = 0;
  pTm->tm_isdst = 0;  // Since this is used for arbitrary compare of date strings, this is ok.

  return static_cast<daten_t>(mktime(pTm));
}

std::string daten_to_mmddyy(daten_t n) {
  time_t t = n;
  return time_t_to_mmddyy(t);
}

std::string time_t_to_mmddyy(time_t t) {
  auto dt = DateTime::from_time_t(t);
  return dt.to_string("%m/%d/%y");
}

std::string daten_to_mmddyyyy(daten_t n) {
  time_t t = n;
  return time_t_to_mmddyyyy(t);
}

std::string time_t_to_mmddyyyy(time_t t) {
  auto dt = DateTime::from_time_t(t);
  return dt.to_string("%m/%d/%Y");
}

std::string daten_to_wwivnet_time(daten_t n) {
  time_t t = n;
  return time_t_to_wwivnet_time(t);
}

std::string time_t_to_wwivnet_time(time_t t) {
  auto dt = DateTime::from_time_t(t);
  return dt.to_string();
}

uint32_t time_t_to_daten(time_t t) {
  return static_cast<uint32_t>(t);
}

std::string date() {
  auto t = time_t_now();
  struct tm * pTm = localtime(&t);
  return wwiv::strings::StringPrintf("%02d/%02d/%02d", pTm->tm_mon + 1, pTm->tm_mday, pTm->tm_year % 100);
}

std::string fulldate() {
  auto t = time_t_now();
  struct tm * pTm = localtime(&t);

  return wwiv::strings::StringPrintf("%02d/%02d/%4d", pTm->tm_mon + 1, pTm->tm_mday, pTm->tm_year + 1900);
}

string times() {
  auto tim = time_t_now();

  struct tm* t = localtime(&tim);
  return wwiv::strings::StringPrintf("%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
}

std::string to_string(std::chrono::duration<double> dd) {
  auto ns = duration_cast<nanoseconds>(dd);
  typedef duration<int, std::ratio<86400>> days;
  std::ostringstream os;
  auto d = duration_cast<days>(ns);
  ns -= d;
  auto h = duration_cast<hours>(ns);
  ns -= h;
  auto m = duration_cast<minutes>(ns);
  ns -= m;
  auto s = duration_cast<seconds>(ns);
  ns -= s;
  auto ms = duration_cast<milliseconds>(ns);
  ns -= ms;
  bool has_one = false;
  if (d.count() > 0) {
    os << d.count() << "d";
    has_one = true;
  }
  if (h.count() > 0) {
    if (has_one) { os << " "; } else { has_one = true; }
    os << h.count() << "h";
  }
  if (m.count() > 0) {
    if (has_one) { os << " "; } else { has_one = true; }
    os << m.count() << "m";
  }
  if (s.count() > 0) {
    if (has_one) { os << " "; } else { has_one = true; }
    os << s.count() << "s";
  }
  if (ms.count() > 0) {
    if (has_one) { os << " "; } else { has_one = true; }
    os << ms.count() << "ms";
  }
  return os.str();
};

std::string DateTime::to_string(const std::string& format)  const {
  return put_time(&tm_, format);
}
std::string DateTime::to_string()  const {
  auto s = string(asctime(&tm_));
  StringTrimEnd(&s);
  return s;
}

}
}
