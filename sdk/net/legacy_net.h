/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2022, WWIV Software Services             */
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
#ifndef INCLUDED_SDK_NET_LEGACY_NET_H
#define INCLUDED_SDK_NET_LEGACY_NET_H

#include <cstdint>

#ifndef DATEN_T_DEFINED
typedef uint32_t daten_t;
#define DATEN_T_DEFINED
#endif

#pragma pack(push, 1)

/* All network nodes except the destination will only look at:
 *   tosys - the destination system.  Is 0 if more than one system
 *   list_len - if (tosys==0) then list_len is the number of destination
 *     systems in a list immediately after the header.  NOTE this list will
 *     be 2*list_len bytes long, since each system entry is 2 bytes.
 *   length - is the overall length of the message, not including the header
 *     or the system ist length.  Thus the overall length of a message in
 *     the net will be (sizeof(net_header_rec) + 2*list_len + length)
 */
struct net_header_rec {
  uint16_t tosys,  /* destination system */
      touser,      /* destination user */
      fromsys,     /* originating system */
      fromuser,    /* originating user */
      main_type,   /* main message type */
      minor_type,  /* minor message type */
      list_len;    /* # of entries in system list */
  daten_t daten;   /* date/time sent */
  uint32_t length; /* # of bytes of msg after header */
  uint16_t method; /* method of compression */
};

/*
 * Please note that not all of these are used yet, some will probably never
 * be used, but sounded like a good idea at the time.
 */

constexpr uint16_t main_type_net_info = 0x0001;      /* type 1 normal network updates */
constexpr uint16_t main_type_email = 0x0002;         /* type 2 email by user number */
constexpr uint16_t main_type_post = 0x0003;          /* type 3 post from sub host */
constexpr uint16_t main_type_file = 0x0004;          /* type 4 file transfer system */
constexpr uint16_t main_type_pre_post = 0x0005;      /* type 5 post to sub host */
constexpr uint16_t main_type_external = 0x0006;      /* type 6 external message */
constexpr uint16_t main_type_email_name = 0x0007;    /* type 7 email by user name */
constexpr uint16_t main_type_net_edit = 0x0008;      /* type 8 network editor packet */
constexpr uint16_t main_type_sub_list = 0x0009;      /* type 9 subs.lst update */
constexpr uint16_t main_type_extra_data = 0x000a;    /* type 10 unused */
constexpr uint16_t main_type_group_bbslist = 0x000b; /* type 11 network update from GC */
constexpr uint16_t main_type_group_connect = 0x000c; /* type 12 network update from GC */
constexpr uint16_t main_type_group_binkp = 0x000d;   /* type 13 network update from GC */
constexpr uint16_t main_type_group_info = 0x000e;    /* type 14 misc update from GC */
constexpr uint16_t main_type_ssm = 0x000f;           /* type 15 xxx read your mail */
constexpr uint16_t main_type_sub_add_req = 0x0010;   /* type 16 add me to your sub */
constexpr uint16_t main_type_sub_drop_req = 0x0011;  /* type 17 remove me from your sub*/
constexpr uint16_t main_type_sub_add_resp = 0x0012;  /* type 18 status of add, 0=ok */
constexpr uint16_t main_type_sub_drop_resp = 0x0013; /* type 19 status of drop, 0=ok */
constexpr uint16_t main_type_sub_list_info = 0x0014; /* type 20 info for subs.lst file */

constexpr uint16_t main_type_new_post = 0x001a;     /* type 26 post by sub name */
constexpr uint16_t main_type_new_external = 0x001b; /* type 27 auto-proc ext. msgs */
constexpr uint16_t main_type_game_pack = 0x001c;    /* type 28 game packs */

// Minor types used by main_type_net_info

constexpr uint16_t net_info_general_message = 0x0000;
constexpr uint16_t net_info_bbslist = 0x0001;
constexpr uint16_t net_info_connect = 0x0002;
constexpr uint16_t net_info_sub_lst = 0x0003;
constexpr uint16_t net_info_wwivnews = 0x0004;
constexpr uint16_t net_info_fbackhdr = 0x0005;
constexpr uint16_t net_info_more_wwivnews = 0x0006;
constexpr uint16_t net_info_categ_net = 0x0007;
constexpr uint16_t net_info_network_lst = 0x0008;
constexpr uint16_t net_info_file = 0x0009;
constexpr uint16_t net_info_binkp = 0x0010;

/* these are in main_type_sub_*_resp, as the first byte of the text */
constexpr uint16_t sub_adddrop_ok = 0x00;            /* you've been added/removed */
constexpr uint16_t sub_adddrop_not_host = 0x01;      /* don't host that sub */
constexpr uint16_t sub_adddrop_not_there = 0x02;     /* can't drop, you're not listed */
constexpr uint16_t sub_adddrop_not_allowed = 0x03;   /* not allowed to add/drop you */
constexpr uint16_t sub_adddrop_already_there = 0x04; /* already in sub */
constexpr uint16_t sub_adddrop_error = 0xff;         /* internal error */

struct net_contact_rec {
  uint16_t systemnumber,   /* System number of the contact */
      numcontacts,         /* # of contacts with system */
      numfails;            /* # of consec failed calls out */
  uint32_t firstcontact,   /* time of first contact w/ system */
      lastcontact,         /* time of most recent contact */
      lastcontactsent,     /* time of last contact w/data sent */
      lasttry;             /* time of last try to connect */
  uint32_t bytes_received, /* bytes received from system */
      bytes_sent,          /* bytes sent to system */
      bytes_waiting;       /* bytes waiting to be sent */
};

/* Each system will hold a file of these records.  Each record will hold the
 * data pertaining to all contacts with other systems.
 *
 * systemnumber tells what other system this data is for.
 * numcontacts - how many times a connection has been made with that system
 * numfails tells how many times this system has tried to call this other
 *   system but failed.  This is consecutive times, and is zeroed out
 *   when a successful connection is made by calling out.
 * firstcontact is time in unix format of the last time that system has been
 *   contacted on the net.
 * lastcontact is the time the last contact with that system was made.
 * lastcontactsent is the last time data was SENT to that system.
 * lasttry is the last time we tried to call out to that system.
 * bytes_received is number of bytes received from that system.
 * bytes_sent is number of bytes sent to that system.
 * bytes_waiting is the number of bytes waiting to be sent out to that
 *   system.
 *
 * This data is maintained so that the BBS can decide which other system to
 * attempt connection with next.
 */
struct net_system_list_rec {
  // system number of the system
  uint16_t sysnum;
  /* phone number of system */
  char phone[13];
  /* name of system */
  char name[40];
  /* group of the system */
  uint8_t group;
  /* max baud rate of system */
  uint16_t speed;
  /* other info about sys (bit-mapped) */
  uint16_t other;
  /* how to get there */
  uint16_t forsys;
  /* how long to get there */
  int16_t numhops;
  union {
    /* routing factor */
    uint16_t rout_fact;
    /* cost factor */
    float cost;
    /* temporary variable */
    int32_t temp;
  } xx;
};

/**
 * Contains the metadata for each network.
 *
 * On disk format for networks.dat
 */
struct net_networks_rec_disk {
  /* type of network */
  uint8_t type;
  /* network name */
  char name[16];
  /* directory for net data */
  char dir[69];
  /* system number */
  uint16_t sysnum;
  uint8_t padding[12];
};

#pragma pack(pop)

// This data is all read in from a text file which holds info about all of
// the systems in the network.  This text file doesn't hold connection info
// between the systems.  The purpose of all records should be obvious.

// BBSLIST designators

constexpr uint16_t  other_inet = 0x0001;         /* $ - System is PPP capable         */
constexpr uint16_t  other_fido = 0x0002;         /* \ - System run Fido frontend      */
constexpr uint16_t  other_telnet = 0x0004;       /* | - System is a telnet node       */
constexpr uint16_t  other_no_links = 0x0008;     /* < - System refuses links          */
constexpr uint16_t  other_fts_blt = 0x0010;      /* > - System uses FTS/BLT system    */
constexpr uint16_t  other_direct = 0x0020;       /* ! - System accepts direct connects*/
constexpr uint16_t  other_unregistered = 0x0040; /* / - System is unregistered        */
constexpr uint16_t  other_fax = 0x0080;          /* ? - System accepts faxes          */
constexpr uint16_t  other_end_system = 0x0100;   /* _ - System is a dead end node     */
constexpr uint16_t  other_net_server = 0x0200;   /* + - System is network server      */
constexpr uint16_t  other_unused = 0x0400;       /* = - Unused identifier 2           */

/* system type designators */
constexpr uint16_t  other_net_coord = 0x0800;   /* & - NC */
constexpr uint16_t  other_group_coord = 0x1000; /* % - GC */
constexpr uint16_t  other_area_coord = 0x2000;  /* ^ - AC */
constexpr uint16_t  other_subs_coord = 0x4000;  /* ~ - Sub Coordinator */

/* This record holds info about other systems that the sysop has determined
 * that we can call out to.
 *
 * sysnum - system number that we can call
 * macnum - macro/script number to use to call that system.  This is set
 *   to zero if we just dial it directly.
 * options - bit mapped.  See constants below.
 * call_every_x_minutes - if non-zero, then the minimum number of days between calls
 *   to that system, even if there is nothing waiting to be sent there.
 * password - is the password used for connection to this system.
 */
constexpr uint16_t unused_options_sendback = 0x0001;     /* & they can send data back */
constexpr uint16_t unused_options_ATT_night = 0x0002;    /* - callout only at AT&T nigh hours */
constexpr uint16_t unused_options_ppp = 0x0004;          /* _ transfer via PPP */
constexpr uint16_t options_no_call = 0x0008;             /* + don't call that system, it will */
constexpr uint16_t unused_options_receive_only = 0x0010; /* ~ never send anything */
constexpr uint16_t unused_options_once_per_day = 0x0020; /* ! only call once per day */
constexpr uint16_t unused_options_compress = 0x0040;     /* ; compress data */
constexpr uint16_t unused_options_hslink = 0x0080;       /* ^ use HSLINK if available */
constexpr uint16_t unused_options_force_ac = 0x0100;     /* $ force area code on dial */
constexpr uint16_t unused_options_dial_ten = 0x0200;     /* * use ten digit dialing format */
constexpr uint16_t options_hide_pend = 0x0400;           /* = hide in pending display */



#endif
