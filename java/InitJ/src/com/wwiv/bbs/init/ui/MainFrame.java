package com.wwiv.bbs.init.ui;

import com.wwiv.bbs.init.Application;
import com.wwiv.bbs.init.panels.GeneralPanel;
import com.wwiv.bbs.init.panels.NoConfigLoadedPanel;
import com.wwiv.bbs.init.panels.NullPanel;

import java.awt.BorderLayout;
import java.awt.CardLayout;
import java.awt.Component;
import java.awt.Container;
import java.awt.Dimension;

import javax.swing.BorderFactory;
import javax.swing.DefaultListModel;
import javax.swing.JLabel;
import javax.swing.JList;
import javax.swing.JMenuBar;
import javax.swing.JPanel;
import javax.swing.JProgressBar;
import javax.swing.JSplitPane;
import javax.swing.event.ListSelectionEvent;
import javax.swing.event.ListSelectionListener;

import org.jdesktop.swingx.JXFrame;
import org.jdesktop.swingx.JXPanel;
import org.jdesktop.swingx.JXStatusBar;
import org.jdesktop.swingx.JXTitledPanel;
import org.jdesktop.swingx.action.ActionContainerFactory;
import org.jdesktop.swingx.action.ActionManager;
import org.jdesktop.swingx.border.DropShadowBorder;
import org.jdesktop.swingx.painter.gradient.BasicGradientPainter;


public class MainFrame extends JXFrame implements ListSelectionListener {
    private BorderLayout layoutMain = new BorderLayout();
    private JSplitPane panelCenter = new JSplitPane(JSplitPane.HORIZONTAL_SPLIT, false);
    private JLabel statusBar = new JLabel();
    private JList navigation = new JList();
    private JXPanel contentPanel = new JXPanel();
    private CardLayout contentLayout = new CardLayout();
    private DefaultListModel model = new DefaultListModel();
    public JProgressBar pbar = new JProgressBar();

    public MainFrame() {
        try {
            jbInit();
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
    
    public void initializeMainFrame() {
        initializeStatusBar();
        postInit();
        disablePanels();
    }

    private void jbInit() throws Exception {
        final ActionManager am = ActionManager.getInstance();
        final ActionContainerFactory af = new ActionContainerFactory();
        this.setLayout(layoutMain);
        this.setTitle("INIT");
                
        final JMenuBar menuBar = new JMenuBar();
        menuBar.add(af.createMenu(new Object[] { "menuFile", "open-legacy", "save-legacy", null, "exit" }));
        menuBar.add(af.createMenu(new Object[] { "menuEdit", "cut", "copy", "paste", null, "select-all" }));
        menuBar.add(af.createMenu(new Object[] { "menuHelp", "about" }));
        this.setJMenuBar(menuBar);

        add(panelCenter, BorderLayout.CENTER);
    
        final JXTitledPanel navigationPanel = new JXTitledPanel();
        navigationPanel.setTitle("Table of Contents");
        final JXPanel navContent = new JXPanel(new BorderLayout());
        navContent.setBorder(BorderFactory.createEmptyBorder(5, 5, 5, 5));
        navContent.add(navigation, BorderLayout.CENTER);
        navContent.setBackground(navigation.getBackground());
        navigationPanel.setContentContainer(navContent);
        navigationPanel.setBorder(BorderFactory.createCompoundBorder(
                                      BorderFactory.createEmptyBorder(10, 10, 10, 10), new DropShadowBorder()));
        panelCenter.setLeftComponent(navigationPanel);
        
        contentPanel.setLayout(contentLayout);
        contentPanel.setBorder(BorderFactory.createCompoundBorder(BorderFactory.createEmptyBorder(10, 10, 10, 10), new DropShadowBorder()));
//        contentPanel.setBackgroundPainter(new BasicGradientPainter(BasicGradientPainter.BLUE_EXPERIENCE));

        panelCenter.setRightComponent(contentPanel);
    }

    private void addPanel(InitPanel panel) {
        model.addElement(panel);
        contentPanel.add(panel, panel.getClass().getName()+":"+panel.getTitle());
        final Container content = panel.getContentContainer();
        if ( content instanceof JXPanel ) {
            JXPanel jxp = (JXPanel) content;
            jxp.setBackgroundPainter(new BasicGradientPainter(BasicGradientPainter.AERITH));
        }
    }

    private void initializeStatusBar() {
        final JXStatusBar bar = new JXStatusBar();
        statusBar.setText(" ");
        final Dimension prefSize = statusBar.getPreferredSize();
        prefSize.width = 400;
        statusBar.setPreferredSize(prefSize);
        bar.add(statusBar);
        bar.add(pbar); // Fill with no inserts - will use remaining space
        add(bar, BorderLayout.SOUTH);
        pbar.setVisible(false);
    }

    private void postInit() {
        contentPanel.add(new NoConfigLoadedPanel(), "null");
        addPanel(new GeneralPanel());
        addPanel(new NullPanel("System Paths"));
        addPanel(new NullPanel("External Protocol Configuration"));
        addPanel(new NullPanel("External Editor Configuration"));
        addPanel(new NullPanel("Security Level Configuration"));
        addPanel(new NullPanel("Auto-Validation Level Configuration"));
        addPanel(new NullPanel("Archiver Configuration"));
        addPanel(new NullPanel("Language Configuration"));
        addPanel(new NullPanel("Network Configuration"));
        addPanel(new NullPanel("Registration Information"));
        addPanel(new NullPanel("Update Sub/Directory Maximums"));
        navigation.setModel(model);
        navigation.addListSelectionListener(this);
    }

    public void valueChanged(ListSelectionEvent e) {
        if (!e.getValueIsAdjusting()) {
            final Object o = navigation.getSelectedValue();
            selectPanel((o instanceof InitPanel) ? (InitPanel)o : null);
        }
    }

    public void selectPanel(InitPanel panel) {
        final Application app = Application.getInstance();
        if (panel != null) {
            final InitPanel lastPanel = (InitPanel)MainFrame.getSelectedCardComponent(contentPanel);
            if (lastPanel != null ) {
                app.setConfiguration(lastPanel.viewToModel(app.getConfiguration()));
            }
            contentLayout.show(contentPanel, panel.getClass().getName()+":"+panel.getTitle());
            panel.modelToView(app.getConfiguration());
        } else {
            contentLayout.show(contentPanel, "null");
        }
    }
    
    private static Component getSelectedCardComponent(Container container) {
        final Component[] components = container.getComponents();
        for( Component c : components ) {
            if ( c.isShowing() ) {
                return c;
            }
        }
        return null;
    }
    
    public void disablePanels() {
        navigation.clearSelection();
        navigation.setEnabled(false);
        selectPanel(null);
    }
    
    public void enablePanels() {
        navigation.setEnabled(true);
        navigation.setSelectedIndex(0);
    }
}
