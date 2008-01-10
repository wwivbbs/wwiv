package com.wwiv.bbs.legacy.datafile;

import com.wwiv.bbs.legacy.model.LegacyConfiguration;

import java.io.File;
import java.io.IOException;

public class Config extends LegacyConfigFile {

    public Config(File file) {
        super(file);
    }

    public void saveConfiguration(LegacyConfiguration configuration) throws IOException {

    }

    public LegacyConfiguration loadConfiguration() throws IOException {
        final LegacyConfiguration c = new LegacyConfiguration();
        try {
            DataFileInput input = new DataFileInput(getFile());
            c.newuserpw = input.readCString(21);
            c.systempw = input.readCString(21);
            c.msgsdir = input.readCString(81);
            c.gfilesdir = input.readCString(81);
            c.datadir = input.readCString(81);
            c.dloadsdir = input.readCString(81);
            c.ramdrive = (char)input.readUnsignedByte();
            c.tempdir = input.readCString(81);
            c.xmark = (char)input.readUnsignedByte();
            c.regcode = input.readCString(83);
            c.bbs_init_modem = input.readCString(51);
            c.answer = input.readCString(21);
            c.connect_300 = input.readCString(21);
            c.connect_1200 = input.readCString(21);
            c.connect_2400 = input.readCString(21);
            c.connect_9600 = input.readCString(21);
            c.connect_19200 = input.readCString(21);
            c.no_carrier = input.readCString(21);
            c.ring = input.readCString(21);
            c.terminal = input.readCString(21);
            c.systemname = input.readCString(51);
            c.systemphone = input.readCString(13);
            c.sysopname = input.readCString(51);
            c.executestr = input.readCString(51);
            c.newusersl = input.readUnsignedByte();
            c.newuserdsl = input.readUnsignedByte();
            c.maxwaiting = input.readUnsignedByte();
            c.comport = input.readCString(5);
            c.com_ISR = input.readCString(5);
            c.primaryport = input.readUnsignedByte();
            c.newuploads = input.readUnsignedByte();
            c.closedsystem = input.readUnsignedByte();
            c.systemnumber = input.readShort();
            for (int i = 0; i < 5; i++) {
                c.baudrate[i] = input.readShort();
            }
            for (int i = 0; i < 5; i++) {
                c.com_base[i] = input.readShort();
            }
            c.maxusers = input.readShort();
            c.newuser_restrict = input.readShort();
            c.sysconfig = input.readShort();
            c.sysoplowtime = input.readShort();
            c.sysophightime = input.readShort();
            c.executetime = input.readShort();
            c.req_ratio = input.readFloat();
            c.newusergold = input.readFloat();
            for (int i = 0; i < c.sl.length; i++) {
                c.sl[i].time_per_day = input.readUnsignedShort();
                c.sl[i].time_per_logon = input.readUnsignedShort();
                c.sl[i].messages_read = input.readUnsignedShort();
                c.sl[i].emails = input.readUnsignedShort();
                c.sl[i].posts = input.readUnsignedShort();
                c.sl[i].ability = input.readInt();
            }
            for (int i = 0; i < c.valrec.length; i++) {
                c.valrec[i].sl = input.readUnsignedByte();
                c.valrec[i].dsl = input.readUnsignedByte();
                c.valrec[i].ar = input.readUnsignedShort();
                c.valrec[i].dar = input.readUnsignedShort();
                c.valrec[i].restrict = input.readUnsignedShort();
            }
            c.hangupphone = input.readCString(21);
            c.pickupphone = input.readCString(21);
            c.netlowtime = input.readUnsignedShort();
            c.nethightime = input.readUnsignedShort();
            c.connect_300_a = input.readCString(21);
            c.connect_1200_a = input.readCString(21);
            c.connect_2400_a = input.readCString(21);
            c.connect_960_a = input.readCString(21);
            c.connect_19200_a = input.readCString(21);
            for (int i = 0; i < c.oldarcrec.length; i++) {
                c.oldarcrec[i].extension = input.readCString(4);
                c.oldarcrec[i].arca = input.readCString(32);
                c.oldarcrec[i].arce = input.readCString(32);
                c.oldarcrec[i].arcl = input.readCString(32);
            }
            c.beginday_c = input.readCString(51);
            c.logon_c = input.readCString(51);
            c.userreclen = input.readShort();
            c.waitingoffset = input.readShort();
            c.inactoffset = input.readShort();
            c.newuser_c = input.readCString(51);
            c.wwiv_reg_number = input.readInt();
            c.dial_prefix = input.readCString(21);
            c.post_call_ratio = input.readFloat();
            c.upload_c = input.readCString(51);
            c.dszbatchdl = input.readCString(81);
            c.modem_type = input.readCString(9);
            c.batchdir = input.readCString(81);
            c.sysstatusoffset = input.readShort();
            c.network_type = input.readByte();
            c.fuoffset = input.readShort();
            c.fsoffset = input.readShort();
            c.fnoffset = input.readShort();
            c.max_subs = input.readUnsignedShort();
            c.max_dirs = input.readUnsignedShort();
            c.qscn_len = input.readUnsignedShort();
            c.email_storage_type = input.readUnsignedByte();
            c.sysconfig1 = input.readInt();
            c.rrd = input.readInt();
            c.menudir = input.readCString(81);
            c.logoff_c = input.readCString(51);
            c.v_scan_c = input.readCString(51);
            input.readFully(c.res);
            return c;
        } catch (IOException ex) {
            ex.printStackTrace();
        }
        return null;
    }

    public static Config createConfigFile(File file, 
                                          LegacyConfiguration configuration) throws IOException {
        if (file.exists()) {
            throw new IllegalStateException(file + 
                                            " already exists, can not create a configuration file there.");
        }
        Config config = new Config(file);
        config.saveConfiguration(configuration);
        return config;
    }

    public static void main(String[] args) {
        try {
            Config config = new Config(new File("D:\\bbs\\config.dat"));
            LegacyConfiguration c = config.loadConfiguration();
            System.out.println(c);
        } catch (Exception ex) {
            ex.printStackTrace();
        }
    }

}
