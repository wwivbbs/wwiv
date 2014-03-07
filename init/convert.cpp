/**************************************************************************/
/*                                                                        */
/*                 WWIV Initialization Utility Version 5.0                */
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
#include "wwivinit.h"


/****************************************************************************/

#define OFFOF(x) (short)(((long)(&(u.x))) - ((long)&u))
#define C_STR(a) strncpy((char*) _new.a, (char*) old.a,sizeof(_new.a)-1)
#define C_NUM(a) _new.a=old.a


/****************************************************************************/



/* DATA FOR EVERY USER */
typedef struct 
{
	char		
            name[31],		/* user's name */
			realname[21],		/* user's real name */
			callsign[7],		/* user's amateur callsign */
			phone[13],		/* user's phone number */
			pw[9],			/* user's password */
			laston[9],		/* last date on */
			firston[9],		/* first date on */
			note[41],		/* sysop's note about user */
			macros[3][81],		/* macro keys */
			sex;			/* user's sex */
	unsigned char	age,			/* user's age */
			inact,			/* if deleted or inactive */
			comp_type,		/* computer type */
			defprot,		/* deflt transfer protocol */
			defed,			/* default editor */
			screenchars,screenlines,/* screen size */
			sl,			/* security level */
			dsl,			/* transfer security level */
			exempt,			/* exempt from ratios, etc */
			colors[8],		/* user's colors */
			votes[20],		/* user's votes */
			illegal,		/* illegal logons */
			waiting,		/* number mail waiting */
			sysopsub,		/* sysop sub board number */
			ontoday;		/* num times on today */
	unsigned short	homeuser,homesys,	/* where user can be found */
			forwardusr,forwardsys,	/* where to forward mail */
			msgpost,		/* number messages posted */
			emailsent,		/* number of email sent */
            feedbacksent,   /* number of f-back sent */
			posttoday,		/* number posts today */
			etoday,			/* number emails today */
            ar,             /* board access */
			dar,			/* directory access */
			restrict,		/* restrictions on account */
			ass_pts,		/* bad things the user did */
			uploaded,		/* number files uploaded */
			downloaded,		/* number files downloaded */
			lastrate,		/* last baud rate on */
			logons;			/* total number of logons */
    unsigned long msgread,  /* total num msgs read */
            uk,             /* number of k uploaded */
            dk,             /* number of k downloaded */
			qscn,			/* which subs to n-scan */
            qscnptr[33],    /* q-scan pointers */
            nscn1,nscn2,    /* which dirs to n-scan */
			daten,			/* numerical time last on */
			sysstatus;		/* status/defaults */
    float       timeontoday,/* time on today */
			extratime,		/* time left today */
			timeon,			/* total time on system */
            pos_account,    /* $ credit */
            neg_account,    /* $ debit */
			gold;			/* game money */
	unsigned char	bwcolors[8];		/* b&w colors */
	unsigned char	month,day,year;		/* user's birthday */
    unsigned int    emailnet,           /* email sent into net */
			postnet;		/* posts sent into net */
	unsigned short	fsenttoday1;		/* feedbacks today */
        unsigned char   num_extended;   /* num lines of ext desc */
        unsigned char   optional_val;   /* optional lines in msgs */
        unsigned long   wwiv_regnum;    /* users WWIV reg number */
        unsigned char   net_num;        /* net_num for forwarding */
        char            res[28];        /* reserved bytes */
        unsigned long   qscn2;          /* additional qscan ptr */
        unsigned long   qscnptr2[32];   /* additional quickscan ptrs */
} olduserrec;

/****************************************************************************/

static int qfl;
static unsigned long *qsc, *qsc_n, *qsc_q, *qsc_p;

#ifdef INIT
extern configrec syscfg;
#else
static configrec syscfg;
#endif


/****************************************************************************/

void c_setup()
{
#ifndef INIT
  int hFile = open("CONFIG.DAT",O_RDONLY|O_BINARY);
  if ( hFile > 0 ) 
  {
    read( hFile, &syscfg, sizeof( syscfg ) );
    close( hFile);

#endif
#ifndef INIT
  } else {
    Printf("CONFIG.DAT not found; run in main BBS dir.\n");
  }
#endif
  if (!syscfg.max_subs)
  {
    syscfg.max_subs=64;
  }
  if (!syscfg.max_dirs)
  {
    syscfg.max_dirs=64;
  }
  if (!syscfg.menudir) 
  {
      sprintf( syscfg.menudir, "%sMENUS\\", syscfg.gfilesdir );
  }

  if (!syscfg.userreclen)
  {
    syscfg.userreclen=sizeof(olduserrec);
  }

  qfl=4*(1+syscfg.max_subs+((syscfg.max_subs+31)/32)+((syscfg.max_dirs+31)/32));
  syscfg.qscn_len=qfl;

  qsc=(unsigned long *)bbsmalloc(qfl);

  qsc_n=qsc+1;
  qsc_q=qsc_n+((syscfg.max_dirs+31)/32);
  qsc_p=qsc_q+((syscfg.max_subs+31)/32);
}

/****************************************************************************/

void c_old_to_new()
{
    int i1;
    olduserrec old;
    userrec _new, u;
    char szUserListFileName[ MAX_PATH ];
    char szNewUserListFileName[ MAX_PATH ];
    char szQScanFileName[ MAX_PATH ];
    char szOldUserListFileName[ MAX_PATH ];

    sprintf( szUserListFileName, "%sUSER.LST", syscfg.datadir );
    sprintf( szNewUserListFileName, "%sUSER.NEW", syscfg.datadir );
    sprintf( szQScanFileName, "%sUSER.QSC", syscfg.datadir );
    sprintf( szOldUserListFileName, "%sUSER.OLD", syscfg.datadir );


    int hOldUserFile = _open(szUserListFileName,O_RDONLY|O_BINARY);
    if ( hOldUserFile < 0 ) 
    {
        Printf("Couldn't open '%s'\n",szUserListFileName);
        return;
    }
    int hNewUserFile = _open(szNewUserListFileName,O_RDWR|O_BINARY|O_CREAT|O_TRUNC, S_IREAD|S_IWRITE);
    if ( hNewUserFile < 0 ) 
    {
        _close( hOldUserFile );
        Printf("Couldn't create '%s'\n",szNewUserListFileName);
        return;
    }
    int hQScanFile = _open(szQScanFileName,O_RDWR|O_BINARY|O_CREAT|O_TRUNC, S_IREAD|S_IWRITE);
    if ( hQScanFile < 0 ) 
    {
        _close( hOldUserFile );
        _close(hNewUserFile);
        Printf("Couldn't create '%s'\n",szQScanFileName);
        return;
    }

    int nu = filelength( hOldUserFile ) / syscfg.userreclen;

    for (int i = 0; i < nu; i++ ) 
    {
        if ( i % 10 == 0 )
        {
            Printf( "%u/%u\r", i , nu );
        }

        memset(&old,0,sizeof(old));
        memset(&_new,0,sizeof(_new));
        memset(qsc,0,qfl);

        if (i) 
        {
            _lseek(hOldUserFile,((long)i)*((long)syscfg.userreclen), SEEK_SET);
            _read(hOldUserFile,&old,syscfg.userreclen);

            C_STR(name);
            C_STR(realname);
            C_STR(callsign);
            C_STR(phone);
            C_STR(pw);
            C_STR(laston);
            C_STR(firston);
            C_STR(note);
            for (i1=0; i1<3; i1++)
            {
                C_STR(macros[i1]);
            }
            C_NUM(sex);
            C_NUM(age);
            C_NUM(inact);
            C_NUM(comp_type);
            C_NUM(defprot);
            C_NUM(defed);
            C_NUM(screenchars);
            C_NUM(screenlines);
            C_NUM(sl);
            C_NUM(dsl);
            C_NUM(exempt);
            for (i1=0; i1<8; i1++) 
            {
                C_NUM(colors[i1]);
                C_NUM(bwcolors[i1]);
            }
            _new.colors[8]=1;
            _new.colors[9]=3;
            _new.bwcolors[8]=7;
            _new.bwcolors[9]=7;
            for (i1=0; i1<20; i1++)
            {
                C_NUM(votes[i1]);
            }
            C_NUM(illegal);
            C_NUM(waiting);
            C_NUM(ontoday);
            C_NUM(homeuser);
            C_NUM(homesys);
            C_NUM(forwardusr);
            C_NUM(forwardsys);
            C_NUM(msgpost);
            C_NUM(emailsent);
            C_NUM(feedbacksent);
            C_NUM(posttoday);
            C_NUM(etoday);
            C_NUM(ar);
            C_NUM(dar);
            C_NUM(restrict);
            C_NUM(ass_pts);
            C_NUM(uploaded);
            C_NUM(downloaded);
            C_NUM(lastrate);
            C_NUM(logons);
            C_NUM(msgread);
            C_NUM(uk);
            C_NUM(dk);
            C_NUM(daten);
            C_NUM(sysstatus);
            C_NUM(timeontoday);
            C_NUM(extratime);
            C_NUM(timeon);
            C_NUM(pos_account);
            C_NUM(neg_account);
            C_NUM(gold);
            C_NUM(month);
            C_NUM(day);
            C_NUM(year);
            C_NUM(emailnet);
            C_NUM(postnet);
            C_NUM(fsenttoday1);
            C_NUM(num_extended);
            C_NUM(optional_val);
            C_NUM(wwiv_regnum);
            C_NUM(net_num);

            *qsc=old.sysopsub;
            if (*qsc==255)
            {
                *qsc=999;
            }
            qsc_n[0]=old.nscn1;
            qsc_n[1]=old.nscn2;
            qsc_q[0]=old.qscn;
            qsc_q[1]=old.qscn2;

            for (i1=0; i1<32; i1++) 
            {
                qsc_p[i1]=old.qscnptr[i1];
                qsc_p[i1+32]=old.qscnptr2[i1];
            }
        }

        _lseek(hNewUserFile,((long)i)*((long)sizeof(_new)), SEEK_SET);
        _write(hNewUserFile,&_new,sizeof(_new));

        _lseek(hQScanFile,((long)i)*((long)qfl),SEEK_SET);
        _write( hQScanFile, qsc, qfl );

    }
    _close(hOldUserFile);
    _close(hNewUserFile);
    _close(hQScanFile);

    unlink(szOldUserListFileName);
    rename(szUserListFileName,szOldUserListFileName);
    rename(szNewUserListFileName,szUserListFileName);

    syscfg.userreclen=sizeof(u);
    syscfg.waitingoffset=OFFOF(waiting);
    syscfg.inactoffset=OFFOF(inact);
    syscfg.sysstatusoffset=OFFOF(sysstatus);
    syscfg.fuoffset=OFFOF(forwardusr);
    syscfg.fsoffset=OFFOF(forwardsys);
    syscfg.fnoffset=OFFOF(net_num);

    int hFile = _open( "CONFIG.DAT", O_RDWR | O_BINARY | O_CREAT, S_IREAD | S_IWRITE );
    if ( hFile > 0 ) 
    {
        _write( hFile, &syscfg, sizeof( syscfg ) );
        _close( hFile );
    }
    Printf( "\nDone.\n" );
}


/****************************************************************************/

bool c_IsUserListInOldFormat()
{
    char szQScanFileName[ MAX_PATH ];
    sprintf( szQScanFileName, "%sUSER.QSC", syscfg.datadir );
    if ( access( szQScanFileName, 0 ) == 0 )
    {
        // new format 
        // return 1;
        return false;
    }
    else
    {
        // old format
        // return 0;
        return true;
    }
}

/****************************************************************************/

int c_check_old_struct()
{
    olduserrec u;

    if ((syscfg.userreclen) && (syscfg.userreclen!=sizeof(u)) && (syscfg.userreclen!=700))
    {
        return 1;
    }
    if ((syscfg.waitingoffset) && (syscfg.waitingoffset != OFFOF(waiting)))
    {
        return 1;
    }
    if ((syscfg.inactoffset) && (syscfg.inactoffset != OFFOF(inact)))
    {
        return 1;
    }
    if ((syscfg.sysstatusoffset) && (syscfg.sysstatusoffset != OFFOF(sysstatus)))
    {
        return 1;
    }
    if ((syscfg.fuoffset) && (syscfg.fuoffset != OFFOF(forwardusr)))
    {
        return 1;
    }
    if ((syscfg.fsoffset) && (syscfg.fsoffset != OFFOF(forwardsys)))
    {
        return 1;
    }
    if ((syscfg.fnoffset) && (syscfg.fnoffset != OFFOF(net_num)))
    {
        return 1;
    }
    return 0;
}

/****************************************************************************/

#ifndef INIT
void main()
{
    c_setup();

    if ( !c_IsUserListInOldFormat() ) 
    {
        // convert back to old format
        Printf( "Your userrec is already in the new format.\n" );
    } 
    else 
    {
        // convert to new format
        if ( c_check_old_struct( )) 
        {
            Printf( "Your current userrec format is unknown.\n" );
            Printf( "Please copy your old userrec format into convert.cpp,\n" );
            Printf( "and compile and run it.\n" );
        } 
        else 
        {
            c_old_to_new();
            Printf( "Userlist converted.\n" );
        }
    }
}
#endif
