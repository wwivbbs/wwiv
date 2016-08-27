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
using System.ComponentModel;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Threading;

namespace WWIV5TelnetServer
{
  class Launcher
  {
    string executable;
    string homeDirectory;
    string argumentsTemplate;
    Action<string> logger;

    public Launcher(string executable, string homeDirectory, string argumentsTemplate, Action<string> logger)
    {
      this.executable = executable;
      this.homeDirectory = homeDirectory;
      this.argumentsTemplate = argumentsTemplate;
      this.logger = logger;
    }

    private const int SW_MINIMIZE = 6;
    [System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Design", "CA1060:MovePInvokesToNativeMethodsClass"), DllImport("user32.dll")]
    private static extern int ShowWindowAsync(IntPtr hWnd, int nCmdShow);

    public Process launchLocalNode(int node)
    {
      return launchNode("-N@N -M", node, -1, false);
    }

    public Process launchBeginDayEvent(int node)
    {
      return launchNode("-N@N -E", node, -1, false);
    }

    public Process launchSocketNode(int node, Int32 socketHandle)
    {
      return launchNode(argumentsTemplate, node, socketHandle, Properties.Settings.Default.launchMinimized);
    }

    Process launchNode(String template, int node, Int32 socketHandle, bool minimize)
    {
      CommandLineBuilder cmdlineBuilder = new CommandLineBuilder(template, executable);

      try
      {
        Process p = new Process();
        p.EnableRaisingEvents = false;
        p.StartInfo.FileName = executable;
        p.StartInfo.Arguments = cmdlineBuilder.CreateTelnetArguments(node, socketHandle);
        p.StartInfo.WorkingDirectory = homeDirectory;
        p.StartInfo.UseShellExecute = false;
        logger("Launching binary: " + cmdlineBuilder.CreateFullCommandLine(node, socketHandle));
        p.Start();
        Console.WriteLine("binary launched.");

        if (minimize)
        {
          for (int i = 0; i < 25; i++)
          {
            // The process is launched asynchronously, so wait for up to a second
            // for the main window handle to be created and set on the process class.
            if (p.MainWindowHandle.ToInt32() != 0)
            {
              break;
            }
            Thread.Sleep(100);
          }
          logger("Trying to minimize process on handle:" + p.MainWindowHandle);
          ShowWindowAsync(p.MainWindowHandle, SW_MINIMIZE);
        }
        return p;
      }
      catch (Win32Exception e)
      {
        // Failed to launch
        Console.WriteLine(e);
      }
      return null;
    }
  }
}
