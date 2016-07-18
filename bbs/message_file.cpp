/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2016, WWIV Software Services             */
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
#include "bbs/message_file.h"

#include <memory>
#include <string>

#include "bbs/bbs.h"
#include "bbs/vars.h"  // only needed for syscfg
#include "core/file.h"
#include "core/strings.h"
#include "sdk/net.h"
#include "sdk/filenames.h"
#include "sdk/status.h"

/////////////////////////////////////////////////////////////////////////////
//
// NOTE: This file containts wwiv message base specific code and should move to SDK
//

static constexpr int  GAT_NUMBER_ELEMENTS = 2048;
static constexpr int GAT_SECTION_SIZE = 4096;
static constexpr int MSG_BLOCK_SIZE = 512;

using std::string;
using std::unique_ptr;
using namespace wwiv::sdk;
using namespace wwiv::strings;


static constexpr int  GATSECLEN = GAT_SECTION_SIZE + GAT_NUMBER_ELEMENTS * MSG_BLOCK_SIZE;
#define MSG_STARTING (gat_section * GATSECLEN + GAT_SECTION_SIZE)

static long gat_section;
// this is initialized in xinit, otherwise we can make it static.
// TODO(rushfan): make it static
uint16_t *gat = nullptr;

/**
* Opens the message area file {messageAreaFileName} and returns the file handle.
* Note: This is a Private method to this module.
*/
static File* OpenMessageFile(const string messageAreaFileName) {
  session()->status_manager()->RefreshStatusCache();

  const string filename = StrCat(syscfg.msgsdir, messageAreaFileName, FILENAME_DAT_EXTENSION);
  File *pFileMessage = new File(filename);
  if (!pFileMessage->Open(File::modeReadWrite | File::modeBinary)) {
    // Create message area file if it doesn't exist.
    pFileMessage->Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite);
    for (int i = 0; i < GAT_NUMBER_ELEMENTS; i++) {
      gat[i] = 0;
    }
    pFileMessage->Write(gat, GAT_SECTION_SIZE);
    pFileMessage->SetLength(GAT_SECTION_SIZE + (75L * 1024L));
    gat_section = 0;
  }
  pFileMessage->Seek(0L, File::seekBegin);
  pFileMessage->Read(gat, GAT_SECTION_SIZE);

  gat_section = 0;
  return pFileMessage;
}

static void set_gat_section(File *pMessageFile, int section) {
  if (gat_section != section) {
    long lFileSize = pMessageFile->GetLength();
    long lSectionPos = static_cast<long>(section) * GATSECLEN;
    if (lFileSize < lSectionPos) {
      pMessageFile->SetLength(lSectionPos);
      lFileSize = lSectionPos;
    }
    pMessageFile->Seek(lSectionPos, File::seekBegin);
    if (lFileSize < (lSectionPos + GAT_SECTION_SIZE)) {
      for (int i = 0; i < GAT_NUMBER_ELEMENTS; i++) {
        gat[i] = 0;
      }
      pMessageFile->Write(gat, GAT_SECTION_SIZE);
    } else {
      pMessageFile->Read(gat, GAT_SECTION_SIZE);
    }
    gat_section = section;
  }
}

static void save_gat(File *pMessageFile) {
  long lSectionPos = static_cast<long>(gat_section) * GATSECLEN;
  pMessageFile->Seek(lSectionPos, File::seekBegin);
  pMessageFile->Write(gat, GAT_SECTION_SIZE);
  WStatus *pStatus = session()->status_manager()->BeginTransaction();
  pStatus->IncrementFileChangedFlag(WStatus::fileChangePosts);
  session()->status_manager()->CommitTransaction(pStatus);
}


/**
* Deletes a message
* This is a public function.
*/
void remove_link(messagerec * pMessageRecord, string fileName) {
  switch (pMessageRecord->storage_type) {
  case 0:
  case 1:
    break;
  case 2:
  {
    unique_ptr<File> file(OpenMessageFile(fileName));
    if (file->IsOpen()) {
      set_gat_section(file.get(), static_cast<int>(pMessageRecord->stored_as / GAT_NUMBER_ELEMENTS));
      long lCurrentSection = pMessageRecord->stored_as % GAT_NUMBER_ELEMENTS;
      while (lCurrentSection > 0 && lCurrentSection < GAT_NUMBER_ELEMENTS) {
        long lNextSection = static_cast<long>(gat[lCurrentSection]);
        gat[lCurrentSection] = 0;
        lCurrentSection = lNextSection;
      }
      save_gat(file.get());
      file->Close();
    }
  }
  break;
  default:
    // illegal storage type
    break;
  }
}

void savefile(const std::string& text, messagerec * pMessageRecord, const string fileName) {
  switch (pMessageRecord->storage_type) {
  case 0:
  case 1:
    break;
  case 2:
  {
    int gati[128];
    memset(&gati, 0, sizeof(gati));
    unique_ptr<File> pMessageFile(OpenMessageFile(fileName));
    if (pMessageFile->IsOpen()) {
      for (int section = 0; section < 1024; section++) {
        set_gat_section(pMessageFile.get(), section);
        int gatp = 0;
        int nNumBlocksRequired = static_cast<int>((text.length() + 511L) / MSG_BLOCK_SIZE);
        int i4 = 1;
        while (gatp < nNumBlocksRequired && i4 < GAT_NUMBER_ELEMENTS) {
          if (gat[i4] == 0) {
            gati[gatp++] = i4;
          }
          ++i4;
        }
        if (gatp >= nNumBlocksRequired) {
          gati[gatp] = -1;
          for (int i = 0; i < nNumBlocksRequired; i++) {
            pMessageFile->Seek(MSG_STARTING + MSG_BLOCK_SIZE * static_cast<long>(gati[i]), File::seekBegin);
            pMessageFile->Write((&text[i * MSG_BLOCK_SIZE]), MSG_BLOCK_SIZE);
            gat[gati[i]] = static_cast<uint16_t>(gati[i + 1]);
          }
          save_gat(pMessageFile.get());
          break;
        }
      }
      pMessageFile->Close();
    }
    pMessageRecord->stored_as = static_cast<long>(gati[0]) + static_cast<long>(gat_section) * GAT_NUMBER_ELEMENTS;
  }
  break;
  default:
  {
    bout.bprintf("WWIV:ERROR:msgbase.cpp: Save - storage_type=%u!\r\n", pMessageRecord->storage_type);
  }
  break;
  }
}

bool readfile(messagerec * pMessageRecord, string fileName,string* out) {
  out->clear();
  if (pMessageRecord->storage_type != 2) {
    return false;
  }
  unique_ptr<File> file(OpenMessageFile(fileName));
  set_gat_section(file.get(), pMessageRecord->stored_as / GAT_NUMBER_ELEMENTS);
  int current_section = pMessageRecord->stored_as % GAT_NUMBER_ELEMENTS;
  long lMessageLength = 0;
  while (current_section > 0 && current_section < GAT_NUMBER_ELEMENTS) {
    lMessageLength += MSG_BLOCK_SIZE;
    current_section = gat[current_section];
  }
  if (lMessageLength == 0) {
    bout << "\r\nNo message found.\r\n\n";
    return false;
  }

  current_section = pMessageRecord->stored_as % GAT_NUMBER_ELEMENTS;
  while (current_section > 0 && current_section < GAT_NUMBER_ELEMENTS) {
    file->Seek(MSG_STARTING + MSG_BLOCK_SIZE * static_cast<uint32_t>(current_section), File::seekBegin);
    char b[MSG_BLOCK_SIZE + 1];
    file->Read(b, MSG_BLOCK_SIZE);
    b[MSG_BLOCK_SIZE] = 0;
    out->append(b);
    current_section = gat[current_section];
  }
  file->Close();
  string::size_type last_cz = out->find_last_of(CZ);
  std::string::size_type last_block_start = out->length() - MSG_BLOCK_SIZE;
  if (last_cz != string::npos && last_block_start >= 0 && last_cz > last_block_start) {
    // last block has a Control-Z in it.  Make sure we add a 0 after it.
    out->resize(last_cz);
  }
  return true;
}

void lineadd(messagerec* pMessageRecord, const string& sx, string fileName) {
  const string line = StringPrintf("%s\r\n\x1a", sx.c_str());

  switch (pMessageRecord->storage_type) {
  case 0:
  case 1:
    break;
  case 2:
  {
    unique_ptr<File> message_file(OpenMessageFile(fileName));
    set_gat_section(message_file.get(), pMessageRecord->stored_as / GAT_NUMBER_ELEMENTS);
    int new1 = 1;
    while (new1 < GAT_NUMBER_ELEMENTS && gat[new1] != 0) {
      ++new1;
    }
    int i = static_cast<int>(pMessageRecord->stored_as % GAT_NUMBER_ELEMENTS);
    while (gat[i] != 65535) {
      i = gat[i];
    }
    char *b = nullptr;
    // The +1 may not be needed but BbsAllocA did it.
    if ((b = static_cast<char*>(calloc(GAT_NUMBER_ELEMENTS + 1, 1))) == nullptr) {
      message_file->Close();
      return;
    }
    message_file->Seek(MSG_STARTING + static_cast<long>(i) * MSG_BLOCK_SIZE, File::seekBegin);
    message_file->Read(b, MSG_BLOCK_SIZE);
    int j = 0;
    while (j < MSG_BLOCK_SIZE && b[j] != CZ) {
      ++j;
    }
    strcpy(&(b[j]), line.c_str());
    message_file->Seek(MSG_STARTING + static_cast<long>(i) * MSG_BLOCK_SIZE, File::seekBegin);
    message_file->Write(b, MSG_BLOCK_SIZE);
    if (((j + line.size()) > MSG_BLOCK_SIZE) && (new1 != GAT_NUMBER_ELEMENTS)) {
      message_file->Seek(MSG_STARTING + static_cast<long>(new1)  * MSG_BLOCK_SIZE, File::seekBegin);
      message_file->Write(b + MSG_BLOCK_SIZE, MSG_BLOCK_SIZE);
      gat[new1] = 65535;
      gat[i] = static_cast<uint16_t>(new1);
      save_gat(message_file.get());
    }
    free(b);
    message_file->Close();
  }
  break;
  default:
    // illegal storage type
    break;
  }
}
