package com.wwiv.bbs.swing;

import com.jgoodies.forms.builder.DefaultFormBuilder;
import com.jgoodies.forms.layout.FormLayout;

import java.awt.FlowLayout;

import java.awt.Font;
import java.awt.Toolkit;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;

import java.awt.event.KeyAdapter;
import java.awt.event.KeyEvent;
import java.awt.event.MouseAdapter;

import java.awt.event.MouseEvent;

import java.util.BitSet;

import java.util.logging.Logger;

import javax.swing.BorderFactory;
import javax.swing.JButton;
import javax.swing.JCheckBox;
import javax.swing.JLabel;
import javax.swing.JOptionPane;
import javax.swing.JPanel;
import javax.swing.JTextField;

/**
 * LCMA*PEVKNU01234
 * 
 * #define restrict_logon              0x0001
 * #define restrict_chat               0x0002
 * #define restrict_validate           0x0004
 * #define restrict_automessage        0x0008
 * #define restrict_anony              0x0010
 * #define restrict_post               0x0020
 * #define restrict_email              0x0040
 * #define restrict_vote               0x0080
 * #define restrict_iichat             0x0100
 * #define restrict_net                0x0200
 * #define restrict_upload             0x0400
 */
public class RestrictionField extends JPanel {
    private JButton button = new JButton();
    private JTextField field = new JTextField();
    private BitSet restriction;
    private final String[] restrictNames = new String[] 
        { "Logon","Chat","Mail Unvalidated","AutoMessage","* Anonymous",
          "Post","Email","Vote","K InterChat","Network","Upload",
          "0 (User Defined)","1 (User Defined)","2 (User Defined)",
          "3 (User Defined)","4 (User Defined)" };
    private final char[] restrictLetters = new char[] { 'L','C','M','A','*','P','E','V','K','N','U','0','1','2','3','4' };
    private Logger logger = Logger.getLogger(RestrictionField.class.getName());
    
    public RestrictionField() {
        this(0);
    }
    
    public RestrictionField(int restrict) {
        super();
        initComponents();
        setRestriction(restrict);
    }
    
    public void setRestriction(int restrictionInt) {
        this.restriction = new BitSet(restrictLetters.length);
        for( int i=0; i<restrictLetters.length; i++ ) {
            final boolean set = ((restrictionInt & (1 << i)) == (1<<i));
            restriction.set(i, set);
        }
        updateTextField();
    }
    
    private void updateTextField() {
        final StringBuilder text = new StringBuilder(20);
        for( int i=0; i<restrictLetters.length; i++ ) {
            text.append(restriction.get(i) ? restrictLetters[i] : ' ' );
        }
        field.setText(text.toString());
        logger.fine("["+text.toString()+"]");
    }

    public int getRestriction() {
        int result = 0;
        for( int i=0; i < restriction.length(); i++ ) {
            if ( restriction.get(i)) {
                result |= (1<<i);
            }
        }
        return result;
    }
    
    public String getRestrictionText() {
        final StringBuilder text = new StringBuilder(20);
        for (int i = 0; i < restrictLetters.length; i++) {
            text.append(restriction.get(i) ? restrictLetters[i] : ' ');
        }
        return text.toString();
    }
    
    private void initComponents() {
        final FlowLayout layout = (FlowLayout)getLayout();
        layout.setHgap(0);
        layout.setVgap(0);
        layout.setAlignment(FlowLayout.LEADING);
        
        field.setColumns(16);
        field.setEditable(false);
        button.setBorder(BorderFactory.createEmptyBorder(2, 4, 2, 2));
        button.setBorderPainted(false);
        button.setRolloverEnabled(false);
        button.setText("...");
        button.addActionListener(new ActionListener() {
                    public void actionPerformed(ActionEvent e) {
                        showSelectionDialog();
                    }
                });
        add(field);
        add(new JLabel(" "));
        add(button);
        field.addMouseListener(new MouseAdapter() {
            public void mouseClicked(MouseEvent e) 
            {
                showSelectionDialog();;
            }
        });
        field.addKeyListener(new KeyAdapter() {
            public void keyTyped(KeyEvent e) {
                final char keyCode = e.getKeyChar();
                if (  keyCode == KeyEvent.VK_ENTER || keyCode == KeyEvent.VK_SPACE ) {
                    showSelectionDialog();
                }
                else {
                    System.out.println(keyCode);
                    Toolkit.getDefaultToolkit().beep();
                }
            }
        });
        final Font oldFont = field.getFont();
        field.setFont(new Font("Monospaced", Font.PLAIN, oldFont.getSize()));
        setOpaque(false);
        
    }

    private void showSelectionDialog() {
        JPanel panel = new JPanel();
        final FormLayout layout = new FormLayout("left:max(40dlu;pref), 3dlu, left:pref, 3dlu, "+
                                                 "left:max(40dlu;pref)", ""); 

        final DefaultFormBuilder builder = new DefaultFormBuilder(layout, panel);
        builder.setDefaultDialogBorder();
        JCheckBox[] boxes = new JCheckBox[restrictLetters.length];
        for(int i=0; i < boxes.length; i++) {
            boxes[i] = new JCheckBox(restrictNames[i]);
            boxes[i].setSelected(restriction.get(i));
            builder.append(boxes[i]);
        }
        int result = JOptionPane.showConfirmDialog(this, panel, "Select Restrictions", JOptionPane.OK_CANCEL_OPTION);
        if (result == JOptionPane.OK_OPTION) {
            restriction.clear();
            for( int i=0; i < boxes.length; i++ ) {
                restriction.set(i, boxes[i].isSelected());
                updateTextField();
            }
        }

    }
    
    
    public static void main(String[] args) {
        System.out.println(new RestrictionField(7).getRestriction());
        System.out.println(new RestrictionField(0).getRestriction());
        System.out.println(new RestrictionField(1).getRestriction());
        System.out.println(new RestrictionField(2).getRestriction());
        System.out.println(new RestrictionField(16).getRestriction());
        System.out.println(new RestrictionField(21).getRestriction());
    }
    
    
}
