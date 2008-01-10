package com.wwiv.bbs.init;

import com.wwiv.bbs.init.ui.MainFrame;
import com.wwiv.bbs.model.SystemConfiguration;

import java.awt.Dimension;

import javax.swing.JFrame;

public final class Application {
    private static Application INSTANCE;
    private Controller configController;
    private MainFrame mainFrame;
    private SystemConfiguration config;
    
    private Application() {
        if (INSTANCE != null) {
            throw new IllegalStateException("Can not create more than 1 application object!");
        }
        configController = new Controller(this);
        configController.initializeActions();
        this.mainFrame = new MainFrame();
        mainFrame.initializeMainFrame();
        mainFrame.setMinimumSize(new Dimension(700, 600));
        mainFrame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        mainFrame.setLocationRelativeTo(null);
    }
    
    public static Application getInstance() {
        return Application.INSTANCE;
    }
    
    public static Application createApplication() {
        INSTANCE = new Application();
        return INSTANCE;
    }
    
    public JFrame getMainWindow() {
        return mainFrame;
    }
    
    public void setConfiguration(SystemConfiguration config) {
        this.config = config;
    }
    
    public SystemConfiguration getConfiguration() {
        return config;
    }
    
    
}
