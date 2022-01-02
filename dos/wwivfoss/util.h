/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5                            */
/*             Copyright (C)2021-2022, WWIV Software Services             */
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
#ifndef UTIL_H
#define UTIL_H

class Array {
public:
  Array(int max_items);
  ~Array();
  void push_back(const char* item);
  int size() const;
  const char* at(int n) const;

  char** items() { return items_; }

  int max_items_;
  int last_item_;
  char** items_;
};


/** Writes a log entry to stderr */
void log(const char* msg, ...);

/** Yields a timeslice to the OS on OS/2 or Windows under DOS */
void os_yield();

/** Writes ch to stdout, processing \r => \r\n */
void outch(int ch);

/** Sleeps for roughly ms milliseconds */
void sleep(int ms);

#endif // UTIL_H







