/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2014-2017, WWIV Software Services             */
/*                                                                        */
/*    Licensed  under the  Apache License, Version  2.0 (the "License");  */
/*    you may not use this  file  except in compliance with the License.  */
/*    You may obtain a copy of the License at                             */
/*                                                                        */
/*                http://www.apache.org/licenses/LICENSE-2.0              */
/*                                                                        */
/*    Unless  required  by  applicable  law  or agreed to  in  writing,   */
/*    software  distributed  under  the  License  is  distributed on an   */
/*    "AS IS"  BASIS, WITHOUT  WARRANTIES  OR  CONDITIONS OF ANY  KIND,   */
/*    either  express  or implied.  See  the  License for  the specific   */
/*    language governing permissions and limitations under the License.   */
/*                                                                        */
/**************************************************************************/
using System;
using System.IO;
using System.Drawing;
using System.Linq;
using System.Windows.Forms;

namespace WWIV5TelnetServer
{
    public partial class LogForm : Form
    {
        public LogForm()
        {
            InitializeComponent();
        }

        private void LogForm_Load(object sender, EventArgs e)
        {
            // Default Number Of Lines Per Log
            int logLength  = Int32.Parse(logLines.Text);

            // Network Log
            string errorText1 = " ERROR: ";
            var i1 = 0;
            // From Bottom To Top
            var lines1 = File.ReadAllLines(Properties.Settings.Default.homeDirectory + @"\network.log").Reverse().Skip(1);

            foreach (string line1 in lines1)
            {
                // Catch ERROR: And Color Orange Else Blue
                if (line1.Contains(errorText1))
                {
                    listBox1.ForeColor = Color.FromArgb(230, 159, 0);
                    this.listBox1.Items.Add(line1);
                }
                else
                {
                    listBox1.ForeColor = Color.FromArgb(0, 114, 178);
                    this.listBox1.Items.Add(line1);
                }
                i1++;
                if (i1 >= logLength) break;
            }

            // Networkb Log
            string errorText2 = " ERROR: ";
            var i2 = 0;
            // From Bottom To Top
            var lines2 = File.ReadAllLines(Properties.Settings.Default.homeDirectory + @"\networkb.log").Reverse().Skip(1);

            foreach (string line2 in lines2)
            {
                // Catch ERROR: And Color Orange Else Blue
                if (line2.Contains(errorText2))
                {
                    listBox2.ForeColor = Color.FromArgb(230, 159, 0);
                    this.listBox2.Items.Add(line2);
                }
                else
                {
                    listBox2.ForeColor = Color.FromArgb(0, 114, 178);
                    this.listBox2.Items.Add(line2);
                }
                i2++;
                if (i2 >= logLength) break;
            }

            // Net.log
            string errorText3 = " ERROR: ";
            var i3 = 0;
            // From Bottom To Top
            var lines3 = File.ReadAllLines(Properties.Settings.Default.homeDirectory + @"\gfiles\NET.LOG").Reverse().Skip(1);

            foreach (string line3 in lines3)
            {
                // Default Color Blue
                listBox3.ForeColor = Color.FromArgb(0, 114, 178);

                // Catch ERROR: And Color Orange Else Blue
                if (line3.Contains(errorText3))
                {
                    listBox3.ForeColor = Color.FromArgb(230, 159, 0);
                }
                this.listBox3.Items.Add(line3);
                i3++;
                if (i3 >= logLength) break;
            }

            // Change Log
            var lines4 = File.ReadAllLines(Properties.Settings.Default.homeDirectory + @"\changelog.txt");

            foreach (string line4 in lines4)
            {
                // Default Color Black
                listBox4.ForeColor = Color.Black;

                this.listBox4.Items.Add(line4);
            }

            // What's New
            var lines5 = File.ReadAllLines(Properties.Settings.Default.homeDirectory + @"\whatsnew.txt");

            foreach (string line5 in lines5)
            {
                // Default Color Black
                listBox5.ForeColor = Color.Black;

                this.listBox5.Items.Add(line5);
            }
        }

        private void exitButton_Click(object sender, EventArgs e)
        {
            Close();
        }

        private void button1_Click(object sender, EventArgs e)
        {
            Refresh();
        }
    }
}
