package com.wwiv.bbs.init.ui;

import com.wwiv.bbs.model.SystemConfiguration;

import javax.swing.BorderFactory;
import javax.swing.JComponent;

import org.jdesktop.swingx.JXPanel;
import org.jdesktop.swingx.JXTitledPanel;
import org.jdesktop.swingx.painter.gradient.BasicGradientPainter;


public abstract class InitPanel extends JXTitledPanel implements ModelBoundPanel<SystemConfiguration> {
    public InitPanel(String title) {
        this.setTitle(title);
    }

    public JComponent getGUI() {
        return this;
    }
    
    public String toString() {
        return getTitle();
    }

}
