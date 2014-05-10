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

#ifndef __INCLUDED_VARDEC_H__
#define __INCLUDED_VARDEC_H__

#include <cstdint>
#include "wtypes.h"


#pragma pack(push, 1)

// DATA FOR EVERY USER
struct userrec {
	unsigned char
	name[31],                                // user's name/handle
	     realname[21],                            // user's real name
	     callsign[7],                             // user's amateur callsign
	     phone[13],                               // user's phone number
	     dataphone[13],                           // user's data phone
	     street[31],                              // street address
	     city[31],                                // city
	     state[3],                                // state code [MO, CA, etc]
	     country[4],                              // country [USA, CAN, FRA, etc]
	     zipcode[11],                             // zipcode [#####-####]
	     pw[9],                                   // user's password
	     laston[9],                               // last date on
	     firston[9],                              // first date on
	     note[61],                                // sysop's note about user
	     macros[3][81],                           // macro keys
	     sex;                                     // user's sex

	char
	email[65],                               // Internet mail address
	      res_char[13];                            // bytes for more strings

	unsigned char
	age,                                     // user's age
	inact;                                   // if deleted or inactive

	signed char comp_type;                       // computer type

	unsigned char defprot,                       // default transfer protocol
	         defed,                                   // default editor
	         screenchars,                             // screen width
	         screenlines,                             // screen height
	         num_extended,                            // extended description lines
	         optional_val,                            // optional lines in msgs
	         sl,                                      // security level
	         dsl,                                     // transfer security level
	         exempt,                                  // exempt from ratios, etc
	         colors[10],                              // user's colors
	         bwcolors[10],                            // user's b&w colors
	         votes[20],                               // user's votes
	         illegal,                                 // illegal logons
	         waiting,                                 // number mail waiting
	         ontoday,                                 // num times on today
	         month,                                   // birth month
	         day,                                     // birth day
	         year,                                    // birth year
	         language,                                // language to use
	         cbv;                                     // called back

	char
	res_byte[49];

	unsigned short
	homeuser,                                // user number where user can found
	homesys,                                 // system where user can be found
	forwardusr,                              // mail forwarded to this user number
	forwardsys,                              // mail forwarded to this sys number
	net_num,                                 // net num for forwarding
	msgpost,                                 // number messages posted
	emailsent,                               // number of email sent
	feedbacksent,                            // number of f-back sent
	fsenttoday1,                             // feedbacks today
	posttoday,                               // number posts today
	etoday,                                  // number emails today
	ar,                                      // board access
	dar,                                     // directory access
	restrict,                                // restrictions on account
	ass_pts,                                 // bad things the user did
	uploaded,                                // number files uploaded
	downloaded,                              // number files downloaded
	lastrate,                                // last baud rate on
	logons,                                  // total number of logons
	emailnet,                                // email sent via net
	postnet,                                 // posts sent thru net
	deletedposts,                            // how many posts deleted
	chainsrun,                               // how many "chains" run
	gfilesread,                              // how many gfiles read
	banktime,                                // how many mins in timebank
	homenet,                                 // home net number
	subconf,                                 // previous conference subs
	dirconf;                                 // previous conference dirs
	unsigned short
	subnum,                                  // last sub at logoff
	dirnum;                                  // last dir at logoff

	char
	res_short[40];                           // reserved for short values

	uint32_t
	msgread,                                 // total num msgs read
	uk,                                      // number of k uploaded
	dk,                                      // number of k downloaded
	daten,                                   // numerical time last on
	sysstatus,                               // status/defaults
	wwiv_regnum,                             // user's WWIV reg number
	filepoints,                              // points to spend for files
	registered,                              // numerical registration date
	expires,                                 // numerical expiration date
	datenscan,                               // numerical date of last file scan
	nameinfo;                                // bit mapping for name case

	char
	res_long[40];                            // reserved for long values

	float
	timeontoday,                             // time on today
	extratime,                               // time left today
	timeon,                                  // total time on system
	pos_account,                             // $ credit
	neg_account,                             // $ debit
	gold;                                    // game money

	char
	res_float[32];                           // reserved for real values

	char
	res_gp[94];                              // reserved for whatever

	unsigned short int qwk_max_msgs;
	unsigned short int qwk_max_msgs_per_sub;
	unsigned short int qwk_dont_scan_mail:    1;
	unsigned short int qwk_delete_mail:       1;
	unsigned short int qwk_dontsetnscan:      1;
	unsigned short int qwk_remove_color:      1;
	unsigned short int qwk_convert_color:     1;
	unsigned short int qwk_archive:           3;
	unsigned short int qwk_leave_bulletin:    1;
	unsigned short int qwk_dontscanfiles:     1;
	unsigned short int qwk_keep_routing:      1;
	unsigned short int full_desc:             1;
	unsigned short int qwk_protocol:          4;

};


// SECLEV DATA FOR 1 SL
struct slrec {
	unsigned short time_per_day,                // time allowed on per day
	         time_per_logon,                         // time allowed on per logon
	         messages_read,                          // messages allowed to read
	         emails,                                 // number emails allowed
	         posts;                                  // number posts allowed
	uint32_t ability;                      // bit mapped abilities
};


// AUTO-VALIDATION DATA
struct valrec {
	unsigned char sl,                           // SL
	         dsl;                                    // DSL

	unsigned short ar,                          // AR
	         dar,                                    // DAR
	         restrict;                               // restrictions
};


struct oldarcrec {
	char extension[4],                          // extension for archive
	     arca[32],                               // add commandline
	     arce[32],                               // extract commandline
	     arcl[32];                               // list commandline
};

struct arcrec {
	char name[32],                              // name of the archiver
	     extension[4],                           // extension for archive
	     arca[50],                               // add commandline
	     arce[50],                               // extract commandline
	     arcl[50],                               // list commandline
	     arcd[50],                               // delete commandline
	     arck[50],                               // comment commandline
	     arct[50];                               // test commandline
};


// STATIC SYSTEM INFORMATION
struct configrec {
	char newuserpw[21],                         // new user password
	     systempw[21],                           // system password
	     msgsdir[81],                            // path for msgs directory
	     gfilesdir[81],                          // path for gfiles dir
	     datadir[81],                            // path for data directory
	     dloadsdir[81],                          // path for dloads dir
	     ramdrive,                               // drive for ramdisk
	     tempdir[81],                            // path for temporary directory
	     xmark,                                  // 0xff
	     regcode[83],                            // registration code
	     bbs_init_modem[51],                     // modem initialization cmd
	     answer[21],                             // modem answer cmd
	     connect_300[21],                        // modem responses for
	     connect_1200[21],                       // connections made at
	     connect_2400[21],                       // various speeds
	     connect_9600[21],                       // ""
	     connect_19200[21],                      // ""
	     no_carrier[21],                         // modem disconnect
	     ring[21],                               // modem ring
	     terminal[21],                           // DOS cmd for run term prg
	     systemname[51],                         // BBS system name
	     systemphone[13],                        // BBS system phone number
	     sysopname[51],                          // sysop's name
	     executestr[51];                         // mail route path name

	unsigned char newusersl,                    // new user SL
	         newuserdsl,                             // new user DSL
	         maxwaiting,                             // max mail waiting
	         comport[5],                             // what connected to comm
	         com_ISR[5],                             // Com Interrupts
	         primaryport,                            // primary comm port
	         newuploads,                             // file dir new uploads go
	         closedsystem;                           // if system is closed

	unsigned short systemnumber,                // BBS system number
	         baudrate[5],                            // Baud rate for com ports
	         com_base[5],                            // Com base addresses
	         maxusers,                               // max users on system
	         newuser_restrict,                       // new user restrictions
	         sysconfig,                              // System configuration
	         sysoplowtime,                           // Chat time on
	         sysophightime,                          // Chat time off
	         executetime;                            // time to run mail router

	float req_ratio,                            // required up/down ratio
	      newusergold;                            // new user gold

	slrec sl[256];                              // security level data

	valrec autoval[10];                         // sysop quik-validation data

	char hangupphone[21],                       // string to hang up phone
	     pickupphone[21];                        // string to pick up phone

	unsigned short netlowtime,                  // net time on
	         nethightime;                            // net time off

	char connect_300_a[21],                     // alternate connect string
	     connect_1200_a[21],                     // alternate connect string
	     connect_2400_a[21],                     // alternate connect string
	     connect_9600_a[21],                     // alternate connect string
	     connect_19200_a[21];                    // alternate connect string

	oldarcrec arcs[4];                          // old archivers

	char beginday_c[51],                        // beginday event
	     logon_c[51];                            // logon event

	short userreclen,                           // user record length
	      waitingoffset,                          // mail waiting offset
	      inactoffset;                            // inactive offset

	char newuser_c[51];                         // newuser event

	uint32_t wwiv_reg_number;              // user's reg number

	char dial_prefix[21];

	float post_call_ratio;

	char upload_c[51],                          // upload event
	     dszbatchdl[81], modem_type[9], batchdir[81];

	short sysstatusoffset;                      // system status offset

	char network_type;                          // network type ID

	short fuoffset, fsoffset, fnoffset;         // offset values

	unsigned short max_subs,                    // max subboards
	         max_dirs,                               // max directories
	         qscn_len;                               // qscan pointer length

	unsigned char email_storage_type;           // how to store email

	uint32_t sysconfig1,
	         rrd;                                    // shareware expiration date

	char menudir[81];                           // path for menu dir

	char logoff_c[51];                          // logoff event

	char v_scan_c[51];                          // virus scanning event

	char res[400];                              // RESERVED
};


struct small_configrec {
	char          * newuserpw,          // new user password
	              * systempw,           // system password

	              * msgsdir,            // path for msgs directory
	              * gfilesdir,          // path for gfiles dir
	              * datadir,            // path for data directory
	              * dloadsdir,          // path for dloads dir
	              * batchdir,
	              * menudir,            // path for menu dir
	              * ansidir,            // path for ansi dir
	              * terminal,           // DOS cmd for run term prg

	              * systemname,         // BBS system name
	              * systemphone,        // BBS system phone number
	              * sysopname,          // sysop's name
	              * regcode,            // registration code
	              * executestr,         // mail route path name

	              * beginday_c,         // beginday event
	              * logon_c,            // logon event
	              * logoff_c,           // logoff event
	              * newuser_c,          // newuser event
	              * upload_c,           // upload event
	              * v_scan_c,           // virus scanner command line
	              * dszbatchdl,
	              * dial_prefix;

	unsigned char   newusersl,          // new user SL
	         newuserdsl,         // new user DSL
	         maxwaiting,         // max mail waiting
	         newuploads,         // file dir new uploads go
	         closedsystem;       // if system is closed


	unsigned short  systemnumber,       // BBS system number
	         maxusers,           // max users on system
	         newuser_restrict,   // new user restrictions
	         sysconfig,          // System configuration
	         sysoplowtime,       // Chat time on
	         sysophightime,      // Chat time off
	         executetime,        // time to run mail router
	         netlowtime,         // net time on
	         nethightime,        // net time off
	         max_subs,
	         max_dirs,
	         qscn_len,
	         userreclen;

	float           post_call_ratio,
	                req_ratio,
	                newusergold;

	valrec          autoval[10];        // sysop quik-validation dat


	uint32_t wwiv_reg_number,   // user's reg number
	         sysconfig1,
	         rrd;
};


// overlay information per instance
struct configoverrec {
	unsigned char   com_ISR[9],
	         primaryport;
	unsigned short  com_base[9];
	char            modem_type[9],
	                tempdir[81],
	                batchdir[81];
	unsigned short  comflags;
	char            bootdrive;
	char            res[310];
};


// DYNAMIC SYSTEM STATUS
struct statusrec {
	char date1[9],                              // last date active
	     date2[9],                               // date before now
	     date3[9],                               // two days ago
	     log1[13],                               // yesterday's log
	     log2[13],                               // two days ago log
	     gfiledate[9],                           // date gfiles last updated
	     filechange[7];                          // flags for files changing

	unsigned short localposts,                  // how many local posts today
	         users,                                  // Current number of users
	         callernum,                              // Current caller number
	         callstoday,                             // Number of calls today
	         msgposttoday,                           // Messages posted today
	         emailtoday,                             // Email sent today
	         fbacktoday,                             // Feedback sent today
	         uptoday,                                // files uploaded today
	         activetoday;                            // Minutes active today

	uint32_t qscanptr;                     // Q-scan pointer value

	char amsganon;                              // auto-message anony stat

	unsigned short amsguser;                    // user who wrote a-msg

	uint32_t callernum1;                   // caller number

	unsigned short net_edit_stuff;              // word for net editor

	unsigned short wwiv_version;                // tell what version it is

	unsigned short net_version;                 // tell what version of net

	float net_bias;                             // network bias factor

	int32_t last_connect,                          // date last connect.net
	     last_bbslist;                            // date last bbslist.net

	float net_req_free;                         // net free factor def 3

	unsigned short days;                        // # days BBS active

	char res[29];                               // RESERVED
};


struct colorrec {
	unsigned char resx[240];
};


// MESSAGE BASE INFORMATION
struct subboardrec {
	char name[41],                              // board name
	     filename[9],                            // board database filename
	     key;                                    // board special key

	unsigned char readsl,                       // sl required to read
	         postsl,                                 // sl required to post
	         anony,                                  // anonymous board?
	         age;                                    // minimum age for sub

	unsigned short maxmsgs,                     // max # of msgs
	         ar,                                     // AR for sub-board
	         storage_type,                           // how messages are stored
	         type;                                   // 4 digit board type
};


// UPLOAD DIRECTORY INFORMATION
struct directoryrec {
	char name[41],                              // directory name
	     filename[9],                            // direct database filename
	     path[81];                               // filename path

	unsigned char dsl,                          // DSL for directory
	         age;                                    // minimum age for directory

	unsigned short dar,                         // DAR for directory
	         maxfiles,                               // max files for directory
	         mask,                                   // file type mask
	         type;                                   // 4 digit directory type
};


// QUICK REFERNCE TO FIND USER NUMBER FROM NAME
struct smalrec {
	unsigned char name[31];

	unsigned short number;
};


// TYPE TO TELL WHERE A MESSAGE IS STORED
struct messagerec {
	unsigned char storage_type;                 // how it is stored
	uint32_t stored_as;                    // where it is stored
};


// DATA HELD FOR EVERY POST
struct postrec {
	char title[81];                             // title of post

	unsigned char anony,                        // anony-stat of message
	         status;                                 // bit-mapped status

	unsigned short ownersys,                    // what system it came from
	         owneruser;                              // who posted it

	uint32_t qscan,                        // qscan pointer
	         daten;                                  // numerical date posted

	messagerec msg;                             // where to find it
};



// DATA HELD FOR EVERY E-MAIL OR F-BACK
struct mailrec {
	char title[81];                             // E-mail title

	unsigned char anony,                        // anonymous mail?
	         status;                                 // status for e-mail

	unsigned short fromsys,                     // originating system
	         fromuser,                               // originating user
	         tosys,                                  // destination system
	         touser;                                 // destination user

	uint32_t daten;                        // date it was sent

	messagerec msg;                             // where to find it
};

// USED IN READMAIL TO STORE EMAIL INFO
struct tmpmailrec {
	short index;                                // index into email.dat

	unsigned short fromsys,                     // originating system
	         fromuser;                               // originating user

	uint32_t daten;                        // date it was sent

	messagerec msg;                             // where to find it
};


// SHORT RESPONSE FOR USER, TELLING HIS MAIL HAS BEEN READ
struct shortmsgrec {
	char message[81];                           // short message to user

	unsigned short tosys, touser;               // who it is to
};



// VOTING RESPONSE DATA
struct voting_response {
	char response[81];                          // Voting question response

	unsigned short numresponses;                // number of responses
};



// VOTING DATA INFORMATION
struct votingrec {
	char question[81];                          // Question

	unsigned char numanswers;                   // number of responses

	voting_response responses[20];              // actual responses
};



// DATA HELD FOR EVERY UPLOAD
struct uploadsrec {
	char filename[13],                          // filename
	     description[59],                        // file description
	     date[9],                                // date u/l'ed
	     upby[37],                               // name of upload user
	     actualdate[9];                          // actual file date

	unsigned char unused_filetype;               // file type for apples

	unsigned short numdloads,                   // number times d/l'ed
	         ownersys, ownerusr,                     // who uploaded it
	         mask;                                   // file type mask

	uint32_t daten,                        // date uploaded
	         numbytes;                               // number bytes long file is
};


struct tagrec {
	uploadsrec u;                               // file information

	short directory;                            // directory number

	unsigned short dir_mask;                    // directory mask
};


// ZLOG INFORMATION FOR PAST SYSTEM USAGE
struct zlogrec {
	char date[9];                               // zlog for what date

	unsigned short active,                      // number minutes active
	         calls,                                  // number calls
	         posts,                                  // number posts
	         email,                                  // number e-mail
	         fback,                                  // number f-back
	         up;                                     // number uploads
};



// DATA FOR OTHER PROGRAMS AVAILABLE
struct chainfilerec {
	char filename[81],                          // filename for .chn file
	     description[81];                        // description of it

	unsigned char sl,                           // seclev restriction
	         ansir;                                  // if ANSI required

	unsigned short ar;                          // AR restriction
};


struct chainregrec {
	short int regby[5],                         // who registered
	      usage;                                  // number of runs

	unsigned char minage,                       // minimum age necessary
	         maxage;                                 // maximum age allowed

	char space[50];                             // reserved space
};


// DATA FOR EXTERNAL PROTOCOLS

struct newexternalrec {
	char description[81],                       // protocol description
	     receivefn[81],                          // receive filename
	     sendfn[81],                             // send filename
	     receivebatchfn[81],                     // batch receive cmd
	     sendbatchfn[81],                        // batch send cmd
	     bibatchfn[81];                          // batch send/receive cmd

	unsigned short ok1,                         // if sent
	         othr;                                   // other flags

	char pad[22];
};


// DATA FOR EXTERNAL EDITORS
struct editorrec {
	char description[81],                       // description of editor
	     filename[81];                           // how to run the editor

	uint32_t xxUNUSED;                     // TODO This was not used (config)

	char filenamecon[81];                       // how to run locally

	char res[119];
};



// DATA FOR CONVERSION OF MAIN MENU KEYS TO SUB-BOARD NUMBERS
struct usersubrec {
	char keys[5];

	short subnum;
};


struct userconfrec {
	short confnum;
};


struct batchrec {
	char sending;

	char filename[13];

	short dir;

	float time;

	int32_t len;
};


enum xfertype {
	xf_up,
	xf_down,
	xf_up_temp,
	xf_down_temp,
	xf_up_batch,
	xf_down_batch,
	xf_bi,
	xf_none
};

// USERREC.inact
#define inact_deleted               0x01
#define inact_inactive              0x02

// USERREC.exempt
#define exempt_ratio                0x01
#define exempt_time                 0x02
#define exempt_post                 0x04
#define exempt_all                  0x08
#define exempt_adel                 0x10

// USERREC.restrict
#define restrict_logon              0x0001
#define restrict_chat               0x0002
#define restrict_validate           0x0004
#define restrict_automessage        0x0008
#define restrict_anony              0x0010
#define restrict_post               0x0020
#define restrict_email              0x0040
#define restrict_vote               0x0080
#define restrict_iichat             0x0100
#define restrict_net                0x0200
#define restrict_upload             0x0400

#define restrict_string "LCMA*PEVKNU     "

// USERREC.sysstatus
#define sysstatus_ansi              0x00000001
#define sysstatus_color             0x00000002
#define sysstatus_music             0x00000004
#define sysstatus_pause_on_page     0x00000008
#define sysstatus_expert            0x00000010
#define sysstatus_smw               0x00000020
#define sysstatus_full_screen       0x00000040
#define sysstatus_nscan_file_system 0x00000080
#define sysstatus_extra_color       0x00000100
#define sysstatus_clr_scrn          0x00000200
#define sysstatus_upper_ASCII       0x00000400
#define sysstatus_no_tag            0x00000800
#define sysstatus_conference        0x00001000
#define sysstatus_nochat            0x00002000
#define sysstatus_no_msgs           0x00004000
#define sysstatus_menusys           0x00008000
#define sysstatus_listplus          0x00010000
#define sysstatus_auto_quote        0x00020000
#define sysstatus_24hr_clock        0x00040000
#define sysstatus_msg_priority      0x00080000


// slrec.ability
#define ability_post_anony          0x0001
#define ability_email_anony         0x0002
#define ability_read_post_anony     0x0004
#define ability_read_email_anony    0x0008
#define ability_limited_cosysop     0x0010
#define ability_cosysop             0x0020
#define ability_val_net             0x0040

// subboardrec.anony
#define anony_enable_anony          0x01
#define anony_enable_dear_abby      0x02
#define anony_force_anony           0x04
#define anony_real_name             0x08
#define anony_val_net               0x10
#define anony_ansi_only             0x20
#define anony_no_tag                0x40
#define anony_require_sv            0x80

// postrec.anony, mailrec.anony
#define anony_sender                0x01
#define anony_sender_da             0x02
#define anony_sender_pp             0x04
#define anony_receiver              0x10
#define anony_receiver_da           0x20
#define anony_receiver_pp           0x40

// directoryrec.mask
#define mask_PD                     0x0001
#define mask_no_uploads             0x0004
#define mask_archive                0x0008
#define mask_pending_batch          0x0010
#define mask_no_ratio               0x0020
#define mask_cdrom                  0x0040
#define mask_offline                0x0080
#define mask_uploadall              0x0200
#define mask_guest                  0x0400
#define mask_extended               0x0800
#define mask_wwivreg                0x1000

// postrec.status
#define status_unvalidated          0x01
#define status_delete               0x02
#define status_no_delete            0x04
#define status_pending_net          0x08
#define status_post_source_verified 0x10
#define status_post_new_net         0x20

// mailrec.status
#define status_multimail            0x01
#define status_source_verified      0x02
#define status_new_net              0x04
#define status_seen                 0x08
#define status_replied              0x10
#define status_forwarded            0x20
#define status_file                 0x80

// configrec.sysconfig
#define sysconfig_no_local          0x00001
#define sysconfig_no_beep           0x00002
#define sysconfig_enable_pipes      0x00004
#define sysconfig_off_hook          0x00008
#define sysconfig_two_color         0x00010
#define sysconfig_enable_mci        0x00020
#define sysconfig_titlebar          0x00040
#define sysconfig_list              0x00080
#define sysconfig_no_xfer           0x00100
#define sysconfig_2_way             0x00200
#define sysconfig_no_alias          0x00400
#define sysconfig_all_sysop         0x00800
#define sysconfig_free_phone        0x02000
#define sysconfig_log_dl            0x04000
#define sysconfig_extended_info     0x08000
#define sysconfig_high_speed        0x10000 // for INIT and NET only
//#define sysconfig_flow_control    0x20000 // for INIT and NET only

// configoverrec.comflags
#define comflags_buffered_uart      0x0001

#define ansir_ansi                  0x01
#define ansir_no_DOS                0x02
#define ansir_emulate_fossil        0x04
#define ansir_shrink                0x08
#define ansir_no_pause              0x10
#define ansir_local_only            0x20
#define ansir_multi_user            0x40

// newexternalrec.othr
#define othr_error_correct          0x0001
#define othr_bimodem                0x0002
#define othr_override_internal      0x0004

// statusrec.filechange [0..6]
#define filechange_names            0
#define filechange_upload           1
#define filechange_posts            2
#define filechange_email            3
#define filechange_net              4

// g_flags
#define g_flag_disable_conf         0x00000001
#define g_flag_disable_pause        0x00000002
#define g_flag_scanned_files        0x00000004
#define g_flag_made_find_str        0x00000008
//#define g_flag_pipe_colors        0x00000010
#define g_flag_allow_extended       0x00000020
#define g_flag_disable_mci          0x00000040
#define g_flag_ansi_movement        0x00000080

#define PREV        		    1
#define NEXT        		    2
#define DONE        		    4
#define ABORTED     		    8

#define NUM_ONLY    		    1
#define UPPER_ONLY   		    2
#define ALL         		    4
#define SET           		    8

#define XFER_TIME(b) (modem_speed?\
    (((double)(((b)+127)/128))*1280.0/((double)modem_speed))\
    :0.0)

struct line {
	char text[160];

	struct line *prev, *next;
};


struct ext_desc_type {
	char name[13];

	short len;
};


struct gfiledirrec {
	char name[41],                              // g-file section name
	     filename[9];                            // g-file database filename

	unsigned char sl,                           // sl required to read
	         age;                                    // minimum age for section

	unsigned short maxfiles,                    // max # of files
	         ar;                                     // AR for g-file section
};


struct gfilerec {
	char description[81],                       // description of file
	     filename[13];                           // filename of file

	int32_t daten;                                 // date added
};


struct languagerec {
	char name[20];                              // language name

	unsigned char num;                          // language number

	char dir[79],                               // language directory
	     mdir[79],                               // menu directory
	     adir[79];                               // ansi directory
};


// modem info structure

#define mode_norm     1                     // normal status
#define mode_ring     2                     // phone is ringing
#define mode_dis      3                     // disconnected (no connection)
#define mode_err      4                     // error encountered
#define mode_ringing  5                     // remote phone is ringing
#define mode_con      6                     // connection established
#define mode_ndt      7                     // no dial tone
#define mode_fax      8                     // fax connection
#define mode_cid_num  9                     // caller-id info, phone #
#define mode_cid_name 10                    // caller-id info, caller's name

#define flag_as       1                     // asymmetrical baud rates
#define flag_ec       2                     // error correction in use
#define flag_dc       4                     // data compression in use
#define flag_fc       8                     // flow control should be used
#define flag_append   16                    // description string should be appended
///////////////////////////////////////////////////////////////////////////////

#define SECONDS_PER_HOUR		    3600L
#define SECONDS_PER_HOUR_FLOAT	    3600.0
#define SECONDS_PER_DAY			    86400L
#define SECONDS_PER_DAY_FLOAT       86400.0
#define HOURS_PER_DAY               24L
#define HOURS_PER_DAY_FLOAT         24.0
#define MINUTES_PER_HOUR            60L
#define MINUTES_PER_HOUR_FLOAT      60.0
#define SECONDS_PER_MINUTE          60L
#define SECONDS_PER_MINUTE_FLOAT	60.0


struct result_info {
	char result[41];
	char description[31];
	unsigned short main_mode;
	unsigned short flag_mask;
	unsigned short flag_value;
	unsigned short com_speed;
	unsigned short modem_speed;
};


struct modem_info {
	unsigned short ver;

	char name[81];

	char init[161];

	char setu[161];

	char ansr[81];

	char pick[81];

	char hang[81];

	char dial[81];

	char sepr[10];

	result_info defl;

	unsigned short num_resl;

	result_info resl[1];
};


// Dropfile stuff

struct pcboard_sys_rec {
	char    display[2], printer[2], page_bell[2], alarm[2], sysop_next,
	        errcheck[2], graphics, nodechat, openbps[5], connectbps[5];

	short int usernum;

	char firstname[15], password[12];

	short int time_on, prev_used;

	char time_logged[5];

	short int time_limit, down_limit;

	char curconf, bitmap1[5], bitmap2[5];

	short int time_added, time_credit;

	char slanguage[4], name[25];

	short int sminsleft;

	char snodenum, seventtime[5], seventactive[2],
	     sslide[2], smemmsg[4], scomport, packflag, bpsflag;

	// PCB 14.5 extra stuff
	char ansi, lastevent[8];

	short int lasteventmin;

	char exittodos, eventupcoming;

	short int lastconfarea;

	char hconfbitmap;
	// end PCB 14.5 additions
};

// conferencing stuff

#define conf_status_ansi       0x0001       // ANSI required
#define conf_status_wwivreg    0x0002       // WWIV regnum required
#define conf_status_offline    0x0004       // conference is "offline"

#define CONF_UPDATE_INSERT     1
#define CONF_UPDATE_DELETE     2
#define CONF_UPDATE_SWAP       3

#define SUBCONF_TYPE unsigned short
#define MAX_CONFERENCES 26

struct confrec {
	unsigned char designator,                 // A to Z?
	         name[61],                                // Name of conference
	         minsl,                                   // Minimum SL needed for access
	         maxsl,                                   // Maximum SL allowed for access
	         mindsl,                                  // Minimum DSL needed for access
	         maxdsl,                                  // Maximum DSL allowed for acces
	         minage,                                  // Minimum age needed for access
	         maxage,                                  // Maximum age allowed for acces
	         sex;                                     // Gender: 0=male, 1=female 2=all
	SUBCONF_TYPE status,                      // Bit-mapped stuff
	             minbps,                                  // Minimum bps rate for access
	             ar,                                      // ARs necessary for access
	             dar,                                     // DARs necessary for access
	             num,                                     // Num "subs" in this conference
	             maxnum,                                  // max num subs allocated in 'subs'
	             *subs;                                    // "Sub" numbers in the conference
};

struct filestatusrec {
	short int user;

	char filename[13];

	int32_t id;

	uint32_t numbytes;
};

///////////////////////////////////////////////////////////////////////////////

#define CHAINFILE_CHAIN       0
#define CHAINFILE_DORINFO     1
#define CHAINFILE_PCBOARD     2
#define CHAINFILE_CALLINFO    3
#define CHAINFILE_DOOR        4
#define CHAINFILE_DOOR32      5

///////////////////////////////////////////////////////////////////////////////

#define EFLAG_NONE            0x0000        // nothing.
#define EFLAG_ABORT           0x0001        // check for a ^C to abort program
#define EFLAG_INTERNAL        0x0002        // make it look internal to BBS
#define EFLAG_NOHUP           0x0004        // don't check for hangup (UL event)
#define EFLAG_COMIO           0x0008        // redirect IO to com port
#define EFLAG_NOPAUSE         0x0040        // disable pause in remote
#define EFLAG_NETPROG         0x0080        // try running out of net dir first
#define EFLAG_TOPSCREEN       0x0100        // leave topscreen as-is
#define EFLAG_FOSSIL          0x0200        // Use Win32 Emulated FOSSIL

///////////////////////////////////////////////////////////////////////////////

#define SPWANOPT_TIMED          0
#define SPWANOPT_NEWUSER        1
#define SPWANOPT_BEGINDAY       2
#define SPWANOPT_LOGON          3
#define SPWANOPT_ULCHK          4
#define SPWANOPT_FSED           5
#define SPWANOPT_PROT_SINGLE    6
#define SPWANOPT_PROT_BATCH     7
#define SPWANOPT_CHAT           8
#define SPWANOPT_ARCH_E         9
#define SPWANOPT_ARCH_L         10
#define SPWANOPT_ARCH_A         11
#define SPWANOPT_ARCH_D         12
#define SPWANOPT_ARCH_K         13
#define SPWANOPT_ARCH_T         14
#define SPWANOPT_NET_CMD1       15
#define SPWANOPT_NET_CMD2       16
#define SPWANOPT_LOGOFF         17
#define SPWANOPT_V_SCAN         18
#define SPWANOPT_NETWORK        19


///////////////////////////////////////////////////////////////////////////////
#define INI_TAG "WWIV"

#define OP_FLAGS_FORCE_NEWUSER_FEEDBACK   0x00000001
#define OP_FLAGS_CHECK_DUPE_PHONENUM      0x00000002
#define OP_FLAGS_HANGUP_DUPE_PHONENUM     0x00000004
#define OP_FLAGS_SIMPLE_ASV               0x00000008
#define OP_FLAGS_POSTTIME_COMPENSATE      0x00000010
#define OP_FLAGS_SHOW_HIER                0x00000020
#define OP_FLAGS_IDZ_DESC                 0x00000040
#define OP_FLAGS_SETLDATE                 0x00000080
#define OP_FLAGS_SLASH_SZ                 0x00000200
#define OP_FLAGS_READ_CD_IDZ              0x00000400
#define OP_FLAGS_FSED_EXT_DESC            0x00000800
#define OP_FLAGS_FAST_TAG_RELIST          0x00001000
#define OP_FLAGS_MAIL_PROMPT              0x00002000
#define OP_FLAGS_SHOW_CITY_ST             0x00004000
#define OP_FLAGS_NO_EASY_DL               0x00008000
#define OP_FLAGS_NEW_EXTRACT              0x00010000
#define OP_FLAGS_FAST_SEARCH              0x00020000
#define OP_FLAGS_NET_CALLOUT              0x00040000
#define OP_FLAGS_WFC_SCREEN               0x00080000
#define OP_FLAGS_FIDO_PROCESS             0x00100000
#define OP_FLAGS_USER_REGISTRATION        0x00200000
#define OP_FLAGS_MSG_TAG                  0x00400000
#define OP_FLAGS_CHAIN_REG                0x00800000
#define OP_FLAGS_CAN_SAVE_SSM             0x01000000
#define OP_FLAGS_EXTRA_COLOR              0x02000000
#define OP_FLAGS_USE_FORCESCAN            0x04000000
#define OP_FLAGS_NEWUSER_MIN              0x08000000
#define OP_FLAGS_THREAD_SUBS              0x10000000
#define OP_FLAGS_CALLBACK                 0x20000000
#define OP_FLAGS_VOICE_VAL                0x40000000
#define OP_FLAGS_ADV_ASV                  0x80000000


///////////////////////////////////////////////////////////////////////////////

struct asv_rec {
	unsigned char
	sl, dsl, exempt;

	unsigned short
	ar, dar, restrict;
};

struct adv_asv_rec {
	unsigned char reg_wwiv, nonreg_wwiv, non_wwiv, cosysop;
};


///////////////////////////////////////////////////////////////////////////////
// begin callback additions

struct cbv_rec {
	unsigned char
	sl, dsl, exempt, longdistance, forced, repeat;

	unsigned short
	ar, dar, restrict;
};

///////////////////////////////////////////////////////////////////////////////
// end callback additions

// QUICK REFERNCE TO FIND USER INPUT_MODE_PHONE NUMBER

struct phonerec {
	short int usernum;                        // user's number

	unsigned char phone[13];                  // user's phone number
};

// begin events additions

#define MAX_EVENT 30                        // max number of timed events

#define EVENT_FORCED       0x0001           // force user off to run event?
#define EVENT_SHRINK       0x0002           // shrink for event?
#define EVENT_HOLD         0x0004           // holdphone for event?
#define EVENT_EXIT         0x0008           // exit bbs completely to run?
#define EVENT_PERIODIC     0x0010           // event runs periodically
#define EVENT_RUNTODAY     0x8000           // has it run today?

struct eventsrec {
	char cmd[81];                             // commandline to execute

	short int days,                           // days to run this event
		time,                                 // time to run event in minutes
		instance,                             // instance to run event on
		status,                               // bit mapped event status
		period,                               // execution period
	    lastrun;                              // timestamp of last execution

	char resv[25];                            // reserved
};

// end events additions


///////////////////////////////////////////////////////////////////////////////

struct threadrec {
	short int used;                           // Record used?

	unsigned short int msg_num,               // Message Number
	         parent_num;                              // Parent Message #

	char message_code[20],                    // Message's ID Code
	     parent_code[20];                         // Message's Reply Code
};

///////////////////////////////////////////////////////////////////////////////

struct ext_desc_rec {
	char name[13];

	int32_t offset;
};



// Text editing modes for input routines
#define INPUT_MODE_FILE_UPPER     0
#define INPUT_MODE_FILE_MIXED     1
#define INPUT_MODE_FILE_PROPER    2
#define INPUT_MODE_FILE_NAME 3
#define INPUT_MODE_DATE      4
#define INPUT_MODE_PHONE     5

// Used by scan(...)
#define SCAN_OPTION_READ_PROMPT		0
#define SCAN_OPTION_LIST_TITLES		1
#define SCAN_OPTION_READ_MESSAGE	2


struct instancerec {
	short
	number, user;
	unsigned short
	flags, loc, subloc;
	uint32_t last_update;
	unsigned short modem_speed;
	uint32_t inst_started;
	unsigned char
	extra[80];
};

struct ch_action {
	int r;
	char aword[12];
	char toprint[80];
	char toperson[80];
	char toall[80];
	char singular[80];
};

struct ch_type {
	char name[60];
	int sl;
	char ar;
	char sex;
	char min_age;
	char max_age;
};


struct fedit_data_rec {
	char tlen,
	     ttl[81],
	     anon;
};


// .ZIP structures and defines
#define ZIP_LOCAL_SIG 0x04034b50
#define ZIP_CENT_START_SIG 0x02014b50
#define ZIP_CENT_END_SIG 0x06054b50

struct zip_local_header {
	uint32_t   signature;                  // 0x04034b50
	unsigned short  extract_ver;
	unsigned short  flags;
	unsigned short  comp_meth;
	unsigned short  mod_time;
	unsigned short  mod_date;
	uint32_t   crc_32;
	uint32_t   comp_size;
	uint32_t   uncomp_size;
	unsigned short  filename_len;
	unsigned short  extra_length;
};


struct zip_central_dir {
	uint32_t   signature;                  // 0x02014b50
	unsigned short  made_ver;
	unsigned short  extract_ver;
	unsigned short  flags;
	unsigned short  comp_meth;
	unsigned short  mod_time;
	unsigned short  mod_date;
	uint32_t   crc_32;
	uint32_t   comp_size;
	uint32_t   uncomp_size;
	unsigned short  filename_len;
	unsigned short  extra_len;
	unsigned short  comment_len;
	unsigned short  disk_start;
	unsigned short  int_attr;
	uint32_t   ext_attr;
	uint32_t   rel_ofs_header;
};


struct zip_end_dir {
	uint32_t   signature;                  // 0x06054b50
	unsigned short  disk_num;
	unsigned short  cent_dir_disk_num;
	unsigned short  total_entries_this_disk;
	unsigned short  total_entries_total;
	uint32_t   central_dir_size;
	uint32_t   ofs_cent_dir;
	unsigned short  comment_len;
};


struct arch {
	unsigned char type;
	char name[13];
	int32_t len;
	short int date, time, crc;
	int32_t size;
};





///////////////////////////////////////////////////////////////////////////////


#ifndef bbsmalloc
#define bbsmalloc(x) malloc(x)
#define BbsFreeMemory(x) free(x)
#endif // bbsmaloc

#pragma pack(pop)

#endif // __INCLUDED_VARDEC_H__
