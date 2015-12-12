/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2015, WWIV Software Services             */
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
#include "bbs/qwk.h"

#include <memory>
#include <string>

#include <ctype.h>
#include <fcntl.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif
#include <sys/stat.h>

#include "bbs/bbsovl3.h"
#include "bbs/conf.h"
#include "bbs/instmsg.h"
#include "bbs/input.h"
#include "bbs/message_file.h"
#include "bbs/pause.h"
#include "bbs/qscan.h"
#include "bbs/stuffin.h"
#include "bbs/subxtr.h"
#include "bbs/bbs.h"
#include "bbs/fcns.h"
#include "bbs/vars.h"
#include "bbs/wconstants.h"
#include "bbs/wwivcolors.h"
#include "bbs/wstatus.h"
#include "bbs/platform/platformfcns.h"
#include "core/file.h"
#include "core/strings.h"
#include "core/wwivport.h"
#include "sdk/filenames.h"
#include "sdk/vardec.h"

#define qwk_iscan(x)         (iscan1(usub[x].subnum))

using std::unique_ptr;
using namespace wwiv::strings;

// Also used in qwk1.cpp
const char *QWKFrom = "\x04""0QWKFrom:";

static int qwk_percent;
static uint16_t max_msgs;

// from xfer.cpp
extern uint32_t this_date;

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
using wwiv::strings::StringPrintf;

static bool replacefile(char *src, char *dst) {
  if (strlen(dst) == 0) {
    return false;
  }
  return copyfile(src, dst, true);
}

void build_qwk_packet() {
  struct qwk_junk qwk_info;
  struct qwk_config qwk_cfg;
  bool save_conf = false;
  SaveQScanPointers save_qscan;

  remove_from_temp("*.*", QWK_DIRECTORY, 0);

  if ((uconfsub[1].confnum != -1) && (okconf(session()->user()))) {
    save_conf = true;
    tmp_disable_conf(true);
  }
  TempDisablePause disable_pause;

  read_qwk_cfg(&qwk_cfg);
  max_msgs = qwk_cfg.max_msgs;
  if (session()->user()->data.qwk_max_msgs < max_msgs && session()->user()->data.qwk_max_msgs) {
    max_msgs = session()->user()->data.qwk_max_msgs;
  }

  if (!qwk_cfg.fu) {
    qwk_cfg.fu = static_cast<int32_t>(time(nullptr));
  }

  ++qwk_cfg.timesd;
  write_qwk_cfg(&qwk_cfg);
  close_qwk_cfg(&qwk_cfg);

  write_inst(INST_LOC_QWK, usub[session()->GetCurrentMessageArea()].subnum, INST_FLAGS_ONLINE);

  const string filename = StrCat(QWK_DIRECTORY, MESSAGES_DAT);
  qwk_info.file = open(filename.c_str(), O_RDWR | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);

  if (qwk_info.file < 1) {
    bout.bputs("Open error");
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

  if (!session()->user()->data.qwk_dont_scan_mail && !qwk_info.abort) {
    qwk_gather_email(&qwk_info);
  }

  checka(&qwk_info.abort);

  bout.cls();
  if (!qwk_info.abort) {
    bout << "|#7\xDA" << string(4, '\xC4') << '\xC2' << string(60, '\xC4') << '\xC2' << string(5, '\xC4') 
      << '\xC2' << string(4, '\xC4') << '\xBF' << wwiv::endl;
  }

  checka(&qwk_info.abort);

  if (!qwk_info.abort) {
    bout << "|#7\xB3|#2Sub |#7\xB3|#3Sub Name" << string(52, ' ') << "|#7\xB3|#8Total|#7\xB3|#5New |#7\xB3" << wwiv::endl;
  }

  checka(&qwk_info.abort);

  if (!qwk_info.abort) {
    bout << "|#7xC3" << string(4, '\xC4') << '\xC5' << string(60, '\xC4') << '\xC5' << string(5, '\xC4') 
        << '\xC5' << string(4, '\xC4') << '\xB4' << wwiv::endl;
  }

  bool msgs_ok = true;
  for (int i = 0; (usub[i].subnum != -1) && (i < session()->num_subs) && (!hangup) && !qwk_info.abort && msgs_ok; i++) {
    msgs_ok = (max_msgs ? qwk_info.qwk_rec_num <= max_msgs : true);
    if (qsc_q[usub[i].subnum / 32] & (1L << (usub[i].subnum % 32))) {
      qwk_gather_sub(i, &qwk_info);
    }
  }

  bout << "|#7\xC3" << string(4, '\xC4') << '\xC5' << string(60, '\xC4') << '\xC5' << string(5, '\xC4') 
      << '\xC5' << string(4, '\xC4') << '\xB4' << wwiv::endl;
  bout.nl(2);

  if (qwk_info.abort) {
    bout.Color(1);
    bout.bprintf("Abort everything? (NO=Download what I have gathered)");
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
  if (qwk_info.abort || session()->user()->data.qwk_dontsetnscan || hangup) {
    save_qscan.restore();
    if (qwk_info.abort) {
      sysoplog("Aborted");
    }
  }
  if (session()->user()->data.qwk_delete_mail && !qwk_info.abort) {
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
  uint32_t sd = WWIVReadLastRead(sn);

  if (qwk_percent || (!sd || sd > qscnptrx)) {
    os = session()->GetCurrentMessageArea();
    session()->SetCurrentMessageArea(bn);
    i = 1;

    // Get total amount of messages in base
    if (!qwk_iscan(session()->GetCurrentMessageArea())) {
      return;
    }

    qscnptrx = qsc_p[sn];

    if (!qwk_percent) {
      // Find out what message number we are on
      for (i = session()->GetNumMessagesInCurrentMessageArea(); (i > 1) && (get_post(i - 1)->qscan > qscnptrx); i--)
        ;
    } else { // Get last qwk_percent of messages in sub
      temp_percent = static_cast<float>(qwk_percent) / 100;
      if (temp_percent > 1.0) {
        temp_percent = 1.0;
      }
      i = session()->GetNumMessagesInCurrentMessageArea() - 
          static_cast<int>(temp_percent * session()->GetNumMessagesInCurrentMessageArea());
    }

    strncpy(thissub, subboards[session()->GetCurrentReadMessageArea()].name, 65);
    thissub[60] = 0;
    sprintf(subinfo, "|#7\xB3|#9%-4d|#7\xB3|#1%-60s|#7\xB3 |#2%-4d|#7\xB3|#3%-4d|#7\xB3",
            bn + 1, thissub, session()->GetNumMessagesInCurrentMessageArea(),
            session()->GetNumMessagesInCurrentMessageArea() - i + 1 - (qwk_percent ? 1 : 0));
    bout.bputs(subinfo);
    bout.nl();

    checka(&qwk_info->abort);

    if ((session()->GetNumMessagesInCurrentMessageArea() > 0)
        && (i <= session()->GetNumMessagesInCurrentMessageArea()) && !qwk_info->abort) {
      if ((get_post(i)->qscan > qsc_p[session()->GetCurrentReadMessageArea()]) || qwk_percent) {
        qwk_start_read(i, qwk_info);  // read messsage
      }
    }

    unique_ptr<WStatus> pStatus(session()->GetStatusManager()->GetStatus());
    qsc_p[session()->GetCurrentReadMessageArea()] = pStatus->GetQScanPointer() - 1;
    session()->SetCurrentMessageArea(os);
  } else {
    os = session()->GetCurrentMessageArea();
    session()->SetCurrentMessageArea(bn);
    i = 1;

    qwk_iscan(session()->GetCurrentMessageArea());

    strncpy(thissub, subboards[session()->GetCurrentReadMessageArea()].name, 65);
    thissub[60] = 0;
    sprintf(subinfo, "|#7\xB3|#9%-4d|#7\xB3|#1%-60s|#7\xB3 |#2%-4d|#7\xB3|#3%-4d|#7\xB3",
            bn + 1, thissub, session()->GetNumMessagesInCurrentMessageArea(), 0);
    bout.bputs(subinfo);
    bout.nl();

    session()->SetCurrentMessageArea(os);

    checka(&qwk_info->abort);
  }
  bout.Color(0);
}

void qwk_start_read(int msgnum, struct qwk_junk *qwk_info) {
  irt[0] = 0;
  irt_name[0] = 0;

  if (session()->GetCurrentReadMessageArea() < 0) {
    return;
  }
  // Used to be inside do loop
  if (xsubs[session()->GetCurrentReadMessageArea()].num_nets) {
    set_net_num(xsubs[session()->GetCurrentReadMessageArea()].nets[0].net_num);
  } else {
    set_net_num(0);
  }

  int amount = 1;
  bool done = false;
  do {
    if ((msgnum > 0) && (msgnum <= session()->GetNumMessagesInCurrentMessageArea())) {
      make_pre_qwk(msgnum, qwk_info);
    }
    ++msgnum;
    if (msgnum > session()->GetNumMessagesInCurrentMessageArea()) {
      done = true;
    }
    if (session()->user()->data.qwk_max_msgs_per_sub ? amount >
        session()->user()->data.qwk_max_msgs_per_sub : 0) {
      done = true;
    }
    if (max_msgs ? qwk_info->qwk_rec_num > max_msgs : 0) {
      done = true;
    }
    ++amount;
    checka(&qwk_info->abort);

  } while (!done && !hangup && !qwk_info->abort);
  bputch('\r');
}

void make_pre_qwk(int msgnum, struct qwk_junk *qwk_info) {
  postrec* p = get_post(msgnum);
  if ((p->status & (status_unvalidated | status_delete)) && !lcs()) {
    return;
  }

  int nn = session()->GetNetworkNumber();
  if (p->status & status_post_new_net) {
    set_net_num(p->network.network_msg.net_number);
  }

  put_in_qwk(p, (subboards[session()->GetCurrentReadMessageArea()].filename), msgnum, qwk_info);
  if (nn != session()->GetNetworkNumber()) {
    set_net_num(nn);
  }

  session()->user()->SetNumMessagesRead(session()->user()->GetNumMessagesRead() + 1);
  session()->SetNumMessagesReadThisLogon(session()->GetNumMessagesReadThisLogon() + 1);

  if (p->qscan > qsc_p[session()->GetCurrentReadMessageArea()]) { // Update qscan pointer right here
    qsc_p[session()->GetCurrentReadMessageArea()] = p->qscan;  // And here
  }
  WStatus* pStatus1 = session()->GetStatusManager()->GetStatus();
  uint32_t lQScanPtr = pStatus1->GetQScanPointer();
  delete pStatus1;
  if (p->qscan >= lQScanPtr) {
    WStatus* pStatus = session()->GetStatusManager()->BeginTransaction();
    pStatus->SetQScanPointer(p->qscan + 1);
    session()->GetStatusManager()->CommitTransaction(pStatus);
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

  string ss;
  if (!readfile(&m, fn, &ss)) {
    bout.bprintf("File not found.");
    bout.nl();
    return;
  }

  long len = ss.length();
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
    strncpy(temp, session()->user()->GetName(), 25);
    temp[25] = 0;
    strupr(temp);

    strncpy(qwk_info->qwk_rec.to, temp, 25);
  }
  strncpy(qwk_info->qwk_rec.from, strupr(stripcolors(n)), 25);

  time_t message_date = m1->daten;
  struct tm *time_now = localtime(&message_date);

  strftime(date, 10, "%m-%d-%y", time_now);
  strncpy(qwk_info->qwk_rec.date, date, 8);

  p = 0;
  p1 = 0;

  len = len - cur;
  make_qwk_ready(&ss[cur], &len, qwk_address);

  int amount_blocks = ((int)len / sizeof(qwk_info->qwk_rec)) + 2;

  // Save Qwk Record
  sprintf(qwk_info->qwk_rec.amount_blocks, "%d", amount_blocks);
  sprintf(qwk_info->qwk_rec.msgnum, "%d", msgnum);

  if (!qwk_info->in_email) {
    strncpy(qwk_info->qwk_rec.subject, stripcolors(pr->title), 25);
  } else {
    strncpy(qwk_info->qwk_rec.subject, stripcolors(qwk_info->email_title), 25);
  }

  qwk_remove_null((char *) &qwk_info->qwk_rec, 123);
  qwk_info->qwk_rec.conf_num = usub[session()->GetCurrentMessageArea()].subnum + 1;
  qwk_info->qwk_rec.logical_num = qwk_info->qwk_rec_num;

  if (append_block(qwk_info->file, &qwk_info->qwk_rec, sizeof(qwk_info->qwk_rec)) != sizeof(qwk_info->qwk_rec)) {
    qwk_info->abort = 1; // Must be out of disk space
    bout.bputs("Write error");
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
    if (session()->GetCurrentMessageArea() != qwk_info->cursub || qwk_info->index < 0) {
      qwk_info->cursub = session()->GetCurrentMessageArea();
      sprintf(filename, "%s%03d.NDX", QWK_DIRECTORY, usub[session()->GetCurrentMessageArea()].subnum + 1);
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
      size_t size = (this_pos + sizeof(qwk_info->qwk_rec) > static_cast<size_t>(len)) ? (len - this_pos - 1) : sizeof(qwk_info->qwk_rec);
      memmove(&qwk_info->qwk_rec, ss.c_str() + cur + this_pos, size);
    }
    // Save this block
    append_block(qwk_info->file, &qwk_info->qwk_rec, sizeof(qwk_info->qwk_rec));

    this_pos += sizeof(qwk_info->qwk_rec);
    ++cur_block;
  }
  // Global variable on total amount of records saved
  ++qwk_info->qwk_rec_num;
}

// Takes text, deletes all ascii '10' and converts '13' to '227' (�)
// And does other conversions as specified
void make_qwk_ready(char *text, long *len, char *address) {
  int pos = 0;
  int new_pos = 0;
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
    } else if (session()->user()->data.qwk_remove_color && x == 3) {
      pos += 2;
    } else if (session()->user()->data.qwk_convert_color && x == 3) {
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
    } else if (session()->user()->data.qwk_keep_routing == false && x == 4 && text[pos + 1] == '0') {
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
  char file[201];
  char system_name[20];
  char date_time[51];
  time_t secs_now = time(nullptr);
  struct tm* time_now = localtime(&secs_now);

  // Creates a string like 'mm-dd-yyyy,hh:mm:ss'
  strftime(date_time, 50, "%m-%d-%Y,%H:%M:%S", time_now);

  sprintf(file, "%sCONTROL.DAT", QWK_DIRECTORY);
  FILE* fp = fopen(file, "wb");

  if (!fp) {
    return;
  }

  struct qwk_config qwk_cfg;
  read_qwk_cfg(&qwk_cfg);
  qwk_system_name(system_name);

  fprintf(fp, "%s.qwk\r\n", system_name);
  fprintf(fp, "%s\r\n", "");   // System City and State
  fprintf(fp, "%s\r\n", syscfg.systemphone);
  fprintf(fp, "%s\r\n", syscfg.sysopname);
  fprintf(fp, "%s,%s\r\n", "00000", system_name);
  fprintf(fp, "%s\r\n", date_time);
  fprintf(fp, "%s\r\n", session()->user()->data.name);
  fprintf(fp, "%s\r\n", "");
  fprintf(fp, "%s\r\n", "0");
  fprintf(fp, "%d\r\n", qwk_info->qwk_rec_num);
  
  int amount = 0;
  for (int cur = 0; (usub[cur].subnum != -1) && (cur < session()->num_subs) && (!hangup); cur++) {
    if (qsc_q[usub[cur].subnum / 32] & (1L << (usub[cur].subnum % 32))) {
      ++amount;
    }
  }
  fprintf(fp, "%d\r\n", amount);
  
  fprintf(fp, "0\r\n");
  fprintf(fp, "E-Mail\r\n");

  for (int cur = 0; (usub[cur].subnum != -1) && (cur < session()->num_subs) && (!hangup); cur++) {
    if (qsc_q[usub[cur].subnum / 32] & (1L << (usub[cur].subnum % 32))) {
      // QWK support says this should be truncated to 10 or 13 characters
      // however QWKE allows for 255 characters. This works fine in multimail which
      // is the only still maintained QWK reader I'm aware of at this time, so we'll
      // allow it to be the full length.
      string sub_name = stripcolors(subboards[usub[cur].subnum].name);

      fprintf(fp, "%d\r\n", usub[cur].subnum + 1);
      fprintf(fp, "%s\r\n", sub_name.c_str());
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

  /* See _fmsbintoieee() for details of formats   */
  sign = ieee[3] & 0x80;
  msbin_exp |= ieee[3] << 1;
  msbin_exp |= ieee[2] >> 7;

  /* An ieee exponent of 0xfe overflows in MBF    */
  if (msbin_exp == 0xfe) {
    return 1;
  }

  msbin_exp += 2;     /* actually, -127 + 128 + 1 */

  for (int i = 0; i < 4; i++) {
    msbin[i] = 0;
  }

  msbin[3] = msbin_exp;
  msbin[2] |= sign;
  msbin[2] |= ieee[2] & 0x7f;
  msbin[1] = ieee[1];
  msbin[0] = ieee[0];

  return 0;
}

char* qwk_system_name(char *qwkname) {
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

void qwk_menu() {
  char temp[101], namepath[101];

  qwk_percent = 0;
  
  bool done = false;
  while (!done && !hangup) {
    bout.cls();
    printfile("QWK");
    if (so()) {
      bout.bputs("1) Sysop QWK config");
    }

    if (qwk_percent) {
      bout.Color(3);
      bout.nl();
      bout.bprintf("Of all messages, you will be downloading %d%%\r\n", qwk_percent);
    }
    bout.nl();
    strcpy(temp, "7[3Q1DCUBS%");
    if (so()) {
      strcat(temp, "1");
    }
    strcat(temp, "7] ");
    bout.bprintf(temp);
    bout.mpl(1);

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
      upload_reply_packet();
      break;

    case 'D':
      sysoplog("Download QWK packet");
      qwk_system_name(temp);
      strcat(temp, ".REP");
      sprintf(namepath, "%s%s", QWK_DIRECTORY, temp);
      unlink(namepath);

      build_qwk_packet();

      if (File::Exists(namepath)) {
        sysoplog("REP was uploaded");
        upload_reply_packet();
      }
      break;

    case 'B':
      sysoplog("Down/Up QWK/REP packet");

      qwk_system_name(temp);
      strcat(temp, ".REP");
      sprintf(namepath, "%s%s", QWK_DIRECTORY, temp);
      unlink(namepath);

      build_qwk_packet();

      if (File::Exists(namepath)) {
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
      bout.Color(2);
      bout.bprintf("Enter percent of all messages in all QSCAN subs to pack:");
      bout.mpl(3);
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

static void qwk_send_file(string fn, bool *sent, bool *abort) {
  // TODO(rushfan): Should this just call send_file from sr.cpp?
  *sent = 0;
  *abort = 0;

  int protocol = -1;
  if (session()->user()->data.qwk_protocol <= 1) {
    protocol = get_protocol(xf_down_temp);
  } else {
    protocol = session()->user()->data.qwk_protocol;
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
    maybe_internal(fn.c_str(), sent, &percent, true, protocol);
  } break;

  default: {
    int exit_code = extern_prot(protocol - WWIV_NUM_INTERNAL_PROTOCOLS, fn.c_str(), 1);
    *abort = 0;
    if (exit_code == externs[protocol - WWIV_NUM_INTERNAL_PROTOCOLS].ok1) {
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
  string temp = stripcolors(text2insert);
  strcpy(text2insert, temp.c_str());

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
  char s[201];

  sprintf(s, "%s%s", syscfg.datadir, "QWK.CFG");

  int f = open(s, O_BINARY | O_RDWR | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE);
  if (f < 0) {
    return;
  }

  write(f, qwk_cfg, sizeof(struct qwk_config));

  int x = 0;
  int new_amount = 0;
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

int get_qwk_max_msgs(uint16_t *qwk_max_msgs, uint16_t *max_per_sub) {
  bout.cls();
  bout.nl();
  bout.Color(2);
  bout.bprintf("Largest packet you want, in msgs? (0=Unlimited) : ");
  bout.mpl(5);

  char temp[6];
  input(temp, 5);

  if (!temp[0]) {
    return 0;
  }

  *qwk_max_msgs = static_cast<uint16_t>(atoi(temp));

  bout.bprintf("Most messages you want per sub? ");
  bout.mpl(5);
  input(temp, 5);

  if (!temp[0]) {
    return 0;
  }

  *max_per_sub = static_cast<uint16_t>(atoi(temp)); 
  return 1;
}

void qwk_nscan() {
#ifdef NEVER // Not ported yet
  uploadsrec u;
  bool abort = false;
  int od, newfile, i, i1, i5, f, count, color = 3;
  char s[201], *ext;

  bout.Color(3);
  bout.bputs("Building NEWFILES.DAT");

  sprintf(s, "%s%s", QWK_DIRECTORY, "NEWFILES.DAT");
  newfile = open(s, O_BINARY | O_RDWR | O_TRUNC | O_CREAT, S_IREAD | S_IWRITE;
  if (newfile < 1) {
    bout.bputs("Open Error");
    return;
  }

  for (i = 0; (i < num_dirs) && (!abort) && (udir[i].subnum != -1); i++) {
    checka(&abort);
    count++;

    bout.bprintf("%d.", color);
    if (count >= DOTS) {
      bout.bprintf("\r");
      bout.bprintf("Searching");
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

      od = session()->GetCurrentFileArea();
      session()->SetCurrentFileArea(i);
      dliscan();
      if (this_date >= nscandate) {
        sprintf(s, "\r\n\r\n%s - #%s, %d %s.\r\n\r\n",
                directories[udir[session()->GetCurrentFileArea()].subnum].name,
                udir[session()->GetCurrentFileArea()].keys, numf, "files");
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
      session()->SetCurrentFileArea(od);

    }
  }
  newfile = close(newfile);
#endif  // NEVER
}

void finish_qwk(struct qwk_junk *qwk_info) {
  char parem1[201], parem2[201];
  char qwkname[201];
  bool sent = false;
  long numbytes;

  struct qwk_config qwk_cfg;
  int x, done = 0;
  int archiver;


  if (!session()->user()->data.qwk_dontscanfiles) {
    qwk_nscan();
  }

  read_qwk_cfg(&qwk_cfg);
  if (!session()->user()->data.qwk_leave_bulletin) {
    bout.bputs("Grabbing hello/news/goodbye text files...");

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
      if (File::Exists(qwk_cfg.blt[x])) {
        // Only copy if bulletin is newer than the users laston date
        // Don't have file_daten anymore
        // if(file_daten(qwk_cfg.blt[x]) > date_to_daten(session()->user()->GetLastOnDateNumber()))
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
  strcat(qwkname, ".qwk");

  if (!session()->user()->data.qwk_archive
      || !arcs[session()->user()->data.qwk_archive - 1].extension[0]) {
    archiver = select_qwk_archiver(qwk_info, 0) - 1;
  } else {
    archiver = session()->user()->data.qwk_archive - 1;
  }

  string qwk_file_to_send;
  if (!qwk_info->abort) {
    sprintf(parem1, "%s%s", QWK_DIRECTORY, qwkname);
    sprintf(parem2, "%s*.*", QWK_DIRECTORY);

    string command = stuff_in(arcs[archiver].arca, parem1, parem2, "", "", "");
    ExecuteExternalProgram(command, session()->GetSpawnOptions(SPAWNOPT_ARCH_A));

    qwk_file_to_send = wwiv::strings::StringPrintf("%s%s", QWK_DIRECTORY, qwkname);
    // TODO(rushfan): Should we just have a make abs path?
    WWIV_make_abs_cmd(session()->GetHomeDir(), &qwk_file_to_send);

    File qwk_file_to_send_file(qwk_file_to_send);
    if (!File::Exists(qwk_file_to_send)){
      bout.bputs("No such file.");
      bout.nl();
      qwk_info->abort = 1;
      return;
    }
    numbytes = qwk_file_to_send_file.GetLength();

    if (numbytes == 0L) {
      bout.bputs("File has nothing in it.");
      qwk_info->abort = 1;
      return;
    }
  }

  if (incom) {
    while (!done && !qwk_info->abort && !hangup) {
      bool abort = false;
      qwk_send_file(qwk_file_to_send, &sent, &abort);
      if (sent) {
        done = 1;
      } else {
        bout.nl();
        bout.Color(2);
        bout.bputs("Packet was not successful...");
        bout.Color(1);
        bout.bprintf("Try transfer again?");

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

      bout.Color(2);
      bout.bprintf("Move to what dir? ");
      bout.mpl(60);
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

      if (!replacefile(parem1, nfile)) {
        bout.Color(7);
        bout.bprintf("Try again?");
        if (!noyes()) {
          qwk_info->abort = 1;
          done = 1;
        }
      } else {
        done = 1;
      }
    }
}
