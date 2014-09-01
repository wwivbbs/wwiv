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
#include "paths.h"

#include <curses.h>
#include <cstdint>
#include <memory>
#include <string>
#ifdef _WIN32
#include <direct.h>
#endif

#include "ifcns.h"
#include "init.h"
#include "input.h"
#include "core/wwivport.h"
#include "core/wfndfile.h"
#include "utility.h"
#include "wwivinit.h"

using std::auto_ptr;
using std::string;

extern char bbsdir[];

static int verify_dir(char *typeDir, char *dirName) {
  int rc = 0;
  char s[81], ch;

  WFindFile fnd;
  fnd.open(dirName, 0);

  if (fnd.next() && fnd.IsDirectory()) {
    out->window()->GotoXY(0, 8);
    sprintf(s, "The %s directory: %s is invalid!", typeDir, dirName);
    out->SetColor(SchemeId::ERROR_TEXT);
    out->window()->Puts(s);
    for (unsigned int i = 0; i < strlen(s); i++) {
      out->window()->Printf("\b \b");
    }
    if ((strcmp(typeDir, "Temporary") == 0) || (strcmp(typeDir, "Batch") == 0)) {
      sprintf(s, "Create %s? ", dirName);
      out->SetColor(SchemeId::PROMPT);
      out->window()->Puts(s);
      ch = out->window()->GetChar();
      if (toupper(ch) == 'Y') {
        mkdir(dirName);
      }
      for (unsigned int j = 0; j < strlen(s); j++) {
        out->window()->Printf("\b \b");
      }
    }
    out->SetColor(SchemeId::PROMPT);
    out->window()->Puts("<ESC> when done.");
    rc = 1;
  }
  chdir(bbsdir);
  return rc;
}

class FilePathItem : public EditItem<char*> {
public:
  FilePathItem(int x, int y, int maxsize, char* data) 
    : EditItem<char*>(x, y, maxsize, data) {}
  virtual ~FilePathItem() {}

  virtual int Run(CursesWindow* window) override {
    window->GotoXY(this->x_, this->y_);
    int return_code = 0;
    editline(window, this->data_, this->maxsize_, EDITLINE_FILENAME_CASE, &return_code, "");
    trimstrpath(this->data_);
    return return_code;
  }

protected:
  virtual void DefaultDisplay(CursesWindow* window) const override {
    std::string blanks(this->maxsize_, ' ');
    window->PutsXY(this->x_, this->y_, blanks.c_str());

    char pattern[81];
    sprintf(pattern, "%%-%ds", this->maxsize_);
    window->PrintfXY(this->x_, this->y_, pattern, this->data_);
  }
};

static const int COL1_LINE = 2;
static const int COL1_POSITION = 14;

/* change msgsdir, gfilesdir, datadir, dloadsdir, ramdrive, tempdir */
void setpaths() {
  out->Cls(ACS_CKBOARD);
  auto_ptr<CursesWindow> window(out->CreateBoxedWindow("System Paths", 15, 76));

  int y = 1;
  window->PrintfXY(COL1_LINE, y++, "Messages  : %s", syscfg.msgsdir);
  window->PrintfXY(COL1_LINE, y++, "GFiles    : %s", syscfg.gfilesdir);
  window->PrintfXY(COL1_LINE, y++, "Menus     : %s", syscfg.menudir);
  window->PrintfXY(COL1_LINE, y++, "Data      : %s", syscfg.datadir);
  window->PrintfXY(COL1_LINE, y++, "Downloads : %s", syscfg.dloadsdir);
  y+=2;
  window->SetColor(SchemeId::WARNING);
  window->PrintfXY(COL1_LINE, y++, "CAUTION: ONLY EXPERIENCED SYSOPS SHOULD MODIFY THESE SETTINGS.");
  y+=1;
  window->SetColor(SchemeId::WINDOW_TEXT);
  window->PrintfXY(COL1_LINE + 2, y++, "Changing any of these requires YOU to MANUALLY move files and / or");
  window->PrintfXY(COL1_LINE + 2, y++, "directory structures.  Consult the documentation prior to changing");
  window->PrintfXY(COL1_LINE + 2, y++, "any of these settings.");

  EditItems items{
    new FilePathItem(COL1_POSITION, 1, 60, syscfg.msgsdir),
    new FilePathItem(COL1_POSITION, 2, 60, syscfg.gfilesdir),
    new FilePathItem(COL1_POSITION, 3, 60, syscfg.menudir),
    new FilePathItem(COL1_POSITION, 4, 60, syscfg.datadir),
    new FilePathItem(COL1_POSITION, 5, 60, syscfg.dloadsdir),
  };

  items.set_curses_io(out, window.get());
  items.Run();

  save_config();
}

