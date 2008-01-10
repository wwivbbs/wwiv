package com.wwiv.bbs.init.panels;

import com.wwiv.bbs.init.ui.InitPanel;
import com.wwiv.bbs.model.SystemConfiguration;

import java.awt.FlowLayout;

import javax.swing.JComponent;
import javax.swing.JLabel;


public class NullPanel extends InitPanel  {
    public NullPanel(String title) {
        super(title);
        setTitle(title);
        this.getContentContainer().setLayout(new FlowLayout());
        this.getContentContainer().add(new JLabel(title));
    }

    @Override
    public JComponent getGUI() {
        return this;
    }

    public SystemConfiguration viewToModel(SystemConfiguration model) {
        return model;
    }

    public void modelToView(SystemConfiguration model) {
        
    }
}
