package com.wwiv.bbs.swing;

import com.jgoodies.forms.builder.DefaultFormBuilder;
import com.jgoodies.forms.layout.FormLayout;

import java.awt.BorderLayout;
import java.awt.Color;
import java.awt.FlowLayout;

import java.util.logging.Level;
import java.util.logging.Logger;

import javax.swing.BorderFactory;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JPanel;
import javax.swing.JTextField;

public class TimeField extends JPanel {
    private JTextField hours = new JTextField(2);
    private JLabel separator = new JLabel();
    private JTextField minutes = new JTextField(2);
    private Logger logger = Logger.getLogger(TimeField.class.getName());
    
    /** Time in number of minutes */
    private int time;
    
    public TimeField() {
        this(0);
    }

    public TimeField(int time) {
        super();
        FlowLayout layout = (FlowLayout)this.getLayout();
        layout.setHgap(0);
        layout.setVgap(0);
        initializeComponents();
        setTime(time);
    }

    private void initializeComponents() {
        separator.setText(":");
        add(hours);
        add(separator);
        add(minutes);
        setBorder(BorderFactory.createEmptyBorder());
    }
    
    public void setTime(int time) {
        this.time = time;
        int hour = time / 60;
        int minute = time % 60;
        hours.setText(Integer.toString(hour));
        minutes.setText(Integer.toString(minute));
        logger.info(String.format("The time set is %d:%d", hour, minute));
    }
    
    public int getTime() {
        return time;
    }
    
    public static void main(String[] args) {
        JFrame f = new JFrame();
        f.setLayout(new BorderLayout());
        JPanel content = new JPanel();
        f.add(content, BorderLayout.CENTER);
        
        final FormLayout layout = new FormLayout("right:pref, 3dlu, left:pref", "");

        final DefaultFormBuilder builder = new DefaultFormBuilder(layout, content);
        builder.setDefaultDialogBorder();
        builder.append("60", new TimeField(60));
        builder.append("120", new TimeField(120));
        builder.append("150", new TimeField(150));
        builder.append("1439", new TimeField(1439));
        f.pack();
        f.setLocationRelativeTo(null);
        f.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        f.setVisible(true);
    }
    
}
