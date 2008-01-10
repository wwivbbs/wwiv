package com.wwiv.bbs.legacy.adapters;

import java.util.HashMap;
import java.util.Map;

public final class LegacyAdapterManager {
    private static LegacyAdapterManager INSTANCE;
    private Map<Class, LegacyAdapter> toLegacyMap = new HashMap<Class, LegacyAdapter>();
    private Map<Class, LegacyAdapter> fromLegacyMap = new HashMap<Class, LegacyAdapter>();

    private LegacyAdapterManager() {
    }

    public static synchronized LegacyAdapterManager getAdapterManager() {
        if (INSTANCE == null) {
            INSTANCE = new LegacyAdapterManager();
        }
        return INSTANCE;
    }

    public void registerToLegacyAdapter(Class modelClass, LegacyAdapter adapter) {
        toLegacyMap.put(modelClass, adapter);
    }

    public void registerFromLegacyAdapter(Class modelClass, LegacyAdapter adapter) {
        fromLegacyMap.put(modelClass, adapter);
    }
    
    public LegacyAdapter getToLegacyAdapter(Class modelClass) {
        return toLegacyMap.get(modelClass);
    }

    public LegacyAdapter getFromLegacyAdapter(Class modelClass) {
        return fromLegacyMap.get(modelClass);
    }

}
