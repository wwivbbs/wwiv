package com.wwiv.bbs.init;

import com.wwiv.bbs.init.ui.AboutBox;
import com.wwiv.bbs.init.ui.MainFrame;
import com.wwiv.bbs.legacy.adapters.ConfigurationAdapter;
import com.wwiv.bbs.legacy.datafile.Config;
import com.wwiv.bbs.legacy.model.LegacyConfiguration;
import com.wwiv.bbs.model.SystemConfiguration;

import java.awt.Component;
import java.awt.Dimension;
import java.awt.EventQueue;

import java.awt.KeyboardFocusManager;

import java.awt.Toolkit;

import java.awt.datatransfer.DataFlavor;

import java.beans.PropertyChangeEvent;
import java.beans.PropertyChangeListener;

import java.io.File;

import java.util.List;

import javax.swing.JFileChooser;
import javax.swing.JOptionPane;
import javax.swing.JPasswordField;
import javax.swing.SwingWorker;

import javax.swing.filechooser.FileNameExtensionFilter;

import javax.swing.text.JTextComponent;

import org.jdesktop.swingx.JXDialog;
import org.jdesktop.swingx.JXFrame;
import org.jdesktop.swingx.action.ActionFactory;
import org.jdesktop.swingx.action.ActionManager;
import org.jdesktop.swingx.action.BoundAction;
import org.jdesktop.swingx.painter.gradient.BasicGradientPainter;


public class Controller implements PropertyChangeListener {
    private final ActionManager am;
    private final Application app;
    private final KeyboardFocusManager fm;

    Controller(Application app) {
        this.am = ActionManager.getInstance();
        this.app = app;
        this.fm = KeyboardFocusManager.getCurrentKeyboardFocusManager();
        fm.addPropertyChangeListener("focusOwner", this);
    }

    public void initializeActions() {
        am.addAction(ActionFactory.createBoundAction("menuFile", "File", "F"));
        am.addAction(ActionFactory.createBoundAction("menuEdit", "Edit", "E"));
        am.addAction(ActionFactory.createBoundAction("menuHelp", "Help", "H"));

        final BoundAction openAction = ActionFactory.createBoundAction("open-legacy", "Open CONFIG.DAT", "O");
        openAction.registerCallback(this, "open");
        am.addAction(openAction);

        final BoundAction saveAction = ActionFactory.createBoundAction("save-legacy", "Save CONFIG.DAT", "S");
        saveAction.registerCallback(this, "save");
        am.addAction(saveAction);

        final BoundAction exitAction = ActionFactory.createBoundAction("exit", "Exit", "x");
        exitAction.registerCallback(this, "exit");
        am.addAction(exitAction);

        final BoundAction aboutAction = ActionFactory.createBoundAction("about", "About INIT", "A");
        aboutAction.registerCallback(this, "about");
        am.addAction(aboutAction);

        final BoundAction cutAction = ActionFactory.createBoundAction("cut", "Cut", "X");
        cutAction.registerCallback(this, "cut");
        am.addAction(cutAction);

        final BoundAction copyAction = ActionFactory.createBoundAction("copy", "Copy", "C");
        copyAction.registerCallback(this, "copy");
        am.addAction(copyAction);

        final BoundAction pasteAction = ActionFactory.createBoundAction("paste", "Paste", "P");
        pasteAction.registerCallback(this, "paste");
        am.addAction(pasteAction);

        final BoundAction selectAllAction = ActionFactory.createBoundAction("select-all", "Select All", "S");
        selectAllAction.registerCallback(this, "selectAll");
        am.addAction(selectAllAction);

        updateAll();
    }

    private void updateEditMenu() {
        Component c = fm.getPermanentFocusOwner();
        if (c instanceof JTextComponent && !(c instanceof JPasswordField)) {
            JTextComponent t = (JTextComponent)c;
            boolean enabled = t.getSelectionEnd() - t.getSelectionStart() > 0;

            am.setEnabled("copy", enabled);
            am.setEnabled("cut", enabled);
        }
        boolean pasteEnabled = Toolkit.getDefaultToolkit().getSystemClipboard().isDataFlavorAvailable(DataFlavor.stringFlavor);
        am.setEnabled("paste", pasteEnabled);
    }

    public void propertyChange(PropertyChangeEvent e) {
        if ("focusOwner".equals(e.getPropertyName())) {
            updateEditMenu();
        }
    }


    public void exit() {
        System.exit(0);
    }

    public void about() {
        JXDialog d = new JXDialog(app.getMainWindow(), new AboutBox());
        d.setMinimumSize(new Dimension(400, 300));
        d.setTitle("About Init/J");
        d.pack();
        d.setLocationRelativeTo(null);
        d.setVisible(true);
    }

    public void save() {
        final MainFrame mw = (MainFrame)app.getMainWindow();
        try {
            app.setConfiguration(null);
            mw.disablePanels();
        } catch (Exception ex) {
            mw.enablePanels();
        } finally {
            updateAll();
        }
    }

    public void open() {
        final MainFrame mw = (MainFrame)app.getMainWindow();
        final JFileChooser chooser = new JFileChooser();
        final FileNameExtensionFilter filter = new FileNameExtensionFilter("config.dat", "dat");
        chooser.setFileFilter(filter);
        final int returnVal = chooser.showOpenDialog(mw);
        if (returnVal == JFileChooser.APPROVE_OPTION) {
            final File selectedFile = chooser.getSelectedFile();
            System.out.println("You chose to open this file: " + selectedFile.getName());
            final Config legacyConfigFile = new Config(selectedFile);
            try {
                final LegacyConfiguration legacyConfiguration = legacyConfigFile.loadConfiguration();
                final ConfigurationAdapter adapter = new ConfigurationAdapter();
                final SystemConfiguration systemConfiguration = adapter.fromLegacy(legacyConfiguration);
                app.setConfiguration(systemConfiguration);
                mw.enablePanels();
            } catch (Exception ex) {
                ex.printStackTrace();
                app.setConfiguration(null);
                mw.disablePanels();
            } finally {
                updateAll();
            }
        }
    }

    public void cut() {
        final Component c = fm.getPermanentFocusOwner();
        if (c instanceof JTextComponent) {
            final JTextComponent t = (JTextComponent)c;
            t.cut();
        }
    }

    public void copy() {
        final Component c = fm.getPermanentFocusOwner();
        if (c instanceof JTextComponent) {
            final JTextComponent t = (JTextComponent)c;
            t.copy();
        }
    }

    public void paste() {
        final Component c = fm.getPermanentFocusOwner();
        if (c instanceof JTextComponent) {
            final JTextComponent t = (JTextComponent)c;
            t.paste();
        }
    }

    public void selectAll() {
        final Component c = fm.getPermanentFocusOwner();
        if (c instanceof JTextComponent) {
            final JTextComponent t = (JTextComponent)c;
            t.selectAll();
        }
    }

    public void updateAll() {
        boolean configLoaded = app.getConfiguration() != null;
        am.setEnabled("open-legacy", !configLoaded);
        am.setEnabled("save-legacy", configLoaded);
    }

}
