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
#include <qwk.h>

#include <memory>
#include <string>

#include <ctype.h>
#include <fcntl.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#endif
#include <sys/stat.h>

#include "bbs/instmsg.h"
#include "bbs/pause.h"
#include "bbs/qscan.h"
#include "bbs/subxtr.h"
#include "bbs/subxtr.h"
#include "bbs/vardec.h"
#include "bbs/wwiv.h"
#include "bbs/wconstants.h"
#include "bbs/wwivcolors.h"

#include "core/strings.h"


// from msgbase.cpp
long current_gat_section();
void current_gat_section(long section);

// Also used in qwk1.cpp
const char *QWKFrom = "\x04""0QWKFrom:";
int qwk_bi_mode;

static int qwk_percent;
static uint16_t max_msgs;

// from xfer.cpp
extern int this_date;

#ifndef _WIN32
long filelength(int handle) {
  struct stat fileinfo;
  if (fstat(handle, &fileinfo) != 0) {
    return -1;
  }
  return fileinfo.st_size;
}
#else  // _WIN32

#if !defined(ftruncate)
#define ftruncate chsize
#endif

#endif  // _WIN32

using std::string;
using wwiv::bbs::SaveQScanPointers;
using wwiv::bbs::TempDisablePause;

static bool replacefile(char *src, char *dst, bool stats) {
  if (strlen(dst) == 0) {
    return false;
  }

  if (WFile::Exists(dst)) {
    WFile::Remove(dst);
  }

  return copyfile(src, dst, stats);
}

static void strip_heart_colors(char *text) {
  int pos = 0;
  int len = strlen(text);

  while (pos < len && text[pos] != 26 && !hangup) {
    if (text[pos] == 3) {
      memmove(text + pos, text + pos + 2, len - pos);
      --len;
      --len;
    } else {
      ++pos;
    }
  }
}

void build_qwk_packet(void) {
  struct qwk_junk qwk_info;
  struct qwk_config qwk_cfg;
  char filename[201];
  int i, msgs_ok;
  bool save_conf = false;
  SaveQScanPointers save_qscan;

  remove_from_temp("*.*", QWK_DIRECTORY, 0);

  if ((uconfsub[1].confnum != -1) && (okconf(GetSession()->GetCurrentUser()))) {
    save_conf = true;
    tmp_disable_conf(true);
  }
  TempDisablePause disable_pause;

  read_qwk_cfg(&qwk_cfg);
  max_msgs = qwk_cfg.max_msgs;
  if (GetSession()->GetCurrentUser()->data.qwk_max_msgs < max_msgs && GetSession()->GetCurrentUser()->data.qwk_max_msgs) {
    max_msgs = GetSession()->GetCurrentUser()->data.qwk_max_msgs;
  }

  if (!qwk_cfg.fu) {
    qwk_cfg.fu = time(nullptr);
  }

  ++qwk_cfg.timesd;
  write_qwk_cfg(&qwk_cfg);
  close_qwk_cfg(&qwk_cfg);

  write_inst(INST_LOC_QWK, usub[GetSession()->GetCurrentMessageArea()].subnum, INST_FLAGS_ONLINE);

  sprintf(filename, "%sMESSAGES.DAT", QWK_DIRECTORY);
  qwk_info.file = open(filename, O_RDWR | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);

  if (qwk_info.file < 1) {
    GetSession()->bout.Write("Open error");
    sysoplog("Couldn't open MESSAGES.DAT");
    return;
  }

  // Setup index and other values
  qwk_info.index = -1;
  qwk_info.cursub = -1;

  qwk_info.in_email = 0;
  qwk_info.personal = -1;
  qwk_info.zero = -1;

  // Required header at the start of MESSAGES.DAT
  write(qwk_info.file, "Produced by Qmail...Copyright (c) 1987 by Sparkware.  All Rights Reserved (For Compatibility with Qmail)                        ", 128);

  // Logical record number
  qwk_info.qwk_rec_num = 1;
  qwk_info.qwk_rec_pos = 2;

  qwk_info.abort = 0;

  if (!GetSession()->GetCurrentUser()->data.qwk_dont_scan_mail && !qwk_info.abort) {
    qwk_gather_email(&qwk_info);
  }

  checka(&qwk_info.abort);

  GetSession()->bout.ClearScreen();
  if (!qwk_info.abort) {
    GetSession()->bout << "|#7\xDA" << string(4, '\xC4') << '\xC2' << string(60, '\xC4') << '\xC2' << string(5, '\xC4') 
      << '\xC2' << string(4, '\xC4') << '\xBF' << wwiv::endl;
  }

  checka(&qwk_info.abort);

  if (!qwk_info.abort) {
    GetSession()->bout << "|#7\xB3|#2Sub |#7\xB3|#3Sub Name" << string(52, ' ') << "|#7\xB3|#8Total|#7\xB3|#5New |#7\xB3" << wwiv::endl;
  }

  checka(&qwk_info.abort);

  if (!qwk_info.abort) {
    GetSession()->bout << "|#7xC3" << string(4, '\xC4') << '\xC5' << string(60, '\xC4') << '\xC5' << string(5, '\xC4') 
        << '\xC5' << string(4, '\xC4') << '\xB4' << wwiv::endl;
  }

  msgs_ok = 1;

  for (i = 0; (usub[i].subnum != -1) && (i < GetSession()->num_subs) && (!hangup) && !qwk_info.abort && msgs_ok; i++) {
    msgs_ok = (max_msgs ? qwk_info.qwk_rec_num <= max_msgs : 1);
    if (qsc_q[usub[i].subnum / 32] & (1L << (usub[i].subnum % 32))) {
      qwk_gather_sub(i, &qwk_info);
    }
  }

  GetSession()->bout << "|#7\xC3" << string(4, '\xC4') << '\xC5' << string(60, '\xC4') << '\xC5' << string(5, '\xC4') 
      << '\xC5' << string(4, '\xC4') << '\xB4' << wwiv::endl;
  GetSession()->bout.NewLine(2);

  if (qwk_info.abort) {
    GetSession()->bout.Color(1);
    GetSession()->bout.WriteFormatted("Abort everything? (NO=Download what I have gathered)");
    if (!yesno()) {
      qwk_info.abort = 0;
    }
  }

  if (qwk_info.file != -1) {
    qwk_info.file = close(qwk_info.file);
  }
  if (qwk_info.index != -1) {
    qwk_info.index = close(qwk_info.index);
  }
  if (qwk_info.personal != -1) {
    qwk_info.personal = close(qwk_info.personal);
  }
  if (qwk_info.zero != -1) {
    qwk_info.zero = close(qwk_info.zero);
  }
  if (!qwk_info.abort) {
    build_control_dat(&qwk_info);
  }
  if (!qwk_info.abort) {
    finish_qwk(&qwk_info);
  }

  // Restore on hangup too, someone might have hungup in the middle of building the list
  if (qwk_info.abort || GetSession()->GetCurrentUser()->data.qwk_dontsetnscan || hangup) {
    save_qscan.restore();
    if (qwk_info.abort) {
      sysoplog("Aborted");
    }
  }
  if (GetSession()->GetCurrentUser()->data.qwk_delete_mail && !qwk_info.abort) {
    qwk_remove_email();  // Delete email
  }

  if (save_conf) {
    tmp_disable_conf(false);
  }
}

void qwk_gather_sub(int bn, struct qwk_junk *qwk_info) {
  int i, os;
  char subinfo[201], thissub[81];

  float temp_percent;
  int sn = usub[bn].subnum;

  if (hangup || (sn < 0)) {
    return;
  }

  uint32_t qscnptrx = qsc_p[sn];
  uint32_t sd = GetSession()->m_SubDateCache[sn];

  if (qwk_percent || ((!sd) || (sd > qscnptrx))) {
    os = GetSession()->GetCurrentMessageArea();
    GetSession()->SetCurrentMessageArea(bn);
    i = 1;

    // Get total amount of messages in base
    if (!qwk_iscan(GetSession()->GetCurrentMessageArea())) {
      return;
    }

    qscnptrx = qsc_p[sn];

    if (!qwk_percent) {
      // Find out what message number we are on
      for (i = GetSession()->GetNumMessagesInCurrentMessageArea(); (i > 1) && (get_post(i - 1)->qscan > qscnptrx); i--)
        ;
    } else { // Get last qwk_percent of messages in sub
      temp_percent = static_cast<float>(qwk_percent) / 100;
      if (temp_percent > 1.0) {
        temp_percent = 1.0;
      }
      i = GetSession()->GetNumMessagesInCurrentMessageArea() - 
          static_cast<int>(temp_percent * GetSession()->GetNumMessagesInCurrentMessageArea());
    }

    strncpy(thissub, subboards[GetSession()->GetCurrentReadMessageArea()].name, 65);
    thissub[60] = 0;
    sprintf(subinfo, "|#7\xB3|#9%-4d|#7\xB3|#1%-60s|#7\xB3 |#2%-4d|#7\xB3|#3%-4d|#7\xB3",
            bn + 1, thissub, GetSession()->GetNumMessagesInCurrentMessageArea(),
            GetSession()->GetNumMessagesInCurrentMessageArea() - i + 1 - (qwk_percent ? 1 : 0));
    GetSession()->bout.Write(subinfo);
    GetSession()->bout.NewLine();

    checka(&qwk_info->abort);

    if ((GetSession()->GetNumMessagesInCurrentMessageArea() > 0)
        && (i <= GetSession()->GetNumMessagesInCurrentMessageArea()) && !qwk_info->abort) {
      if ((get_post(i)->qscan > qsc_p[GetSession()->GetCurrentReadMessageArea()]) || qwk_percent) {
        qwk_start_read(i, qwk_info);  // read messsage
      }
    }

    std::unique_ptr<WStatus> pStatus(GetApplication()->GetStatusManager()->GetStatus());
    qsc_p[GetSession()->GetCurrentReadMessageArea()] = pStatus->GetQScanPointer() - 1;
    GetSession()->SetCurrentMessageArea(os);
  } else {
    os = GetSession()->GetCurrentMessageArea();
    GetSession()->SetCurrentMessageArea(bn);
    i = 1;

    qwk_iscan(GetSession()->GetCurrentMessageArea());

    strncpy(thissub, subboards[GetSession()->GetCurrentReadMessageArea()].name, 65);
    thissub[60] = 0;
    sprintf(subinfo, "|#7\xB3|#9%-4d|#7\xB3|#1%-60s|#7\xB3 |#2%-4d|#7\xB3|#3%-4d|#7\xB3",
            bn + 1, thissub, GetSession()->GetNumMessagesInCurrentMessageArea(), 0);
    GetSession()->bout.Write(subinfo);
    GetSession()->bout.NewLine();

    GetSession()->SetCurrentMessageArea(os);

    checka(&qwk_info->abort);
  }
  GetSession()->bout.Color(0);
}

void qwk_start_read(int msgnum, struct qwk_junk *qwk_info) {
  int amount = 1;

  irt[0] = 0;
  irt_name[0] = 0;
  int done = 0;
  int val = 0;

  if (GetSession()->GetCurrentReadMessageArea() < 0) {
    return;
  }
  // Used to be inside do loop
  if (xsubs[GetSession()->GetCurrentReadMessageArea()].num_nets) {
    set_net_num(xsubs[GetSession()->GetCurrentReadMessageArea()].nets[0].net_num);
  } else {
    set_net_num(0);
  }

  do {
    if ((msgnum > 0) && (msgnum <= GetSession()->GetNumMessagesInCurrentMessageArea())) {
      make_pre_qwk(msgnum, &val, qwk_info);
    }
    ++msgnum;
    if (msgnum > GetSession()->GetNumMessagesInCurrentMessageArea()) {
      done = 1;
    }
    if (GetSession()->GetCurrentUser()->data.qwk_max_msgs_per_sub ? amount >
        GetSession()->GetCurrentUser()->data.qwk_max_msgs_per_sub : 0) {
      done = 1;
    }
    if (max_msgs ? qwk_info->qwk_rec_num > max_msgs : 0) {
      done = 1;
    }
    ++amount;
    checka(&qwk_info->abort);

  } while (!done && !hangup && !qwk_info->abort);
  bputch('\r');
}

void make_pre_qwk(int msgnum, int *val, struct qwk_junk *qwk_info) {
  postrec* p = get_post(msgnum);
  if (p->status & (status_unvalidated | status_delete)) {
    if (!lcs()) {
      return;
    }
    *val |= 1;
  }

  int nn = GetSession()->GetNetworkNumber();
  if (p->status & status_post_new_net) {
    set_net_num(p->title[80]);
  }

  put_in_qwk(p, (subboards[GetSession()->GetCurrentReadMessageArea()].filename), msgnum, qwk_info);
  if (nn != GetSession()->GetNetworkNumber()) {
    set_net_num(nn);
  }

  GetSession()->GetCurrentUser()->SetNumMessagesRead(GetSession()->GetCurrentUser()->GetNumMessagesRead() + 1);
  GetSession()->SetNumMessagesReadThisLogon(GetSession()->GetNumMessagesReadThisLogon() + 1);

  if (p->qscan > qsc_p[GetSession()->GetCurrentReadMessageArea()]) { // Update qscan pointer right here
    qsc_p[GetSession()->GetCurrentReadMessageArea()] = p->qscan;  // And here
  }
  WStatus* pStatus = GetApplication()->GetStatusManager()->GetStatus();
  uint32_t lQScanPtr = pStatus->GetQScanPointer();
  delete pStatus;
  if (p->qscan >= lQScanPtr) {
    WStatus* pStatus = GetApplication()->GetStatusManager()->BeginTransaction();
    pStatus->SetQScanPointer(p->qscan + 1);
    GetApplication()->GetStatusManager()->CommitTransaction(pStatus);
  }
}

void put_in_qwk(postrec *m1, const char *fn, int msgnum, struct qwk_junk *qwk_info) {
  char n[205], d[81], temp[101];
  char qwk_address[201];
  char filename[101], date[10];

  postrec* pr = get_post(msgnum);
  if (pr == nullptr) {
    return;
  }

  if (pr->status & (status_unvalidated | status_delete)) {
    if (!lcs()) {
      return;
    }
  }
  memset(&qwk_info->qwk_rec, ' ', sizeof(qwk_info->qwk_rec));
  messagerec m = (m1->msg);
  int cur = 0;

  long len;
  char* ss = readfile(&m, fn, &len);

  if (ss == nullptr) {
    GetSession()->bout.WriteFormatted("File not found.");
    GetSession()->bout.NewLine();
    return;
  }

  int p = 0;
  // n = name...
  while ((ss[p] != 13) && ((long)p < len) && (p < 200) && !hangup) {
    n[p] = ss[p];
    p++;
  }
  n[p] = 0;
  ++p;

  // if next is ascii 10 (linefeed?) go one more...
  int p1 = 0;
  if (ss[p] == 10) {
    ++p;
  }

  // d = date
  while ((ss[p + p1] != 13) && ((long)p + p1 < len) && (p1 < 60) && !hangup) {
    d[p1] = ss[(p1) + p];
    p1++;
  }
  d[p1] = 0;

  // ss+cur now is where the text starts
  cur = p + p1 + 1;

  sprintf(qwk_address, "%s%s", QWKFrom, n);  // Copy wwivnet address to qwk_address
  if (!strchr(qwk_address, '@')) {
    sprintf(temp, "@%d", m1->ownersys);
    strcat(qwk_address, temp);
  }

  // Took the annonomouse stuff out right here
  if (!qwk_info->in_email) {
    strncpy(qwk_info->qwk_rec.to, "ALL", 3);
  } else {
    strncpy(temp, GetSession()->GetCurrentUser()->GetName(), 25);
    temp[25] = 0;
    strupr(temp);

    strncpy(qwk_info->qwk_rec.to, temp, 25);
  }

  strip_heart_colors(n);
  strncpy(qwk_info->qwk_rec.from, strupr(n), 25);

  struct tm *time_now = localtime((time_t *)&m1->daten);
  strftime(date, 10, "%m-%d-%y", time_now);
  strncpy(qwk_info->qwk_rec.date, date, 8);

  p = 0;
  p1 = 0;

  len = len - cur;
  make_qwk_ready(ss + cur, &len, qwk_address);

  int amount_blocks = ((int)len / sizeof(qwk_info->qwk_rec)) + 2;

  // Save Qwk Record
  sprintf(qwk_info->qwk_rec.amount_blocks, "%d", amount_blocks);
  sprintf(qwk_info->qwk_rec.msgnum, "%d", msgnum);

  strip_heart_colors(pr->title);
  strip_heart_colors(qwk_info->email_title);

  if (!qwk_info->in_email) {
    strncpy(qwk_info->qwk_rec.subject, pr->title, 25);
  } else {
    strncpy(qwk_info->qwk_rec.subject, qwk_info->email_title, 25);
  }

  qwk_remove_null((char *) &qwk_info->qwk_rec, 123);
  qwk_info->qwk_rec.conf_num = usub[GetSession()->GetCurrentMessageArea()].subnum + 1;
  qwk_info->qwk_rec.logical_num = qwk_info->qwk_rec_num;

  if (append_block(qwk_info->file, &qwk_info->qwk_rec, sizeof(qwk_info->qwk_rec)) != sizeof(qwk_info->qwk_rec)) {
    qwk_info->abort = 1; // Must be out of disk space
    GetSession()->bout.Write("Write error");
    pausescr();
  }

  // Save Qwk NDX
  qwk_info->qwk_ndx.pos = static_cast<float>(qwk_info->qwk_rec_pos);
  float msbin;
  _fieeetomsbin(&qwk_info->qwk_ndx.pos, &msbin);
  qwk_info->qwk_ndx.pos = msbin;
  qwk_info->qwk_ndx.nouse = 0;

  if (!qwk_info->in_email) { // Only if currently doing messages...
    // Create new index if it hasnt been already
    if (GetSession()->GetCurrentMessageArea() != qwk_info->cursub || qwk_info->index < 0) {
      qwk_info->cursub = GetSession()->GetCurrentMessageArea();
      sprintf(filename, "%s%03d.NDX", QWK_DIRECTORY, usub[GetSession()->GetCurrentMessageArea()].subnum + 1);
      if (qwk_info->index > 0) {
        qwk_info->index = close(qwk_info->index);
      }
      qwk_info->index = open(filename, O_RDWR | O_APPEND | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);
    }

    append_block(qwk_info->index, &qwk_info->qwk_ndx, sizeof(qwk_info->qwk_ndx));
  } else { // Write to email indexes
    append_block(qwk_info->zero, &qwk_info->qwk_ndx, sizeof(qwk_info->qwk_ndx));
    append_block(qwk_info->personal, &qwk_info->qwk_ndx, sizeof(qwk_info->qwk_ndx));
  }

  // Setup next NDX position
  qwk_info->qwk_rec_pos += static_cast<uint16_t>(amount_blocks);

  int cur_block = 2;
  while (cur_block <= amount_blocks && !hangup) {
    int this_pos;
    memset(&qwk_info->qwk_rec, ' ', sizeof(qwk_info->qwk_rec));
    this_pos = ((cur_block - 2) * sizeof(qwk_info->qwk_rec));

    if (this_pos < len) {
      memmove(&qwk_info->qwk_rec, ss + cur + this_pos, this_pos + sizeof(qwk_info->qwk_rec) > (int)len
              ? (int)len - this_pos - 1 : sizeof(qwk_info->qwk_rec));
    }
    // Save this block
    append_block(qwk_info->file, &qwk_info->qwk_rec, sizeof(qwk_info->qwk_rec));

    this_pos += sizeof(qwk_info->qwk_rec);
    ++cur_block;
  }
  // Global variable on total amount of records saved
  ++qwk_info->qwk_rec_num;

  if (ss != nullptr) {
    free(ss);
  }
}

// Takes text, deletes all ascii '10' and converts '13' to '227' (ã)
// And does other conversions as specified
void make_qwk_ready(char *text, long *len, char *address) {
  unsigned pos = 0, new_pos = 0;
  unsigned char x;
  long new_size = *len + PAD_SPACE + 1;

  char* temp = static_cast<char *>(malloc(new_size));

  if (!temp) {
    sysoplog("Couldn't allocate memory to make qwk ready");
    return;
  }
  while (pos < *len && new_pos < new_size && !hangup) {
    x = (unsigned char)text[pos];
    if (x == 0) {
      break;
    } else if (x == 13) {
      temp[new_pos] = '\xE3';
      ++pos;
      ++new_pos;
    } else if (x == 10 || x < 3) {
    // Strip out Newlines, NULLS, 1's and 2's
      ++pos;
    } else if (GetSession()->GetCurrentUser()->data.qwk_remove_color && x == 3) {
      pos += 2;
    } else if (GetSession()->GetCurrentUser()->data.qwk_convert_color && x == 3) {
      char ansi_string[30];
      int save_curatr = curatr;
      curatr = 255;
      // Only convert to ansi if we have memory for it, but still strip heart
      // code even if we don't have the memory.
      if (new_pos + 10 < new_size) {
        makeansi(text[pos + 1], ansi_string, 1);
        temp[new_pos] = 0;
        strcat(temp, ansi_string);
        new_pos = strlen(temp);
      }
      pos += 2;
      curatr = save_curatr;
    } else if (GetSession()->GetCurrentUser()->data.qwk_keep_routing == false && x == 4 && text[pos + 1] == '0') {
      if (text[pos + 1] == 0) {
        ++pos;
      } else {
        while (text[pos] != '\xE3' && text[pos] != '\r' && pos < *len && text[pos] != 0 && !hangup) {
          ++pos;
        }
      }
      ++pos;
      if (text[pos] == '\n') {
        ++pos;
      }
    } else if (x == 4 && text[pos + 1] != '0') {
      pos += 2;
    } else {
      temp[new_pos] = x;
      ++pos;
      ++new_pos;
    }
  }

  temp[new_pos] = 0;
  strcpy(text, temp);
  free(temp);

  *len = new_pos;

  // Only add address if it does not yet exist
  if (!strstr(text, QWKFrom + 2)) { // Don't search for diamond or number, just text after that
    insert_after_routing(text, address, len);
  }
}

void qwk_remove_null(char *memory, int size) {
  int pos = 0;

  while (pos < size && !hangup) {
    if (memory[pos] == 0) {
      (memory)[pos] = ' ';
    }
    ++pos;
  }
}

void build_control_dat(struct qwk_junk *qwk_info) {
  FILE *fp;
  struct qwk_config qwk_cfg;
  int amount = 0;
  int cur = 0;
  char file[201];
  char system_name[20], temp[20];
  char date_time[51];
  time_t secs_now;
  struct tm *time_now;

  time(&secs_now);
  time_now = localtime(&secs_now);

  // Creates a string like 'mm-dd-yyyy,hh:mm:ss'
  strftime(date_time, 50, "%m-%d-%Y,%H:%M:%S", time_now);

  sprintf(file, "%sCONTROL.DAT", QWK_DIRECTORY);
  fp = fopen(file, "wb");

  if (!fp) {
    return;
  }

  read_qwk_cfg(&qwk_cfg);
  qwk_system_name(system_name);

  fprintf(fp, "%s.QWK\r\n", system_name);
  fprintf(fp, "%s\r\n", "");   // System City and State
  fprintf(fp, "%s\r\n", syscfg.systemphone);
  fprintf(fp, "%s\r\n", syscfg.sysopname);
  fprintf(fp, "%s,%s\r\n", "00000", system_name);
  fprintf(fp, "%s\r\n", date_time);
  fprintf(fp, "%s\r\n", GetSession()->GetCurrentUser()->data.name);
  fprintf(fp, "%s\r\n", "");
  fprintf(fp, "%s\r\n", "0");
  fprintf(fp, "%d\r\n", qwk_info->qwk_rec_num);
  
  for (cur = 0; (usub[cur].subnum != -1) && (cur < GetSession()->num_subs) && (!hangup); cur++) {
    if (qsc_q[usub[cur].subnum / 32] & (1L << (usub[cur].subnum % 32))) {
      ++amount;
    }
  }
  fprintf(fp, "%d\r\n", amount);
  
  fprintf(fp, "0\r\n");
  fprintf(fp, "E-Mail\r\n");

  for (cur = 0; (usub[cur].subnum != -1) && (cur < GetSession()->num_subs) && (!hangup); cur++) {
    if (qsc_q[usub[cur].subnum / 32] & (1L << (usub[cur].subnum % 32))) {
      strncpy(temp, stripcolors(subboards[usub[cur].subnum].name), 13);
      temp[13] = 0;

      fprintf(fp, "%d\r\n", usub[cur].subnum + 1);
      fprintf(fp, "%s\r\n", temp);
    }
  }

  fprintf(fp, "%s\r\n", qwk_cfg.hello);
  fprintf(fp, "%s\r\n", qwk_cfg.news);
  fprintf(fp, "%s\r\n", qwk_cfg.bye);

  fclose(fp);
  close_qwk_cfg(&qwk_cfg);
}

int _fmsbintoieee(float *src4, float *dest4) {
  unsigned char *msbin = (unsigned char *)src4;
  unsigned char *ieee = (unsigned char *)dest4;
  unsigned char sign = 0x00;
  unsigned char ieee_exp = 0x00;
  int i;

  /* MS Binary Format                         */
  /* byte order =>    m3 | m2 | m1 | exponent */
  /* m1 is most significant byte => sbbb|bbbb */
  /* m3 is the least significant byte         */
  /*      m = mantissa byte                   */
  /*      s = sign bit                        */
  /*      b = bit                             */

  sign = msbin[2] & 0x80;      /* 1000|0000b  */

  /* IEEE Single Precision Float Format       */
  /*    m3        m2        m1     exponent   */
  /* mmmm|mmmm mmmm|mmmm emmm|mmmm seee|eeee  */
  /*          s = sign bit                    */
  /*          e = exponent bit                */
  /*          m = mantissa bit                */

  for (i = 0; i < 4; i++) {
    ieee[i] = 0;
  }

  /* any msbin w/ exponent of zero = zero */
  if (msbin[3] == 0) {
    return 0;
  }

  ieee[3] |= sign;

  /* MBF is bias 128 and IEEE is bias 127. ALSO, MBF places   */
  /* the decimal point before the assumed bit, while          */
  /* IEEE places the decimal point after the assumed bit.     */

  ieee_exp = msbin[3] - 2;    /* actually, msbin[3]-1-128+127 */

  /* the first 7 bits of the exponent in ieee[3] */
  ieee[3] |= ieee_exp >> 1;

  /* the one remaining bit in first bin of ieee[2] */
  ieee[2] |= ieee_exp << 7;

  /* 0111|1111b : mask out the msbin sign bit */
  ieee[2] |= msbin[2] & 0x7f;

  ieee[1] = msbin[1];
  ieee[0] = msbin[0];

  return 0;
}

int _fieeetomsbin(float *src4, float *dest4) {
  unsigned char *ieee = (unsigned char *)src4;
  unsigned char *msbin = (unsigned char *)dest4;
  unsigned char sign = 0x00;
  unsigned char msbin_exp = 0x00;
  int i;

  /* See _fmsbintoieee() for details of formats   */
  sign = ieee[3] & 0x80;
  msbin_exp |= ieee[3] << 1;
  msbin_exp |= ieee[2] >> 7;

  /* An ieee exponent of 0xfe overflows in MBF    */
  if (msbin_exp == 0xfe) {
    return 1;
  }

  msbin_exp += 2;     /* actually, -127 + 128 + 1 */

  for (i = 0; i < 4; i++) {
    msbin[i] = 0;
  }

  msbin[3] = msbin_exp;

  msbin[2] |= sign;
  msbin[2] |= ieee[2] & 0x7f;
  msbin[1] = ieee[1];
  msbin[0] = ieee[0];

  return 0;
}

char * qwk_system_name(char *qwkname) {
  struct qwk_config qwk_cfg;

  read_qwk_cfg(&qwk_cfg);
  strcpy(qwkname, qwk_cfg.packet_name);
  close_qwk_cfg(&qwk_cfg);

  if (!qwkname[0]) {
    strncpy(qwkname, syscfg.systemname, 8);
  }

  qwkname[8] = 0;
  int x = 0;
  while (qwkname[x] && !hangup && x < 9) {
    if (qwkname[x] == ' ' || qwkname[x] == '.') {
      qwkname[x] = '-';
    }
    ++x;
  }
  qwkname[8] = 0;
  strupr(qwkname);
  return qwkname;
}

void qwk_menu(void) {
  char temp[101], namepath[101];

  qwk_percent = 0;
  qwk_bi_mode = 0;

  bool done = false;
  while (!done && !hangup) {
    GetSession()->bout.ClearScreen();
    printfile("QWK");
    if (so()) {
      GetSession()->bout.Write("1) Sysop QWK config");
    }

    if (qwk_percent) {
      GetSession()->bout.Color(3);
      GetSession()->bout.NewLine();
      GetSession()->bout.WriteFormatted("Of all messages, you will be downloading %d%%\r\n", qwk_percent);
    }
    GetSession()->bout.NewLine();
    strcpy(temp, "7[3Q1DCUBS%");
    if (so()) {
      strcat(temp, "1");
    }
    strcat(temp, "7] ");
    GetSession()->bout.WriteFormatted(temp);
    GetSession()->bout.ColorizedInputField(1);

    strcpy(temp, "Q\r?CDUBS%");
    if (so()) {
      strcat(temp, "1");
    }
    int key = onek(temp);

    switch (key) {
    case '?':
      break;

    case 'Q':
    case '\r':
      done = true;
      break;

    case 'U':
      sysoplog("Upload REP packet");
      qwk_bi_mode = 0;
      upload_reply_packet();
      break;

    case 'D':
      sysoplog("Download QWK packet");
      qwk_system_name(temp);
      strcat(temp, ".REP");
      sprintf(namepath, "%s%s", QWK_DIRECTORY, temp);
      unlink(namepath);

      build_qwk_packet();

      if (WFile::Exists(namepath)) {
        sysoplog("REP was uploaded");
        qwk_bi_mode = 1;
        upload_reply_packet();
      }
      break;

    case 'B':
      sysoplog("Down/Up QWK/REP packet");
      qwk_bi_mode = 1;

      qwk_system_name(temp);
      strcat(temp, ".REP");
      sprintf(namepath, "%s%s", QWK_DIRECTORY, temp);
      unlink(namepath);

      build_qwk_packet();

      if (WFile::Exists(namepath)) {
        upload_reply_packet();
      }
      break;

    case 'S':
      sysoplog("Select Subs");
      config_qscan();
      break;

    case 'C':
      sysoplog("Config Options");
      config_qwk_bw();
      break;

    case '%':
      sysoplog("Set %");
      GetSession()->bout.Color(2);
      GetSession()->bout.WriteFormatted("Enter percent of all messages in all QSCAN subs to pack:");
      GetSession()->bout.ColorizedInputField(3);
      input(temp, 3);
      qwk_percent = atoi(temp);
      if (qwk_percent > 100) {
        qwk_percent = 100;
      }
      break;

    case '1':
      if (so()) {
        sysoplog("Ran Sysop Config");
        qwk_sysop();
      }
      break;
    }
  }
}

void qwk_send_file(const char *fn, bool *sent, bool *abort) {
  // TODO(rushfan): Should this just call send_file from sr.cpp?
  *sent = 0;
  *abort = 0;

  int protocol = -1;
  if (GetSession()->GetCurrentUser()->data.qwk_protocol <= 1 || qwk_bi_mode) {
    if (qwk_bi_mode) {
      protocol = get_protocol(xf_bi);
    } else {
      protocol = get_protocol(xf_down_temp);
    }
  } else {
    protocol = GetSession()->GetCurrentUser()->data.qwk_protocol;
  }
  switch (protocol) {
  case -1:
    *abort = 1;

    break;
  case 0:
  case WWIV_INTERNAL_PROT_ASCII:
    break;

  case WWIV_INTERNAL_PROT_XMODEM:
  case WWIV_INTERNAL_PROT_XMODEMCRC:
  case WWIV_INTERNAL_PROT_YMODEM:
  case WWIV_INTERNAL_PROT_ZMODEM: {
    double percent = 0.0;
    maybe_internal(fn, sent, &percent, true, protocol);
  } break;

  default: {
    int exit_code = extern_prot(protocol - 6, fn, 1);
    *abort = 0;
    if (exit_code == externs[protocol - 6].ok1) {
      *sent = 1;
    }
  } break;
  }
}

int select_qwk_protocol(struct qwk_junk *qwk_info) {
  int protocol = get_protocol(xf_down_temp);
  if (protocol == -1) {
    qwk_info->abort = 1;
  }
  return protocol;
}

void insert_after_routing(char *text, char *text2insert, long *len) {
  strip_heart_colors(text2insert);

  int pos = 0;
  while (pos < *len && text[pos] != 0 && !hangup) {
    if (text[pos] == 4 && text[pos + 1] == '0') {
      while (pos < *len && text[pos] != '\xE3' && !hangup) {
        ++pos;
      }

      if (text[pos] == '\xE3') {
        ++pos;
      }
    } else if (pos < *len) {
      strcat(text2insert, "\xE3\xE3");
      int addsize = strlen(text2insert);

      char* dst = text + pos + addsize;
      char* src = text + pos;
      long size = (*len) - pos + 1;

      memmove(dst, src, size);
      strncpy(src, text2insert, addsize);

      *len = (*len) + addsize;
      return;
    }
  }
}

void close_qwk_cfg(struct qwk_config *qwk_cfg) {
  int x = 0;
  while (x < qwk_cfg->amount_blts) {
    if (qwk_cfg->blt[x]) {
      free(qwk_cfg->blt[x]);
    }
    if (qwk_cfg->bltname[x]) {
      free(qwk_cfg->bltname[x]);
    }
    ++x;
  }
}

void read_qwk_cfg(struct qwk_config *qwk_cfg) {
  char s[201];
  sprintf(s, "%s%s", syscfg.datadir, "QWK.CFG");
  memset(qwk_cfg, 0, sizeof(struct qwk_config));

  int f = open(s, O_BINARY | O_RDONLY);
  if (f < 0) {
    return;
  }
  read(f,  qwk_cfg, sizeof(struct qwk_config));
  int x = 0;
  while (x < qwk_cfg->amount_blts) {
    long pos = sizeof(struct qwk_config) + (x * BULL_SIZE);
    lseek(f, pos, SEEK_SET);
    qwk_cfg->blt[x] = (char *)malloc(BULL_SIZE);
    read(f, qwk_cfg->blt[x], BULL_SIZE);

    ++x;
  }

  x = 0;
  while (x < qwk_cfg->amount_blts) {
    long pos = sizeof(struct qwk_config) + (qwk_cfg->amount_blts * BULL_SIZE) + (x * BNAME_SIZE);
    lseek(f, pos, SEEK_SET);
    qwk_cfg->bltname[x] = (char *)malloc(BNAME_SIZE * qwk_cfg->amount_blts);
    read(f, qwk_cfg->bltname[x], BNAME_SIZE);

    ++x;
  }
  close(f);
}

void write_qwk_cfg(struct qwk_config *qwk_cfg) {
  int new_amount = 0;
  char s[201];

  sprintf(s, "%s%s", syscfg.datadir, "QWK.CFG");

  int f = open(s, O_BINARY | O_RDWR | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE);
  if (f < 0) {
    return;
  }

  write(f, qwk_cfg, sizeof(struct qwk_config));

  int x = 0;
  while (x < qwk_cfg->amount_blts) {
    long pos = sizeof(struct qwk_config) + (new_amount * BULL_SIZE);
    lseek(f, pos, SEEK_SET);

    if (qwk_cfg->blt[x]) {
      write(f, qwk_cfg->blt[x], BULL_SIZE);
      ++new_amount;
    }
    ++x;
  }

  x = 0;
  while (x < qwk_cfg->amount_blts) {
    long pos = sizeof(struct qwk_config) + (qwk_cfg->amount_blts * BULL_SIZE) + (x * BNAME_SIZE);
    lseek(f, pos, SEEK_SET);

    if (qwk_cfg->bltname[x]) {
      write(f, qwk_cfg->bltname[x], BNAME_SIZE);
    }

    ++x;
  }

  qwk_cfg->amount_blts = new_amount;
  lseek(f, 0, SEEK_SET);
  write(f, qwk_cfg, sizeof(struct qwk_config));

  close(f);
}

int get_qwk_max_msgs(uint16_t *max_msgs, uint16_t *max_per_sub) {
  GetSession()->bout.ClearScreen();
  GetSession()->bout.NewLine();
  GetSession()->bout.Color(2);
  GetSession()->bout.WriteFormatted("Largest packet you want, in msgs? (0=Unlimited) : ");
  GetSession()->bout.ColorizedInputField(5);

  char temp[6];
  input(temp, 5);

  if (!temp[0]) {
    return 0;
  }

  *max_msgs = static_cast<uint16_t>(atoi(temp)); 

  GetSession()->bout.WriteFormatted("Most messages you want per sub? ");
  GetSession()->bout.ColorizedInputField(5);
  input(temp, 5);

  if (!temp[0]) {
    return 0;
  }

  *max_per_sub = static_cast<uint16_t>(atoi(temp)); 

  return 1;
}

void qwk_nscan(void) {
#ifdef NEVER // Not ported yet
  uploadsrec u;
  bool abort = false;
  int od, newfile, i, i1, i5, f, count, color = 3;
  char s[201], *ext;

  GetSession()->bout.Color(3);
  GetSession()->bout.Write("Building NEWFILES.DAT");

  sprintf(s, "%s%s", QWK_DIRECTORY, "NEWFILES.DAT");
  newfile = open(s, O_BINARY | O_RDWR | O_TRUNC | O_CREAT, S_IREAD | S_IWRITE;
  if (newfile < 1) {
    GetSession()->bout.Write("Open Error");
    return;
  }

  for (i = 0; (i < num_dirs) && (!abort) && (udir[i].subnum != -1); i++) {
    checka(&abort);
    count++;

    GetSession()->bout.WriteFormatted("%d.", color);
    if (count >= DOTS) {
      GetSession()->bout.WriteFormatted("\r");
      GetSession()->bout.WriteFormatted("Searching");
      color++;
      count = 0;
      if (color == 4) {
        color++;
      }
      if (color == 10) {
        color = 0;
      }
    }

    i1 = udir[i].subnum;
    if (qsc_n[i1 / 32] & (1L << (i1 % 32))) {
      if ((dir_dates[udir[i].subnum]) && (dir_dates[udir[i].subnum] < nscandate)) {
        continue;
      }

      od = GetSession()->GetCurrentFileArea();
      GetSession()->SetCurrentFileArea(i);
      dliscan();
      if (this_date >= nscandate) {
        sprintf(s, "\r\n\r\n%s - #%s, %d %s.\r\n\r\n",
                directories[udir[GetSession()->GetCurrentFileArea()].subnum].name,
                udir[GetSession()->GetCurrentFileArea()].keys, numf, "files");
        write(newfile,  s, strlen(s));

        f = open(dlfn, O_RDONLY | O_BINARY);
        for (i5 = 1; (i5 <= numf) && (!(abort)) && (!hangup); i5++) {
          SETREC(f, i5);
          read(f, &u, sizeof(uploadsrec));
          if (u.daten >= nscandate) {
            sprintf(s, "%s %5ldk  %s\r\n", u.filename, (long) bytes_to_k(u.numbytes), u.description);
            write(newfile,  s, strlen(s));


#ifndef HUGE_TRAN
            if (u.mask & mask_extended) {
              int pos = 0;
              ext = read_extended_description(u.filename);

              if (ext) {
                int spos = 21, x;

                strcpy(s, "                     ");
                while (ext[pos]) {
                  x = ext[pos];

                  if (x != '\r' && x != '\n' && x > 2) {
                    s[spos] = x;
                  }


                  if (x == '\n' || x == 0) {
                    s[spos] = 0;
                    write(newfile, s, strlen(s));
                    write(newfile, "\r\n", 2);
                    strcpy(s, "                     ");
                    spos = 21;
                  }

                  if (x != '\r' && x != '\n' && x > 2) {
                    ++spos;
                  }

                  ++pos;
                }
                free(ext);
              }
            }
#endif

          } else if (!empty()) {
            checka(&abort);
          }

        }
        f = close(f);
      }
      GetSession()->SetCurrentFileArea(od);

    }
  }


  newfile = close(newfile);
#endif  // NEVER
}

void finish_qwk(struct qwk_junk *qwk_info) {
  char parem1[201], parem2[201];
  char qwkname[201];
  int f;
  bool sent = false;
  long numbytes;

  struct qwk_config qwk_cfg;
  int x, done = 0;
  int archiver;


  if (!GetSession()->GetCurrentUser()->data.qwk_dontscanfiles) {
    qwk_nscan();
  }

  read_qwk_cfg(&qwk_cfg);
  if (!GetSession()->GetCurrentUser()->data.qwk_leave_bulletin) {
    GetSession()->bout.Write("Grabbing hello/news/goodbye text files...");

    if (qwk_cfg.hello[0]) {
      sprintf(parem1, "%s%s", syscfg.gfilesdir, qwk_cfg.hello);
      sprintf(parem2, "%s%s", QWK_DIRECTORY, qwk_cfg.hello);
      copyfile(parem1, parem2, 1);
    }

    if (qwk_cfg.news[0]) {
      sprintf(parem1, "%s%s", syscfg.gfilesdir, qwk_cfg.news);
      sprintf(parem2, "%s%s", QWK_DIRECTORY, qwk_cfg.news);
      copyfile(parem1, parem2, 1);
    }

    if (qwk_cfg.bye[0]) {
      sprintf(parem1, "%s%s", syscfg.gfilesdir, qwk_cfg.bye);
      sprintf(parem2, "%s%s", QWK_DIRECTORY, qwk_cfg.bye);
      copyfile(parem1, parem2, 1);
    }

    x = 0;
    while (x < qwk_cfg.amount_blts) {
      if (WFile::Exists(qwk_cfg.blt[x])) {
        // Only copy if bulletin is newer than the users laston date
        // Don't have file_daten anymore
        // if(file_daten(qwk_cfg.blt[x]) > date_to_daten(GetSession()->GetCurrentUser()->GetLastOnDateNumber()))
        {
          sprintf(parem2, "%s%s", QWK_DIRECTORY, qwk_cfg.bltname[x]);
          copyfile(qwk_cfg.blt[x], parem2, 1);
        }
        ++x;
      }
    }
  }
  close_qwk_cfg(&qwk_cfg);

  qwk_system_name(qwkname);
  strcat(qwkname, ".QWK");

  if (!GetSession()->GetCurrentUser()->data.qwk_archive
      || !arcs[GetSession()->GetCurrentUser()->data.qwk_archive - 1].extension[0]) {
    archiver = select_qwk_archiver(qwk_info, 0) - 1;
  } else {
    archiver = GetSession()->GetCurrentUser()->data.qwk_archive - 1;
  }

  std::string command;
  if (!qwk_info->abort) {
    sprintf(parem1, "%s%s", QWK_DIRECTORY, qwkname);
    sprintf(parem2, "%s*.*", QWK_DIRECTORY);

    command = stuff_in(arcs[archiver].arca, parem1, parem2, "", "", "");
    ExecuteExternalProgram(command, EFLAG_NOPAUSE);

    command = wwiv::strings::StringPrintf("%s%s", QWK_DIRECTORY, qwkname);
    WWIV_make_abs_cmd(&command);

    f = open(command.c_str(), O_RDONLY | O_BINARY);
    if (f < 0) {
      GetSession()->bout.Write("No such file.");
      GetSession()->bout.NewLine();
      qwk_info->abort = 1;
      return;
    }
    numbytes = filelength(f);

    close(f);

    if (numbytes == 0L) {
      GetSession()->bout.Write("File has nothing in it.");
      qwk_info->abort = 1;
      return;
    }
  }

  if (incom) {
    while (!done && !qwk_info->abort && !hangup) {
      bool abort = false;
      qwk_send_file(command.c_str(), &sent, &abort);
      if (sent) {
        done = 1;
      } else {
        GetSession()->bout.NewLine();
        GetSession()->bout.Color(2);
        GetSession()->bout.Write("Packet was not successful...");
        GetSession()->bout.Color(1);
        GetSession()->bout.WriteFormatted("Try transfer again?");

        if (!noyes()) {
          done = 1;
          abort = 1;
          qwk_info->abort = 1;
        } else {
          abort = 0;
        }

      }
      if (abort) {
        qwk_info->abort = 1;
      }
    }
  } else while (!done && !hangup && !qwk_info->abort) {
      char new_dir[61];
      char nfile[81];

      GetSession()->bout.Color(2);
      GetSession()->bout.WriteFormatted("Move to what dir? ");
      GetSession()->bout.ColorizedInputField(60);
      input(new_dir, 60);

      StringTrim(new_dir);

      if (new_dir[0]) {

        if (new_dir[strlen(new_dir) - 1] != '\\') {
          strcat(new_dir, "\\");
        }

        sprintf(nfile, "%s%s", new_dir, qwkname);
      } else {
        nfile[0] = 0;
      }

      if (!replacefile(parem1, nfile, true)) {
        GetSession()->bout.Color(7);
        GetSession()->bout.WriteFormatted("Try again?");
        if (!noyes()) {
          qwk_info->abort = 1;
          done = 1;
        }
      } else {
        done = 1;
      }
    }
}

int qwk_open_file(char *fn) {
  int i;
  char s[81];

  sprintf(s, "%s%s.DAT", syscfg.msgsdir, fn);
  int f = open(s, O_RDWR | O_BINARY);

  if (f < 0) {
    f = open(s, O_RDWR | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);
    for (i = 0; i < 2048; i++) {
      gat[i] = 0;
    }

    if (f < 0) {
      return (-1);
    }
    write(f, gat, 4096);
    // strcpy(gatfn,fn); (removed with g_szMessageGatFileName)
    ftruncate(f, 4096L + (75L * 1024L));
    current_gat_section(0);
  }

  lseek(f, 0L, SEEK_SET);
  read(f, gat, 4096);
  // strcpy(gatfn,fn); (removed with g_szMessageGatFileName)
  current_gat_section(0);
  return f;
}
