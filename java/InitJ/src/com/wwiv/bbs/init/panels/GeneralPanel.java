package com.wwiv.bbs.init.panels;

import com.jgoodies.forms.builder.DefaultFormBuilder;
import com.jgoodies.forms.layout.FormLayout;

import com.wwiv.bbs.init.ui.InitPanel;
import com.wwiv.bbs.model.SystemConfiguration;
import com.wwiv.bbs.swing.RestrictionField;
import com.wwiv.bbs.swing.TimeField;

import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;

import javax.swing.JCheckBox;
import javax.swing.JComponent;
import javax.swing.JFrame;
import javax.swing.JPasswordField;
import javax.swing.JSpinner;
import javax.swing.JTextField;
import javax.swing.SpinnerNumberModel;
import javax.swing.UIManager;

import org.jdesktop.swingx.JXPanel;


/**
 * System PW        : ####################
 * System name      : Mystic Rhythms BBS
 * System phone     : 415-NET-ONLY
 * Closed system    : N
 * Newuser PW       :
 * Newuser restrict :   M
 * Newuser SL       : 50
 * Newuser DSL      : 50
 * Newuser gold     : 100
 * Sysop name       : RushFan
 * Sysop low time   : 00:00
 * Sysop high time  : 23:59
 * Net low time     : 00:00
 * Net high time    : 00:00
 * Up/Download ratio: 0.000
 * Post/Call ratio  : 0.000
 * Max waiting      : 100
 * Max users        : 500
 * Caller number    : 1888
 * Days active      : 1540
 */
public class GeneralPanel extends InitPanel {
    private JPasswordField tfSystemPassword = new JPasswordField(20);
    private JTextField tfSystemName = new JTextField(50);
    private JTextField tfSystemPhone = new JTextField(12);
    private JCheckBox cbClosedSystem = new JCheckBox("Closed Sytem");
    private JTextField tfNewUserPassword = new JTextField(20);
    private RestrictionField tfNewUserRestrict = new RestrictionField();
    private JSpinner tfNewUserSL = new JSpinner();
    private JSpinner tfNewUserDSL = new JSpinner();
    private JSpinner tfNewUserGold = new JSpinner();
    private JTextField tfSysopName = new JTextField(50);
    private TimeField tfSysopLowTime = new TimeField();
    private TimeField tfSysopHighTime = new TimeField();
    private TimeField tfNetLowTime = new TimeField();
    private TimeField tfNetHighTime = new TimeField();
    private JTextField tfUploadDownloadRatio = new JTextField(10);
    private JTextField tfPostCallRatio = new JTextField(10);
    private JSpinner tfMaxWaiting = new JSpinner();
    private JSpinner tfMaxUsers = new JSpinner();
    private JSpinner tfCallerNumber = new JSpinner();
    private JSpinner tfDaysActive = new JSpinner();

    public GeneralPanel() {
        super("General System Configuration");
        try {
            initComponents();
            initComponentModels();
            updateComponents();
            jbInit();
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private void initComponents() {
        JXPanel panel = new JXPanel();
        final FormLayout layout = new FormLayout("right:max(40dlu;pref), 3dlu, left:min(100dlu;pref), 7dlu, " +
                                                 "right:max(40dlu;pref), 3dlu, left:pref", 
                                                 "");
        final DefaultFormBuilder builder = new DefaultFormBuilder(layout, panel);
        builder.setDefaultDialogBorder();

        builder.appendSeparator("System");
        builder.append("System Password", tfSystemPassword, 3);
        builder.nextLine();
        builder.append("System Name", tfSystemName, 5);
        builder.nextLine();
        builder.append("System Phone", tfSystemPhone);
        builder.nextLine();
        builder.append("Net Low Time", tfNetLowTime);
        builder.append("Net High Time", tfNetHighTime);
        builder.nextLine();
        builder.append("Upload/Download Ratio", tfUploadDownloadRatio);
        builder.append("Post/Call Ratio", tfPostCallRatio);
        builder.nextLine();
        builder.append("Max Waiting", tfMaxWaiting);
        builder.append("Max Users", tfMaxUsers);
        builder.nextLine();
        builder.append("Caller Number", tfCallerNumber);
        builder.append("Days Active", tfDaysActive);
        builder.nextLine();
        builder.appendSeparator("Sysop Information");
        builder.append("Sysop Name", tfSysopName, 4);
        builder.nextLine();
        builder.append("Sysop Low Time", tfSysopLowTime);
        builder.append("Sysop High Time", tfSysopHighTime);
        builder.nextLine();
        builder.appendSeparator("New User");
        builder.append("System Type", cbClosedSystem, 5);
        builder.nextLine();
        builder.append("New User Password", tfNewUserPassword, 3);
        builder.nextLine();
        builder.append("New User Restrict", tfNewUserRestrict, 5);
        builder.nextLine();
        builder.append("New User SL", tfNewUserSL);
        builder.append("New User DSL", tfNewUserDSL);
        builder.nextLine();
        builder.append("New User Gold", tfNewUserGold);
        builder.nextLine();
        
        this.setContentContainer(builder.getPanel());
    }

    public SystemConfiguration viewToModel(SystemConfiguration model) {
        return model;
    }

    public void modelToView(SystemConfiguration model) {
        tfSystemPassword.setText(model.getSystemPassword());
        tfSystemName.setText(model.getSystemName());
        tfSystemPhone.setText(model.getSystemPhone());
        cbClosedSystem.setSelected(model.getClosedSystem()!=0);
        tfNewUserPassword.setText(model.getNewUserPassword());
        tfNewUserSL.setValue(model.getNewUserSL());
        tfNewUserDSL.setValue(model.getNewUserDSL());
        tfNewUserRestrict.setRestriction(model.getNewuserRestrict());
        updateComponents();
    }
    
    private void updateComponents() {
        tfNewUserPassword.setEnabled(cbClosedSystem.isSelected());
    }

    public JComponent getGUI() {
        return this;
    }

    private void jbInit() throws Exception {
    }

    public static void main(String[] args) {
        try {
            UIManager.setLookAndFeel(UIManager.getSystemLookAndFeelClassName());    
        } catch (Exception ex) {

        }
        JFrame f = new JFrame();
        f.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        f.add(new GeneralPanel());
        f.pack();
        f.setLocationRelativeTo(null);
        f.setVisible(true);
    }

    public String getTitle() {
        return "General System Configuration";
    }

    private void initComponentModels() {
        tfNewUserSL.setModel(new SpinnerNumberModel(50, 0, 255, 1));
        tfNewUserDSL.setModel(new SpinnerNumberModel(50, 0, 255, 1));
        tfNewUserGold.setModel(new SpinnerNumberModel(0, 0, 999, 1));
        tfMaxWaiting.setModel(new SpinnerNumberModel(0, 0, 999, 1));
        tfMaxUsers.setModel(new SpinnerNumberModel(0, 0, 99999, 1));
        tfCallerNumber.setModel(new SpinnerNumberModel(0, 0, 99999, 1));
        tfDaysActive.setModel(new SpinnerNumberModel(0, 0, 99999, 1));
        cbClosedSystem.addActionListener(new ActionListener() {
                    public void actionPerformed(ActionEvent e) {
                        updateComponents();
                    }
                });
    }
}
