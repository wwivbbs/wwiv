package com.wwiv.bbs.legacy.datafile;

import java.io.File;

public class LegacyConfigFile {
    private final File file;
    public LegacyConfigFile(File file) {
        this.file = file;
    }
    
    protected File getFile() {
        return file;
    }
}
