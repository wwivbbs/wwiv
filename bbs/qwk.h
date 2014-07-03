#ifndef _QWK_H_
#define _QWK_H_

#include "vardec.h"
#include "printfile.h"

#define QWK_DIRECTORY (syscfgovr.batchdir)
/* #define QWK_DIRECTORY (sysinfo.qwk_dir) */


#define qwk_iscan(x)         (iscan1(usub[x].subnum, 1))
#define qwk_iscan_literal(x) (iscan1(x, 1))


// If you have a HUGE transfer section, this define will not read extended
// descriptions
// #define HUGE_TRAN


/* #define SLOWER_BUT_SAFER */

#define MAXMAIL 255
#define EMAIL_STORAGE 2

#define append_block(file, memory, size) write(file, memory, size)
#define SETREC(f,i)  lseek(f,((long) (i))*((long)sizeof(uploadsrec)),SEEK_SET);
#define SET_BLOCK(file, pos, size) lseek(file, (long)pos * (long)size, SEEK_SET)

#define GATSECLEN (4096L+2048L*512L)
#ifndef MSG_STARTING
#define MSG_STARTING ((current_gat_section()) * GATSECLEN + 4096L)
#endif

#define DOTS 5

#define MAX_BULLETINS 50
#define BULL_SIZE     81
#define BNAME_SIZE    13


// Give us 3000 extra bytes to play with in the message text
#define PAD_SPACE 3000

extern int numlock;


struct qwk_record {
  char status;   // ' ' for public

  char msgnum[7]; // all strings are space padded
  char date[8];
  char time[5];

  char to[25]; // Uppercase, left justified
  char from[25]; // Uppercase left justified
  char subject[25];
  char password[12];
  char reference[8];

  char amount_blocks[6];
  char flag;

  unsigned int conf_num;
  unsigned int logical_num;
  char tagline;
};


struct qwk_index {
  float pos;
  char nouse;
};

struct qwk_junk {
  int qwk_rec_num;
  int qwk_rec_pos;

  // File number for the MESSAGES.DAT file
  int file;

  // File number for the *.NDX files
  int index;
  int cursub;
  int personal;  // personal.ndx
  int zero;      // 000.ndx for email

  struct qwk_record qwk_rec;
  struct qwk_index qwk_ndx;

  char abort;

  char in_email;
  char email_title[25];
};

struct qwk_config {
  long fu;
  long timesd;
  long timesu;
  unsigned max_msgs;

  char hello[13];
  char news[13];
  char bye[13];
  char packet_name[9];
  char res[190];

  int  amount_blts;
  char *blt[MAX_BULLETINS];
  char *bltname[MAX_BULLETINS];
};

struct qwk_sub_conf {
  int import_num;
  char import_name[14];
  int to_num;
  char to_name[41];
};

enum CONFIG_QWK_RETURNS {
  QWK_SELECT,
  QWK_ARCHIVER,
  QWK_MAX_PACKET,
  QWK_MAX_SUB,
  QWK_SELECT_PROTOCOL,
  QWK_QUIT_SAVE,
  QWK_ABORT
};


/* File: qwk.c */

void build_qwk_packet(void);
void qwk_gather_sub(int bn, struct qwk_junk *qwk_info);
void qwk_start_read(int msgnum, struct qwk_junk *qwk_info);
void make_pre_qwk(int msgnum, int *val, struct qwk_junk *qwk_info);
void put_in_qwk(postrec *m1, char *fn, int msgnum, struct qwk_junk *qwk_info);
void make_qwk_ready(char *text, long *len, char *address);
void qwk_remove_null(char *memory, int size);
void build_control_dat(struct qwk_junk *qwk_info);
int _fmsbintoieee(float *src4, float *dest4);
int _fieeetomsbin(float *src4, float *dest4);
char * qwk_system_name(char *qwkname);
void qwk_menu(void);
void qwk_send_file(char *fn, bool *sent, bool *abort);
int select_qwk_protocol(struct qwk_junk *qwk_info);
long * qwk_save_qscan(void);
void qwk_restore_qscan(long *save_qsc_p);
void insert_after_routing(char *text, char *text2insert, long *len);
void close_qwk_cfg(struct qwk_config *qwk_cfg);
void read_qwk_cfg(struct qwk_config *qwk_cfg);
void write_qwk_cfg(struct qwk_config *qwk_cfg);
int get_qwk_max_msgs(unsigned int *max_msgs, unsigned int *max_per_sub);
void qwk_nscan(void);
void finish_qwk(struct qwk_junk *qwk_info);
char *qwk_readfile(messagerec *m1, char *aux, long *l);
int qwk_open_file(char *fn);


/* File: qwk1.c */

void qwk_remove_email(void);
void qwk_gather_email(struct qwk_junk *qwk_info);
int select_qwk_archiver(struct qwk_junk *qwk_info, int ask);
void qwk_which_zip(char *thiszip);
void qwk_which_protocol(char *thisprotocol);
void upload_reply_packet(void);
void ready_reply_packet(char *name);
void make_text_ready(char *text, long len);
char * make_text_file(int filenumber, long *size, int curpos, int blocks);
void qwk_email_text(char *text, long size, char *title, char *to);
void qwk_inmsg(const char *text, long size, messagerec *m1, const char *aux, const char *name, long thetime);
void process_reply_dat(char *name);
void qwk_post_text(char *text, long size, char *title, int sub);
int find_qwk_sub(struct qwk_sub_conf *subs, int amount, int fromsub, char *title);
void qwk_receive_file(char *fn, bool *received, int i);
void qwk_sysop(void);
void modify_bulletins(struct qwk_config *qwk_cfg);
void config_qwk_bw(void);
const char *qwk_current_text(int pos, char *text);


// HACKS
const char* get_string(int n);
const char *get_stringx(int filen, int n);


#if defined(__unix__)

#define stricmp strcasecmp
#define strnicmp strncasecmp
#define chsize ftruncate

#endif  // __unix__


#endif
