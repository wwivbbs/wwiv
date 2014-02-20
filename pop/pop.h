/*
 * Copyright 2001,2004 Frank Reid
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *      http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */ 
#if !defined( __POP_H_INCLUDED__ )
#define __POP_H_INCLUDED__ 

#define SMTP_STATUS   211
#define SMTP_HELP     214
#define SMTP_READY    220
#define SMTP_BYE      221
#define SMTP_OK       250
#define SMTP_WILL_FWD 251

#define SMTP_GIMME    354

#define SMTP_OOPS     421
#define SMTP_BUSY     450
#define SMTP_ERROR    451
#define SMTP_SQUEEZED 452

#define SMTP_SYNTAX   500
#define SMTP_PARAM    501
#define SMTP_COM_NI   502
#define SMTP_BAD_SEQ  503
#define SMTP_BAD_PARM 504
#define SMTP_ACCESS   550
#define SMTP_YOU_FWD  551
#define SMTP_FULL     552
#define SMTP_BAD_NAM  553
#define SMTP_FAILED   554


#define POP_OK               200
#define POP_NOT_MSG          400
#define POP_BAD_HOST         500
#define POP_HOST_UNAVAILABLE 501
#define POP_BAD_MBOX         510
#define POP_BAD_PASS         511
#define POP_UNKNOWN          599


#define POPLIB_OK        200
#define POPLIB_BAD_FILE  401
#define POPLIB_BAD_HOST  510
#define POPLIB_S_TIMEOU  510
#define POPLIB_S_CLOSED  511
#define POPLIB_SMTP_ERR  520
#define POPLIB_POP_ERR   521
#define POPLIB_SMTP_PROB 410
#define POPLIB_POP_PROB  411

// States returned by pop_top
#define STATE_UNKNOWN	-1
#define STATE_ERROR		0
#define STATE_UUENCODE	1
#define STATE_ARCHIVE	2
#define STATE_IMAGE		3	
#define STATE_BAD		4
#define STATE_SPAM		5
#define STATE_SUBSCRIBE	6
#define STATE_DUPLICATE	7
#define STATE_FIDONET	8

// States returned by pop_get_nextf, pop_getf, and pop_delete
#define STATE_ERROR_RETREIVE 0
#define STATE_SUCCESS 1
#define STATE_ERROR_DELETE 2

// Undefine the next line if you want to keep WINS from deleting 
// messages from the POP server, usefull when debugging, just make sure to
// delete the MSGID.DAT from your FILENET directory to ensure the packet
// doesn't get marked as a duplicate.
//#define __NO_POP_DELETE__


struct Message_ID
{
  char msgid[81];
};

struct ACCT
{
  char popname[40];
  char pophost[60];
  char poppass[40];
};


#define _TEMP_BUFFER_LEN 8192

ACCT *acct;

#ifdef DOMAIN
//TODO(rushfan): rename DOMAIN since math.h now defines this.
#undef DOMAIN
#endif  // DOMAIN

int POP_Err_Cond, SMTP_Err_Cond;
char from_user[81], net_data[_MAX_PATH], net_pkt[21], maindir[160], fdlfn[21], id[81];
char LISTNAME[45], MAILFROM[60], PROXY[40], listaddr[25];
char POPHOST[60], POPNAME[40], POPPASS[20], DOMAIN[60], NODEPASS[20];
int POPPORT, SMTPPORT;
bool fdl;
char _temp_buffer[_TEMP_BUFFER_LEN];
static int POP_stat, SMTP_stat;
bool DEBUG = true;
bool compact_ids = false;
bool aborted, ALLMAIL;

char *version = "WWIV Internet Network Support (WINS) POP3/SMTP Client " VERSION;

char pktowner[36];


#endif // #if !defined( __POP_H_INCLUDED__ )
