package com.wwiv.bbs.legacy.adapters;

import com.wwiv.bbs.legacy.model.LegacyConfiguration;
import com.wwiv.bbs.model.SystemConfiguration;

import java.util.logging.Level;
import java.util.logging.Logger;

public class ConfigurationAdapter implements LegacyAdapter<LegacyConfiguration, SystemConfiguration> {
    final Logger logger = Logger.getLogger(ConfigurationAdapter.class.getName());
    public ConfigurationAdapter() {
    }

    public SystemConfiguration fromLegacy(LegacyConfiguration l) {
        try {
            final SystemConfiguration c = new SystemConfiguration();
            c.setNewUserPassword(l.newuserpw);
            c.setSystemPassword(l.systempw);
            c.setMessageDirectory(l.msgsdir);
            c.setGFilesDirectory(l.gfilesdir);
            c.setDataDirectory(l.gfilesdir);
            c.setDataDirectory(l.datadir);
            c.setDownloadDirectory(l.dloadsdir);
            c.setTempDirectory(l.tempdir);
            c.setRegistrationCode(l.regcode);
            c.setSystemName(l.systemname);
            c.setSystemPhone(l.systemphone);
            c.setSysopName(l.sysopname);
            c.setNewUserSL(l.newusersl);
            c.setNewUserDSL(l.newuserdsl);
            c.setMaxMailWaiting(l.maxwaiting);
            c.setPrimaryPort(l.primaryport);
            c.setNewUploads(l.newuploads);
            c.setClosedSystem(l.closedsystem);
            c.setSystemNumber(l.systemnumber);
            c.setNewuserRestrict(l.newuser_restrict);
            
            return c;
        } catch (Exception ex) {
            logger.log(Level.SEVERE, "error converting from legacy config.dat", ex);
        }
        return null;
    }

    public LegacyConfiguration toLegacy(SystemConfiguration config) {
        try {
            final LegacyConfiguration c = new LegacyConfiguration();
            return c;
        } catch (Exception ex) {
            logger.log(Level.SEVERE, "error converting to legacy config.dat", ex);
        }
        return null;
    }
}
