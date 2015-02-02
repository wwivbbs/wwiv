/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*                Copyright (C)2015, WWIV Software Services               */
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
#include "bbs/capture.h"

#include <cstddef>
#include "bbs/bbs.h"
#include "bbs/datetime.h"
#include "bbs/vars.h"
#include "core/file.h"
#include "core/strings.h"

using namespace wwiv::strings;

namespace wwiv {
namespace bbs {

static const std::size_t GLOBAL_SIZE = 4096;

Capture::Capture() : wx_(0) {}
Capture::~Capture() {}

void Capture::set_global_handle(bool bOpenFile, bool bOnlyUpdateVariable) {
  if (x_only) {
    return;
  }

  if (bOpenFile) {
    if (!fileGlobalCap.IsOpen()) {
      fileGlobalCap.SetName(StringPrintf("%sglobal-%d.txt", syscfg.gfilesdir, application()->GetInstanceNumber()));
      fileGlobalCap.Open(File::modeBinary | File::modeAppend | File::modeCreateFile | File::modeReadWrite);
      global_buf.clear();
    }
  } else {
    if (fileGlobalCap.IsOpen() && !bOnlyUpdateVariable) {
      fileGlobalCap.Write(global_buf);
      fileGlobalCap.Close();
      global_buf.clear();
    }
  }
}

void Capture::global_char(char ch) {
  if (fileGlobalCap.IsOpen()) {
    global_buf.push_back(ch);
    if (global_buf.size() >= GLOBAL_SIZE) {
      fileGlobalCap.Write(global_buf);
      global_buf.clear();
    }
  }
}

void Capture::set_x_only(bool tf, const char *pszFileName, bool ovwr) {
  static bool bOldGlobalHandle = false;

  if (x_only) {
    if (!tf) {
      if (fileGlobalCap.IsOpen()) {
        fileGlobalCap.Write(global_buf);
        fileGlobalCap.Close();
        global_buf.clear();
      }
      x_only = false;
      set_global_handle(bOldGlobalHandle);
      bOldGlobalHandle = false;
      express = expressabort = false;
    }
  } else {
    if (tf) {
      bOldGlobalHandle = fileGlobalCap.IsOpen();
      set_global_handle(false);
      x_only = true;
      wx_ = 0;
      fileGlobalCap.SetName(syscfgovr.tempdir, pszFileName);

      int mode = File::modeBinary | File::modeText | File::modeCreateFile | File::modeReadWrite;
      if (!ovwr) {
        mode |= File::modeAppend;
      }
      fileGlobalCap.Open(mode);
      global_buf.clear();
      express = true;
      expressabort = false;
      if (!fileGlobalCap.IsOpen()) {
        set_x_only(false, nullptr, false);
      }
    }
  }
  timelastchar1 = timer1();
}

}
}
