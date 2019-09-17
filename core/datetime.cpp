/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2019, WWIV Software Services            */
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
#include "core/datetime.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <regex>
#include <sstream>
#include <string>

#include "core/strings.h"

using std::string;

using namespace std::chrono;
using namespace wwiv::strings;

namespace wwiv::core {

time_t time_t_now() { return time(nullptr); }

daten_t daten_t_now() { return time_t_to_daten(time_t_now()); }

// This kludge will get us through 2029 and should not interfere anywhere else.
daten_t date_to_daten(const std::string& datet) {
  if (datet.size() != 8) {
    return 0;
  }

  time_t t = time_t_now();
  struct tm* pTm = localtime(&t);
  pTm->tm_mon = to_number<int>(datet) - 1;
  pTm->tm_mday = atoi(datet.c_str() + 3);
  // N.B. tm_year is years since 1900
  pTm->tm_year = atoi(datet.c_str() + 6); // fixed for 1930-2029
  if (datet[6] < '3') {
    pTm->tm_year += 100;
  }
  pTm->tm_hour = 0;
  pTm->tm_min = 0;
  pTm->tm_sec = 0;
  pTm->tm_isdst = 0; // Since this is used for arbitrary compare of date strings, this is ok.

  return time_t_to_daten(mktime(pTm));
}

std::string daten_to_wwivnet_time(daten_t n) {
  return time_t_to_wwivnet_time(static_cast<time_t>(n));
}

std::string time_t_to_wwivnet_time(time_t t) {
  auto dt = DateTime::from_time_t(t);
  return dt.to_string();
}

daten_t time_t_to_daten(time_t t) { return static_cast<daten_t>(t); }

std::string date() {
  auto dt = DateTime::now();
  return dt.to_string("%m/%d/%y");
}

std::string fulldate() {
  auto dt = DateTime::now();
  return dt.to_string("%m/%d/%Y");
}

string times() {
  auto dt = DateTime::now();
  return dt.to_string("%H:%M:%S");
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
    if (has_one) {
      os << " ";
    } else {
      has_one = true;
    }
    os << h.count() << "h";
  }
  if (m.count() > 0) {
    if (has_one) {
      os << " ";
    } else {
      has_one = true;
    }
    os << m.count() << "m";
  }
  if (s.count() > 0) {
    if (has_one) {
      os << " ";
    } else {
      has_one = true;
    }
    os << s.count() << "s";
  }
  if (ms.count() > 0) {
    if (has_one) {
      os << " ";
    }
    os << ms.count() << "ms";
  }
  return os.str();
};

DateTime parse_yyyymmdd(const std::string& date_str) {
  // Avoid https://developercommunity.visualstudio.com/content/problem/18311/stdget-time-asserts-with-istreambuf-iterator-is-no.html
  std::regex date_time_regex("([0-9]{4})-([0-9]{2})-([0-9]{2})");
  std::smatch result;
  if (!std::regex_match(date_str, date_time_regex)) {
    return DateTime::now();
  }

  std::istringstream ss{date_str};
  ss.exceptions(std::ios::goodbit);
  std::tm dt = {};
  ss >> std::get_time(&dt, "%Y-%m-%d");
  if (ss.fail()) {
    return DateTime::now();
  }
  dt.tm_hour = 0;
  dt.tm_min = 0;
  dt.tm_sec = 0;
  return DateTime::from_tm(&dt);
}

DateTime parse_yyyymmdd_with_optional_hms(const std::string& date_str) {
  // Avoid https://developercommunity.visualstudio.com/content/problem/18311/stdget-time-asserts-with-istreambuf-iterator-is-no.html
  std::regex date_time_regex("([0-9]{4})-([0-9]{2})-([0-9]{2})\\s([0-9]{2}):([0-9]{2}):([0-9]{2})");
  std::smatch result;
  if (!std::regex_match(date_str, date_time_regex)) {
    return parse_yyyymmdd(date_str);
  }

  std::istringstream ss{date_str};
  ss.exceptions(std::ios::goodbit);
  std::tm dt{};
  ss >> std::get_time(&dt, "%Y-%m-%d %H:%M:%S");
  if (ss.fail()) {
    return parse_yyyymmdd(date_str);
  }
  return DateTime::from_tm(&dt);
}

DateTime::DateTime(system_clock::time_point t)
    : t_(system_clock::to_time_t(t)),
      millis_(static_cast<int>(duration_cast<milliseconds>(t.time_since_epoch()).count() % 1000)) {
  update_tm();
}

static time_t mktime_no_dst_changes(tm* t) noexcept { 
  // Kludge to match the is_dst match so that our hour
  // matches exactly in the tm struct and isn't offset
  // for daylight savings time.
  auto t2{*t};
  mktime(&t2);
  t->tm_isdst = t2.tm_isdst;
  return mktime(t); 
}

DateTime::DateTime(tm* t) : t_(mktime_no_dst_changes(t)), tm_(*t) , millis_(0) {}

DateTime::DateTime(time_t t) : t_(t), millis_(0) { update_tm(); }

DateTime::DateTime() : DateTime(static_cast<time_t>(0)) {}

std::string DateTime::to_string(const std::string& format) const { return put_time(&tm_, format); }

std::string DateTime::to_string() const {
  auto t = asctime(&tm_);
  if (!t) {
    return {};
  }
  auto s = string(t);
  StringTrimEnd(&s);
  return s;
}

// static
DateTime DateTime::now() { return DateTime(system_clock::now()); }

struct tm DateTime::to_tm() const noexcept {
  return tm_;
}

void DateTime::update_tm() noexcept {
  auto tm = localtime(&t_);
  tm_ = *tm;
}

std::chrono::system_clock::time_point DateTime::to_system_clock() const noexcept {
  return std::chrono::system_clock::from_time_t(t_);
}

bool operator==(const DateTime& lhs, const DateTime& rhs) { return lhs.to_time_t() == rhs.to_time_t(); }
bool operator!=(const DateTime& lhs, const DateTime& rhs) { return lhs.to_time_t() != rhs.to_time_t(); }

bool operator>(const DateTime& lhs, const DateTime& rhs) { return rhs < lhs; }

bool operator<=(const DateTime& lhs, const DateTime& rhs) { return !(lhs > rhs); }

bool operator>=(const DateTime& lhs, const DateTime& rhs) { return !(lhs < rhs); }

 DateTime operator+(DateTime lhs, std::chrono::duration<double> d) {
  const auto du = std::chrono::duration_cast<std::chrono::seconds>(d);
  return DateTime::from_time_t(lhs.to_time_t() + static_cast<time_t>(du.count()));
}

DateTime operator-(DateTime lhs, std::chrono::duration<double> d) {
  const auto du = std::chrono::duration_cast<std::chrono::seconds>(d);
  return DateTime::from_time_t(lhs.to_time_t() - static_cast<time_t>(du.count()));
}


} // namespace wwiv
