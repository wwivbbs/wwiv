/**************************************************************************/
/*                                                                        */
/*                              Wwiv Version 5.x                          */
/*             Copyright (C)1998-2017, WWIV Software Services             */
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

#ifdef __MSDOS__
#include "sdk/msdos_stdint.h"
#else
#include <cstddef>
#include <cstdint>
#endif  // __MSDOS__
#include <string>

#ifndef __MSDOS__
#pragma pack(push, 1)
#endif  // __MSDOS__

#ifndef DATEN_T_DEFINED
typedef uint32_t daten_t;
#define DATEN_T_DEFINED
#endif

// DATA FOR EVERY USER
struct userrec {
  // user's name/handle
  unsigned char name[31];
  // user's real name
  unsigned char realname[21];
  // user's amateur callsign
  char callsign[7];
  // user's phone number
  char phone[13];
  // user's data phone
  char dataphone[13];
  // street address
  char street[31];
  // city
  char city[31];
  // state code [MO, CA, etc]
  char state[3];
  // country [USA, CAN, FRA, etc]
  char country[4];
  // zipcode [#####-####]
  char zipcode[11];
  // user's password
  char pw[9];
  // last date on
  char laston[9];
  // first date on
  char firston[9];
  // sysop's note about user
  char note[61];
  // macro keys
  unsigned char macros[3][81];
  // gender.
  char sex;

  // Internet mail address
  char email[65];
  // bytes for more strings
  char res_char[13];
  
  // user's age
  uint8_t age;
  // if deleted or inactive
  uint8_t inact;

  // computer type
  int8_t comp_type;

  uint8_t defprot,                                  // default transfer protocol
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
           unused_cbv;                              // called back

  uint32_t lp_options;
  uint8_t lp_colors[32];
  // Selected AMENU set to use
  char menu_set[9];
  // Use hot keys in AMENU
  uint8_t hot_keys;

  char res_byte[3];

  uint16_t
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

  // last sub at logoff
  uint16_t subnum;
  // last dir at logoff
  uint16_t dirnum;

  // reserved for short values
  char res_short[40];

  // total num msgs read
  uint32_t msgread;
  // number of k uploaded
  uint32_t uk;
  // number of k downloaded
  uint32_t dk;
  // numerical time last on
  daten_t daten;
  // status/defaults
  uint32_t sysstatus;
  // user's WWIV reg number
  uint32_t wwiv_regnum;
  // points to spend for files
  uint32_t filepoints;
  // numerical registration date
  daten_t unused_registered;
  // numerical expiration date
  daten_t unused_expires;
  // numerical date of last file scan
  daten_t datenscan;
  // bit mapping for name case
  uint32_t unued_nameinfo;

  // reserved for long values
  char res_long[40];

  // time on today
  float timeontoday;
  // time left today
  float extratime;
  // total time on system
  float timeon;
  // $ credit
  float pos_account;
  // $ debit
  float neg_account;

  // game money
  float gold;

  // reserved for real values
  char res_float[32];
  // reserved for whatever
  char res_gp[94];

  uint16_t qwk_max_msgs;
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
  uint16_t time_per_day,                // time allowed on per day
           time_per_logon,              // time allowed on per logon
           messages_read,               // messages allowed to read
           emails,                      // number emails allowed
           posts;                       // number posts allowed
  uint32_t ability;                     // bit mapped abilities
};


// AUTO-VALIDATION DATA
struct valrec {
  uint8_t sl,                           // SL
          dsl;                          // DSL

  uint16_t ar,                          // AR
           dar,                         // DAR
           restrict;                    // restrictions
};


struct arcrec_424_t {
  char extension[4],                          // extension for archive
       arca[32],                               // add commandline
       arce[32],                               // extract commandline
       arcl[32];                               // list commandline
};

// Archiver Configuration introduced in WWIV 4.30
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

/*
 * WWIV 5.2+ config.dat header.
 */
struct configrec_header_t {
  // "WWIV"+ NULL
  char signature[5];
  uint8_t padding[1];
  uint16_t written_by_wwiv_num_version;
  uint32_t config_revision_number;
  uint32_t config_size;
  uint8_t unused[5];
};

// STATIC SYSTEM INFORMATION
struct configrec {
  union {
    // new user password
    char newuserpw[21];
    configrec_header_t header;
  } header;
  char systempw[21],                           // system password
       msgsdir[81],                            // path for msgs directory
       gfilesdir[81],                          // path for gfiles dir
       datadir[81],                            // path for data directory
       dloadsdir[81],                          // path for dloads dir
       unused_ramdrive,                        // drive for ramdisk
       tempdir[81],                            // path for temporary directory
       xmark,                                  // 0xff
       // former reg code + modem config.
       // Script base directory.  Typically C:\wwiv\scripts
       scriptdir[81],                           
       unused1[242],
       systemname[51],                         // BBS system name
       systemphone[13],                        // BBS system phone number
       sysopname[51],                          // sysop's name
       unused2[51];                  // Old single event command (removed in 4.3i0)

  uint8_t newusersl,                           // new user SL
          newuserdsl,                          // new user DSL
          maxwaiting,                          // max mail waiting
          unused3[10],
          primaryport,                         // primary comm port
          newuploads,                          // file dir new uploads go
          closedsystem;                        // if system is closed

  uint16_t systemnumber;                       // BBS system number
  
  // Former baud rate and comports
  uint8_t  unused4[20];
  // max users on system
  uint16_t maxusers;
  // new user restrictions
  uint16_t newuser_restrict;
  // System configuration
  uint16_t sysconfig;
  // Chat time on
  uint16_t sysoplowtime;
  // Chat time off
  uint16_t sysophightime;                      
  // time to run mail router
  uint16_t unused_executetime;

  // Formerly required up/down ratio. This has been moved to wwiv.ini
  float req_ratio,
        newusergold;                           // new user gold

  slrec sl[256];                               // security level data
 
  valrec autoval[10];                          // sysop quik-validation data

  uint8_t unused5[151];

  arcrec_424_t arcs[4];                       // old archivers (4.24 format)

  uint8_t unused6[102];

  int16_t userreclen,                         // user record length
        waitingoffset,                        // mail waiting offset
        inactoffset;                          // inactive offset

  uint8_t unused7[51];           // newuser event

  uint32_t wwiv_reg_number;                   // user's reg number

  char newuserpw[21];

  float post_call_ratio;

  uint8_t unused8[222];

  int16_t sysstatusoffset;                      // system status offset

  char unused_network_type;                     // network type ID

  int16_t fuoffset, fsoffset, fnoffset;         // offset values

  // max subboards
  uint16_t max_subs;
  // max directories
  uint16_t max_dirs;                               
  // qscan pointer length in bytes
  uint16_t qscn_len;

  uint8_t email_storage_type;                 // how to store email

  uint8_t unused9[8];

  char menudir[81];                           // path for menu dir
  uint8_t res[502];                              // RESERVED
};


// overlay information per instance
struct legacy_configovrrec_424_t {
  uint8_t unused1[9];
  uint8_t primaryport;
  uint8_t unused2[27];
  char tempdir[81];
  char batchdir[81];
  char unused3[313];
};


// DYNAMIC SYSTEM STATUS
struct statusrec_t {
  char date1[9],                               // last date active
       date2[9],                               // date before now
       date3[9],                               // two days ago
       log1[13],                               // yesterday's log
       log2[13],                               // two days ago log
       gfiledate[9],                           // date gfiles last updated
       filechange[7];                          // flags for files changing

  // how many local posts today
  uint16_t localposts;
  // Current number of users
  uint16_t users;
  // Current caller number
  uint16_t callernum;
  // Number of calls today
  uint16_t callstoday;
  // Messages posted today
  uint16_t msgposttoday;
  // Email sent today
  uint16_t emailtoday;
  // Feedback sent today
  uint16_t fbacktoday;
  // files uploaded today
  uint16_t uptoday;
  // Minutes active today
  uint16_t activetoday;
  // Q-scan pointer value
  uint32_t qscanptr;
  // auto-message anony stat
  char amsganon;
  // user who wrote a-msg
  uint16_t amsguser;
  // caller number
  uint32_t callernum1;
  // word for net editor
  uint16_t unused_net_edit_stuff;
  // tell what version it is
  uint16_t wwiv_version;
  // tell what version of net
  uint16_t net_version;
  // network bias factor
  float net_bias;
  // date last connect.net
  int32_t last_connect;
  // date last bbslist.net
  int32_t last_bbslist;
  // net free factor def 3
  float net_req_free;
  // # days BBS active
  uint16_t days;
  // RESERVED
  char res[29];
};

/**
 * ON DISK format for MESSAGE BASE INFORMATION (SUBS.DAT)
 * This has been the same since *at least* 4.22.
 */
struct subboardrec_422_t {
  // board name
  char name[41];
  // board database filename
  char filename[9];
  // board special key
  char key;

  // sl required to read
  uint8_t readsl;
  // sl required to post
  uint8_t postsl;
  // anonymous board?
  uint8_t anony;
  // minimum age for sub
  uint8_t age;
  // max # of msgs
  uint16_t maxmsgs;
  // AR for sub-board
  uint16_t ar;
  // how messages are stored
  uint16_t storage_type;
  // 4 digit board type
  uint16_t unused_legacy_type;
};

// UPLOAD DIRECTORY INFORMATION
struct directoryrec {
  // directory name
  char name[41];
  // direct database filename
  char filename[9];
  // filename path
  char  path[81];
  // DSL for directory
  uint8_t dsl;
  // minimum age for directory
  uint8_t age;
  // DAR for directory
  uint16_t dar;
  // max files for directory
  uint16_t maxfiles;
  // file type mask
  uint16_t mask;
  // 4 digit directory type
  uint16_t type;
};

// QUICK REFERNCE TO FIND USER NUMBER FROM NAME
struct smalrec {
  // User name.
  unsigned char name[31];
  // User number.
  uint16_t number;
};

// TYPE-1 and TYPE-2 locator to find the message text.
struct messagerec {
  // how it is stored (type, 1 or 2)
  uint8_t storage_type;
  // where it is stored (type specific)
  uint32_t stored_as;
};

// Union types used by postrec/mailrec network settings.
// Used when it is source verified.
struct source_verified_message_t {
  // network number for the message
  uint8_t net_number;
  // Type for a verified message. (same as minor type)
  uint16_t source_verified_type;
};

// Union types used by postrec/mailrec network settings.
// Used when it's not source verified.
struct network_message_t {
  uint16_t unused;
  // network number for the message
  uint8_t net_number;
};

struct subfile_header_t {
  // "WWIV^Z\000"
  char signature[6];
  // WWIV version used to create this sub.
  uint16_t wwiv_version;
  // Message SDK compatibility revision used to create this sub.  
  // Currently only value is 1.
  uint32_t revision;
  // 32-bit time_t value for when this sub was created.
  int64_t daten_created;
  // Modification count for this sub.  Every write to this file should
  // Increase the mod count.  This can be used for caching.
  uint64_t mod_count;
  // For future expansion: 32-bit CRC password to access this sub.
  uint32_t password_crc32;
  // in WWIV type-2 message bases, this is always 0.
  uint32_t base_message_num;
  // UNUSED
  uint8_t padding_1[49];
  // Number of messages in this area.
  uint16_t active_message_count;
  // UNUSED
  uint8_t padding_2[13];
};

// DATA HELD FOR EVERY POST
struct postrec {
  // title of post
  char title[72];
  // UNUSED padding from title.
  uint8_t padding_from_title[6];
  union {
    // Used if this is a source verified message.
    source_verified_message_t src_verified_msg;
    // Used if this is a network message.
    network_message_t network_msg;
  } network;

  // anony-stat of message
  uint8_t anony;
  // bit-mapped status
  uint8_t status;

  // what system it came from
  uint16_t ownersys;
  // User number who posted it
  uint16_t owneruser;

  // qscan pointer
  uint32_t qscan;
  // 32-bit time_t value for the date posted
  daten_t daten;

  // where to find it in the type-2 messagebase.
  messagerec msg;
};

// DATA HELD FOR EVERY E-MAIL OR F-BACK
struct mailrec {
  char title[72];                       // title of message
  uint8_t padding_from_title[6];
  union {
    source_verified_message_t src_verified_msg;
    network_message_t network_msg;
  } network;

  uint8_t anony,                        // anonymous mail?
          status;                       // status for e-mail

  uint16_t fromsys,                     // originating system
           fromuser,                    // originating user
           tosys,                       // destination system
           touser;                      // destination user

  daten_t daten;                       // date it was sent

  messagerec msg;                       // where to find it
};

// USED IN READMAIL TO STORE EMAIL INFO
struct tmpmailrec {
  int16_t index;                                // index into email.dat

  uint16_t fromsys,                     // originating system
           fromuser;                               // originating user

  daten_t daten;                        // date it was sent

  messagerec msg;                             // where to find it
};


// SHORT RESPONSE FOR USER, TELLING HIS MAIL HAS BEEN READ
struct shortmsgrec {
  char message[81];                           // short message to user

  uint16_t tosys, touser;               // who it is to
};



// VOTING RESPONSE DATA
struct voting_response {
  char response[81];                          // Voting question response

  uint16_t numresponses;                // number of responses
};



// VOTING DATA INFORMATION
struct votingrec {
  char question[81];                          // Question

  uint8_t numanswers;                   // number of responses

  voting_response responses[20];              // actual responses
};



// DATA HELD FOR EVERY UPLOAD
struct uploadsrec {
  char filename[13],                          // filename
       description[59],                        // file description
       date[9],                                // date u/l'ed
       upby[37],                               // name of upload user
       actualdate[9];                          // actual file date

  uint8_t unused_filetype;               // file type for apples

  uint16_t numdloads,                   // number times d/l'ed
           ownersys, ownerusr,                     // who uploaded it
           mask;                                   // file type mask

  daten_t daten;                        // date uploaded
  daten_t numbytes;                               // number bytes long file is
};

// ZLOG INFORMATION FOR PAST SYSTEM USAGE
struct zlogrec {
  char date[9];                               // zlog for what date

  uint16_t active,                      // number minutes active
           calls,                                  // number calls
           posts,                                  // number posts
           email,                                  // number e-mail
           fback,                                  // number f-back
           up;                                     // number uploads
};



// DATA FOR OTHER PROGRAMS AVAILABLE
struct chainfilerec {
  char filename[81],                    // filename for .chn file
       description[81];                 // description of it

  uint8_t sl,                           // seclev restriction
          ansir;                        // chain flags (ANSI required, FOSSIL, etc)

  uint16_t ar;                          // AR restriction
};


struct chainregrec {
  int16_t regby[5],                         // who registered
        usage;                                  // number of runs

  uint8_t minage,                       // minimum age necessary
           maxage;                                 // maximum age allowed

  char space[50];                             // reserved space
};


// DATA FOR EXTERNAL PROTOCOLS

struct newexternalrec {
  char description[81],                        // protocol description
       receivefn[81],                          // receive filename
       sendfn[81],                             // send filename
       receivebatchfn[81],                     // batch receive cmd
       sendbatchfn[81],                        // batch send cmd
       unused_bibatchfn[81];                   // batch send/receive cmd

  uint16_t ok1,                                // if sent
           othr;                               // other flags

  char pad[22];
};


#define EDITORREC_EDITOR_TYPE_WWIV 0
#define EDITORREC_EDITOR_TYPE_QBBS 1

// DATA FOR EXTERNAL EDITORS
struct editorrec {
  char description[81],                       // description of editor
       filename[81];                          // how to run the editor
  uint8_t bbs_type;                           // 0=WWIV, 1=QBBS
  uint8_t ansir;                              // Editor Flags (ANSI required, FOSSIL, etc)
  uint8_t unused[2];                          // TODO This was not used (config)

  char filenamecon[81];                       // how to run locally

  char res[119];
};

// DATA FOR CONVERSION OF MAIN MENU KEYS TO SUB-BOARD NUMBERS
struct usersubrec {
  char keys[5];

  int16_t subnum;
};


struct userconfrec {
  int16_t confnum;
};


struct batchrec {
  bool sending;

  char filename[13];

  int16_t dir;

  float time;

  int32_t len;
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

// subs anony and flags. Lower half of this is anoymous
// and the high bit is flags.
#define anony_none                  0x00
#define anony_enable_anony          0x01
#define anony_enable_dear_abby      0x02
#define anony_force_anony           0x04
#define anony_real_name             0x08
#define anony_val_net               0x10
#define anony_ansi_only             0x20
#define anony_no_tag                0x40
// Don't use the full screen reader for displaying message text.
// This is used for ANSI art subs.
#define anony_no_fullscreen         0x80

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
#define unused_sysconfig_no_local  0x00001
#define unused_sysconfig_no_beep           0x00002
#define unused_sysconfig_enable_pipes 0x00004
#define sysconfig_no_newuser_feedback 0x00008
#define unused_sysconfig_two_color         0x00010
#define unused_sysconfig_enable_mci 0x00020
#define sysconfig_titlebar          0x00040
#define unused_sysconfig_list              0x00080
#define sysconfig_no_xfer           0x00100
#define sysconfig_2_way             0x00200
#define sysconfig_no_alias          0x00400
#define sysconfig_all_sysop         0x00800
#define sysconfig_free_phone        0x02000
#define sysconfig_log_dl            0x04000
#define sysconfig_extended_info     0x08000
#define unused_sysconfig_1          0x10000
#define unused_sysconfig_2          0x20000

#define ansir_ansi                  0x01
#define ansir_no_DOS                0x02
#define ansir_emulate_fossil        0x04
#define ansir_stdio                 0x08
// Execute this command from the instance temp directory
// instead of the main BBS directory.
#define ansir_temp_dir              0x10
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
// #define g_flag_allow_extended       0x00000020
// #define g_flag_disable_mci          0x00000040
// Used to tell the message reader that ANSI movement has occurred.
//#define g_flag_ansi_movement        0x00000080

struct ext_desc_type {
  char name[13];

  int16_t len;
};


struct gfiledirrec {
  char name[41],                              // g-file section name
       filename[9];                            // g-file database filename

  uint8_t sl,                           // sl required to read
           age;                                    // minimum age for section

  uint16_t maxfiles,                    // max # of files
           ar;                                     // AR for g-file section
};


struct gfilerec {
  char description[81],                       // description of file
       filename[13];                           // filename of file

  daten_t daten;                                 // date added
};


struct languagerec {
  char name[20];                              // language name

  uint8_t num;                          // language number

  char dir[79],                               // language directory
       mdir[79],                               // menu directory
       adir[79];                               // ansi directory
};

// conferencing stuff

#define conf_status_ansi       0x0001       // ANSI required
#define conf_status_wwivreg    0x0002       // WWIV regnum required
#define conf_status_offline    0x0004       // conference is "offline"

constexpr int MAX_CONFERENCES = 26;
constexpr int WWIV_MESSAGE_TITLE_LENGTH = 72;

struct filestatusrec {
  int16_t user;

  char filename[13];

  int32_t id;

  uint32_t numbytes;
};

///////////////////////////////////////////////////////////////////////////////
// Execute Flags.  Used when spawning chains and external programs.

// Default option, do nothing.
#define EFLAG_NONE            0x0000
// don't check for hangup (typically used for the upload event)
#define EFLAG_NOHUP           0x0004
// redirect DOS IO to com port
#define EFLAG_COMIO           0x0008
// try running out of net dir first
#define EFLAG_NETPROG         0x0080
// Use Win32 Emulated FOSSIL
#define EFLAG_FOSSIL          0x0200
// Use STDIO based doors for Window/Linux.  This will set the stdin/stdout
// file descriptors to the socket before spawning the chain.
#define EFLAG_STDIO           0x0400
// Run out of the TEMP directory instead of the BBS directory
#define EFLAG_TEMP_DIR        0x0800

///////////////////////////////////////////////////////////////////////////////

#define SPAWNOPT_TIMED          0
#define SPAWNOPT_NEWUSER        1
#define SPAWNOPT_BEGINDAY       2
#define SPAWNOPT_LOGON          3
#define SPAWNOPT_ULCHK          4
// SPAWNOPT[FSED] nolonger used. (5)
#define SPAWNOPT_PROT_SINGLE    6
#define SPAWNOPT_PROT_BATCH     7
#define SPAWNOPT_CHAT           8
#define SPAWNOPT_ARCH_E         9
#define SPAWNOPT_ARCH_L         10
#define SPAWNOPT_ARCH_A         11
#define SPAWNOPT_ARCH_D         12
#define SPAWNOPT_ARCH_K         13
#define SPAWNOPT_ARCH_T         14
#define SPAWNOPT_NET_CMD1       15
#define SPAWNOPT_NET_CMD2       16
#define SPAWNOPT_LOGOFF         17
// SPAWNOPT[V_SCAN] nolonger used. (18)
#define SPAWNOPT_NETWORK        19


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
#define OP_FLAGS_UNUSED_1                 0x00000200
#define OP_FLAGS_READ_CD_IDZ              0x00000400
#define OP_FLAGS_FSED_EXT_DESC            0x00000800
#define OP_FLAGS_FAST_TAG_RELIST          0x00001000
#define OP_FLAGS_MAIL_PROMPT              0x00002000
#define OP_FLAGS_SHOW_CITY_ST             0x00004000
//#define OP_FLAGS_NO_EASY_DL               0x00008000
//#define OP_FLAGS_NEW_EXTRACT              0x00010000
#define OP_FLAGS_FAST_SEARCH              0x00020000
#define OP_FLAGS_NET_CALLOUT              0x00040000
#define OP_FLAGS_WFC_SCREEN               0x00080000
#define OP_FLAGS_FIDO_PROCESS             0x00100000
//#define OP_FLAGS_USER_REGISTRATION        0x00200000
#define OP_FLAGS_MSG_TAG                  0x00400000
#define OP_FLAGS_CHAIN_REG                0x00800000
#define OP_FLAGS_CAN_SAVE_SSM             0x01000000
#define OP_FLAGS_EXTRA_COLOR              0x02000000
#define OP_FLAGS_USE_FORCESCAN            0x04000000
#define OP_FLAGS_NEWUSER_MIN              0x08000000
#define OP_FLAGS_NET_PROCESS              0x10000000
#define OP_FLAGS_UNUSED_2                 0x20000000
#define OP_FLAGS_UNUSED_4                 0x40000000
//#define OP_FLAGS_ADV_ASV                  0x80000000

// QUICK REFERNCE TO FIND USER INPUT_MODE_PHONE NUMBER

struct phonerec {
  int16_t usernum;                          // user's number
  char phone[13];                           // user's phone number
};

// begin events additions

#define MAX_EVENT 30                        // max number of timed events

#define EVENT_FORCED       0x0001           // force user off to run event?
#define EVENT_SHRINK       0x0002           // shrink for event?
#define UNUSED_EVENT_HOLD  0x0004           // holdphone for event?
#define EVENT_EXIT         0x0008           // exit bbs completely to run?
#define EVENT_PERIODIC     0x0010           // event runs periodically
#define EVENT_RUNTODAY     0x8000           // has it run today?

struct eventsrec {
  char cmd[81];                             // commandline to execute

  int16_t days,                           // days to run this event
        time,                                 // time to run event in minutes
        instance,                             // instance to run event on
        status,                               // bit mapped event status
        period,                               // execution period
        lastrun;                              // timestamp of last execution

  char resv[25];                            // reserved
};

// end events additions

///////////////////////////////////////////////////////////////////////////////

struct ext_desc_rec {
  char name[13];
  int32_t offset;
};

struct instancerec {
  int16_t
  number, user;
  uint16_t
  flags, loc, subloc;
  daten_t last_update;
  uint16_t modem_speed;
  uint32_t inst_started;
  uint8_t
  extra[80];
};

struct fedit_data_rec {
  char tlen,
       ttl[81],
       anon;
};

#ifndef __MSDOS__

static_assert(sizeof(userrec) == 1024, "userrec == 1024");
static_assert(sizeof(slrec) == 14, "slrec == 14");
static_assert(sizeof(valrec) == 8, "valrec == 8");
static_assert(sizeof(arcrec) == 336, "arcrec == 336");
static_assert(sizeof(configrec) == 6228, "configrec == 6228");
static_assert(sizeof(legacy_configovrrec_424_t) == 512, "legacy_configovrrec_424_t == 512");
static_assert(sizeof(statusrec_t) == 151, "statusrec == 151");
static_assert(sizeof(subboardrec_422_t) == 63, "subboardrec_422_t == 63");
static_assert(sizeof(directoryrec) == 141, "directoryrec == 141");
static_assert(sizeof(smalrec) == 33, "smalrec == 33");
static_assert(sizeof(messagerec) == 5, "messagerec == 5");
static_assert(sizeof(postrec) == 100, "postrec == 100");
static_assert(sizeof(subfile_header_t) == 100, "subfile_header_t == 100");
static_assert(offsetof(postrec, owneruser) == offsetof(subfile_header_t, active_message_count),
  "owneruser offset != active_message_count: ");
static_assert(sizeof(mailrec) == 100, "mailrec == 100");
static_assert(sizeof(tmpmailrec) == 15, "tmpmailrec == 15");
static_assert(sizeof(shortmsgrec) == 85, "shortmsgrec == 85");
static_assert(sizeof(voting_response) == 83, "voting_response == 83");
static_assert(sizeof(uploadsrec) == 144, "uploadsrec == 144");
static_assert(sizeof(zlogrec) == 21, "zlogrec == 21");
static_assert(sizeof(chainfilerec) == 166, "chainfilerec == 166");
static_assert(sizeof(chainregrec) == 64, "chainregrec == 64");
static_assert(sizeof(newexternalrec) == 512, "newexternalrec == 512");
static_assert(sizeof(editorrec) == 366, "editorrec == 366");
static_assert(sizeof(usersubrec) == 7, "usersubrec == 7");
static_assert(sizeof(userconfrec) == 2, "userconfrec == 2");
static_assert(sizeof(batchrec) == 24, "batchrec == 24");
static_assert(sizeof(ext_desc_type) == 15, "ext_desc_type == 15");
static_assert(sizeof(gfiledirrec) == 56, "gfiledirrec == 56");
static_assert(sizeof(gfilerec) == 98, "gfilerec == 98");
static_assert(sizeof(languagerec) == 258, "languagerec == 258");
static_assert(sizeof(filestatusrec) == 23, "filestatusrec == 23");
static_assert(sizeof(phonerec) == 15, "phonerec == 15");
static_assert(sizeof(eventsrec) == 118, "eventsrec == 118");
static_assert(sizeof(ext_desc_rec) == 17, "ext_desc_rec == 17");
static_assert(sizeof(instancerec) == 100, "instancerec == 100");

#pragma pack(pop)

#endif  // __MSDOS__

#endif // __INCLUDED_VARDEC_H__
