package test.com.wwiv.bbs.swing;

import com.wwiv.bbs.swing.RestrictionField;

import junit.framework.TestCase;

import junit.textui.TestRunner;

public class TestRestrictionField extends TestCase {
    public TestRestrictionField() {
        super(TestRestrictionField.class.getName());
    }

    public static void main(String[] args) {
        TestRunner runner = new TestRunner();
        runner.doRun(runner.getTest(TestRestrictionField.class.getName()));
    }

    public void testSetGetEquality() {
        for( int i = 0; i < 65535; i++ ) {
            assertEquals(new RestrictionField(i).getRestriction(), i);
        }
    }
    
    public void testRandomSamplingOfRestrictions() {
     // TODO: Implement this one.   
    }
}
