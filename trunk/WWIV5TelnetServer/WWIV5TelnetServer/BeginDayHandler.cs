using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Timers;

namespace WWIV5TelnetServer
{
  class BeginDayHandler
  {
    private System.Timers.Timer timer;
    private Action<string> logger;

    public BeginDayHandler(Action<string> logger)
    {
      timer = new System.Timers.Timer(60 * 1000); // 10 seconds
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

  }
}
