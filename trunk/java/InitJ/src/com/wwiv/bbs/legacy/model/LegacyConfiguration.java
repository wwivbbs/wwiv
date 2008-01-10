package com.wwiv.bbs.legacy.model;

public class LegacyConfiguration {
    public String newuserpw;
    public String systempw;
    public String msgsdir;
    public String gfilesdir;
    public String datadir;
    public String dloadsdir;
    public char ramdrive;
    public String tempdir;
    public char xmark;
    public String regcode;
    public String bbs_init_modem;
    public String answer; // modem answer cmd
    public String connect_300; // modem responses for
    public String connect_1200; // connections made at
    public String connect_2400; // various speeds
    public String connect_9600; // ""
    public String connect_19200; // ""
    public String no_carrier; // modem disconnect
    public String ring; // modem ring
    public String terminal; // DOS cmd for run term prg
    public String systemname; // BBS system name
    public String systemphone; // BBS system phone number
    public String sysopname; // sysop's name
    public String executestr;
    public int newusersl; // new user SL
    public int newuserdsl; // new user DSL
    public int maxwaiting; // max mail waiting
    public String comport; // what connected to comm
    public String com_ISR; // Com Interrupts
    public int primaryport; // primary comm port
    public int newuploads; // file dir new uploads go
    public int closedsystem;
    public short systemnumber;
    public short baudrate[] = new short[5];
    public short com_base[] = new short[5];
    public short maxusers;
    public short newuser_restrict;
    public short sysconfig;
    public short sysoplowtime;
    public short sysophightime;
    public short executetime;
    public float req_ratio; // required up/down ratio
    public float newusergold; // new user gold
    public Slrec[] sl = new Slrec[256];
    public Valrec[] valrec = new Valrec[10];

    public String hangupphone;
    public String pickupphone;
    public int netlowtime;
    public int nethightime;
    public String connect_300_a;
    public String connect_1200_a;
    public String connect_2400_a;
    public String connect_960_a;
    public String connect_19200_a;
    public Oldarcrec[] oldarcrec = new Oldarcrec[4];
    public String beginday_c;
    public String logon_c;
    public int userreclen;
    public int waitingoffset;
    public int inactoffset;
    public String newuser_c;
    public int wwiv_reg_number;
    
    public String dial_prefix;
    public float post_call_ratio;
    public String upload_c;
    public String dszbatchdl;
    public String modem_type;
    public String batchdir;
    public short sysstatusoffset;
    public byte network_type;
    public short fuoffset;
    public short fsoffset;
    public short fnoffset;
    public int max_subs;
    public int max_dirs;
    public int qscn_len;
    public int email_storage_type;
    public long sysconfig1;
    public long rrd;
    public String menudir;
    public String logoff_c;
    public String v_scan_c;
    public byte[] res = new byte[400];

    public LegacyConfiguration() {
        for (int i = 0; i < sl.length; i++) {
            sl[i] = new Slrec();
        }
        for (int i = 0; i < valrec.length; i++) {
            valrec[i] = new Valrec();
        }
        for (int i = 0; i < oldarcrec.length; i++) {
            oldarcrec[i] = new Oldarcrec();
        }
    }

    public class Slrec {
        public int time_per_day;
        public int time_per_logon;
        public int messages_read;
        public int emails;
        public int posts;
        public int ability;
    }

    public class Valrec {
        public int sl;
        public int dsl;
        public int ar;
        public int dar;
        public int restrict;
    }

    public class Oldarcrec {
        public String extension;
        public String arca;
        public String arce;
        public String arcl;
    }

}
