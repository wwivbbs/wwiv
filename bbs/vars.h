/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
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
#ifndef __INCLUDED_VARS_H__
#define __INCLUDED_VARS_H__

#include <ctime>

#include "sdk/vardec.h"
#include "sdk/net.h"

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

///////////////////////////////////////////////////////////////////////////////

#ifdef _DEFINE_GLOBALS_
#define __EXTRN__
#else
#define __EXTRN__ extern
#endif


///////////////////////////////////////////////////////////////////////////////

__EXTRN__ char  ansistr[81],
          charbuffer[ 255 ],
          dc[81],
          dcd[81],
          g_szDownloadFileName[MAX_PATH],
          g_szDSZLogFileName[MAX_PATH],
          dtc[81],
          g_szExtDescrFileName[MAX_PATH],
          endofline[81],
          irt[81],
          irt_name[205],
          irt_sub[81],
          net_email_name[205],
          odc[81],
          *quotes_ind,
          tc[81];

__EXTRN__ unsigned char  checksum;

__EXTRN__ int ansiptr,
          bquote,
          change_color,
          charbufferpointer,
          chatting,
          defscreenbottom,
          do_event,
          equote,
          fsenttoday,
          fwaiting,
          lines_listed,
          questused[20],
          nsp;

__EXTRN__ bool  bChatLine,
          g_preloaded,
          newline,
          global_xx,
          forcescansub,
          local_echo,
          mailcheck,
          guest_user,
          no_hangup,
          smwcheck,
          chatcall,
          chat_file,
          express,
          emchg,
          expressabort,
          hangup,
          hungup,
          incom,
          outcom,
          okskey,
          okmacro,
          use_workspace,
          ok_modem_stuff,
          x_only,
          returning;

// Chatroom additions
__EXTRN__ bool  in_chatroom,
          invis,
          avail;

__EXTRN__ unsigned short
#if !defined ( NETWORK )
*csn_index,
net_sysnum,
#endif // NETWORK
crc,
*gat;

__EXTRN__ int modem_speed;

__EXTRN__ int curatr,
          dirconfnum,
          subconfnum;

__EXTRN__ int g_num_listed,
          timelastchar1;

__EXTRN__ time_t nscandate;


__EXTRN__ int
g_flags,
com_speed;

__EXTRN__ uint32_t
*qsc,
*qsc_n,
*qsc_q,
*qsc_p;

__EXTRN__ float batchtime;

__EXTRN__ double
extratimecall,
timeon,
time_event;

#if defined (INIT) || defined (NETWORK) || defined (FIX)
__EXTRN__ configrec syscfg;
#else
__EXTRN__ small_configrec syscfg;
#endif

__EXTRN__ configoverrec syscfgovr;
#ifdef NOT_BBS
__EXTRN__ statusrec status;
#endif  // NOT_BBS
__EXTRN__ colorrec rescolor;
__EXTRN__ smalrec *smallist;
__EXTRN__ subboardrec *subboards;
__EXTRN__ directoryrec *directories;
__EXTRN__ usersubrec *usub, *udir;
__EXTRN__ userconfrec *uconfsub, *uconfdir;
__EXTRN__ batchrec *batch;
__EXTRN__ tagrec *filelist;
__EXTRN__ chainfilerec *chains;
__EXTRN__ chainregrec *chains_reg;
__EXTRN__ newexternalrec *externs, *over_intern;
__EXTRN__ editorrec *editors;
__EXTRN__ gfiledirrec *gfilesec;
__EXTRN__ net_system_list_rec *csn;
__EXTRN__ net_networks_rec *net_networks;
__EXTRN__ arcrec *arcs;
__EXTRN__ eventsrec *events;
__EXTRN__ threadrec *thread;

__EXTRN__ languagerec *languages;
__EXTRN__ char *cur_lang_name;

__EXTRN__ confrec *subconfs, *dirconfs;

__EXTRN__ int iia;

__EXTRN__ int32_t last_iia;

// from version.cpp
extern const char *wwiv_version;
extern const char *beta_version;
extern const char *wwiv_date;
extern unsigned short wwiv_num_version;


// confedit
#define CONF_SUBS 1
#define CONF_DIRS 2

#endif // __INCLUDED_VARS_H__

