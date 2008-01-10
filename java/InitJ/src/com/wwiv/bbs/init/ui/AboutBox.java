package com.wwiv.bbs.init.ui;

import java.awt.BorderLayout;
import java.awt.Color;
import java.awt.Container;
import java.awt.Dimension;
import java.awt.GridBagConstraints;
import java.awt.GridBagLayout;
import java.awt.Insets;

import javax.swing.BorderFactory;
import javax.swing.JLabel;
import javax.swing.border.Border;

import org.jdesktop.swingx.JXHeader;
import org.jdesktop.swingx.JXPanel;
import org.jdesktop.swingx.JXTitledPanel;
import org.jdesktop.swingx.painter.gradient.BasicGradientPainter;


public class AboutBox extends JXPanel {
    private JLabel labelAuthor = new JLabel();
    private JLabel labelCopyright = new JLabel();
    private JLabel labelCompany = new JLabel();
    private GridBagLayout layoutMain = new GridBagLayout();

    public AboutBox() {
        try {
            jbInit();
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private void jbInit() throws Exception {
        JXHeader header = new JXHeader();
        this.setLayout(new BorderLayout());
          header.setTitle("Init/J v5.0");
          header.setDescription("WWIV BBS Initialization and Configuration Utility.");
        this.add(header, BorderLayout.NORTH);
        labelAuthor.setText( "WWIV Software Services" );
        labelCopyright.setText( "Copyright 2007 WWIV Software Services." );
        labelCompany.setText( "All Rights Reserved." );
        final JXPanel content = new JXPanel();
        this.add(content, BorderLayout.CENTER);
        content.setLayout( layoutMain );
        content.add( labelAuthor, new GridBagConstraints(0, 1, 1, 1, 0.0, 0.0, GridBagConstraints.WEST, GridBagConstraints.HORIZONTAL, new Insets(0, 15, 0, 15), 0, 0) );
        content.add( labelCopyright, new GridBagConstraints(0, 2, 1, 1, 0.0, 0.0, GridBagConstraints.WEST, GridBagConstraints.HORIZONTAL, new Insets(0, 15, 0, 15), 0, 0) );
        content.add( labelCompany, new GridBagConstraints(0, 3, 1, 1, 0.0, 0.0, GridBagConstraints.WEST, GridBagConstraints.HORIZONTAL, new Insets(0, 15, 5, 15), 0, 0) );
        setMaximumSize(new Dimension(400, 300));
        content.setBackgroundPainter(new BasicGradientPainter(BasicGradientPainter.AERITH));

    }
}
