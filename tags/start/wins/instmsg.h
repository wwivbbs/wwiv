/****************************************************************************/
/*                                                                          */
/*                             WWIV Version 5.xx                            */
/*             Copyright (C) 1998-2001 by WWIV Software Services            */
/*                                                                          */
/*      Distribution or publication of this source code, it's individual    */
/*       components, or a compiled version thereof, whether modified or     */
/*        unmodified, without PRIOR, WRITTEN APPROVAL of WWIV Software      */
/*        Services is expressly prohibited.  Distribution of compiled       */
/*            versions of WWIV is restricted to copies compiled by          */
/*           WWIV Software Services.  Violators will be procecuted!         */
/*                                                                          */
/****************************************************************************/
#ifndef _INSTMSG_H_
#define _INSTMSG_H_

/****************************************************************************/

#define INST_MSG_STRING      1  /* A string to print out to the user */
#define INST_MSG_SHUTDOWN    2  /* Hangs up, ends BBS execution */
#define INST_MSG_SYSMSG      3  /* Message from the system, not a user */
#define INST_MSG_CLEANNET    4  /* should call cleanup_net */
#define INST_MSG_CHAT        6  /* External chat request */

/****************************************************************************/

/*
 * Structure for inter-instance messages. File would be comprised of headers
 * using the following structure, followed by the "message" (if any).
 */

typedef struct {
  unsigned short
    main,                    /* Message main type */
    minor,                   /* Message minor type */
    from_inst,               /* Originating instance */
    from_user,               /* Originating usernum */
    dest_inst;               /* Destination instance */
  unsigned long
    daten,                   /* Secs-since-1970 Unix datetime */
    msg_size,                /* Length of the "message" */
    flags;                   /* Bit-mapped flags */
} inst_msg_header;

#endif

