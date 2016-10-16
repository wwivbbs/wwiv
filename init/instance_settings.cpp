/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*               Copyright (C)2014-2016 WWIV Software Services            */
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
#include "init/instance_settings.h"

#include <memory>

#include <curses.h>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#endif
#include <string>
#include <vector>
#include <sys/stat.h>

#include "core/inifile.h"
#include "core/file.h"
#include "core/wwivport.h"
#include "init/init.h"
#include "init/wwivinit.h"
#include "localui/input.h"

#include "sdk/filenames.h"

using std::string;
using std::unique_ptr;
using std::vector;
using namespace wwiv::core;

void show_instance(EditItems* items) {
  items->Display();
}

void instance_editor() {
  IniFile ini("wwiv.ini", {"WWIV"});
  string temp(ini.value<string>("TEMP_DIRECTORY"));
  if (ini.IsOpen() && !temp.empty()) {
    messagebox(out->window(), "TEMP_DIRECTORY must be set in WWIV.INI");
    return;
  }

  string batch(ini.value<string>("BATCH_DIRECTORY", temp));
  int num_instances = ini.value<int>("NUM_INSTANCES", 4);

  out->Cls(ACS_CKBOARD);
  unique_ptr<CursesWindow> window(out->CreateBoxedWindow("Temporary Directory Configuration", 10, 76));

  window->PrintfXY(2, 1, "Temporary Dir Pattern : %s", temp.c_str());
  window->PrintfXY(2, 2, "Batch Dir Pattern     : %s", batch.c_str());
  window->PrintfXY(2, 3, "Number of Instances:  : %d", num_instances);

  window->SetColor(SchemeId::WINDOW_DATA);
  window->PrintfXY(2, 5, "To change these values please edit 'wwiv.ini'");

  window->SetColor(SchemeId::WINDOW_PROMPT);
  window->PrintfXY(2, 7, "Press Any Key");
  window->GetChar();
}
