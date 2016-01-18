/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2014-2016 WWIV Software Services              */
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
using System.Diagnostics;
using System.Timers;

namespace WWIV5TelnetServer
{
    class BeginDayHandler
    {
        private Timer timer;
        private Action<string> logger;

        public BeginDayHandler(Action<string> logger)
        {
            timer = new Timer(1000 * 60); // 60 seconds
            timer.Elapsed += new ElapsedEventHandler(OnTimerEvent);
            timer.Enabled = true;

            this.logger = logger;
            OnTimerEvent(this, null);
        }

        private void OnTimerEvent(object source, ElapsedEventArgs e)
        {
            string currentDate = DateTime.Now.ToString("yyyy-MM-dd");
            Console.WriteLine("Checking for beginday event: current date = " + currentDate + "; last = " + Properties.Settings.Default.lastBeginDayDate);

            if (Properties.Settings.Default.lastBeginDayDate != currentDate)
            {
                Console.WriteLine("Last date = " + Properties.Settings.Default.lastBeginDayDate);
                Properties.Settings.Default.lastBeginDayDate = currentDate;
                if (Properties.Settings.Default.useBegindayEvent)
                {
                    logger.Invoke("Running BeginDay Event");
                    launchBeginDayEvent();
                }
                else
                {
                    logger.Invoke("Skipping BeginDay Event (not enabled).");
                }
                Properties.Settings.Default.Save();
            }
        }

        private void launchBeginDayEvent()
        {
            var executable = Properties.Settings.Default.executable;
            var argumentsTemplate = Properties.Settings.Default.parameters;
            var homeDirectory = Properties.Settings.Default.homeDirectory;

            Launcher launcher = new Launcher(executable, homeDirectory, argumentsTemplate, logger);
            Process p = launcher.launchBeginDayEvent(Convert.ToInt32(Properties.Settings.Default.localNode));
            p.WaitForExit();
        }

        private void DoUpdateCheck(object source, ElapsedEventArgs e)
        {
            //Save logic
        }
    }
}
