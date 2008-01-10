package com.wwiv.bbs.init.ui;

import javax.swing.JComponent;

public interface ModelBoundPanel<T> {
    JComponent getGUI();
    T viewToModel(T model);
    void modelToView(T model);
}
