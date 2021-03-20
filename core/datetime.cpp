/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services            */
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

#include "core/clock.h"
#include "core/log.h"
#include "core/strings.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <regex>
#include <sstream>
#include <string>

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

  auto t = time_t_now();
  auto pTm = localtime(&t);
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

std::string daten_to_wwivnet_time(daten_t t) {
  return time_t_to_wwivnet_time(static_cast<time_t>(t));
}

std::string time_t_to_wwivnet_time(time_t t) {
  const auto dt = DateTime::from_time_t(t);
  return dt.to_string();
}

daten_t time_t_to_daten(time_t t) { return static_cast<daten_t>(t); }

std::string date() {
  const auto dt = DateTime::now();
  return dt.to_string("%m/%d/%y");
}

std::string fulldate() {
  const auto dt = DateTime::now();
  return dt.to_string("%m/%d/%Y");
}

std::string times() {
  const auto dt = DateTime::now();
  return dt.to_string("%H:%M:%S");
}

std::string to_string(duration<double> dd) {
  auto ns = duration_cast<nanoseconds>(dd);
  typedef duration<int, std::ratio<86400>> days;
  std::ostringstream os;
  const auto d = duration_cast<days>(ns);
  ns -= d;
  const auto h = duration_cast<hours>(ns);
  ns -= h;
  const auto m = duration_cast<minutes>(ns);
  ns -= m;
  const auto s = duration_cast<seconds>(ns);
  ns -= s;
  const auto ms = duration_cast<milliseconds>(ns);
  ns -= ms;
  auto has_one = false;
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
}

std::optional<duration<double>> parse_time_span(const std::string& s) {
  if (s.length() < 2) {
    return std::nullopt;
  }

  // we don't allow negative timespans, so we'll fail on those.
  if (!isdigit(s.front())) {
    return std::nullopt;
  }
  const auto num = wwiv::strings::to_number<int>(s.substr(0, s.size() - 1));
  if (num == 0) {
    return std::nullopt;
  }

  switch (to_lower_case(s.back())) {
  case 's':
    return seconds(num);
  case 'm':
    return minutes(num);
  case 'h':
    return hours(num);
  case 'd':
    return hours(num * 24);
  default:
    return std::nullopt;
  }
}

int years_old(int m, int d, int y, Clock& clock) {
  const auto t = clock.Now();
  const auto tm_now = t.to_tm();
  y -= 1900;
  --m; // Reduce by one because tm_mon is 0-11, not 1-12

  // Find the range of impossible dates (ie, pTm can't be
  // less than the input date)
  if (tm_now.tm_year < y) {
    return 0;
  }
  if (tm_now.tm_year == y) {
    if (tm_now.tm_mon < m) {
      return 0;
    }
    if (tm_now.tm_mon == m) {
      if (tm_now.tm_mday < d) {
        return 0;
      }
    }
  }

  auto age = tm_now.tm_year - y;
  // ReSharper disable once CppRedundantParentheses
  if (tm_now.tm_mon < m || (tm_now.tm_mon == m && tm_now.tm_mday < d)) {
    --age;
  }
  return age;
}

DateTime parse_yyyymmdd(const std::string& date_str) {
  // Avoid https://developercommunity.visualstudio.com/content/problem/18311/stdget-time-asserts-with-istreambuf-iterator-is-no.html
  std::regex date_time_regex("([0-9]{4})-([0-9]{2})-([0-9]{2})");
  if (!std::regex_match(date_str, date_time_regex)) {
    return DateTime::now();
  }

  std::istringstream ss{date_str};
  ss.exceptions(std::ios::goodbit);
  std::tm dt{};
  ss >> std::get_time(&dt, "%Y-%m-%d");
  if (ss.fail()) {
    return DateTime::now();
  }
  dt.tm_hour = 12;
  dt.tm_min = 0;
  dt.tm_sec = 0;
  return DateTime::from_tm(&dt);
}

DateTime parse_yyyymmdd_with_optional_hms(const std::string& date_str) {
  // Avoid https://developercommunity.visualstudio.com/content/problem/18311/stdget-time-asserts-with-istreambuf-iterator-is-no.html
  std::regex date_time_regex("([0-9]{4})-([0-9]{2})-([0-9]{2})\\s([0-9]{2}):([0-9]{2}):([0-9]{2})");
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

DateTime::DateTime(tm* t)
  : t_(mktime_no_dst_changes(t)), tm_(*t), millis_(0) {
  if (t_ < 0) {
    t_ = 0;
    update_tm();
  }
}

DateTime::DateTime(time_t t) : t_(t), millis_(0) { update_tm(); }

DateTime::DateTime()
  : DateTime(static_cast<time_t>(0)) {
}

DateTime::DateTime(const DateTime& other) : DateTime(other.to_time_t()) {
  millis_ = other.millis_;
}

DateTime::DateTime(DateTime&& other) noexcept : DateTime(other.to_time_t()) {
  millis_ = other.millis_;
}

DateTime& DateTime::operator=(const DateTime& o) {
  t_ = o.to_time_t();
  millis_ = o.millis_;
  update_tm();
  return *this;
}

DateTime& DateTime::operator=(DateTime&& o) noexcept {
  t_ = o.to_time_t();
  millis_ = o.millis_;
  update_tm();
  return *this;
}

std::string DateTime::to_string(const std::string& format) const {
  std::ostringstream ss;
  ss << std::put_time(&tm_, format.c_str());
  return ss.str();
}

std::string DateTime::to_string() const {
  const auto* t = asctime(&tm_);
  if (!t) {
    return {};
  }
  auto s = std::string(t);
  StringTrimEnd(&s);
  return s;
}

// static
DateTime DateTime::now() { return DateTime(system_clock::now()); }

struct tm DateTime::to_tm() const noexcept {
  return tm_;
}

void DateTime::update_tm() noexcept {
  if (t_ < 0) {
    t_ = 1;
  }
  if (const auto * tm = localtime(&t_);tm) {
    tm_ = *tm;
  } else {
    LOG(ERROR) << "Invalid Time passed to update_tm";    
    auto tnow = time(nullptr);
    const auto* nowtm = localtime(&tnow);
    tm_ = *nowtm;
  }
}

system_clock::time_point DateTime::to_system_clock() const noexcept {
  return system_clock::from_time_t(t_);
}

bool operator==(const DateTime& lhs, const DateTime& rhs) {
  return lhs.to_time_t() == rhs.to_time_t();
}

bool operator!=(const DateTime& lhs, const DateTime& rhs) {
  return lhs.to_time_t() != rhs.to_time_t();
}

bool operator>(const DateTime& lhs, const DateTime& rhs) { return rhs < lhs; }

bool operator<=(const DateTime& lhs, const DateTime& rhs) { return !(lhs > rhs); }

bool operator>=(const DateTime& lhs, const DateTime& rhs) { return !(lhs < rhs); }

DateTime operator+(const DateTime& lhs, duration<double> d) {
  const auto du = std::chrono::duration_cast<seconds>(d);
  return DateTime::from_time_t(lhs.to_time_t() + static_cast<time_t>(du.count()));
}

DateTime operator-(const DateTime& lhs, duration<double> d) {
  const auto du = std::chrono::duration_cast<seconds>(d);
  return DateTime::from_time_t(lhs.to_time_t() - static_cast<time_t>(du.count()));
}


} // namespace wwiv
