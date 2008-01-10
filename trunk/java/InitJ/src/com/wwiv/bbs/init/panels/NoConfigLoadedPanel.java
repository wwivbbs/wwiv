package com.wwiv.bbs.init.panels;

import com.wwiv.bbs.init.ui.InitPanel;

import com.wwiv.bbs.model.SystemConfiguration;

import java.awt.Container;
import java.awt.FlowLayout;

import javax.swing.JComponent;
import javax.swing.JLabel;

import org.jdesktop.swingx.JXHyperlink;
import org.jdesktop.swingx.JXPanel;
import org.jdesktop.swingx.VerticalLayout;
import org.jdesktop.swingx.action.ActionManager;
import org.jdesktop.swingx.painter.gradient.BasicGradientPainter;

public class NoConfigLoadedPanel extends InitPanel {
    private String title;

    public NoConfigLoadedPanel() {
        super("CONFIG.DAT is not loaded.");
        
        final VerticalLayout layout = new VerticalLayout();
        layout.setGap(10);
        JXPanel c = new JXPanel();
        setContentContainer(c);
        c.setLayout(layout);
        c.add(new JLabel(getTitle()));
        JXHyperlink link = new JXHyperlink();
        link.setAction(ActionManager.getInstance().getAction("open-legacy"));
        link.setBorderPainted(false);
        c.add(link);
        c.setBackgroundPainter( new BasicGradientPainter(BasicGradientPainter.AERITH) );
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
