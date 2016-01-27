using System;
using System.IO;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
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
            // Network Log
            // Pass The File Path and File Name To The StreamReader Constructor
            StreamReader sr1 = new StreamReader(Properties.Settings.Default.homeDirectory + @"\network.log");
            string line1 = string.Empty;
            try
            {
                // Read The First Line Of Text
                line1 = sr1.ReadLine();
                string errorText = "ERROR:";

                // Continue To Read Till End Of File
                while (line1 != null)
                {
                    // Catch ERROR: And Color Red Else Green
                    if (!line1.Contains(errorText))
                    {
                        listBox1.ForeColor = Color.Green;
                    }
                    else
                    {
                        listBox1.ForeColor = Color.DarkRed;
                    }

                    // Create List Item
                    this.listBox1.Items.Add(line1);

                    // Read The Next Line
                    line1 = sr1.ReadLine();
                }

                //close the file
                sr1.Close();
            }
            catch (Exception ex1)
            {
                MessageBox.Show(ex1.Message.ToString());
            }
            finally
            {
                //close the file
                sr1.Close();
            }

            // Networkb Log
            string errorText2 = " ERROR: ";
            //const int max = 500;
            // From Bottom To Top
            var lines2 = File.ReadAllLines(Properties.Settings.Default.homeDirectory + @"\networkb.log").Reverse();

            foreach (string line2 in lines2)
            {
                // Default Color Green
                listBox2.ForeColor = Color.Green;

                // Catch ERROR: And Color Red Else Green
                if (line2.Contains(errorText2))
                {
                    listBox2.ForeColor = Color.DarkRed;
                }
                this.listBox2.Items.Add(line2);
            }

            /*FileStream fs = new FileStream(Properties.Settings.Default.homeDirectory + @"\networkb.log", FileMode.Open, FileAccess.Read, FileShare.ReadWrite);
            fs = fs.Seek(-20000, SeekOrigin.End);
            StreamReader sr = new StreamReader(fs);
            string x = sr.ReadToEnd();
            sr.Close();

            // Pass The File Path and File Name To The StreamReader Constructor
            StreamReader sr2 = new StreamReader(Properties.Settings.Default.homeDirectory + @"\networkb.log");
            string line2 = string.Empty;
            try
            {
                // Read The First Line Of Text
                line2 = sr2.ReadLine();
                string errorText = "ERROR:";

                // Continue To Read Till End Of File
                while (line2 != null)
                {
                    // Catch ERROR: And Color Red Else Green
                    if (!line2.Contains(errorText))
                    {
                        listBox2.ForeColor = Color.Green;
                    }
                    else
                    {
                        listBox2.ForeColor = Color.DarkRed;
                    }

                    // Create List Item
                    this.listBox2.Items.Add(line2);

                    // Read The Next Line
                    line2 = sr2.ReadLine();
                }

                //close the file
                sr2.Close();
            }
            catch (Exception ex2)
            {
                MessageBox.Show(ex2.Message.ToString());
            }
            finally
            {
                //close the file
                sr2.Close();
            }*/

            // WWIVSync Log
            // Pass The File Path and File Name To The StreamReader Constructor
            StreamReader sr3 = new StreamReader(Properties.Settings.Default.homeDirectory + @"\wwivsync.log");
            string line3 = string.Empty;
            try
            {
                // Read The First Line Of Text
                line3 = sr3.ReadLine();
                string errorText = "ERROR:";

                // Continue To Read Till End Of File
                while (line3 != null)
                {
                    // Catch ERROR: And Color Red Else Green
                    if (!line3.Contains(errorText))
                    {
                        listBox3.ForeColor = Color.Green;
                    }
                    else
                    {
                        listBox3.ForeColor = Color.DarkRed;
                    }

                    // Create List Item
                    this.listBox3.Items.Add(line3);

                    // Read The Next Line
                    line3 = sr3.ReadLine();
                }

                //close the file
                sr3.Close();
            }
            catch (Exception ex3)
            {
                MessageBox.Show(ex3.Message.ToString());
            }
            finally
            {
                //close the file
                sr3.Close();
            }
        }
    }
}
