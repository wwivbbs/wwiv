package com.wwiv.bbs.model;

import com.wwiv.bbs.legacy.model.LegacyConfiguration;

public class SystemConfiguration {
    private String newUserPassword;
    private String systemPassword;
    private String messageDirectory;
    private String gFilesDirectory;
    private String dataDirectory;
    private String downloadDirectory;
    private String tempDirectory;
    private String registrationCode;
    private String systemName; // BBS system name
    private String systemPhone; // BBS system phone number
    private String sysopName; // sysop's name
    private int newUserSL; // new user SL
    private int newUserDSL; // new user DSL
    private int maxMailWaiting; // max mail waiting
    private int primaryPort; // primary comm port
    private int newUploads; // file dir new uploads go
    private int closedSystem;
    private short systemNumber;
    private short newuserRestrict;
    private int maxUsers;
    // These booleans come from sysconfif
    private boolean noLocal;
    private boolean noBeep;
    private boolean enablePipes;
    private boolean offHook;
    private boolean twoColor;
    private boolean enableMci;
    private boolean titleBar;
    private boolean useList;
    private boolean noXfer;
    private boolean twoWay;
    private boolean noAlias;
    private boolean allSysop;
    private boolean freePhone;
    private boolean logDownloads;
    private boolean extendedInfo;
    private boolean highSpeed;
    private boolean flowControl;

    private int sysoplowtime;
    private int sysophightime;
    private int executetime;
    private float req_ratio; // required up/down ratio
    private float newusergold; // new user gold

    private SecurityLevel[] sl = new SecurityLevel[256];
    private ValidationConfiguration[] valrec = new ValidationConfiguration[10];

    private int netlowtime;
    private int nethightime;
    private int userreclen;
    private int waitingoffset;
    private int inactoffset;
    private int wwiv_reg_number;
    private float post_call_ratio;
    private String dszbatchdl;
    private String modem_type;
    private String batchdir;
    private short sysstatusoffset;
    private byte network_type;
    private short fuoffset;
    private short fsoffset;
    private short fnoffset;
    private int max_subs;
    private int max_dirs;
    private int qscn_len;
    private int email_storage_type;
    private long sysconfig1;
    private long rrd;
    private String menudir;


    public SystemConfiguration() {
        for (int i = 0; i < sl.length; i++) {
            sl[i] = new SecurityLevel();
        }
        for (int i = 0; i < valrec.length; i++) {
            valrec[i] = new ValidationConfiguration();
        }
    }

    public void setNewUserPassword(String param) {
        this.newUserPassword = param;
    }

    public String getNewUserPassword() {
        return newUserPassword;
    }

    public void setSystemPassword(String param) {
        this.systemPassword = param;
    }

    public String getSystemPassword() {
        return systemPassword;
    }

    public void setMessageDirectory(String param) {
        this.messageDirectory = param;
    }

    public String getMessageDirectory() {
        return messageDirectory;
    }

    public void setGFilesDirectory(String param) {
        this.gFilesDirectory = param;
    }

    public String getGFilesDirectory() {
        return gFilesDirectory;
    }

    public void setDataDirectory(String param) {
        this.dataDirectory = param;
    }

    public String getDataDirectory() {
        return dataDirectory;
    }

    public void setDownloadDirectory(String param) {
        this.downloadDirectory = param;
    }

    public String getDownloadDirectory() {
        return downloadDirectory;
    }

    public void setTempDirectory(String param) {
        this.tempDirectory = param;
    }

    public String getTempDirectory() {
        return tempDirectory;
    }

    public void setRegistrationCode(String param) {
        this.registrationCode = param;
    }

    public String getRegistrationCode() {
        return registrationCode;
    }

    public void setSystemName(String param) {
        this.systemName = param;
    }

    public String getSystemName() {
        return systemName;
    }

    public void setSystemPhone(String param) {
        this.systemPhone = param;
    }

    public String getSystemPhone() {
        return systemPhone;
    }

    public void setSysopName(String param) {
        this.sysopName = param;
    }

    public String getSysopName() {
        return sysopName;
    }

    public void setNewUserSL(int param) {
        this.newUserSL = param;
    }

    public int getNewUserSL() {
        return newUserSL;
    }

    public void setNewUserDSL(int param) {
        this.newUserDSL = param;
    }

    public int getNewUserDSL() {
        return newUserDSL;
    }

    public void setMaxMailWaiting(int param) {
        this.maxMailWaiting = param;
    }

    public int getMaxMailWaiting() {
        return maxMailWaiting;
    }

    public void setPrimaryPort(int param) {
        this.primaryPort = param;
    }

    public int getPrimaryPort() {
        return primaryPort;
    }

    public void setNewUploads(int param) {
        this.newUploads = param;
    }

    public int getNewUploads() {
        return newUploads;
    }

    public void setClosedSystem(int param) {
        this.closedSystem = param;
    }

    public int getClosedSystem() {
        return closedSystem;
    }

    public void setSystemNumber(short param) {
        this.systemNumber = param;
    }

    public short getSystemNumber() {
        return systemNumber;
    }

    public void setSl(SystemConfiguration.SecurityLevel[] param) {
        this.sl = param;
    }

    public SystemConfiguration.SecurityLevel[] getSl() {
        return sl;
    }

    public void setValrec(SystemConfiguration.ValidationConfiguration[] param) {
        this.valrec = param;
    }

    public SystemConfiguration.ValidationConfiguration[] getValrec() {
        return valrec;
    }

    public void setNewuserRestrict(short r) {
        this.newuserRestrict = r;
    }

    public short getNewuserRestrict() {
        return newuserRestrict;
    }

    public void setMaxUsers(int param) {
        this.maxUsers = param;
    }

    public int getMaxUsers() {
        return maxUsers;
    }

    public void setNoLocal(boolean param) {
        this.noLocal = param;
    }

    public boolean getNoLocal() {
        return noLocal;
    }

    public void setNoBeep(boolean param) {
        this.noBeep = param;
    }

    public boolean getNoBeep() {
        return noBeep;
    }

    public void setEnablePipes(boolean param) {
        this.enablePipes = param;
    }

    public boolean getEnablePipes() {
        return enablePipes;
    }

    public void setOffHook(boolean param) {
        this.offHook = param;
    }

    public boolean getOffHook() {
        return offHook;
    }

    public void setTwoColor(boolean param) {
        this.twoColor = param;
    }

    public boolean getTwoColor() {
        return twoColor;
    }

    public void setEnableMci(boolean param) {
        this.enableMci = param;
    }

    public boolean getEnableMci() {
        return enableMci;
    }

    public void setTitleBar(boolean param) {
        this.titleBar = param;
    }

    public boolean getTitleBar() {
        return titleBar;
    }

    public void setNoXfer(boolean param) {
        this.noXfer = param;
    }

    public boolean getNoXfer() {
        return noXfer;
    }

    public void setTwoWay(boolean param) {
        this.twoWay = param;
    }

    public boolean getTwoWay() {
        return twoWay;
    }

    public void setNoAlias(boolean param) {
        this.noAlias = param;
    }

    public boolean getNoAlias() {
        return noAlias;
    }

    public void setAllSysop(boolean param) {
        this.allSysop = param;
    }

    public boolean getAllSysop() {
        return allSysop;
    }

    public void setFreePhone(boolean param) {
        this.freePhone = param;
    }

    public boolean getFreePhone() {
        return freePhone;
    }

    public void setExtendedInfo(boolean param) {
        this.extendedInfo = param;
    }

    public boolean getExtendedInfo() {
        return extendedInfo;
    }

    public void setHighSpeed(boolean param) {
        this.highSpeed = param;
    }

    public boolean getHighSpeed() {
        return highSpeed;
    }

    public void setFlowControl(boolean param) {
        this.flowControl = param;
    }

    public boolean getFlowControl() {
        return flowControl;
    }

    public void setUseList(boolean param) {
        this.useList = param;
    }

    public boolean getUseList() {
        return useList;
    }

    public void setLogDownloads(boolean param) {
        this.logDownloads = param;
    }

    public boolean getLogDownloads() {
        return logDownloads;
    }

    public class SecurityLevel {
        public int time_per_day;
        public int time_per_logon;
        public int messages_read;
        public int emails;
        public int posts;
        public int ability;
    }

    public class ValidationConfiguration {
        public int sl;
        public int dsl;
        public int ar;
        public int dar;
        public int restrict;
    }

}
