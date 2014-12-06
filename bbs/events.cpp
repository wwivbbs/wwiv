/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2014, WWIV Software Services             */
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

#include "bbs/datetime.h"
#include "bbs/wwiv.h"
#include "bbs/input.h"
#include "bbs/instmsg.h"
#include "bbs/printfile.h"
#include "bbs/wcomm.h"
#include "bbs/wconstants.h"
#include "core/wwivassert.h"
#include "core/wwivport.h"

using wwiv::bbs::InputMode;

// Local Functions

int  t_now();
char *ttc(int d);
char *ttclastrun(int d);
void sort_events();
void show_events();
void select_event_days(int evnt);
void modify_event(int evnt);
void insert_event();
void delete_event(int n);


int t_now() {
  time_t t = time(nullptr);
  struct tm * pTm = localtime(&t);
  return (pTm->tm_hour * 60) + pTm->tm_min;
}


char *ttc(int d) {
  static char ch[7];

  int h = d / 60;
  int m = d - (h * 60);
  sprintf(ch, "%02d:%02d", h, m);
  return ch;
}

// TODO: remove this and ttc in favor of building in place.
char *ttclastrun(int d) {
  static char ch[7];

  int h = d / 60;
  int m = d - (h * 60);
  sprintf(ch, "%02d:%02d", h, m);
  return ch;
}

void sort_events() {
  // keeping events sorted in time order makes things easier.
  for (int i = 0; i < (session()->num_events - 1); i++) {
    int z = i;
    for (int j = (i + 1); j < session()->num_events; j++) {
      if (events[j].time < events[z].time) {
        z = j;
      }
    }
    if (z != i) {
      eventsrec e;
      e = events[i];
      events[i] = events[z];
      events[z] = e;
    }
  }
}

void init_events() {
  if (events) {
    free(events);
    events = nullptr;
  }
  events = static_cast<eventsrec *>(BbsAllocA(MAX_EVENT * sizeof(eventsrec)));
  WWIV_ASSERT(events != nullptr);

  File eventsFile(syscfg.datadir, EVENTS_DAT);
  if (eventsFile.Open(File::modeBinary | File::modeReadOnly)) {
    session()->num_events = eventsFile.GetLength() / sizeof(eventsrec);
    eventsFile.Read(events, session()->num_events * sizeof(eventsrec));
    get_next_forced_event();
  } else {
    session()->num_events = 0;
  }
}


void get_next_forced_event() {
  syscfg.executetime = 0;
  time_event = 0.0;
  int first = -1;
  int tl = t_now();
  int day = dow() + 1;
  if (day == 7) {
    day = 0;
  }
  for (int i = 0; i < session()->num_events; i++) {
    if ((events[i].instance == application()->GetInstanceNumber() || events[i].instance == 0) &&
        events[i].status & EVENT_FORCED) {
      if (first < 0 && events[i].time < tl && ((events[i].days & (1 << day)) > 0)) {
        first = events[i].time;
      }
      if ((events[i].status & EVENT_RUNTODAY) == 0 && (events[i].days & (1 << dow())) > 0 && !syscfg.executetime) {
        time_event = static_cast<double>(events[i].time) * SECONDS_PER_MINUTE_FLOAT;
        syscfg.executetime = events[i].time;
        if (!syscfg.executetime) {
          ++syscfg.executetime;
        }
      }
    }
  }
  if (first >= 0 && !syscfg.executetime) {
    // all of todays events are
    time_event = static_cast<double>(first) * SECONDS_PER_MINUTE_FLOAT;     // complete, set next forced
    syscfg.executetime = static_cast<unsigned short>(first);                // event to first one
    if (!syscfg.executetime) {                                              // scheduled for tomorrow
      ++syscfg.executetime;
    }
  }
}


void cleanup_events() {
  if (!session()->num_events) {
    return;
  }

  // since the date has changed, make sure all events for yesterday have been
  // run, then clear all status to "not run" note in this case all events end up
  // running on the same node, but this is preferable to not running at all
  int day = dow() - 1;
  if (day < 0) {
    day = 6;
  }

  int i;
  for (i = 0; i < session()->num_events; i++) {
    if (((events[i].status & EVENT_RUNTODAY) == 0) &&
        ((events[i].days & (1 << day)) > 0)) {
      run_event(i);
    }
  }
  for (i = 0; i < session()->num_events; i++) {
    events[i].status &= ~EVENT_RUNTODAY;
    // zero out last run in case this is a periodic event
    events[i].lastrun = 0;
  }

  File eventsFile(syscfg.datadir, EVENTS_DAT);
  eventsFile.Open(File::modeReadWrite | File::modeBinary);
  eventsFile.Write(events, session()->num_events * sizeof(eventsrec));
  eventsFile.Close();
}


void check_event() {
  int i;

  int tl = t_now();
  for (i = 0; i < session()->num_events && !do_event; i++) {
    if (((events[i].status & EVENT_RUNTODAY) == 0) && (events[i].time <= tl) &&
        ((events[i].days & (1 << dow())) > 0) &&
        ((events[i].instance == application()->GetInstanceNumber()) ||
         (events[i].instance == 0))) {
      // make sure the event hasn't already been executed on another node,then mark it as run
      File eventsFile(syscfg.datadir, EVENTS_DAT);
      eventsFile.Open(File::modeReadWrite | File::modeBinary);
      eventsFile.Seek(i * sizeof(eventsrec), File::seekBegin);
      eventsFile.Read(&events[i], sizeof(eventsrec));

      if ((events[i].status & EVENT_RUNTODAY) == 0) {
        events[i].status |= EVENT_RUNTODAY;
        eventsFile.Seek(i * sizeof(eventsrec), File::seekBegin);
        eventsFile.Write(&events[i], sizeof(eventsrec));
        do_event = i + 1;
      }
      eventsFile.Close();
    } else if ((events[i].status & EVENT_PERIODIC) &&
               ((events[i].days & (1 << dow())) > 0) &&
               ((events[i].instance == application()->GetInstanceNumber()) ||
                (events[i].instance == 0))) {
      // periodic events run after N minutes from last execution.
      short int nextrun = ((events[i].lastrun == 0) ? events[i].time : events[i].lastrun) + events[i].period;
      // if the next runtime is before now trigger it to run
      if (nextrun <= tl) {
        // flag the event to run
        File eventsFile(syscfg.datadir, EVENTS_DAT);
        eventsFile.Open(File::modeReadWrite | File::modeBinary);
        eventsFile.Seek(i * sizeof(eventsrec), File::seekBegin);
        eventsFile.Read(&events[i], sizeof(eventsrec));

        // make sure other nodes didn't run it already
        if ((((events[i].lastrun == 0) ? events[i].time : events[i].lastrun) + events[i].period) <= tl) {
          events[i].status |= EVENT_RUNTODAY;
          // record that we ran it now.
          events[i].lastrun = nextrun;
          eventsFile.Seek(i * sizeof(eventsrec), File::seekBegin);
          eventsFile.Write(&events[i], sizeof(eventsrec));
          do_event = i + 1;
        }
        eventsFile.Close();
      }
    }

  }
}

void run_event(int evnt) {
  int exitlevel;

  write_inst(INST_LOC_EVENT, 0, INST_FLAGS_NONE);
#ifndef __unix__
  session()->localIO()->SetCursor(WLocalIO::cursorNormal);
#endif
  bout.cls();
  bout << "\r\nNow running external event.\r\n\n";
  if (events[evnt].status & EVENT_HOLD) {
    holdphone(true);
  }
  if (events[evnt].status & EVENT_EXIT) {
    exitlevel = static_cast<int>(events[evnt].cmd[0]);
    if (ok_modem_stuff && session()->remoteIO() != nullptr) {
      session()->remoteIO()->close();
    }
    exit(exitlevel);
  }
  ExecuteExternalProgram(events[evnt].cmd, EFLAG_NONE);
  do_event = 0;
  get_next_forced_event();
  cleanup_net();
  holdphone(false);
#ifndef __unix__
  wfc_cls();
#endif
}

void show_events() {
  char s[121] = { 0 }, s1[81] = { 0 }, daystr[8] = { 0 };

  bout.cls();
  bool abort = false;
  char y = "Yes"[0];
  char n = "No"[0];
  pla("|#1                                         Hold   Force   Run            Run", &abort);
  pla("|#1Evnt Time  Command                 Node  Phone  Event  Today   Freq    Days", &abort);
  pla("|#7=============================================================================", &abort);
  for (int i = 0; (i < session()->num_events) && !abort; i++) {
    if (events[i].status & EVENT_EXIT) {
      sprintf(s1, "Exit Level = %d", events[i].cmd[0]);
    } else {
      strncpy(s1, events[i].cmd, sizeof(s1));
    }
    strcpy(daystr, "SMTWTFS");
    for (int j = 0; j <= 6; j++) {
      if ((events[i].days & (1 << j)) == 0) {
        daystr[j] = ' ';
      }
    }
    if (events[i].status & EVENT_PERIODIC) {
      if (events[i].status & EVENT_RUNTODAY) {
        sprintf(s, " %2d  %-5.5s %-23.23s  %2d     %1c      %1c    %-5.5s    %2dm   %s",
                i, ttc(events[i].time), s1, events[i].instance,
                events[i].status & EVENT_HOLD ? y : n,
                events[i].status & EVENT_FORCED ? y : n,
                ttclastrun(events[i].lastrun),
                events[i].period,
                daystr);
      } else {
        sprintf(s, " %2d  %-5.5s %-23.23s  %2d     %1c      %1c      %1c      %2dm   %s",
                i, ttc(events[i].time), s1, events[i].instance,
                events[i].status & EVENT_HOLD ? y : n,
                events[i].status & EVENT_FORCED ? y : n,
                n,
                events[i].period,
                daystr);
      }
    } else {
      sprintf(s, " %2d  %-5.5s %-23.23s  %2d     %1c      %1c      %1c            %s",
              i, ttc(events[i].time), s1, events[i].instance,
              events[i].status & EVENT_HOLD ? y : n,
              events[i].status & EVENT_FORCED ? y : n,
              events[i].status & EVENT_RUNTODAY ? y : n,
              daystr);
    }
    pla(s, &abort);
  }
}


void select_event_days(int evnt) {
  int i;
  char ch, daystr[9], days[8];

  bout.nl();
  strcpy(days, "SMTWTFS");
  for (i = 0; i <= 6; i++) {
    if (events[evnt].days & (1 << i)) {
      daystr[i] = days[i];
    } else {
      daystr[i] = ' ';
    }
  }
  daystr[8] = '\0';
  bout << "Enter number to toggle day of the week, 'Q' to quit.\r\n\n";
  bout << "                   1234567\r\n";
  bout << "Days to run event: ";
  do {
    bout << daystr;
    ch = onek_ncr("1234567Q");
    if ((ch >= '1') && (ch <= '7')) {
      i = ch - '1';
      events[evnt].days ^= (1 << i);
      if (events[evnt].days & (1 << i)) {
        daystr[i] = days[i];
      } else {
        daystr[i] = ' ';
      }
      bout << "\b\b\b\b\b\b\b";
    }
  } while (ch != 'Q' && !hangup);
}


void modify_event(int evnt) {
  char s[81], s1[81], ch;
  int j;

  bool ok     = true;
  bool done   = false;
  int i       = evnt;
  do {
    bout.cls();
    bout << "A) Event Time......: " << ttc(events[i].time) << wwiv::endl;
    if (events[i].status & EVENT_EXIT) {
      sprintf(s1, "Exit BBS with DOS Errorlevel %d", events[i].cmd[0]);
    } else {
      strcpy(s1, events[i].cmd);
    }
    bout << "B) Event Command...: " << s1 << wwiv::endl;
    bout << "C) Phone Off Hook?.: " << ((events[i].status & EVENT_HOLD) ? "Yes" : "No") << wwiv::endl;
    bout << "D) Already Run?....: " << ((events[i].status & EVENT_RUNTODAY) ? "Yes" : "No") << wwiv::endl;
    bout << "E) Shrink?.........: " << ((events[i].status & EVENT_SHRINK) ? "Yes" : "No") << wwiv::endl;
    bout << "F) Force User Off?.: " << ((events[i].status & EVENT_FORCED) ? "Yes" : "No") << wwiv::endl;
    strcpy(s1, "SMTWTFS");
    for (j = 0; j <= 6; j++) {
      if ((events[i].days & (1 << j)) == 0) {
        s1[j] = ' ';
      }
    }
    bout << "G) Days to Execute.: " << s1 << wwiv::endl;
    bout << "H) Node (0=Any)....: " << events[i].instance << wwiv::endl;
    bout << "I) Periodic........: " << ((events[i].status & EVENT_PERIODIC) ? "Yes" : "No");
    if (events[i].status & EVENT_PERIODIC) {
      bout << " (every " << std::to_string(events[i].period) << " minutes)";
    }
    bout << wwiv::endl;
    bout.nl();
    bout << "|#5Which? |#7[|#1A-I,[,],Q=Quit|#7] |#0: ";
    ch = onek("QABCDEFGHI[]");
    switch (ch) {
    case 'Q':
      done = true;
      break;
    case ']':
      i++;
      if (i >= session()->num_events) {
        i = 0;
      }
      break;
    case '[':
      i--;
      if (i < 0) {
        i = session()->num_events - 1;
      }
      break;
    case 'A':
      bout.nl();
      bout << "|#2Enter event times in 24 hour format. i.e. 00:01 or 15:20\r\n";
      bout << "|#2Event time? ";
      ok = true;
      j = 0;
      do {
        if (j == 2) {
          s[j++] = ':';
          bputch(':');
        } else {
          switch (j) {
          case 0:
            ch = onek_ncr("012\r");
            break;
          case 3:
            ch = onek_ncr("012345\b");
            break;
          case 5:
            ch = onek_ncr("\b\r");
            break;
          case 1:
            if (s[0] == '2') {
              ch = onek_ncr("0123\b");
              break;
            }
          default:
            ch = onek_ncr("0123456789\b");
            break;
          }
          if (hangup) {
            ok = false;
            s[0] = '\0';
            break;
          }
          switch (ch) {
          case '\r':
            switch (j) {
            case 0:
              ok = false;
              break;
            case 5:
              s[5] = '\0';
              break;
            default:
              ch = 0;
              break;
            }
            break;
          case '\b':
            bout << " \b";
            --j;
            if (j == 2) {
              bout.bs();
              --j;
            }
            break;
          default:
            s[j++] = ch;
            break;
          }
        }
      } while (ch != '\r' && !hangup);
      if (ok) {
        events[i].time = static_cast<short>((60 * atoi(s)) + atoi(&(s[3])));
      }
      break;
    case 'B':
      bout.nl();
      bout << "|#2Exit BBS for event? ";
      if (yesno()) {
        events[i].status |= EVENT_EXIT;
        bout << "|#2DOS ERRORLEVEL on exit? ";
        input(s, 3);
        j = atoi(s);
        if (s[0] != 0 && j >= 0 && j < 256) {
          events[i].cmd[0] = static_cast<char>(j);
        }
      } else {
        events[i].status &= ~EVENT_EXIT;
        bout << "|#2Commandline to run? ";
        input(s, 80);
        if (s[0] != '\0') {
          strcpy(events[i].cmd, s);
        }
      }
      break;
    case 'C':
      events[i].status ^= EVENT_HOLD;
      break;
    case 'D':
      events[i].status ^= EVENT_RUNTODAY;
      // reset it in case it is periodic
      events[i].lastrun = 0;
      break;
    case 'E':
      events[i].status ^= EVENT_SHRINK;
      break;
    case 'F':
      events[i].status ^= EVENT_FORCED;
      break;
    case 'G':
      bout.nl();
      bout << "|#2Run event every day? ";
      if (noyes()) {
        events[i].days = 127;
      } else {
        select_event_days(i);
      }
      break;
    case 'H':
      bout.nl();
      bout << "|#2Run event on which node (0=any)? ";
      input(s, 3);
      j = atoi(s);
      if (s[0] != '\0' && j >= 0 && j < 1000) {
        events[i].instance = static_cast<short>(j);
      }
      break;
    case 'I':
      events[i].status ^= EVENT_PERIODIC;
      if (events[i].status & EVENT_PERIODIC) {
        bout.nl();
        bout << "|#2Run again after how many minutes (0=never, max=240)? ";
        input(s, 4);
        j = atoi(s);
        if (s[0] != '\0' && j >= 1 && j <= 240) {
          events[i].period = static_cast<short>(j);
        } else {
          // user entered invalid time period, disable periodic
          events[i].status &= ~EVENT_PERIODIC;
          events[i].period = 0;
        }
      }
      break;
    }
  } while (!done && !hangup);
}


void insert_event() {
  strcpy(events[session()->num_events].cmd, "**New Event**");
  events[session()->num_events].time = 0;
  events[session()->num_events].status = 0;
  events[session()->num_events].instance = 0;
  events[session()->num_events].days = 127;                // Default to all 7 days
  modify_event(session()->num_events);
  session()->num_events = session()->num_events + 1;
}


void delete_event(int n) {
  for (int i = n; i < session()->num_events; i++) {
    events[i] = events[i + 1];
  }
  session()->num_events = session()->num_events - 1;
}


void eventedit() {
  char s[81];

  if (!ValidateSysopPassword()) {
    return;
  }
  bool done = false;
  do {
    char ch = 0;
    show_events();
    bout.nl();
    bout <<
                       "|#9Events: |#1I|#9nsert, |#1D|#9elete, |#1M|#9odify, e|#1X|#9ecute, |#1Q|#9uit :";
    if (so()) {
      ch = onek("QDIM?X");
    } else {
      ch = onek("QDIM?");
    }
    switch (ch) {
    case '?':
      show_events();
      break;
    case 'Q':
      done = true;
      break;
    case 'X': {
      bout.nl();
      bout << "|#2Run which Event? ";
      input(s, 2);
      int nEventNum = atoi(s);
      if (s[0] != '\0' && nEventNum >= 0 && nEventNum < session()->num_events) {
        run_event(nEventNum);
      }
    }
    break;
    case 'M': {
      bout.nl();
      bout << "|#2Modify which Event? ";
      input(s, 2);
      int nEventNum = atoi(s);
      if (s[0] != '\0' && nEventNum >= 0 && nEventNum < session()->num_events) {
        modify_event(nEventNum);
      }
    }
    break;
    case 'I':
      if (session()->num_events < MAX_EVENT) {
        insert_event();
      } else {
        bout << "\r\n|#6Can't add any more events!\r\n\n";
        pausescr();
      }
      break;
    case 'D':
      if (session()->num_events) {
        bout.nl();
        bout << "|#2Delete which Event? ";
        input(s, 2);
        int nEventNum = atoi(s);
        if (s[0] && nEventNum >= 0 && nEventNum < session()->num_events) {
          bout.nl();
          if (events[nEventNum].status & EVENT_EXIT) {
            sprintf(s, "Exit Level = %d", events[nEventNum].cmd[0]);
          } else {
            strcpy(s, events[nEventNum].cmd);
          }
          bout << "|#5Delete " << s << "?";
          if (yesno()) {
            delete_event(nEventNum);
          }
        }
      } else {
        bout << "\r\n|#6No events to delete!\r\n\n";
        pausescr();
      }
      break;
    }
  } while (!done && !hangup);
  sort_events();

  File eventsFile(syscfg.datadir, EVENTS_DAT);
  if (session()->num_events) {
    eventsFile.Open(File::modeReadWrite | File::modeCreateFile | File::modeBinary | File::modeTruncate);
    eventsFile.Write(events, session()->num_events * sizeof(eventsrec));
    eventsFile.Close();
  } else {
    eventsFile.Delete();
  }
}
