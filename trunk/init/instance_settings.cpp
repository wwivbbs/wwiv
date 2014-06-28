/**************************************************************************/
/*                                                                        */
/*                 WWIV Initialization Utility Version 5.0                */
/*                 Copyright (C)2014, WWIV Software Services              */
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
#include "instance_settings.h"

#include <curses.h>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#endif
#include <string>
#include <sys/stat.h>

#include "ifcns.h"
#include "init.h"
#include "input.h"
#include "platform/incl1.h"
#include "wwivinit.h"

static const int COL1_POSITION = 21;
static const int PROMPT_LINE = 6;

#if __unix__
#define FILENAME_UPPERCASE false
#else
#define FILENAME_UPPERCASE true
#endif  // __unix__

void show_instance(EditItems* items) {
  items->Display();
}

static void show_help() {

  // Additions
  out->SetColor(out->footer(), Scheme::FOOTER_KEY);
  waddstr(out->footer(), "A");
  out->SetColor(out->footer(), Scheme::FOOTER_TEXT);
  waddstr(out->footer(), "-Add ");
}

int number_instances() {
  int configfile = open("config.ovr", O_RDWR | O_BINARY);
  if (configfile < 0) {
    return 0;
  }
  long length = filelength(configfile);
  close(configfile);
  return length / sizeof(configoverrec);
}

bool read_instance(int num, configoverrec* instance) {
  int configfile = open("config.ovr", O_RDWR | O_BINARY);
  if (configfile < 0) {
    return false;
  }
  lseek(configfile, (num - 1) * sizeof(configoverrec), SEEK_SET);
  read(configfile, instance, sizeof(configoverrec));
  close(configfile);
  return true;
}

bool write_instance(int num, configoverrec* instance) {
  int configfile = open("config.ovr", O_RDWR | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);
  if (configfile > 0) {
    int n = filelength(configfile) / sizeof(configoverrec);
    while (n < (num - 1)) {
      lseek(configfile, 0L, SEEK_END);
      write(configfile, instance, sizeof(configoverrec));
      n++;
    }
    lseek(configfile, sizeof(configoverrec) * (num - 1), SEEK_SET);
    write(configfile, instance, sizeof(configoverrec));
    close(configfile);
  }
  return true;
}

static void tweak_dir(char *s, int inst) {
  if (inst == 1) {
    return;
  }

  int i = strlen(s);
  if (i == 0) {
    sprintf(s, "temp%d", inst);
  } else {
    char *lcp = s + i - 1;
    while ((((*lcp >= '0') && (*lcp <= '9')) || (*lcp == WWIV_FILE_SEPERATOR_CHAR)) && (lcp >= s)) {
      lcp--;
    }
    sprintf(lcp + 1, "%d%c", inst, WWIV_FILE_SEPERATOR_CHAR);
  }
}

void instance_editor() {
  configoverrec instance;
  int num_instances = number_instances();
  out->Cls();

  out->SetColor(Scheme::NORMAL);
  Printf("Temporary Directory: %s\n", syscfgovr.tempdir);
  Printf("Batch Directory    : %s\n", syscfgovr.batchdir);

  int current_instance = 1;
  read_instance(current_instance, &instance);

  EditItems items{
    new StringEditItem<char*>(COL1_POSITION, 0, 50, instance.tempdir, FILENAME_UPPERCASE),
    new StringEditItem<char*>(COL1_POSITION, 1, 50, instance.batchdir, FILENAME_UPPERCASE),
  };
  items.set_additional_helpfn(show_help);

  show_instance(&items);

  for (;;)  {
    PutsXY(0, PROMPT_LINE, "Command: ");
    char ch = onek("\033AQ[]{}\r");
    switch (ch) {
    case '\r': {
      //clear_help(14 + 1);
      items.Run();
      if (dialog_yn("Save Instance")) {
        write_instance(current_instance, &instance);
      }
      move(PROMPT_LINE, 0); 
      wclrtoeol(out->window());
      out->Refresh();
    } break;
    case 'A': {
      num_instances++;
      tweak_dir(instance.tempdir, num_instances);
      tweak_dir(instance.batchdir, num_instances);
      write_instance(num_instances, &instance);
    } break;
    case 'Q':
    case '\033': {
      werase(out->footer());
      return;
    }
    case ']':
      if (++current_instance > num_instances) {
        current_instance = 1;
      }
      break;
    case '[': {
      if (--current_instance < 1) {
        current_instance = num_instances;
      }
    } break;
    case '}':
      current_instance += 10;
      if (current_instance > num_instances) {
        current_instance = 1;
      }
      break;
    case '{':
      current_instance -= 10;
      if (current_instance < 1) {
        current_instance = num_instances;
      }
      break;
    }

    read_instance(current_instance, &instance);
    show_instance(&items);
  }
}

