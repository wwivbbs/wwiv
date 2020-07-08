/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2020, WWIV Software Services             */
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
#include "bbs/extract.h"

#include "bbs/bbs.h"
#include "bbs/com.h"
#include "bbs/input.h"
#include "bbs/misccmd.h"
#include "bbs/printfile.h"
#include "bbs/xfer.h"
#include "core/log.h"
#include "core/strings.h"
#include "local_io/keycodes.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include <string>

using std::string;
using namespace wwiv::core;
using namespace wwiv::strings;

void extract_out(char *b, long len, const std::string& title) {
  // TODO Fix platform specific path issues...

  CHECK_NOTNULL(b);
  char s1[81], s2[81], s3[255], ch = 26, ch1, s4[81];

  print_help_file(MEXTRACT_NOEXT);
  bool done = false;
  bool uued = false;
  do {
    uued = false;
    s1[0] = 0;
    s2[0] = 0;
    s3[0] = 0;
    done = true;
    bout << "|#5Which (2-4,Q,?): ";
    ch1 = onek("Q234?");
    switch (ch1) {
    case '2':
      to_char_array(s2, a()->config()->gfilesdir());
      break;
    case '3':
      to_char_array(s2, a()->config()->datadir());
      break;
    case '4':
      to_char_array(s2, a()->temp_directory());
      break;
    case '?':
      print_help_file(MEXTRACT_NOEXT);
      done = false;
      break;
    }
  } while (!done && !a()->hangup_);

  if (s2[0]) {
    do {
      bout << "|#2Save under what filename? ";
      input(s1, 50);
      if (s1[0]) {
        if ((strchr(s1, ':')) || (strchr(s1, '\\'))) {
          strcpy(s3, s1);
        } else {
          sprintf(s3, "%s%s", s2, s1);
        }

        strcpy(s4, s2);

        if (strstr(s3, ".UUE") != nullptr) {
          bout << "|#1UUEncoded File.  Save Output File As? ";

          input(s1, 30);
          if (strchr(s1, '.') == nullptr) {
            strcat(s1, ".MOD");
          }

          strcat(s4, s1);
          uued = true;

          if (File::Exists(s4)) {
            bout << s1 << s4 << " already exists!\r\n";
            uued = false;
          }
        }

        if (File::Exists(s3)) {
          bout << "\r\nFilename already in use.\r\n\n";
          bout << "|#0O|#1)verwrite, |#0A|#1)ppend, |#0N|#1)ew name, |#0Q|#1)uit? |#0";
          ch1 = onek("QOAN");
          switch (ch1) {
          case 'Q':
            s3[0] = 1;
            s1[0] = 0;
            break;
          case 'N':
            s3[0] = 0;
            break;
          case 'A':
            break;
          case 'O':
            File::Remove(s3);
            break;
          }
          bout.nl();
        }
      } else {
        s3[0] = 1;
      }
    } while (!a()->hangup_ && s3[0] == '\0');

    if (s3[0] && !a()->hangup_) {
      if (s3[0] != '\x01') {
        File file(s3);
        if (!file.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite)) {
          bout << "|#6Could not open file for writing.\r\n";
        } else {
          if (file.length() > 0) {
            file.Seek(-1L, File::Whence::end);
            file.Read(&ch1, 1);
            if (ch1 == CZ) {
              file.Seek(-1L, File::Whence::end);
            }
          }
          file.Write(title.c_str(), title.size());
          file.Write("\r\n", 2);
          file.Write(b, len);
          file.Write(&ch, 1);
          file.Close();
          bout <<  "|#9Message written to|#0: |#2" << s3 << wwiv::endl;
          if (uued == true) {
            uudecode(s3, s4);
          }
        }
      }
    }
  }
}

