package com.wwiv.bbs.init;

import com.wwiv.bbs.init.ui.MainFrame;
import com.wwiv.bbs.model.SystemConfiguration;

import java.awt.Dimension;
import java.awt.EventQueue;

import javax.swing.JFrame;
import javax.swing.UIManager;


public class Main implements Runnable {
    private static Application app;
    
    public Main() {
    }

    public static void main(String[] args) {
        EventQueue.invokeLater(new Main());
    }
    
    /**
     * @deprecated Use Application#setConfiguration(SystemConfiguration)
     * @param config
     */
    @Deprecated public void setConfiguration(SystemConfiguration config) {
        app.setConfiguration(config);
    }

    /**
     * @deprecated Use Application#getConfiguration()
     * @return
     */
    @Deprecated public SystemConfiguration getConfiguration() {
        return app.getConfiguration();
    }
    
    public JFrame getMainWindow() {
        return app.getMainWindow();
    }

    public void run() {
        try {
            UIManager.setLookAndFeel(UIManager.getSystemLookAndFeelClassName());
        } catch (Exception ex) {
        }
        Application.createApplication().getMainWindow().setVisible(true);
    }

}
