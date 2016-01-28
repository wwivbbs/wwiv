namespace WWIV5TelnetServer
{
    partial class LogForm
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(LogForm));
            this.tabNetwork = new System.Windows.Forms.TabControl();
            this.tabPage1 = new System.Windows.Forms.TabPage();
            this.listBox1 = new System.Windows.Forms.ListBox();
            this.tabPage2 = new System.Windows.Forms.TabPage();
            this.listBox2 = new System.Windows.Forms.ListBox();
            this.tabPage3 = new System.Windows.Forms.TabPage();
            this.listBox3 = new System.Windows.Forms.ListBox();
            this.exitButton = new System.Windows.Forms.Button();
            this.label1 = new System.Windows.Forms.Label();
            this.logLines = new System.Windows.Forms.TextBox();
            this.refreshButton = new System.Windows.Forms.Button();
            this.tabPage4 = new System.Windows.Forms.TabPage();
            this.tabPage5 = new System.Windows.Forms.TabPage();
            this.listBox4 = new System.Windows.Forms.ListBox();
            this.listBox5 = new System.Windows.Forms.ListBox();
            this.tabNetwork.SuspendLayout();
            this.tabPage1.SuspendLayout();
            this.tabPage2.SuspendLayout();
            this.tabPage3.SuspendLayout();
            this.tabPage4.SuspendLayout();
            this.tabPage5.SuspendLayout();
            this.SuspendLayout();
            // 
            // tabNetwork
            // 
            this.tabNetwork.Controls.Add(this.tabPage1);
            this.tabNetwork.Controls.Add(this.tabPage2);
            this.tabNetwork.Controls.Add(this.tabPage3);
            this.tabNetwork.Controls.Add(this.tabPage4);
            this.tabNetwork.Controls.Add(this.tabPage5);
            this.tabNetwork.Location = new System.Drawing.Point(2, 1);
            this.tabNetwork.Name = "tabNetwork";
            this.tabNetwork.SelectedIndex = 0;
            this.tabNetwork.Size = new System.Drawing.Size(761, 335);
            this.tabNetwork.TabIndex = 0;
            // 
            // tabPage1
            // 
            this.tabPage1.Controls.Add(this.listBox1);
            this.tabPage1.Location = new System.Drawing.Point(4, 22);
            this.tabPage1.Name = "tabPage1";
            this.tabPage1.Padding = new System.Windows.Forms.Padding(3);
            this.tabPage1.Size = new System.Drawing.Size(753, 309);
            this.tabPage1.TabIndex = 0;
            this.tabPage1.Text = "Network Log";
            this.tabPage1.UseVisualStyleBackColor = true;
            // 
            // listBox1
            // 
            this.listBox1.Font = new System.Drawing.Font("Lucida Console", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.listBox1.FormattingEnabled = true;
            this.listBox1.ItemHeight = 12;
            this.listBox1.Location = new System.Drawing.Point(0, 3);
            this.listBox1.Name = "listBox1";
            this.listBox1.Size = new System.Drawing.Size(750, 304);
            this.listBox1.TabIndex = 0;
            // 
            // tabPage2
            // 
            this.tabPage2.Controls.Add(this.listBox2);
            this.tabPage2.Location = new System.Drawing.Point(4, 22);
            this.tabPage2.Name = "tabPage2";
            this.tabPage2.Padding = new System.Windows.Forms.Padding(3);
            this.tabPage2.Size = new System.Drawing.Size(753, 309);
            this.tabPage2.TabIndex = 1;
            this.tabPage2.Text = "Networkb Log";
            this.tabPage2.UseVisualStyleBackColor = true;
            // 
            // listBox2
            // 
            this.listBox2.Font = new System.Drawing.Font("Lucida Console", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.listBox2.FormattingEnabled = true;
            this.listBox2.ItemHeight = 12;
            this.listBox2.Location = new System.Drawing.Point(0, 3);
            this.listBox2.Name = "listBox2";
            this.listBox2.Size = new System.Drawing.Size(750, 304);
            this.listBox2.TabIndex = 1;
            // 
            // tabPage3
            // 
            this.tabPage3.Controls.Add(this.listBox3);
            this.tabPage3.Location = new System.Drawing.Point(4, 22);
            this.tabPage3.Name = "tabPage3";
            this.tabPage3.Size = new System.Drawing.Size(753, 309);
            this.tabPage3.TabIndex = 2;
            this.tabPage3.Text = "Net Log";
            this.tabPage3.UseVisualStyleBackColor = true;
            // 
            // listBox3
            // 
            this.listBox3.Font = new System.Drawing.Font("Lucida Console", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.listBox3.FormattingEnabled = true;
            this.listBox3.ItemHeight = 12;
            this.listBox3.Location = new System.Drawing.Point(0, 3);
            this.listBox3.Name = "listBox3";
            this.listBox3.Size = new System.Drawing.Size(750, 304);
            this.listBox3.TabIndex = 1;
            // 
            // exitButton
            // 
            this.exitButton.Font = new System.Drawing.Font("Calibri", 8.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.exitButton.Location = new System.Drawing.Point(709, 338);
            this.exitButton.Name = "exitButton";
            this.exitButton.Size = new System.Drawing.Size(50, 20);
            this.exitButton.TabIndex = 1;
            this.exitButton.Text = "EXIT";
            this.exitButton.UseVisualStyleBackColor = true;
            this.exitButton.Click += new System.EventHandler(this.exitButton_Click);
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(3, 341);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(130, 13);
            this.label1.TabIndex = 2;
            this.label1.Text = "Number of lines to display:";
            // 
            // logLines
            // 
            this.logLines.Location = new System.Drawing.Point(139, 337);
            this.logLines.Name = "logLines";
            this.logLines.Size = new System.Drawing.Size(39, 20);
            this.logLines.TabIndex = 3;
            this.logLines.Text = "1000";
            // 
            // refreshButton
            // 
            this.refreshButton.Font = new System.Drawing.Font("Calibri", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.refreshButton.Location = new System.Drawing.Point(184, 337);
            this.refreshButton.Name = "refreshButton";
            this.refreshButton.Size = new System.Drawing.Size(62, 20);
            this.refreshButton.TabIndex = 4;
            this.refreshButton.Text = "REFRESH";
            this.refreshButton.UseVisualStyleBackColor = true;
            this.refreshButton.Click += new System.EventHandler(this.button1_Click);
            // 
            // tabPage4
            // 
            this.tabPage4.Controls.Add(this.listBox4);
            this.tabPage4.Location = new System.Drawing.Point(4, 22);
            this.tabPage4.Name = "tabPage4";
            this.tabPage4.Padding = new System.Windows.Forms.Padding(3);
            this.tabPage4.Size = new System.Drawing.Size(753, 309);
            this.tabPage4.TabIndex = 3;
            this.tabPage4.Text = "Change Log";
            this.tabPage4.UseVisualStyleBackColor = true;
            // 
            // tabPage5
            // 
            this.tabPage5.Controls.Add(this.listBox5);
            this.tabPage5.Location = new System.Drawing.Point(4, 22);
            this.tabPage5.Name = "tabPage5";
            this.tabPage5.Padding = new System.Windows.Forms.Padding(3);
            this.tabPage5.Size = new System.Drawing.Size(753, 309);
            this.tabPage5.TabIndex = 4;
            this.tabPage5.Text = "What\'s New";
            this.tabPage5.UseVisualStyleBackColor = true;
            // 
            // listBox4
            // 
            this.listBox4.Font = new System.Drawing.Font("Lucida Console", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.listBox4.FormattingEnabled = true;
            this.listBox4.ItemHeight = 12;
            this.listBox4.Location = new System.Drawing.Point(0, 3);
            this.listBox4.Name = "listBox4";
            this.listBox4.Size = new System.Drawing.Size(750, 304);
            this.listBox4.TabIndex = 2;
            // 
            // listBox5
            // 
            this.listBox5.Font = new System.Drawing.Font("Lucida Console", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.listBox5.FormattingEnabled = true;
            this.listBox5.ItemHeight = 12;
            this.listBox5.Location = new System.Drawing.Point(0, 3);
            this.listBox5.Name = "listBox5";
            this.listBox5.Size = new System.Drawing.Size(750, 304);
            this.listBox5.TabIndex = 3;
            // 
            // LogForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(766, 361);
            this.Controls.Add(this.refreshButton);
            this.Controls.Add(this.logLines);
            this.Controls.Add(this.label1);
            this.Controls.Add(this.exitButton);
            this.Controls.Add(this.tabNetwork);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedSingle;
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.Name = "LogForm";
            this.Text = "WWIV 5 Logs";
            this.Load += new System.EventHandler(this.LogForm_Load);
            this.tabNetwork.ResumeLayout(false);
            this.tabPage1.ResumeLayout(false);
            this.tabPage2.ResumeLayout(false);
            this.tabPage3.ResumeLayout(false);
            this.tabPage4.ResumeLayout(false);
            this.tabPage5.ResumeLayout(false);
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.TabControl tabNetwork;
        private System.Windows.Forms.TabPage tabPage1;
        private System.Windows.Forms.ListBox listBox1;
        private System.Windows.Forms.TabPage tabPage2;
        private System.Windows.Forms.TabPage tabPage3;
        private System.Windows.Forms.ListBox listBox2;
        private System.Windows.Forms.ListBox listBox3;
        private System.Windows.Forms.Button exitButton;
        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.TextBox logLines;
        private System.Windows.Forms.Button refreshButton;
        private System.Windows.Forms.TabPage tabPage4;
        private System.Windows.Forms.TabPage tabPage5;
        private System.Windows.Forms.ListBox listBox4;
        private System.Windows.Forms.ListBox listBox5;
    }
}