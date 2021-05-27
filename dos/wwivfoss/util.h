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







