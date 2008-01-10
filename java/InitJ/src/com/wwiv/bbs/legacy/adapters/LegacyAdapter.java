package com.wwiv.bbs.legacy.adapters;

import com.wwiv.bbs.legacy.model.LegacyConfiguration;
import com.wwiv.bbs.model.SystemConfiguration;

public interface LegacyAdapter<L,M> {
    M fromLegacy(L legacy);
    L toLegacy(M config);
}
